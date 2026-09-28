#!/usr/bin/env python3
"""Legal, reproducible PaperBoat3DS host-side asset preparation.

This tool never downloads a ROM and never uploads generated archives. It only
accepts a user-supplied ROM whose SHA-1 is present in the pinned asset contract.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import zipfile


PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CONTRACT = PROJECT_ROOT / "upstream" / "ASSET_CONTRACT.json"
DEFAULT_LOCK = PROJECT_ROOT / "upstream" / "PAPERBOAT.lock"
Z64_MAGIC = bytes.fromhex("80371240")
ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)
COPY_CHUNK = 1024 * 1024
MAX_ARCHIVE_ENTRIES = 100_000
MAX_ENTRY_BYTES = 256 * 1024 * 1024
MAX_TOTAL_BYTES = 2 * 1024 * 1024 * 1024


class AssetError(RuntimeError):
    """A user-facing asset contract failure."""


def load_contract(path: Path = DEFAULT_CONTRACT) -> dict:
    try:
        contract = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise AssetError(f"could not read asset contract {path}: {error}") from error
    if contract.get("schema") != 1:
        raise AssetError("unsupported asset contract schema")
    return contract


def load_lock(path: Path = DEFAULT_LOCK) -> dict[str, str]:
    values: dict[str, str] = {}
    assignment = re.compile(r"^([A-Z][A-Z0-9_]*)=([^\s]+)$")
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise AssetError(f"could not read upstream lock {path}: {error}") from error
    for number, raw_line in enumerate(lines, 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        match = assignment.fullmatch(line)
        if match is None:
            raise AssetError(f"unsafe lock-file syntax at {path}:{number}")
        values[match.group(1)] = match.group(2)
    return values


def run(command: list[str], cwd: Path | None = None) -> None:
    print(f"+ {shlex.join(command)}")
    try:
        subprocess.run(command, cwd=cwd, check=True)
    except (OSError, subprocess.CalledProcessError) as error:
        raise AssetError(f"command failed: {shlex.join(command)}") from error


def command_output(command: list[str], cwd: Path | None = None) -> str:
    try:
        result = subprocess.run(
            command, cwd=cwd, check=True, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
    except (OSError, subprocess.CalledProcessError) as error:
        raise AssetError(f"command failed: {shlex.join(command)}") from error
    return result.stdout.strip()


def digest_file(path: Path, algorithm: str) -> str:
    digest = hashlib.new(algorithm)
    try:
        with path.open("rb") as stream:
            while chunk := stream.read(COPY_CHUNK):
                digest.update(chunk)
    except OSError as error:
        raise AssetError(f"could not read {path}: {error}") from error
    return digest.hexdigest()


def verify_rom(path: Path, contract: dict) -> dict:
    if not path.is_file():
        raise AssetError(f"ROM is not a regular file: {path}")
    try:
        with path.open("rb") as stream:
            header = stream.read(0x40)
    except OSError as error:
        raise AssetError(f"could not read ROM header: {error}") from error
    if len(header) < 0x40 or header[:4] != Z64_MAGIC:
        raise AssetError("ROM must be an unmodified big-endian .z64 image")

    rom_sha1 = digest_file(path, "sha1")
    supported = {
        item["sha1"].lower(): item for item in contract["supported_roms"]
    }
    if rom_sha1 not in supported:
        expected = ", ".join(sorted(supported))
        raise AssetError(
            f"unsupported ROM SHA-1 {rom_sha1}; expected one of: {expected}"
        )

    result = {
        "name": supported[rom_sha1]["name"],
        "sha1": rom_sha1,
        "size_bytes": path.stat().st_size,
        "crc1": header[0x10:0x14].hex(),
    }
    print(
        f"ROM verified: {result['name']} sha1={rom_sha1} "
        f"size={result['size_bytes']}"
    )
    return result


def parse_git_commit(repository: Path) -> str:
    return command_output(["git", "rev-parse", "HEAD"], cwd=repository)


def fetch_repository(name: str, url: str, commit: str, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    if not (destination / ".git").is_dir():
        run(["git", "init", "-q"], cwd=destination)
        run(["git", "remote", "add", "origin", url], cwd=destination)
    else:
        run(["git", "remote", "set-url", "origin", url], cwd=destination)

    current = ""
    try:
        current = command_output(["git", "rev-parse", "HEAD"], cwd=destination)
    except AssetError:
        pass
    if current != commit:
        run(["git", "fetch", "--quiet", "--depth", "1", "origin", commit], cwd=destination)
        run(["git", "checkout", "--quiet", "--detach", "--force", "FETCH_HEAD"], cwd=destination)
    resolved = parse_git_commit(destination)
    if resolved != commit:
        raise AssetError(f"{name} resolved to {resolved}, expected {commit}")
    print(f"{name}: {resolved}")


def fetch_upstream(upstream_root: Path, lock: dict[str, str]) -> Path:
    paperboat = upstream_root / "PaperBoat"
    fetch_repository(
        "PaperBoat", lock["PAPERBOAT_REPOSITORY"],
        lock["PAPERBOAT_COMMIT"], paperboat
    )
    fetch_repository(
        "libultraship", lock["LIBULTRASHIP_REPOSITORY"],
        lock["LIBULTRASHIP_COMMIT"], paperboat / "external" / "libultraship"
    )
    fetch_repository(
        "Torch", lock["TORCH_REPOSITORY"], lock["TORCH_COMMIT"],
        paperboat / "external" / "torch"
    )
    return paperboat


def audit_upstream(paperboat: Path, contract: dict, lock: dict[str, str]) -> dict:
    torch = paperboat / "external" / "torch"
    libultraship = paperboat / "external" / "libultraship"
    paperboat_commit = parse_git_commit(paperboat)
    libultraship_commit = parse_git_commit(libultraship)
    torch_commit = parse_git_commit(torch)
    expected_paperboat = contract["paperboat_commit"]
    expected_libultraship = contract["libultraship_commit"]
    expected_torch = contract["torch_commit"]
    if paperboat_commit != expected_paperboat or lock.get("PAPERBOAT_COMMIT") != expected_paperboat:
        raise AssetError("PaperBoat commit does not match the asset contract")
    if lock.get("PAPERBOAT_RELEASE") != contract["paperboat_release"]:
        raise AssetError("PaperBoat release does not match the asset contract")
    if (
        libultraship_commit != expected_libultraship
        or lock.get("LIBULTRASHIP_COMMIT") != expected_libultraship
    ):
        raise AssetError("libultraship commit does not match the asset contract")
    if torch_commit != expected_torch or lock.get("TORCH_COMMIT") != expected_torch:
        raise AssetError("Torch commit does not match the asset contract")

    config_path = paperboat / "config.yml"
    try:
        config = config_path.read_text(encoding="utf-8")
    except OSError as error:
        raise AssetError(f"could not read {config_path}: {error}") from error
    for rom in contract["supported_roms"]:
        if re.search(rf"(?m)^{re.escape(rom['sha1'])}:\s*$", config) is None:
            raise AssetError(f"ROM SHA-1 is absent from pinned config.yml: {rom['sha1']}")
    game_name = contract["archives"]["pm64"]["filename"]
    if re.search(rf"(?m)^\s+binary:\s*{re.escape(game_name)}\s*$", config) is None:
        raise AssetError(f"pinned config.yml does not output {game_name}")

    recipes = paperboat / "assets" / "yaml" / "us"
    recipe_count = sum(
        1 for path in recipes.rglob("*")
        if path.is_file() and path.suffix.lower() in {".yml", ".yaml"}
    )
    port_source = paperboat / contract["archives"]["paperboat"]["source_directory"]
    port_count = sum(1 for path in port_source.rglob("*") if path.is_file())
    if recipe_count < 200:
        raise AssetError(f"pinned PaperBoat recipe set is unexpectedly small: {recipe_count}")
    if port_count < 1:
        raise AssetError("pinned PaperBoat port asset directory is empty")

    result = {
        "paperboat_commit": paperboat_commit,
        "libultraship_commit": libultraship_commit,
        "torch_commit": torch_commit,
        "recipe_files": recipe_count,
        "port_files": port_count,
    }
    print(
        f"Asset contract verified: recipes={recipe_count} port_files={port_count}"
    )
    return result


def source_files(directory: Path) -> list[tuple[str, Path]]:
    if not directory.is_dir():
        raise AssetError(f"asset source directory does not exist: {directory}")
    files: list[tuple[str, Path]] = []
    for path in sorted(directory.rglob("*")):
        if path.is_symlink():
            raise AssetError(f"asset sources may not contain symlinks: {path}")
        if path.is_file():
            name = path.relative_to(directory).as_posix()
            validate_member_name(name)
            files.append((name, path))
    if not files:
        raise AssetError(f"asset source directory is empty: {directory}")
    return files


def validate_member_name(name: str) -> None:
    if not name or "\\" in name or "\x00" in name:
        raise AssetError(f"unsafe archive member name: {name!r}")
    path = PurePosixPath(name)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
        raise AssetError(f"unsafe archive member path: {name}")
    if re.match(r"^[A-Za-z]:", name):
        raise AssetError(f"unsafe archive member drive path: {name}")


def zip_info(name: str) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name, ZIP_TIMESTAMP)
    info.create_system = 3
    info.external_attr = (0o100644 & 0xFFFF) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    return info


def write_zip_from_sources(source: Path, output: Path, force: bool) -> None:
    files = source_files(source)
    if output.exists() and not force:
        raise AssetError(f"refusing to overwrite existing archive: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(f".{output.name}.tmp")
    try:
        with zipfile.ZipFile(
            temporary, "w", compression=zipfile.ZIP_DEFLATED,
            compresslevel=9, allowZip64=True
        ) as archive:
            for name, path in files:
                with path.open("rb") as source_stream, archive.open(
                    zip_info(name), "w", force_zip64=True
                ) as target_stream:
                    shutil.copyfileobj(source_stream, target_stream, COPY_CHUNK)
        os.replace(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)


def normalize_zip(source: Path, output: Path, force: bool) -> None:
    if output.exists() and output != source and not force:
        raise AssetError(f"refusing to overwrite existing archive: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(f".{output.name}.normalize.tmp")
    try:
        with zipfile.ZipFile(source, "r") as input_zip, zipfile.ZipFile(
            temporary, "w", compression=zipfile.ZIP_DEFLATED,
            compresslevel=9, allowZip64=True
        ) as output_zip:
            members = [member for member in input_zip.infolist() if not member.is_dir()]
            for member in sorted(members, key=lambda item: item.filename):
                validate_member_name(member.filename)
                with input_zip.open(member, "r") as source_stream, output_zip.open(
                    zip_info(member.filename), "w", force_zip64=True
                ) as target_stream:
                    shutil.copyfileobj(source_stream, target_stream, COPY_CHUNK)
        os.replace(temporary, output)
    except (OSError, zipfile.BadZipFile) as error:
        raise AssetError(f"could not normalize {source}: {error}") from error
    finally:
        temporary.unlink(missing_ok=True)


def expected_version_entry(rom: Path) -> bytes:
    try:
        with rom.open("rb") as stream:
            header = stream.read(0x14)
    except OSError as error:
        raise AssetError(f"could not read ROM version header: {error}") from error
    if len(header) < 0x14:
        raise AssetError("ROM header is truncated")
    # The pinned PaperBoat build enables Torch's ROM_CRC_BSWAP option.
    return b"\x01" + header[0x10:0x14][::-1]


def expected_port_version(contract: dict) -> bytes:
    try:
        major, minor, patch = (
            int(part) for part in contract["paperboat_release"].split(".")
        )
    except (KeyError, ValueError) as error:
        raise AssetError("invalid PaperBoat release in asset contract") from error
    return struct.pack(">HHH", major, minor, patch)


def verify_archive(
    archive_path: Path,
    kind: str,
    contract: dict,
    source_directory: Path | None = None,
    rom: Path | None = None,
) -> dict:
    if not archive_path.is_file():
        raise AssetError(f"archive is not a regular file: {archive_path}")
    if kind not in {"paperboat", "pm64"}:
        raise AssetError(f"unknown archive kind: {kind}")
    archive_config = contract["archives"][kind]
    seen: set[str] = set()
    inventory = hashlib.sha256()
    total_size = 0

    try:
        with zipfile.ZipFile(archive_path, "r") as archive:
            all_members = archive.infolist()
            all_seen: set[str] = set()
            all_seen_casefold: set[str] = set()
            for member in all_members:
                validate_member_name(member.filename)
                folded = member.filename.casefold()
                if member.filename in all_seen or folded in all_seen_casefold:
                    raise AssetError(f"duplicate archive member: {member.filename}")
                all_seen.add(member.filename)
                all_seen_casefold.add(folded)
                mode = (member.external_attr >> 16) & 0xFFFF
                if stat.S_ISLNK(mode):
                    raise AssetError(f"symbolic-link archive member: {member.filename}")

            members = [member for member in all_members if not member.is_dir()]
            if not members or len(members) > MAX_ARCHIVE_ENTRIES:
                raise AssetError(f"invalid archive entry count: {len(members)}")
            member_map: dict[str, zipfile.ZipInfo] = {}
            for member in members:
                seen.add(member.filename)
                if member.flag_bits & 0x1:
                    raise AssetError(f"encrypted archive member: {member.filename}")
                if member.compress_type not in {zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED}:
                    raise AssetError(f"unsupported compression for {member.filename}")
                if member.file_size > MAX_ENTRY_BYTES:
                    raise AssetError(f"archive member is too large: {member.filename}")
                total_size += member.file_size
                if total_size > MAX_TOTAL_BYTES:
                    raise AssetError("archive expands beyond the 2 GiB validation limit")
                member_map[member.filename] = member

            corrupt = archive.testzip()
            if corrupt is not None:
                raise AssetError(f"archive CRC check failed: {corrupt}")

            for name in sorted(member_map):
                digest = hashlib.sha256()
                with archive.open(member_map[name], "r") as stream:
                    while chunk := stream.read(COPY_CHUNK):
                        digest.update(chunk)
                inventory.update(name.encode("utf-8"))
                inventory.update(b"\x00")
                inventory.update(digest.digest())

            if kind == "paperboat":
                required_prefixes = ("fonts/", "shaders/", "textures/")
                for prefix in required_prefixes:
                    if not any(name.startswith(prefix) for name in member_map):
                        raise AssetError(f"paperboat archive lacks {prefix} assets")
                if source_directory is not None:
                    expected = {name: path for name, path in source_files(source_directory)}
                    if set(expected) != set(member_map):
                        raise AssetError("paperboat archive does not match the pinned port/ inventory")
                    for name, path in expected.items():
                        with archive.open(member_map[name], "r") as stored:
                            with path.open("rb") as source_stream:
                                while True:
                                    stored_chunk = stored.read(COPY_CHUNK)
                                    source_chunk = source_stream.read(COPY_CHUNK)
                                    if stored_chunk != source_chunk:
                                        raise AssetError(f"paperboat archive content mismatch: {name}")
                                    if not stored_chunk:
                                        break
            elif kind == "pm64":
                required = set(archive_config["required_entries"])
                missing = required - set(member_map)
                if missing:
                    raise AssetError(f"pm64 archive lacks required entries: {sorted(missing)}")
                if len(member_map) < int(archive_config["minimum_entries"]):
                    raise AssetError("pm64 archive has too few entries for the pinned recipe set")
                version = archive.read("version")
                port_version = archive.read("portVersion")
                if len(version) != 5 or version[0] != 1:
                    raise AssetError("pm64 version entry is malformed")
                if port_version != expected_port_version(contract):
                    raise AssetError("pm64 portVersion does not match pinned PaperBoat")
                if rom is not None and version != expected_version_entry(rom):
                    raise AssetError("pm64 archive CRC metadata does not match the supplied ROM")
    except (OSError, zipfile.BadZipFile) as error:
        raise AssetError(f"invalid O2R ZIP archive: {archive_path}") from error

    result = {
        "filename": archive_path.name,
        "sha256": digest_file(archive_path, "sha256"),
        "size_bytes": archive_path.stat().st_size,
        "entry_count": len(seen),
        "uncompressed_bytes": total_size,
        "inventory_sha256": inventory.hexdigest(),
    }
    print(
        f"{kind} archive verified: entries={result['entry_count']} "
        f"sha256={result['sha256']}"
    )
    return result


def build_torch(paperboat: Path, build_directory: Path, jobs: int) -> Path:
    torch_source = paperboat / "external" / "torch"
    options = [
        "-DCMAKE_BUILD_TYPE=Release",
        "-DUSE_STANDALONE=ON",
        "-DBUILD_UI=OFF",
        "-DBUILD_STORMLIB=OFF",
        "-DBUILD_PM64=ON",
        "-DBUILD_NAUDIO=ON",
        "-DBUILD_SM64=OFF",
        "-DBUILD_MK64=OFF",
        "-DBUILD_SF64=OFF",
        "-DBUILD_FZERO=OFF",
        "-DBUILD_BK64=OFF",
        "-DBUILD_MARIO_ARTIST=OFF",
        "-DBUILD_OOT=OFF",
        "-DROM_CRC_BSWAP=ON",
    ]
    run(["cmake", "-S", str(torch_source), "-B", str(build_directory), *options])
    run([
        "cmake", "--build", str(build_directory), "--config", "Release",
        "--parallel", str(max(1, jobs))
    ])
    candidates = [
        build_directory / "torch",
        build_directory / "torch.exe",
        build_directory / "Release" / "torch.exe",
    ]
    for candidate in candidates:
        if candidate.is_file():
            run([str(candidate), "--help"])
            return candidate
    raise AssetError(f"Torch build completed but no executable was found in {build_directory}")


def write_manifest(path: Path, data: dict, force: bool) -> None:
    if path.exists() and not force:
        raise AssetError(f"refusing to overwrite existing manifest: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp")
    try:
        temporary.write_text(
            json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def verify_assets_manifest(
    path: Path, contract: dict, archive_records: dict[str, dict]
) -> dict:
    if not path.is_file():
        raise AssetError(f"asset manifest is not a regular file: {path}")
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise AssetError(f"could not read asset manifest {path}: {error}") from error

    if manifest.get("schema") != 1:
        raise AssetError("unsupported asset manifest schema")
    contract_fields = (
        "paperboat_release",
        "paperboat_commit",
        "libultraship_commit",
        "torch_commit",
    )
    for field in contract_fields:
        if manifest.get(field) != contract.get(field):
            raise AssetError(f"asset manifest {field} does not match the pinned contract")

    rom = manifest.get("rom")
    if not isinstance(rom, dict):
        raise AssetError("asset manifest lacks ROM provenance")
    allowed_rom_fields = {"name", "sha1", "size_bytes", "crc1"}
    if set(rom) != allowed_rom_fields:
        raise AssetError("asset manifest contains incomplete or unexpected ROM metadata")
    supported = {
        item["sha1"].lower(): item["name"] for item in contract["supported_roms"]
    }
    rom_sha1 = str(rom.get("sha1", "")).lower()
    if rom_sha1 not in supported or rom.get("name") != supported[rom_sha1]:
        raise AssetError("asset manifest ROM does not match the supported contract")

    recorded_archives = manifest.get("archives")
    if not isinstance(recorded_archives, dict):
        raise AssetError("asset manifest lacks archive records")
    record_fields = (
        "filename",
        "sha256",
        "size_bytes",
        "entry_count",
        "uncompressed_bytes",
        "inventory_sha256",
    )
    for kind, actual in archive_records.items():
        recorded = recorded_archives.get(kind)
        if not isinstance(recorded, dict):
            raise AssetError(f"asset manifest lacks the {kind} archive record")
        for field in record_fields:
            if recorded.get(field) != actual.get(field):
                raise AssetError(
                    f"asset manifest {kind}.{field} does not match the archive"
                )
    print(f"Asset manifest verified: {path}")
    return manifest


def prepare_assets(args: argparse.Namespace, contract: dict) -> None:
    lock = load_lock(Path(args.lock))
    upstream_root = Path(args.upstream_root).resolve()
    paperboat = upstream_root / "PaperBoat"
    if not args.no_fetch:
        paperboat = fetch_upstream(upstream_root, lock)
    audit = audit_upstream(paperboat, contract, lock)
    rom_path = Path(args.rom).resolve()
    rom_record = verify_rom(rom_path, contract)

    if args.torch:
        torch_binary = Path(args.torch).resolve()
        if not torch_binary.is_file():
            raise AssetError(f"Torch executable does not exist: {torch_binary}")
    else:
        torch_binary = build_torch(
            paperboat, Path(args.build_directory).resolve(), args.jobs
        )

    output_directory = Path(args.output_directory).resolve()
    output_directory.mkdir(parents=True, exist_ok=True)
    engine_name = contract["archives"]["paperboat"]["filename"]
    game_name = contract["archives"]["pm64"]["filename"]
    engine_output = output_directory / engine_name
    game_output = output_directory / game_name
    manifest_output = output_directory / "assets-manifest.json"
    for output in (engine_output, game_output, manifest_output):
        if output.exists() and not args.force:
            raise AssetError(f"refusing to overwrite existing output: {output}")

    with tempfile.TemporaryDirectory(prefix="pb3ds-m7-") as temporary_name:
        temporary = Path(temporary_name)
        temporary_engine = temporary / engine_name
        write_zip_from_sources(paperboat / "port", temporary_engine, force=False)
        engine_record = verify_archive(
            temporary_engine, "paperboat", contract,
            source_directory=paperboat / "port"
        )

        torch_output = temporary / "torch-output"
        torch_output.mkdir()
        run([
            str(torch_binary), "o2r", str(rom_path),
            "-s", str(paperboat), "-d", str(torch_output),
            "-u", contract["paperboat_release"],
        ])
        raw_game = torch_output / game_name
        if not raw_game.is_file():
            raise AssetError(f"Torch did not produce {raw_game}")
        temporary_game = temporary / game_name
        normalize_zip(raw_game, temporary_game, force=False)
        game_record = verify_archive(
            temporary_game, "pm64", contract, rom=rom_path
        )

        manifest = {
            "schema": 1,
            "paperboat_release": contract["paperboat_release"],
            "paperboat_commit": audit["paperboat_commit"],
            "libultraship_commit": audit["libultraship_commit"],
            "torch_commit": audit["torch_commit"],
            "rom": rom_record,
            "archives": {
                "paperboat": engine_record,
                "pm64": game_record,
            },
        }
        temporary_manifest = temporary / "assets-manifest.json"
        write_manifest(temporary_manifest, manifest, force=False)
        verify_assets_manifest(
            temporary_manifest, contract,
            {"paperboat": engine_record, "pm64": game_record},
        )

        os.replace(temporary_engine, engine_output)
        os.replace(temporary_game, game_output)
        os.replace(temporary_manifest, manifest_output)
    print(f"Assets ready in {output_directory}")


def stage_assets(args: argparse.Namespace, contract: dict) -> None:
    assets = Path(args.assets_directory).resolve()
    destination = Path(args.sd_root).resolve() / "3ds" / "PaperBoat3DS"
    names = [
        contract["archives"]["paperboat"]["filename"],
        contract["archives"]["pm64"]["filename"],
        "assets-manifest.json",
    ]
    archive_records = {
        "paperboat": verify_archive(assets / names[0], "paperboat", contract),
        "pm64": verify_archive(assets / names[1], "pm64", contract),
    }
    verify_assets_manifest(assets / names[2], contract, archive_records)
    for name in names:
        target = destination / name
        if target.exists() and not args.force:
            raise AssetError(f"refusing to overwrite staged file: {target}")
    destination.mkdir(parents=True, exist_ok=True)
    for name in names:
        source = assets / name
        target = destination / name
        temporary = target.with_name(f".{target.name}.tmp")
        try:
            shutil.copyfile(source, temporary)
            os.replace(temporary, target)
        finally:
            temporary.unlink(missing_ok=True)
    print(f"Assets staged at {destination}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", default=str(DEFAULT_CONTRACT))
    subparsers = parser.add_subparsers(dest="command", required=True)

    verify_rom_parser = subparsers.add_parser("verify-rom")
    verify_rom_parser.add_argument("rom")

    audit_parser = subparsers.add_parser("audit")
    audit_parser.add_argument("--upstream-root", default=str(PROJECT_ROOT / ".cache" / "upstream"))
    audit_parser.add_argument("--lock", default=str(DEFAULT_LOCK))

    pack_parser = subparsers.add_parser("pack-port")
    pack_parser.add_argument("--source", required=True)
    pack_parser.add_argument("--output", required=True)
    pack_parser.add_argument("--force", action="store_true")

    verify_parser = subparsers.add_parser("verify-archive")
    verify_parser.add_argument("archive")
    verify_parser.add_argument("--kind", choices=("paperboat", "pm64"), required=True)
    verify_parser.add_argument("--source-directory")
    verify_parser.add_argument("--rom")
    verify_parser.add_argument("--manifest")
    verify_parser.add_argument("--force", action="store_true")

    torch_parser = subparsers.add_parser("build-torch")
    torch_parser.add_argument("--paperboat-root", required=True)
    torch_parser.add_argument("--build-directory", required=True)
    torch_parser.add_argument("--jobs", type=int, default=2)

    prepare_parser = subparsers.add_parser("prepare")
    prepare_parser.add_argument("rom")
    prepare_parser.add_argument("--upstream-root", default=str(PROJECT_ROOT / ".cache" / "upstream"))
    prepare_parser.add_argument("--lock", default=str(DEFAULT_LOCK))
    prepare_parser.add_argument("--build-directory", default=str(PROJECT_ROOT / "build" / "m7-host" / "torch"))
    prepare_parser.add_argument("--output-directory", default=str(PROJECT_ROOT / "build" / "assets"))
    prepare_parser.add_argument("--torch")
    prepare_parser.add_argument("--jobs", type=int, default=2)
    prepare_parser.add_argument("--no-fetch", action="store_true")
    prepare_parser.add_argument("--force", action="store_true")

    stage_parser = subparsers.add_parser("stage")
    stage_parser.add_argument("--assets-directory", default=str(PROJECT_ROOT / "build" / "assets"))
    stage_parser.add_argument("--sd-root", required=True)
    stage_parser.add_argument("--force", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        contract = load_contract(Path(args.contract))
        if args.command == "verify-rom":
            verify_rom(Path(args.rom), contract)
        elif args.command == "audit":
            lock = load_lock(Path(args.lock))
            audit_upstream(Path(args.upstream_root).resolve() / "PaperBoat", contract, lock)
        elif args.command == "pack-port":
            output = Path(args.output)
            source = Path(args.source)
            write_zip_from_sources(source, output, args.force)
            verify_archive(output, "paperboat", contract, source_directory=source)
        elif args.command == "verify-archive":
            record = verify_archive(
                Path(args.archive), args.kind, contract,
                source_directory=Path(args.source_directory) if args.source_directory else None,
                rom=Path(args.rom) if args.rom else None,
            )
            if args.manifest:
                write_manifest(Path(args.manifest), record, args.force)
        elif args.command == "build-torch":
            binary = build_torch(
                Path(args.paperboat_root), Path(args.build_directory), args.jobs
            )
            print(binary)
        elif args.command == "prepare":
            prepare_assets(args, contract)
        elif args.command == "stage":
            stage_assets(args, contract)
        else:
            parser.error(f"unknown command: {args.command}")
    except AssetError as error:
        print(f"asset pipeline error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
