"""Where link_modules.py finds a game's bundled modules, without game binaries."""
from paths import ROOT  # noqa: F401
from pathlib import Path
import tempfile
import unittest
import game_profiles
import link_modules


class ModulePathTests(unittest.TestCase):
    def test_sce_module_first_then_module_prx(self):
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp)
            (game / 'sce_module').mkdir()
            (game / 'module/prx').mkdir(parents=True)
            (game / 'sce_module/libc.prx').write_bytes(b'a')
            (game / 'module/prx/libc.prx').write_bytes(b'b')
            (game / 'module/prx/libSceSecure.prx').write_bytes(b'c')
            self.assertEqual(link_modules.module_path(game, 'libc.prx'), game / 'sce_module/libc.prx')
            self.assertEqual(link_modules.module_path(game, 'libSceSecure.prx'), game / 'module/prx/libSceSecure.prx')

    def test_missing_module_names_the_folders(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(FileNotFoundError, 'sce_module, module/prx'):
                link_modules.module_path(Path(tmp), 'libSceSecure.prx')


class ProfileTests(unittest.TestCase):
    def test_city_shrouded_in_shadow(self):
        p = game_profiles.profile('CUSA03562')
        self.assertIn('libSceSecure.prx', p['native_modules'])
        self.assertEqual(p['stage_modules'], 'module/MasterORBIS/*/StageModule.prx')
        self.assertNotIn('stage_modules', game_profiles.profile('CUSA36381'))
        env = {'VW_DEFERRED_READBACK': '1', 'VW_STALE_READBACK': '1'}  # as from vw.ini
        game_profiles.apply('CUSA03562', env)
        self.assertEqual((env['VW_DEFERRED_READBACK'], env['VW_STALE_READBACK']), ('0', '0'))


if __name__ == '__main__':
    unittest.main()
