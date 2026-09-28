import hashlib
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
import zipfile

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import pb3ds_assets as assets


def contract_for(rom_sha1: str, minimum_entries: int = 3) -> dict:
    return {
        "schema": 1,
        "paperboat_release": "1.0.1",
        "paperboat_commit": "a" * 40,
        "libultraship_commit": "c" * 40,
        "torch_commit": "b" * 40,
        "archives": {
            "paperboat": {
                "filename": "paperboat.o2r",
                "source_directory": "port",
            },
            "pm64": {
                "filename": "pm64.o2r",
                "minimum_entries": minimum_entries,
                "required_entries": ["portVersion", "version"],
            },
        },
        "supported_roms": [
            {
                "name": "Synthetic test ROM",
                "byte_order": "z64-big-endian",
                "sha1": rom_sha1,
            }
        ],
    }


def write_zip(path: Path, members: list[tuple[str, bytes]]) -> None:
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in members:
            archive.writestr(name, data)


class AssetPipelineTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="pb3ds-assets-test-")
        self.root = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def make_rom(self) -> tuple[Path, dict]:
        header = bytearray(0x40)
        header[:4] = assets.Z64_MAGIC
        header[0x10:0x14] = bytes.fromhex("12345678")
        rom = self.root / "test.z64"
        rom.write_bytes(header + bytes(range(64)))
        sha1 = hashlib.sha1(rom.read_bytes()).hexdigest()
        return rom, contract_for(sha1)

    def make_port_source(self) -> Path:
        source = self.root / "port"
        files = {
            "fonts/test.ttf": b"font",
            "shaders/opengl/test.glsl": b"shader",
            "textures/buttons/test.png": b"texture",
        }
        for name, data in files.items():
            path = source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        return source

    def make_pm64_archive(self, path: Path, rom: Path, contract: dict) -> None:
        write_zip(
            path,
            [
                ("version", assets.expected_version_entry(rom)),
                ("portVersion", assets.expected_port_version(contract)),
                ("logos/LOGO_1", b"resource"),
            ],
        )

    def make_asset_set(self) -> tuple[Path, dict]:
        rom, contract = self.make_rom()
        asset_directory = self.root / "assets"
        asset_directory.mkdir()
        engine = asset_directory / "paperboat.o2r"
        game = asset_directory / "pm64.o2r"
        assets.write_zip_from_sources(self.make_port_source(), engine, force=False)
        self.make_pm64_archive(game, rom, contract)
        records = {
            "paperboat": assets.verify_archive(engine, "paperboat", contract),
            "pm64": assets.verify_archive(game, "pm64", contract, rom=rom),
        }
        manifest = {
            "schema": 1,
            "paperboat_release": contract["paperboat_release"],
            "paperboat_commit": contract["paperboat_commit"],
            "libultraship_commit": contract["libultraship_commit"],
            "torch_commit": contract["torch_commit"],
            "rom": assets.verify_rom(rom, contract),
            "archives": records,
        }
        assets.write_manifest(
            asset_directory / "assets-manifest.json", manifest, force=False
        )
        return asset_directory, contract

    def test_supported_rom_is_accepted(self) -> None:
        rom, contract = self.make_rom()
        record = assets.verify_rom(rom, contract)
        self.assertEqual(record["name"], "Synthetic test ROM")
        self.assertEqual(record["crc1"], "12345678")

    def test_wrong_rom_hash_is_rejected(self) -> None:
        rom, contract = self.make_rom()
        rom.write_bytes(rom.read_bytes() + b"changed")
        with self.assertRaisesRegex(assets.AssetError, "unsupported ROM SHA-1"):
            assets.verify_rom(rom, contract)

    def test_non_z64_byte_order_is_rejected_before_hash(self) -> None:
        rom, contract = self.make_rom()
        data = bytearray(rom.read_bytes())
        data[:4] = bytes.fromhex("37804012")
        rom.write_bytes(data)
        with self.assertRaisesRegex(assets.AssetError, "big-endian"):
            assets.verify_rom(rom, contract)

    def test_port_archive_is_deterministic_and_exact(self) -> None:
        source = self.make_port_source()
        first = self.root / "first.o2r"
        second = self.root / "second.o2r"
        assets.write_zip_from_sources(source, first, force=False)
        assets.write_zip_from_sources(source, second, force=False)
        self.assertEqual(first.read_bytes(), second.read_bytes())
        contract = contract_for("0" * 40)
        record = assets.verify_archive(
            first, "paperboat", contract, source_directory=source
        )
        self.assertEqual(record["entry_count"], 3)

    def test_port_sources_reject_symlinks(self) -> None:
        source = self.make_port_source()
        target = source / "fonts" / "test.ttf"
        link = source / "fonts" / "alias.ttf"
        try:
            link.symlink_to(target)
        except OSError:
            self.skipTest("symlinks are unavailable")
        with self.assertRaisesRegex(assets.AssetError, "symlinks"):
            assets.source_files(source)

    def test_archive_rejects_parent_traversal(self) -> None:
        archive = self.root / "unsafe.o2r"
        write_zip(archive, [("../escape", b"bad")])
        with self.assertRaisesRegex(assets.AssetError, "unsafe archive member"):
            assets.verify_archive(archive, "paperboat", contract_for("0" * 40))

    def test_archive_rejects_casefold_duplicates(self) -> None:
        archive = self.root / "duplicate.o2r"
        write_zip(archive, [("fonts/A", b"one"), ("fonts/a", b"two")])
        with self.assertRaisesRegex(assets.AssetError, "duplicate"):
            assets.verify_archive(archive, "paperboat", contract_for("0" * 40))

    def test_pm64_metadata_is_bound_to_rom_and_release(self) -> None:
        rom, contract = self.make_rom()
        archive = self.root / "pm64.o2r"
        self.make_pm64_archive(archive, rom, contract)
        record = assets.verify_archive(archive, "pm64", contract, rom=rom)
        self.assertEqual(record["entry_count"], 3)

        write_zip(
            archive,
            [
                ("version", b"\x01\x00\x00\x00\x00"),
                ("portVersion", assets.expected_port_version(contract)),
                ("logos/LOGO_1", b"resource"),
            ],
        )
        with self.assertRaisesRegex(assets.AssetError, "CRC metadata"):
            assets.verify_archive(archive, "pm64", contract, rom=rom)

    def test_normalization_removes_order_and_timestamp_variance(self) -> None:
        first_raw = self.root / "first-raw.o2r"
        second_raw = self.root / "second-raw.o2r"
        write_zip(first_raw, [("b", b"two"), ("a", b"one")])
        write_zip(second_raw, [("a", b"one"), ("b", b"two")])
        first = self.root / "first-normal.o2r"
        second = self.root / "second-normal.o2r"
        assets.normalize_zip(first_raw, first, force=False)
        assets.normalize_zip(second_raw, second, force=False)
        self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_manifest_is_stable_and_does_not_overwrite(self) -> None:
        manifest = self.root / "manifest.json"
        value = {"b": 2, "a": 1}
        assets.write_manifest(manifest, value, force=False)
        self.assertEqual(json.loads(manifest.read_text()), value)
        self.assertTrue(manifest.read_text().startswith('{\n  "a"'))
        with self.assertRaisesRegex(assets.AssetError, "refusing to overwrite"):
            assets.write_manifest(manifest, value, force=False)

    def test_stage_requires_a_matching_manifest(self) -> None:
        asset_directory, contract = self.make_asset_set()
        manifest_path = asset_directory / "assets-manifest.json"
        manifest = json.loads(manifest_path.read_text())
        manifest["archives"]["pm64"]["sha256"] = "0" * 64
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        args = SimpleNamespace(
            assets_directory=str(asset_directory),
            sd_root=str(self.root / "sd"),
            force=False,
        )
        with self.assertRaisesRegex(assets.AssetError, "pm64.sha256"):
            assets.stage_assets(args, contract)
        self.assertFalse((self.root / "sd").exists())

    def test_stage_copies_only_a_validated_asset_set(self) -> None:
        asset_directory, contract = self.make_asset_set()
        sd_root = self.root / "sd"
        args = SimpleNamespace(
            assets_directory=str(asset_directory),
            sd_root=str(sd_root),
            force=False,
        )
        assets.stage_assets(args, contract)
        staged = sd_root / "3ds" / "PaperBoat3DS"
        self.assertEqual(
            sorted(path.name for path in staged.iterdir()),
            ["assets-manifest.json", "paperboat.o2r", "pm64.o2r"],
        )


if __name__ == "__main__":
    unittest.main()
