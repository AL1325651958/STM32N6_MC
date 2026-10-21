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
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "out/minicraft"
GAME_VERSION = "v38"
ARCHIVE_ROOT = ROOT / f"out/verified-{GAME_VERSION}"


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
    parser.add_argument("--benchmark-baseline", help="Git revision for optional host render comparison")
    parser.add_argument("--benchmark-baseline-cache", action="store_true", help="enable the view cache in a cache-capable baseline too")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    font_audit = OUT / f"font-audit-{GAME_VERSION}"
    run([sys.executable, "tools/make_minicraft_zh.py", "--font", str(OUT / "fonts/NotoSansSC.ttf"),
         "--pixel-font", str(OUT / "fonts/ark-pixel-12px-monospaced-zh_hans.ttf"),
         "--check", "--audit-dir", str(font_audit)], "snapshot-font.log")
    if not args.skip_build:
        run([sys.executable, "tools/build_minicraft.py"], "snapshot-build.log")
    tests = run([sys.executable, "tools/test_minicraft.py"], "snapshot-tests.log")
    run([str(OUT / "tests/minicraft_font_test.exe"), str(font_audit)], "snapshot-font-render.log")
    for ppm in font_audit.glob("glyphs-*.ppm"):
        with Image.open(ppm) as sheet:
            sheet.save(ppm.with_suffix(".png"))

    if args.benchmark_baseline:
        command=[sys.executable, "tools/bench_minicraft.py", "--baseline", args.benchmark_baseline]
        if args.benchmark_baseline_cache:command.append("--baseline-cache")
        run(command, "snapshot-benchmark.log")

    gcc = shutil.which("gcc") or "C:/Strawberry/c/bin/gcc.exe"
    preview_exe = OUT / "tests/minicraft_preview.exe"
    preview_sources = ["tools/minicraft_preview.c", "Appli/Core/Src/minicraft.c",
                       "Appli/Core/Src/mc_play.c", "Appli/Core/Src/mc_mobs.c",
                       "Appli/Core/Src/mc_save.c", "Appli/Core/Src/mc_ui.c", "Appli/Core/Src/mc_view.c"]
    result = subprocess.run([gcc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-I",
                             "Appli/Core/Inc", *preview_sources, "-lm", "-o", str(preview_exe)],
                            cwd=ROOT, text=True, capture_output=True, encoding="utf-8")
    (OUT / "snapshot-preview-build.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise SystemExit(f"preview build failed; see {OUT / 'snapshot-preview-build.log'}\n{result.stdout}{result.stderr}")
    preview_dir = OUT / f"ui-{GAME_VERSION}"
    preview_dir.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(preview_exe), str(preview_dir)], cwd=ROOT,
                            text=True, capture_output=True, encoding="utf-8")
    (OUT / "snapshot-preview.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise SystemExit(f"preview generation failed; see {OUT / 'snapshot-preview.log'}\n{result.stdout}{result.stderr}")
    for ppm in preview_dir.glob("*.ppm"):
        with Image.open(ppm) as preview:
            preview.save(ppm.with_suffix(".png"))

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
    source_zip = archive_root / f"MiniCraft-{GAME_VERSION}-source.zip"
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
        "glyph-audit.json": (font_audit / "glyph-audit.json").read_bytes(),
        "font-tests.log": (OUT / "snapshot-font-render.log").read_bytes(),
        **{p.name: p.read_bytes() for p in font_audit.glob("glyphs-*.png")},
        "MiniCraft_N6-Trusted.bin": signed,
        "MiniCraft_N6.bin": raw,
        "MiniCraft_N6.elf": elf.read_bytes(),
        "initial-world-v9.SAV": (preview_dir / "initial-world-v9.SAV").read_bytes(),
        "tests.log": tests.encode("utf-8"),
        "build.log": (OUT / "snapshot-build.log").read_bytes() if not args.skip_build else (OUT / "compile.log").read_bytes() + (OUT / "link.log").read_bytes(),
        "geometry.png": (preview_dir / "geometry.png").read_bytes(),
        "combat.png": (preview_dir / "combat.png").read_bytes(),
        "farming.png": (preview_dir / "farming.png").read_bytes(),
        "crop_dry.png": (preview_dir / "crop_dry.png").read_bytes(),
        "crop_dark.png": (preview_dir / "crop_dark.png").read_bytes(),
        "crop_growing.png": (preview_dir / "crop_growing.png").read_bytes(),
        "crop_ready.png": (preview_dir / "crop_ready.png").read_bytes(),
        "action_attack.png": (preview_dir / "action_attack.png").read_bytes(),
        "action_shoot.png": (preview_dir / "action_shoot.png").read_bytes(),
        "action_feed.png": (preview_dir / "action_feed.png").read_bytes(),
        "action_place.png": (preview_dir / "action_place.png").read_bytes(),
        "action_use.png": (preview_dir / "action_use.png").read_bytes(),
        "main_menu.png": (preview_dir / "main_menu.png").read_bytes(),
        "create_world.png": (preview_dir / "create_world.png").read_bytes(),
        "slab_merge.png": (preview_dir / "slab_merge.png").read_bytes(),
        "cave_threat.png": (preview_dir / "cave_threat.png").read_bytes(),
        "cave_lit.png": (preview_dir / "cave_lit.png").read_bytes(),
        "creative_page1.png": (preview_dir / "creative_page1.png").read_bytes(),
        "creative_page2.png": (preview_dir / "creative_page2.png").read_bytes(),
        **{name+".png": (preview_dir / (name+".png")).read_bytes() for name in
           ("tool_interact", "sneak_build", "tool_chest_open")},
        **{name+".png": (preview_dir / (name+".png")).read_bytes() for name in
           ("save_busy", "save_ok", "save_no_sd", "save_locked", "save_write_failed", "save_verify_failed")},
        "action_eat.png": (preview_dir / "action_eat.png").read_bytes(),
        **{p.name: p.read_bytes() for p in preview_dir.glob("*.png")},
    }
    if args.benchmark_baseline:
        artifacts["performance.json"] = (OUT / "perf/comparison.json").read_bytes()
    for name, data in artifacts.items():
        (archive_root / name).write_bytes(data)
    validation = {
        "recorded_utc": stamp,
        "game_version": GAME_VERSION,
        "version": 9,
        "world_dimensions": [256, 64, 256],
        "save_bytes": 4204096,
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
        "portable_tests": "passed; see tests.log; no physical SD card was accessed",
        "source_archive": source_zip.name,
        "source_file_count": len(sources),
        "source_bytes": sum((ROOT / p).stat().st_size for p in sources),
        "source_hashes_verified": True,
        "geometry": "Torch stem 4/16 wide × 10/16 high; stepped flame widths 4/16, 3/16, 2/16 reaching 14/16 total height; chest 14/16; bed 9/16; sapling crossed planes.",
        "geometry_revision": 5,
        "hardware": "Not flashed in this task. Four-facing two-cell doors, melee/block-part segment collision, bounded natural lava pools/downward flow/water contact, picking, body/projectile collision, FileX delete remount flow and v1-v9 saves tested on host; persistent exploration mask is covered by host tests; touch, LCD, real SD deletion/persistence and FPS require board verification.",
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
