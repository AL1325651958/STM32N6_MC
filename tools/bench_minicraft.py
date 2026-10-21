"""Compare CPU render+HUD time and every-frame checksums against a Git revision.
Host timing is relative evidence only; it does not predict STM32 FPS.
Usage: python tools/bench_minicraft.py --baseline 9011b91
"""
import argparse
import json
from pathlib import Path
import shutil
import statistics
import subprocess
ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',required=True)
    args=parser.parse_args()
    out=ROOT/'out/minicraft/perf';out.mkdir(parents=True,exist_ok=True)
    gcc=shutil.which('gcc') or 'C:/Strawberry/c/bin/gcc.exe'
    revision=subprocess.check_output(['git','rev-parse','--verify',args.baseline+'^{commit}'],cwd=ROOT,text=True).strip()
    for name in ('minicraft.c','mc_ui.c','mc_play.c'):
        (out/('baseline_'+name)).write_bytes(subprocess.check_output(['git','show',revision+':Appli/Core/Src/'+name],cwd=ROOT))
    for variant in ('baseline','optimized'):
        sources=[str(out/('baseline_'+name)) if variant=='baseline' else 'Appli/Core/Src/'+name for name in ('minicraft.c','mc_ui.c','mc_play.c')]
        subprocess.run([gcc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-I','Appli/Core/Inc','tools/minicraft_bench.c',*sources,'Appli/Core/Src/mc_mobs.c','-lm','-o',str(out/(variant+'.exe'))],cwd=ROOT,check=True)
    timings={};hashes={}
    for trial in range(3):
        for variant in (('baseline','optimized') if trial%2==0 else ('optimized','baseline')):
            result=subprocess.check_output([str(out/(variant+'.exe'))],cwd=ROOT,text=True)
            for line in result.splitlines():
                scene,ms,crc,*frames=line.split()
                assert len(frames)==32
                timings.setdefault(scene,{}).setdefault(variant,[]).append(float(ms))
                hashes.setdefault(scene,{}).setdefault(variant,set()).add((crc,tuple(frames)))
    report={'baseline_commit':revision,'frames_per_scene':32,'trials':3,'host_only':True,'scenes':{}}
    for scene,variants in timings.items():
        assert hashes[scene]['baseline']==hashes[scene]['optimized'] and len(hashes[scene]['baseline'])==1, (scene,hashes[scene])
        old=statistics.median(variants['baseline']);new=statistics.median(variants['optimized'])
        report['scenes'][scene]={'baseline_ms':old,'optimized_ms':new,'speedup':round(old/new,3),'checksum':next(iter(hashes[scene]['baseline']))[0],'frame_checksums':list(next(iter(hashes[scene]['baseline']))[1]),'samples_ms':variants,'timing_unit':'ms per simulation tick (64 ticks per sample)' if scene.startswith(('systems_','physics_')) else 'ms per render frame'}
    (out/'comparison.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
