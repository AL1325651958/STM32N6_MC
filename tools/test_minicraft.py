"""Build/run portable gameplay and save checks; optionally read real SD saves.
Usage: python tools/test_minicraft.py [--sd G:/]
Never writes to the SD card.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--sd',type=Path)
    args=parser.parse_args()
    gcc=shutil.which('gcc') or 'C:/Strawberry/c/bin/gcc.exe'
    out=ROOT/'out/minicraft/tests';out.mkdir(parents=True,exist_ok=True)
    core=['Appli/Core/Src/minicraft.c','Appli/Core/Src/mc_play.c','Appli/Core/Src/mc_mobs.c']
    cases={
        'minicraft_test':core,
        'minicraft_play_test':core+['Appli/Core/Src/mc_ui.c'],
        'minicraft_save_test':core+['Appli/Core/Src/mc_save.c'],
        'minicraft_mobs_test':core+['Appli/Core/Src/mc_save.c','Appli/Core/Src/mc_ui.c'],
        'minicraft_font_test':core+['Appli/Core/Src/mc_ui.c'],
    }
    for name,sources in cases.items():
        exe=out/(name+'.exe')
        subprocess.run([gcc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-I','Appli/Core/Inc',
                        'tools/'+name+'.c',*sources,'-lm','-o',str(exe)],cwd=ROOT,check=True)
        extra=[]
        if args.sd and name=='minicraft_save_test':
            extra=[str(p.resolve()) for p in sorted(args.sd.glob('MC[0-3].SAV'))]
        if name=='minicraft_save_test':
            for version in (2,3,4):
                legacy=ROOT/f'out/minicraft/initial-world-v{version}.SAV'
                if legacy.exists():extra.append(str(legacy))
        subprocess.run([str(exe),*extra],cwd=ROOT,check=True)
    print('All portable checks passed. Hardware touch/LCD/FPS remain on-device checks.')
if __name__=='__main__':
    main()
