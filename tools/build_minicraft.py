"""Reproducible STM32N6 application build, independent of generated Debug makefiles.
Usage: python tools/build_minicraft.py [--toolchain PATH_TO_BIN] [--signer PATH]
"""
import argparse
import concurrent.futures
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "out/minicraft"

def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--toolchain", type=Path)
    parser.add_argument("--signer", type=Path)
    args = parser.parse_args()
    candidates = sorted(Path("F:/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins").glob("*gnu-tools-for-stm32*/tools/bin"))
    compiler = shutil.which("arm-none-eabi-gcc")
    bindir = args.toolchain or (Path(compiler).parent if compiler else candidates[-1] if candidates else None)
    if not bindir:
        parser.error("Use --toolchain to specify the ARM GCC bin folder")
    gcc = bindir / "arm-none-eabi-gcc.exe"
    OUT.mkdir(parents=True, exist_ok=True)
    objdir = OUT / "objects"
    objdir.mkdir(exist_ok=True)
    sources = {p for p in (ROOT / "Appli/Core/Src").glob("*.c") if p.stem != "imx335"}
    sources.update((ROOT / "Appli/Core/Startup").glob("*.s"))
    sources.update((ROOT / "Appli/FileX").rglob("*.c"))
    tree = ET.parse(ROOT / "Appli/.project")
    for link in tree.findall("./linkedResources/link"):
        uri = link.findtext("locationURI", "")
        if uri.startswith("PARENT-1-PROJECT_LOC/"):
            p = ROOT / uri.removeprefix("PARENT-1-PROJECT_LOC/")
            if p.suffix.lower() in (".c", ".s") and p.stem.lower() != "imx335" and \
                    "STM32_ISP_Library" not in p.parts:
                sources.add(p)
    includes = ["Appli/Core/Inc", "Secure_nsclib", "Drivers/STM32N6xx_HAL_Driver/Inc",
                "Drivers/STM32N6xx_HAL_Driver/Inc/Legacy", "Drivers/CMSIS/Device/ST/STM32N6xx/Include",
                "Drivers/CMSIS/Include", "Appli/FileX/App", "Appli/FileX/Target",
                "Middlewares/ST/filex/common/inc", "Middlewares/ST/filex/ports/generic/inc"]
    arch = ["-mcpu=cortex-m55", "-mfpu=fpv5-d16", "-mfloat-abi=hard", "-mthumb", "-mcmse"]
    flags = arch + ["-std=gnu11", "-O2", "-g3", "-ffunction-sections", "-fdata-sections",
                    "-Wall", "-fstack-usage", "--specs=nano.specs", "-DDEBUG", "-DUSE_HAL_DRIVER",
                    "-DSTM32N657xx", "-DFX_INCLUDE_USER_DEFINE_FILE", "-DMINICRAFT_GAME_ONLY"]
    flags += ["-I" + str(ROOT / p) for p in includes]
    def compile_one(src):
        name = src.stem + "_" + hashlib.sha1(str(src).encode()).hexdigest()[:10] + ".o"
        obj = objdir / name
        result = subprocess.run([str(gcc), *flags, "-MMD", "-MF", str(obj.with_suffix('.d')),
                                 "-c", str(src), "-o", str(obj)], text=True, capture_output=True)
        return obj, result.returncode, result.stdout + result.stderr
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(12, os.cpu_count() or 4)) as pool:
        results = list(pool.map(compile_one, sorted(sources)))
    log = "".join(text for _, _, text in results)
    (OUT / "compile.log").write_text(log, encoding="utf-8")
    failures = [text for _, code, text in results if code]
    if failures:
        print("\n".join(failures))
        return 1
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj.as_posix() + '"' for obj, _, _ in results), encoding="utf-8")
    elf = OUT / "MiniCraft_N6.elf"
    link = [str(gcc), "@"+str(response), *arch, "-o", str(elf),
            "-T"+str(ROOT / "Appli/STM32N657X0HXQ_ROMxspi2_RAMxspi1.ld"),
            "--specs=nosys.specs", "--specs=nano.specs", "-static", "-Wl,--gc-sections", "-Wl,--cmse-implib",
            "-Wl,--out-implib="+str(OUT / "secure_nsclib.o"), "-Wl,-Map="+str(OUT / "MiniCraft_N6.map"),
            "-Wl,--start-group", "-lc", "-lm", "-Wl,--end-group"]
    result = subprocess.run(link, text=True, capture_output=True)
    (OUT / "link.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        print(result.stdout + result.stderr)
        return result.returncode
    binary = OUT / "MiniCraft_N6.bin"
    subprocess.run([str(bindir / "arm-none-eabi-objcopy.exe"), "-O", "binary", str(elf), str(binary)], check=True)
    subprocess.run([str(bindir / "arm-none-eabi-size.exe"), str(elf)], check=True)
    signer = args.signer or Path("F:/STM32CubeProgramer/bin/STM32_SigningTool_CLI.exe")
    if signer.is_file():
        trusted = OUT / "MiniCraft_N6-Trusted.bin"
        result = subprocess.run([str(signer), "-bin", str(binary), "-nk", "-of", "0x80000000", "-t", "fsbl",
                                 "-o", str(trusted), "-hv", "2.3", "-align", "-dump", str(trusted)],
                                input="y\n", text=True, capture_output=True)
        (OUT / "sign.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            print(result.stdout + result.stderr)
            return result.returncode
    else:
        print("Signer unavailable; unsigned ELF and BIN only.")
    print(f"Built {len(sources)} sources: {OUT}")
    print(f"Compiler diagnostics: {OUT / 'compile.log'}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
