/* Devices and system features a PC does not have: PlayStation VR (Hmd, VrTracker), the camera,
 * Move controllers, screenshots, video recording, live streaming, Remote Play and Share Play;
 * the Razor CPU profiler; PNG encoding. Nothing is connected and nothing is recorded; return
 * values and error codes follow shadPS4's libraries (hmd, vr_tracker, camera, move, ...).
 * PngEnc writes real PNG files with stored (uncompressed) deflate blocks. */
#include "runtime_offline.h"
#include <stdio.h>
#include <string.h>

/* ---- PlayStation VR headset: no headset is detected. ---- */
#define HMD_NOT_INITIALIZED ((int32_t)0x81110002)
#define HMD_HANDLE_INVALID ((int32_t)0x81110003)
#define HMD_ALREADY_INITIALIZED ((int32_t)0x81110006)
#define HMD_PARAMETER_NULL ((int32_t)0x81110008)
#define HMD_STATUS_NOT_DETECTED 2
#define HMD_HANDLE 0xf000000 /* shadPS4: positive, unlike the other devices' handles */
static int hmd_initialized;
static int32_t hmd_user = -1, hmd_handle;
static ABI int32_t hmd_initialize(const void *param) {
    if (hmd_initialized) return HMD_ALREADY_INITIALIZED;
    if (!param) return HMD_PARAMETER_NULL;
    hmd_initialized = 1;
    note("PlayStation VR initialized; no headset is connected");
    return 0;
}
static ABI int32_t hmd_terminate(void) {
    if (!hmd_initialized) return HMD_NOT_INITIALIZED;
    hmd_initialized = 0;
    return 0;
}
/* The SDK opens the headset port whether or not one is connected; the device information
 * then says it is not detected. */
static ABI int32_t hmd_open(int32_t user, int32_t type, int32_t index, const void *param) {
    (void)type; (void)index; (void)param;
    if (!hmd_initialized) return HMD_NOT_INITIALIZED;
    hmd_user = user;
    hmd_handle = HMD_HANDLE;
    return hmd_handle;
}
static ABI int32_t hmd_close(int32_t handle) {
    if (!hmd_initialized) return HMD_NOT_INITIALIZED;
    if (handle != hmd_handle || !handle) return HMD_HANDLE_INVALID;
    hmd_handle = 0; hmd_user = -1;
    return 0;
}
/* OrbisHmdDeviceInformation (32 bytes): status, user id, reserved, panel size and latency,
 * HMU mount. */
static ABI int32_t hmd_device_information(void *info) {
    if (!info) return HMD_PARAMETER_NULL;
    if (!hmd_initialized) return HMD_NOT_INITIALIZED;
    uint32_t words[2] = {HMD_STATUS_NOT_DETECTED, (uint32_t)hmd_user};
    memset(info, 0, 32);
    memcpy(info, words, sizeof(words));
    return 0;
}
/* SDK 4.50 and later: without a headset these report an invalid handle (shadPS4). */
static ABI int32_t hmd_by_handle(int32_t handle, void *out) { (void)handle; (void)out; return HMD_HANDLE_INVALID; }
static ABI int32_t hmd_garlic_align(void) { return 0x100; }
static ABI int32_t hmd_garlic_size(void) { return 0x100000; }
static ABI int32_t hmd_onion_align(void) { return 0x100; }
static ABI int32_t hmd_onion_size(void) { return 0x810; }

/* ---- VR tracker: initializes, never tracks (the camera gives no frames). ---- */
#define VRT_NOT_INIT ((int32_t)0x81260801)
#define VRT_ALREADY_INITIALIZED ((int32_t)0x81260802)
#define VRT_ARGUMENT_INVALID ((int32_t)0x81260806)
#define VRT_NOT_EXECUTE_GPU_SUBMIT ((int32_t)0x81260815)
static int vrt_initialized;
/* OrbisVrTrackerQueryMemoryResult: size, then onion size/alignment, garlic size/alignment,
 * work size/alignment. The onion size is the larger one (Move calibration on). */
static ABI int32_t vrt_query_memory(const void *param, uint32_t *result) {
    if (!param || !result) return VRT_ARGUMENT_INVALID;
    const uint32_t sizes[6] = {0x800000, 0x10000, 0x3000000, 0x10000, 0x1000000, 0x10000};
    memcpy(result + 1, sizes, sizeof(sizes));
    return 0;
}
static ABI int32_t vrt_init(const void *param) {
    if (vrt_initialized) return VRT_ALREADY_INITIALIZED;
    if (!param) return VRT_ARGUMENT_INVALID;
    vrt_initialized = 1;
    return 0;
}
static ABI int32_t vrt_term(void) {
    if (!vrt_initialized) return VRT_NOT_INIT;
    vrt_initialized = 0;
    return 0;
}
static ABI int32_t vrt_ok(void) { return vrt_initialized ? 0 : VRT_NOT_INIT; }
static ABI int32_t vrt_gpu_submit(const void *param) { (void)param; return vrt_initialized ? VRT_ARGUMENT_INVALID : VRT_NOT_INIT; }
static ABI int32_t vrt_gpu_wait(void) { return vrt_initialized ? VRT_NOT_EXECUTE_GPU_SUBMIT : VRT_NOT_INIT; }
static ABI int32_t vrt_get_time(uint64_t *time) {
    if (!vrt_initialized) return VRT_NOT_INIT;
    if (!time) return VRT_ARGUMENT_INVALID;
    *time = 0;
    return 0;
}

/* ---- PlayStation Camera and Move: none connected. ---- */
#define CAMERA_NOT_OPEN ((int32_t)0x802E0004)
#define CAMERA_NOT_CONNECTED ((int32_t)0x802E0010)
#define MOVE_NO_CONTROLLER_CONNECTED 1 /* shadPS4 move_error.h */
static ABI int32_t camera_open(int32_t user, int32_t type, int32_t index, const void *param) {
    (void)user; (void)type; (void)index; (void)param;
    return CAMERA_NOT_CONNECTED;
}
static ABI int32_t camera_not_open(void) { return CAMERA_NOT_OPEN; }
static ABI int32_t move_not_connected(void) { return MOVE_NO_CONTROLLER_CONNECTED; }

/* ---- Capture, streaming and remote features: allowed, never active. ---- */
/* OrbisGameLiveStreamingStatus2 (72 bytes): user id, then off air with no spectators. */
static ABI int32_t live_status2(void *status) {
    if (status) { memset(status, 0, 72); memcpy(status, &(int32_t){USER_ID}, 4); }
    return 0;
}
static ABI int32_t remoteplay_status(int32_t user, int32_t *status) {
    (void)user;
    if (status) *status = 0; /* disconnected */
    return 0;
}

/* ---- PNG encoder ---- */
#define PNG_INVALID_ADDR ((int32_t)0x80690101)
#define PNG_INVALID_SIZE ((int32_t)0x80690102)
#define PNG_INVALID_PARAM ((int32_t)0x80690103)
#define PNG_INVALID_HANDLE ((int32_t)0x80690104)
#define PNG_DATA_OVERFLOW ((int32_t)0x80690110)
typedef struct { uint32_t this_size, attribute, max_width, max_filters; } PngCreateParam;
typedef struct {
    const uint8_t *image; uint8_t *png;
    uint32_t image_size, png_size, width, height, pitch;
    uint16_t pixel_format, color_space, bit_depth, clut_number, filter_type, compression_level;
} PngEncodeParam;
typedef struct { uint32_t data_size, processed_height; } PngOutputInfo; /* image_pitch: bytes per row */
#define PNG_CHUNK_PIXELS 512 /* pixels converted per step of a row */
#define PNG_HANDLE_MAGIC 0x504e4745u /* "PNGE" in the caller's memory */

static ABI int32_t png_query_memory(const PngCreateParam *param) {
    if (!param) return PNG_INVALID_ADDR;
    if (param->this_size != sizeof(*param) || !param->max_width) return PNG_INVALID_PARAM;
    return 64;
}
static ABI int32_t png_create(const PngCreateParam *param, void *memory, uint32_t size, void **handle) {
    if (!param || !memory || !handle) return PNG_INVALID_ADDR;
    if (size < 64) return PNG_INVALID_SIZE;
    uint32_t header[2] = {PNG_HANDLE_MAGIC, param->max_width};
    memcpy(memory, header, sizeof(header));
    *handle = memory;
    return 0;
}
static ABI int32_t png_delete(void *handle) {
    uint32_t magic;
    if (!handle) return PNG_INVALID_HANDLE;
    memcpy(&magic, handle, 4);
    if (magic != PNG_HANDLE_MAGIC) return PNG_INVALID_HANDLE;
    memset(handle, 0, 8);
    return 0;
}
static uint32_t crc_table[256];
static uint32_t crc(uint32_t c, const uint8_t *p, size_t n) {
    if (!crc_table[1])
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t v = i;
            for (int k = 0; k < 8; ++k) v = v & 1 ? 0xedb88320u ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
    c = ~c;
    while (n--) c = crc_table[(c ^ *p++) & 255] ^ (c >> 8);
    return ~c;
}
typedef struct { uint8_t *out; size_t size, at; int overflow; } PngWriter;
static void put(PngWriter *w, const void *data, size_t n) {
    if (w->overflow || n > w->size - w->at) { w->overflow = 1; return; }
    memcpy(w->out + w->at, data, n);
    w->at += n;
}
static void put32(PngWriter *w, uint32_t v) { uint8_t b[4] = {v >> 24, v >> 16, v >> 8, v}; put(w, b, 4); }
static void chunk_end(PngWriter *w, size_t start) {
    if (w->overflow) return;
    uint32_t length = (uint32_t)(w->at - start - 8);
    uint8_t b[4] = {length >> 24, length >> 16, length >> 8, length};
    memcpy(w->out + start, b, 4);
    put32(w, crc(0, w->out + start + 4, length + 4));
}
static ABI int32_t png_encode(void *handle, const PngEncodeParam *p, PngOutputInfo *info) {
    uint32_t magic;
    if (!handle) return PNG_INVALID_HANDLE;
    memcpy(&magic, handle, 4);
    if (magic != PNG_HANDLE_MAGIC) return PNG_INVALID_HANDLE;
    if (!p || !p->image || !p->png) return PNG_INVALID_ADDR;
    int alpha = p->color_space == 19;
    if ((p->color_space != 3 && !alpha) || p->pixel_format > 1 || p->bit_depth != 8 || !p->width || !p->height ||
        p->pitch / 4 < p->width || (uint64_t)p->pitch * (p->height - 1) + (uint64_t)p->width * 4 > p->image_size)
        return PNG_INVALID_PARAM;
    const unsigned channels = alpha ? 4 : 3;
    const uint64_t row = 1 + (uint64_t)p->width * channels, raw = row * p->height;
    PngWriter w = {p->png, p->png_size, 0, 0};
    put(&w, "\x89PNG\r\n\x1a\n", 8);
    size_t start = w.at;
    put(&w, "\0\0\0\0IHDR", 8);
    put32(&w, p->width); put32(&w, p->height);
    put(&w, (uint8_t[5]){8, alpha ? 6 : 2, 0, 0, 0}, 5);
    chunk_end(&w, start);
    start = w.at;
    put(&w, "\0\0\0\0IDAT", 8);
    put(&w, "\x78\x01", 2); /* zlib header: deflate, no compression */
    uint32_t a = 1, b = 0;
    uint8_t line[1 + 4 * PNG_CHUNK_PIXELS]; /* small: guest threads may have 64 KiB stacks */
    uint64_t left = raw;
    size_t block_left = 0;
    for (uint32_t y = 0; y < p->height && !w.overflow; ++y) {
        const uint8_t *src = p->image + (uint64_t)y * p->pitch; /* pitch in bytes */
        for (uint32_t x0 = 0; x0 < p->width || x0 == 0; x0 += PNG_CHUNK_PIXELS) {
            size_t n = 0;
            if (!x0) line[n++] = 0; /* filter: none */
            for (uint32_t x = x0; x < p->width && x < x0 + PNG_CHUNK_PIXELS; ++x) {
                const uint8_t *px = src + (uint64_t)x * 4;
                line[n++] = px[p->pixel_format ? 2 : 0]; line[n++] = px[1]; line[n++] = px[p->pixel_format ? 0 : 2];
                if (alpha) line[n++] = px[3];
            }
            for (size_t i = 0; i < n; ++i) { a = (a + line[i]) % 65521; b = (b + a) % 65521; }
            for (size_t i = 0; i < n;) {
                if (!block_left) { /* stored block header: final flag, LEN, NLEN */
                    block_left = left < 65535 ? (size_t)left : 65535;
                    uint16_t len = (uint16_t)block_left, nlen = (uint16_t)~len;
                    put(&w, (uint8_t[5]){left <= 65535, len & 255, len >> 8, nlen & 255, nlen >> 8}, 5);
                }
                size_t m = n - i < block_left ? n - i : block_left;
                put(&w, line + i, m);
                i += m; block_left -= m; left -= m;
            }
            if (p->width <= x0 + PNG_CHUNK_PIXELS) break;
        }
    }
    put32(&w, b << 16 | a);
    chunk_end(&w, start);
    start = w.at;
    put(&w, "\0\0\0\0IEND", 8);
    chunk_end(&w, start);
    if (w.overflow) return PNG_DATA_OVERFLOW;
    if (info) { info->data_size = (uint32_t)w.at; info->processed_height = p->height; }
    return (int32_t)w.at;
}

static const RuntimeExport peripherals_exports[] = {
    {"sceHmdInitialize315", hmd_initialize}, {"sceHmdTerminate", hmd_terminate},
    {"sceHmdOpen", hmd_open}, {"sceHmdClose", hmd_close},
    {"sceHmdGetDeviceInformation", hmd_device_information},
    {"sceHmdGetDeviceInformationByHandle", hmd_by_handle}, {"sceHmdGetFieldOfView", hmd_by_handle},
    {"sceHmdReprojectionInitialize", ok_void}, {"sceHmdReprojectionFinalize", ok_void},
    {"sceHmdReprojectionStart", ok_void}, {"sceHmdReprojectionStop", ok_void},
    {"sceHmdReprojectionSetDisplayBuffers", ok_void}, {"sceHmdReprojectionUnsetDisplayBuffers", ok_void},
    {"sceHmdReprojectionSetUserEventStart", ok_void},
    {"sceHmdReprojectionQueryGarlicBuffAlign", hmd_garlic_align}, {"sceHmdReprojectionQueryGarlicBuffSize", hmd_garlic_size},
    {"sceHmdReprojectionQueryOnionBuffAlign", hmd_onion_align}, {"sceHmdReprojectionQueryOnionBuffSize", hmd_onion_size},
    {"sceVrTrackerQueryMemory", vrt_query_memory}, {"sceVrTrackerInit", vrt_init}, {"sceVrTrackerTerm", vrt_term},
    {"sceVrTrackerRegisterDevice", vrt_ok}, {"sceVrTrackerUnregisterDevice", vrt_ok},
    {"sceVrTrackerRecalibrate", vrt_ok}, {"sceVrTrackerUpdateMotionSensorData", vrt_ok},
    {"sceVrTrackerGetResult", vrt_ok}, {"sceVrTrackerGpuSubmit", vrt_gpu_submit},
    {"sceVrTrackerGpuWaitAndCpuProcess", vrt_gpu_wait}, {"sceVrTrackerGetTime", vrt_get_time},
    {"sceCameraOpen", camera_open}, {"sceCameraStart", camera_not_open}, {"sceCameraStop", camera_not_open},
    {"sceCameraSetConfig", camera_not_open}, {"sceCameraSetVideoSync", camera_not_open},
    {"sceCameraGetFrameData", camera_not_open},
    {"sceMoveGetDeviceInfo", move_not_connected}, {"sceMoveReadStateLatest", move_not_connected},
    {"sceScreenShotEnable", ok_void}, {"sceScreenShotDisable", ok_void}, {"sceScreenShotSetParam", ok_void},
    {"sceScreenShotSetOverlayImage", ok_void}, {"sceScreenShotSetOverlayImageWithOrigin", ok_void},
    {"sceVideoRecordingOpen2", ok_void}, {"sceVideoRecordingClose", ok_void},
    {"sceVideoRecordingStart", ok_void}, {"sceVideoRecordingStop", ok_void},
    {"sceVideoRecordingSetInfo", ok_void}, {"sceVideoRecordingGetStatus", ok_void},
    {"sceGameLiveStreamingEnableLiveStreaming", ok_void}, {"sceGameLiveStreamingGetCurrentStatus2", live_status2},
    {"sceGameLiveStreamingGetSocialFeedbackMessages", ok_void},
    {"sceGameLiveStreamingGetSocialFeedbackMessagesCount", ok_void},
    {"sceGameLiveStreamingSetPresetSocialFeedbackCommands", ok_void},
    {"sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription", ok_void},
    {"sceRemoteplayInitialize", ok_void}, {"sceRemoteplayTerminate", ok_void},
    {"sceRemoteplayProhibitStreaming", ok_void}, {"sceRemoteplayGetConnectionStatus", remoteplay_status},
    {"sceSharePlayInitialize", ok_void}, {"sceSharePlayTerminate", ok_void}, {"sceSharePlaySetProhibition", ok_void},
    {"sceRazorCpuPushMarkerStatic", ok_void}, {"sceRazorCpuPopMarker", ok_void},
    {"scePngEncQueryMemorySize", png_query_memory}, {"scePngEncCreate", png_create},
    {"scePngEncDelete", png_delete}, {"scePngEncEncode", png_encode},
};
uintptr_t runtime_peripherals_resolve(const char *name) { return RUNTIME_LOOKUP(peripherals_exports, name); }
