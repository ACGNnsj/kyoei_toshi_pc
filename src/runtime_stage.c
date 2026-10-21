/* Modules the game loads and unloads itself (sceKernelLoadStartModule): City Shrouded in Shadow
 * keeps one StageModule.prx per stage, each exporting the same function, which the eboot calls
 * through one import. link_modules.py placed and relocated every stage module in the image and
 * wrote out/stages.bin; here a load restores the module's writable segments to their linked
 * bytes, runs its initializer and points the shared import at its export (a jump through
 * vw_stage_target). Loading a module that was linked into the image only gives a handle. */
#include "runtime.h"
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ERR(n) ((int32_t)(UINT32_C(0x80020000)|(n)))
#define HANDLE_BASE 0x1000
#define MAX_STAGES 64

typedef struct {
    char path[128];
    uint64_t base, size, init, fini, export, writable_count, writable[4][2];
    unsigned char *pristine[4];
    int loaded;
} Stage;

static unsigned char *image;
static Stage stages[MAX_STAGES];
static uint64_t stage_count;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static ABI __attribute__((noreturn)) void stage_unbound(void) {
    fputs("STOP: the game called its stage module's export with no stage module loaded\n", stderr);
    runtime_report();
    exit(23);
}
/* The eboot's StageModule import: a jump to the export of the stage module loaded last. */
extern uintptr_t vw_stage_target;
uintptr_t vw_stage_target = (uintptr_t)stage_unbound;
void vw_stage_trampoline(void);
__asm__(".text\n.globl vw_stage_trampoline\nvw_stage_trampoline:\n\tjmp *vw_stage_target(%rip)\n");

static int read_all(FILE *f, void *buffer, size_t size) { return fread(buffer, 1, size, f) == size; }

void runtime_stage_configure(unsigned char *linked_image, uint64_t image_size, const char *table) {
    image = linked_image;
    FILE *f = fopen(table, "rb");
    if (!f) return; /* a game without stage modules */
    char magic[8];
    uint64_t count = 0;
    if (!read_all(f, magic, 8) || memcmp(magic, "VWSTAGE1", 8) || !read_all(f, &count, 8) || count > MAX_STAGES) {
        fprintf(stderr, "Runtime: %s is not a stage module table; stage modules disabled\n", table);
        fclose(f);
        return;
    }
    for (uint64_t i = 0; i < count; ++i) {
        Stage *s = &stages[i];
        uint64_t fields[14];
        if (!read_all(f, s->path, sizeof(s->path)) || !read_all(f, fields, sizeof(fields))) break;
        s->path[sizeof(s->path) - 1] = 0;
        s->base = fields[0]; s->size = fields[1]; s->init = fields[2]; s->fini = fields[3]; s->export = fields[4];
        s->writable_count = fields[5] > 4 ? 0 : fields[5];
        if (s->base > image_size || s->size > image_size - s->base) break;
        for (uint64_t w = 0; w < s->writable_count; ++w) {
            s->writable[w][0] = fields[6 + 2 * w];
            s->writable[w][1] = fields[7 + 2 * w];
            if (s->writable[w][0] < s->base || s->writable[w][1] > s->base + s->size - s->writable[w][0]) break;
            /* The bytes as linked (relocated, imports bound) are what each load starts from. */
            s->pristine[w] = malloc(s->writable[w][1] ? s->writable[w][1] : 1);
            if (!s->pristine[w]) break;
            memcpy(s->pristine[w], image + s->writable[w][0], s->writable[w][1]);
        }
        stage_count = i + 1;
    }
    fclose(f);
    printf("Runtime: %" PRIu64 " loadable modules (%s)\n", stage_count, table);
}

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int same(const char *a, const char *b) {
    while (*a && lower((unsigned char)*a) == lower((unsigned char)*b)) ++a, ++b;
    return !*a && !*b;
}
/* Guest paths are /app0/... (PS4 file names compare without case); a linked module is also
 * found by its file name alone, as the game may load a system module by another folder. */
static Stage *find(const char *path) {
    if (!path) return NULL;
    const char *relative = path;
    if (!strncmp(relative, "/app0/", 6)) relative += 6;
    else if (!strncmp(relative, "/hostapp/", 9)) relative += 9;
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    for (uint64_t i = 0; i < stage_count; ++i) {
        if (same(stages[i].path, relative)) return &stages[i];
        if (!stages[i].export) {
            const char *linked = strrchr(stages[i].path, '/');
            if (same(linked ? linked + 1 : stages[i].path, name)) return &stages[i];
        }
    }
    return NULL;
}
static void retarget(void) {
    uintptr_t target = (uintptr_t)stage_unbound;
    for (uint64_t i = 0; i < stage_count; ++i)
        if (stages[i].loaded && stages[i].export) target = (uintptr_t)(image + stages[i].export);
    __atomic_store_n(&vw_stage_target, target, __ATOMIC_RELEASE);
}

/* DT_INIT and DT_FINI both take (args, argp, entry): a non-NULL entry is called (init tail-jumps
 * to it, fini calls it) instead of the module's own module_start/module_stop. Always NULL here:
 * a fini called with only two arguments called whatever rdx held (a stage change faulted). */
typedef int32_t (ABI *ModuleEntry)(uint64_t args, const void *argp, void *entry);
static ABI int32_t load_start_module(const char *path, uint64_t args, const void *argp, uint32_t flags,
                                     const void *opt, int32_t *result) {
    (void)flags; (void)opt;
    Stage *s = find(path);
    if (!s) {
        printf("Runtime: sceKernelLoadStartModule(%s): no such module\n", path ? path : "(null)");
        return ERR(2); /* ENOENT */
    }
    int32_t handle = HANDLE_BASE + (int32_t)(s - stages);
    if (!s->export) {
        printf("Runtime: sceKernelLoadStartModule(%s): linked into the image, handle %d\n", path, handle);
        if (result) *result = 0;
        return handle;
    }
    pthread_mutex_lock(&lock);
    if (s->loaded) {
        pthread_mutex_unlock(&lock);
        return ERR(17); /* EEXIST: the PS4 refuses a second load of the same file too */
    }
    for (uint64_t w = 0; w < s->writable_count; ++w)
        memcpy(image + s->writable[w][0], s->pristine[w], s->writable[w][1]);
    pthread_mutex_unlock(&lock);
    int32_t started = ((ModuleEntry)(image + s->init))(args, argp, NULL);
    printf("Runtime: sceKernelLoadStartModule(%s): handle %d, module start returned %d\n", path, handle, started);
    if (result) *result = started;
    pthread_mutex_lock(&lock);
    s->loaded = 1;
    retarget();
    pthread_mutex_unlock(&lock);
    return handle;
}
static ABI int32_t stop_unload_module(int32_t handle, uint64_t args, const void *argp, uint32_t flags,
                                      const void *opt, int32_t *result) {
    (void)flags; (void)opt;
    uint64_t index = (uint64_t)(handle - HANDLE_BASE);
    if (handle < HANDLE_BASE || index >= stage_count) {
        printf("Runtime: sceKernelStopUnloadModule(%d): no such module\n", handle);
        return ERR(3); /* ESRCH */
    }
    Stage *s = &stages[index];
    if (!s->export) {
        if (result) *result = 0;
        return 0; /* a linked module stays */
    }
    pthread_mutex_lock(&lock);
    int loaded = s->loaded;
    s->loaded = 0;
    retarget(); /* before its stop: the export is no longer called into */
    pthread_mutex_unlock(&lock);
    if (!loaded) return ERR(3);
    int32_t stopped = s->fini ? ((ModuleEntry)(image + s->fini))(args, argp, NULL) : 0;
    printf("Runtime: sceKernelStopUnloadModule(%s): module stop returned %d\n", s->path, stopped);
    if (result) *result = stopped;
    return 0;
}

static const RuntimeExport stage_exports[] = {
    {"sceKernelLoadStartModule", load_start_module}, {"sceKernelStopUnloadModule", stop_unload_module},
};
uintptr_t runtime_stage_resolve(const char *name) {
    /* The eboot imports the export of StageModule.prx under that module's own library name. */
    if (!strcmp(name, "6rNUbZHCWM0#StageModule")) return (uintptr_t)vw_stage_trampoline;
    return RUNTIME_LOOKUP(stage_exports, name);
}
