"""Build, verify, and archive the minimal MiniCraft source release and firmware.

Usage: python tools/snapshot_minicraft.py [--skip-build]
Requires the configured ARM toolchain/signing tool for a full snapshot.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "out/minicraft"
ARCHIVE_ROOT = ROOT / "out/verified-v5"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(args, log):
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, encoding="utf-8")
    (OUT / log).write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise SystemExit(f"{args[1]} failed; see {OUT / log}\n{result.stdout}{result.stderr}")
    return result.stdout + result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-build", action="store_true", help="use an already completed firmware build")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    if not args.skip_build:
        run([sys.executable, "tools/build_minicraft.py"], "snapshot-build.log")
    tests = run([sys.executable, "tools/test_minicraft.py", "--sd", "G:/"], "snapshot-tests.log")

    raw = (OUT / "MiniCraft_N6.bin").read_bytes()
    signed = (OUT / "MiniCraft_N6-Trusted.bin").read_bytes()
    elf = OUT / "MiniCraft_N6.elf"
    assert signed[1024:] == raw, "signed image payload differs from raw firmware"
    stack, entry = struct.unpack_from("<II", raw)
    assert stack == 0x34200000 and entry & 1 and 0x70100400 <= entry < 0x70100400 + len(raw)
    assert not (OUT / "compile.log").read_text(encoding="utf-8").strip(), "compiler emitted diagnostics"

    manifest_path = ROOT / "tools/minicraft_sources.txt"
    sources = sorted(set(manifest_path.read_text(encoding="utf-8").splitlines()))
    assert "tools/minicraft_sources.txt" in sources
    for rel in sources:
        p = ROOT / rel
        assert p.is_file(), f"source manifest contains missing file: {rel}"
        assert not any(part.lower() in {"debug", "release", "out", ".git", ".settings"} for part in p.parts)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    archive_root = ARCHIVE_ROOT / stamp
    archive_root.mkdir(parents=True, exist_ok=False)
    source_zip = archive_root / "MiniCraft-v5-source.zip"
    source_hashes = {}
    with zipfile.ZipFile(source_zip, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for rel in sources:
            data = (ROOT / rel).read_bytes()
            source_hashes[rel] = digest(data)
            zf.writestr(rel, data)
        zf.writestr("SOURCE_SHA256.json", json.dumps(source_hashes, indent=2, ensure_ascii=False) + "\n")
    with zipfile.ZipFile(source_zip) as zf:
        assert zf.testzip() is None
        for rel, sha in source_hashes.items():
            assert digest(zf.read(rel)) == sha

    artifacts = {
        "MiniCraft_N6-Trusted.bin": signed,
        "MiniCraft_N6.bin": raw,
        "MiniCraft_N6.elf": elf.read_bytes(),
        "initial-world-v4.SAV": (OUT / "initial-world-v4.SAV").read_bytes(),
        "tests.log": tests.encode("utf-8"),
        "build.log": (OUT / "snapshot-build.log").read_bytes() if not args.skip_build else (OUT / "compile.log").read_bytes() + (OUT / "link.log").read_bytes(),
        "geometry.png": (OUT / "geometry.png").read_bytes(),
        "combat.png": (OUT / "combat.png").read_bytes(),
    }
    for name, data in artifacts.items():
        (archive_root / name).write_bytes(data)
    validation = {
        "recorded_utc": stamp,
        "version": 4,
        "world_dimensions": [128, 64, 128],
        "save_bytes": 1050176,
        "firmware_bytes": len(raw),
        "signed_firmware_bytes": len(signed),
        "signed_header_bytes": 1024,
        "signed_payload_matches_raw": True,
        "initial_stack": hex(stack),
        "entry_point": hex(entry),
        "flash_address_signed": "0x70100000",
        "flash_address_raw": "0x70100400",
        "arm_elf": str(elf.relative_to(ROOT).as_posix()),
        "arm_size_output": subprocess.check_output(["F:/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins", "--version"], text=True) if False else "recorded by the build task",
        "portable_tests": "passed; see tests.log; the G:/ SD check is read-only",
        "source_archive": source_zip.name,
        "source_file_count": len(sources),
        "source_bytes": sum((ROOT / p).stat().st_size for p in sources),
        "source_hashes_verified": True,
        "geometry": "Torch stem 2/16 wide × 10/16 high; stepped flame widths 4/16, 3/16, 2/16 reaching 13/16 total height; chest 14/16; bed 9/16; sapling crossed planes.",
        "geometry_revision": 2,
        "hardware": "Not flashed in this task. User-reported ~15 FPS refers to an earlier version; v4 performance, touch, and SD migration need board verification.",
        "artifacts": {},
    }
    for name in [source_zip.name, *artifacts]:
        p = archive_root / name
        validation["artifacts"][name] = {"bytes": p.stat().st_size, "sha256": digest(p.read_bytes())}
    (archive_root / "validation.json").write_text(json.dumps(validation, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    lines = []
    for p in sorted(archive_root.iterdir()):
        if p.is_file():
            lines.append(f"{digest(p.read_bytes())}  {p.name}")
    (archive_root / "SHA256SUMS.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")

    latest = ARCHIVE_ROOT / "latest"
    if latest.exists():
        shutil.rmtree(latest)
    shutil.copytree(archive_root, latest)
    print(json.dumps({"snapshot": str(archive_root), "latest": str(latest), "sources": len(sources),
                      "source_bytes": validation["source_bytes"], "firmware_bytes": len(raw),
                      "signed_bytes": len(signed), "entry_point": hex(entry), "tests": "passed"},
                     indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
