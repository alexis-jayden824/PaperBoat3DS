import hashlib
import json
from pathlib import Path
import re
from types import SimpleNamespace
import tempfile
import unittest
from unittest import mock
import zipfile
import struct

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


def otr_resource(type_name: str, body: bytes) -> bytes:
    header = bytearray(64)
    header[0] = 1
    struct.pack_into(">II", header, 4, assets.OTR_TYPES[type_name], 0)
    return bytes(header) + body


def otr_blob(payload: bytes) -> bytes:
    return otr_resource("OBLB", struct.pack(">I", len(payload)) + payload)


def otr_texture(texture_type: int, width: int, height: int, image: bytes) -> bytes:
    body = struct.pack(">IIII", texture_type, width, height, len(image)) + image
    return otr_resource("OTEX", body)


def otr_display_list(*words: int) -> bytes:
    body = bytes((4, 0, 0, 0, 0, 0, 0, 0))
    body += struct.pack(">" + "I" * len(words), *words)
    return otr_resource("ODLT", body)


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

    def test_runtime_preflight_matches_asset_contract(self) -> None:
        """Keep build-time archive checks identical to the native gate."""
        repository = Path(__file__).resolve().parents[1]
        source = (repository / "source/runtime_resources.c").read_text()
        body = source.split(
            "static const PBRuntimeResourceRequirement m13_requirements[] = {",
            1,
        )[1].split("\n};", 1)[0]
        contract = json.loads(
            (repository / "upstream/ASSET_CONTRACT.json").read_text()
        )
        declared = {
            item["name"]: item
            for item in contract["archives"]["pm64"]["required_resources"]
        }
        expected: dict[str, dict[str, int | str]] = {}

        for name, width, height in re.findall(
            r'REQUIRE_CI4_PAIR\("([^"]+)",\s*(\d+)U,\s*(\d+)U\)',
            body,
        ):
            expected[name] = {
                "type": "OTEX", "texture_type": 3,
                "width": int(width), "height": int(height),
            }
            expected[name + ".pal"] = {
                "type": "OTEX", "texture_type": 2,
                "width": 16, "height": 1,
            }

        texture_types = {
            "PB_RESOURCE_TEXTURE_RGBA32": 1,
            "PB_RESOURCE_TEXTURE_RGBA16": 2,
            "PB_RESOURCE_TEXTURE_CI4": 3,
            "PB_RESOURCE_TEXTURE_CI8": 4,
            "PB_RESOURCE_TEXTURE_I4": 5,
            "PB_RESOURCE_TEXTURE_I8": 6,
            "PB_RESOURCE_TEXTURE_IA4": 7,
            "PB_RESOURCE_TEXTURE_IA8": 8,
            "PB_RESOURCE_TEXTURE_IA16": 9,
        }
        for name, texture_type, width, height in re.findall(
            r'REQUIRE_TEXTURE\("([^"]+)",\s*([A-Z0-9_]+),\s*'
            r'(\d+)U,\s*(\d+)U\)',
            body,
        ):
            expected[name] = {
                "type": "OTEX", "texture_type": texture_types[texture_type],
                "width": int(width), "height": int(height),
            }

        resource_types = {
            "TYPE_BLOB": "OBLB", "TYPE_VERTEX": "OVTX",
            "TYPE_DL": "ODLT", "TYPE_MATRIX": "OMTX",
        }
        for name, resource_type, minimum in re.findall(
            r'REQUIRE\("([^"]+)",\s*([A-Z0-9_]+),\s*'
            r'(sizeof\(PBRuntimeGfx\)|0x[0-9A-Fa-f]+U|\d+U)\)',
            body,
        ):
            size = (8 if minimum == "sizeof(PBRuntimeGfx)" else
                    int(minimum[:-1], 0))
            expected[name] = {
                "type": resource_types[resource_type],
                "minimum_payload_size": size,
            }

        self.assertEqual(set(declared), set(expected))
        for name, fields in expected.items():
            for field, value in fields.items():
                self.assertEqual(declared[name].get(field), value,
                                 f"{name}: {field}")

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

    def test_pm64_resource_hash_matches_runtime_and_rejects_collisions(self) -> None:
        self.assertEqual(
            assets.resource_name_crc64("sprites/player_sprite_1_raster_0"),
            0xC75C43705A0EE73F,
        )
        rom, contract = self.make_rom()
        contract["archives"]["pm64"]["minimum_entries"] = 4
        archive = self.root / "hash-collision.o2r"
        write_zip(
            archive,
            [
                ("version", assets.expected_version_entry(rom)),
                ("portVersion", assets.expected_port_version(contract)),
                ("resource/a", b"a"),
                ("resource/b", b"b"),
            ],
        )
        with mock.patch.object(assets, "resource_name_crc64", return_value=1):
            with self.assertRaisesRegex(assets.AssetError, "CRC64 collision"):
                assets.verify_archive(archive, "pm64", contract, rom=rom)

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

    def test_pm64_gameplay_resource_contract_is_enforced(self) -> None:
        rom, contract = self.make_rom()
        pm64 = contract["archives"]["pm64"]
        pm64["minimum_entries"] = 4
        pm64["required_prefix_counts"] = {"shapes/mac_00_shape/dlist_": 1}
        pm64["required_resources"] = [
            {
                "name": "title_screen/title_logo_img",
                "type": "OTEX",
                "texture_type": 1,
                "width": 2,
                "height": 1,
                "minimum_payload_size": 8,
            },
            {
                "name": "shapes/mac_00_shape",
                "type": "OBLB",
                "minimum_payload_size": 4,
            },
        ]
        archive = self.root / "gameplay.o2r"
        common = [
            ("version", assets.expected_version_entry(rom)),
            ("portVersion", assets.expected_port_version(contract)),
            ("title_screen/title_logo_img", otr_texture(1, 2, 1, b"12345678")),
            ("shapes/mac_00_shape", otr_blob(b"shape")),
            ("shapes/mac_00_shape/dlist_20", b"prefix-only"),
        ]
        write_zip(archive, common)
        record = assets.verify_archive(archive, "pm64", contract, rom=rom)
        self.assertEqual(record["entry_count"], len(common))

        malformed = list(common)
        malformed[2] = (
            "title_screen/title_logo_img",
            otr_texture(1, 1, 1, b"1234"),
        )
        write_zip(archive, malformed)
        with self.assertRaisesRegex(assets.AssetError, "wrong width"):
            assets.verify_archive(archive, "pm64", contract, rom=rom)

        write_zip(archive, [item for item in common if item[0] != "shapes/mac_00_shape"])
        with self.assertRaisesRegex(assets.AssetError, "lacks required gameplay resource"):
            assets.verify_archive(archive, "pm64", contract, rom=rom)

    def test_pm64_gameplay_resource_rejects_malformed_otr(self) -> None:
        rom, contract = self.make_rom()
        contract["archives"]["pm64"]["required_resources"] = [
            {"name": "bad", "type": "OBLB", "minimum_payload_size": 1}
        ]
        archive = self.root / "malformed-gameplay.o2r"
        write_zip(
            archive,
            [
                ("version", assets.expected_version_entry(rom)),
                ("portVersion", assets.expected_port_version(contract)),
                ("bad", b"not-an-otr-resource"),
            ],
        )
        with self.assertRaisesRegex(assets.AssetError, "malformed OTR header"):
            assets.verify_archive(archive, "pm64", contract, rom=rom)

    def test_pinned_native_resource_envelopes_are_validated(self) -> None:
        valid = {
            "matrix": otr_resource("OMTX", struct.pack(">16I", *range(16))),
            "lights": otr_resource("LGTS", bytes(range(24))),
            "viewport": otr_resource(
                "OVPT", struct.pack(">hhhhhhhh", 640, 480, 511, 0,
                                    640, 480, 511, 0)
            ),
            "vec3s": otr_resource(
                "VC3S", struct.pack(">Ihhhhhh", 2, -1, 2, -3, 4, -5, 6)
            ),
            "display-list": otr_display_list(
                0x42000000, 0x08000100,
                0x01234567, 0x89ABCDEF,
                0xDF000000, 0,
            ),
        }
        for name, data in valid.items():
            with self.subTest(name=name):
                parsed = assets.parse_otr_resource(data, name)
                self.assertGreater(parsed["payload_size"], 0)

        malformed = {
            "matrix": otr_resource("OMTX", bytes(63)),
            "lights": otr_resource("LGTS", bytes(23)),
            "viewport": otr_resource("OVPT", bytes(15)),
            "vec3s": otr_resource("VC3S", struct.pack(">Ihhh", 2, 1, 2, 3)),
            "display-list": otr_display_list(0x42000000, 0),
        }
        for name, data in malformed.items():
            with self.subTest(name=name):
                with self.assertRaises(assets.AssetError):
                    assets.parse_otr_resource(data, name)

    def test_full_pm64_envelope_validation_rejects_unchecked_entries(self) -> None:
        rom, contract = self.make_rom()
        contract["archives"]["pm64"]["validate_resource_envelopes"] = True
        archive = self.root / "all-envelopes.o2r"
        common = [
            ("version", assets.expected_version_entry(rom)),
            ("portVersion", assets.expected_port_version(contract)),
            ("logos/LOGO_1", otr_blob(b"valid")),
        ]
        write_zip(archive, common)
        assets.verify_archive(archive, "pm64", contract, rom=rom)
        common[-1] = ("logos/LOGO_1", b"not-an-envelope")
        write_zip(archive, common)
        with self.assertRaisesRegex(assets.AssetError, "malformed OTR header"):
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
