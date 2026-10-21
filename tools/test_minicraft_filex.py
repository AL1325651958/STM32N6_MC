"""Exercise actual FileX and SD wrappers on a RAM disk; never access G:/ or hardware."""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    gcc = shutil.which('gcc') or 'C:/Strawberry/c/bin/gcc.exe'
    out = ROOT / 'out/minicraft/tests'
    out.mkdir(parents=True, exist_ok=True)
    exe = out / 'minicraft_filex_test.exe'
    sources = sorted((ROOT / 'Middlewares/ST/filex/common/src').glob('*.c'))
    command = [gcc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
               '-DFX_INCLUDE_USER_DEFINE_FILE', '-DALIGN_TYPE_DEFINED', '-DALIGN_TYPE=uintptr_t',
               '-Itools/filex_host', '-IAppli/FileX/App',
               '-IAppli/Core/Inc', '-IMiddlewares/ST/filex/common/inc',
               '-IMiddlewares/ST/filex/ports/generic/inc', 'tools/minicraft_filex_test.c',
               'Appli/Core/Src/minicraft.c', 'Appli/Core/Src/mc_play.c',
               'Appli/Core/Src/mc_mobs.c', 'Appli/Core/Src/mc_save.c',
               *map(str, sources), '-lm', '-o', str(exe)]
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run([str(exe)], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
