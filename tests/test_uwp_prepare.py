"""Protect the original dump and reject incomplete preparation inputs."""
from paths import ROOT
import importlib.util
from pathlib import Path
import tempfile
import unittest
from test_prepare import make_test_sfo

spec = importlib.util.spec_from_file_location("uwp_prepare", ROOT / "xbox/scripts/prepare-game.py")
uwp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(uwp)


class UwpPreparationTests(unittest.TestCase):
    def test_output_inside_dump_is_rejected_without_modifying_input(self):
        with tempfile.TemporaryDirectory() as tmp:
            dump = Path(tmp)
            marker = dump / "eboot.bin"
            marker.write_bytes(b"original")
            with self.assertRaisesRegex(ValueError, "outside"):
                uwp.prepare_game(dump, 1, dump / "Prepared")
            self.assertEqual(marker.read_bytes(), b"original")
            self.assertFalse((dump / "Prepared").exists())

    def test_existing_output_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dump = root / "dump"
            dump.mkdir()
            destination = root / "prepared"
            destination.mkdir()
            marker = destination / "boot-linked.bin"
            marker.write_bytes(b"previous")
            with self.assertRaises(FileExistsError):
                uwp.prepare_game(dump, 1, destination)
            self.assertEqual(marker.read_bytes(), b"previous")

    def test_incomplete_collection_produces_no_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dump = root / "dump"
            (dump / "sce_sys").mkdir(parents=True)
            (dump / "sce_sys/param.sfo").write_bytes(make_test_sfo("UNCHARTED: The Nathan Drake Collection"))
            with self.assertRaisesRegex(ValueError, "big2-ps4_Shipping.elf"):
                uwp.prepare_game(dump, 2, root / "prepared")
            self.assertFalse((root / "prepared").exists())

    def test_wrong_game_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dump = root / "dump"
            (dump / "sce_sys").mkdir(parents=True)
            (dump / "sce_sys/param.sfo").write_bytes(make_test_sfo("God of War III"))
            with self.assertRaisesRegex(ValueError, "Uncharted"):
                uwp.prepare_game(dump, 1, root / "prepared")


if __name__ == "__main__":
    unittest.main()
