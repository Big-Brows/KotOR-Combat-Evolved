"""Select JKA fast/blue clips through the established new-stance exporter.

Does not change the completed red/yellow/staff/dual services or resources.
Run with Blender background, optionally -- --npc. Owned outputs stay ignored.
"""
from pathlib import Path
import re
import shutil

service=Path(__file__).with_name('prepare-k1-red-prototype.py')
code=service.read_text()
# Blue has its own authored guard; red's historical rollback is red-only.
start=code.index('# Restore only the original held-guard controllers.')
end=code.index('# Retarget each source component once',start)
code=code[:start]+code[end:]
start=code.index('# Independently compare the restored player guard')
end=code.index('# Unreplaced yellow clips',start)
code=code[:start]+'snapshot_error=0.\n'+code[end:]
selections={
    'red-stance':'blue-stance','k1jadred':'k1jadblue','k1jared':'k1jablue',
    'k1darkred':'k1darkblue','k1arena_darkred':'k1arena_darkblue',
    'Dark Jedi (Red)':'Dark Jedi (Blue)',
    'BOTH_SABERSLOW_STANCE':'BOTH_SABERFAST_STANCE',
    'BOTH_S3_':'BOTH_S1_','BOTH_A3_':'BOTH_A1_','BOTH_R3_':'BOTH_R1_',
    'BOTH_T3_':'BOTH_T1_', 'RedChainClip':'BlueChainClip','redChains':'blueChains',
    'red-chain-contract':'blue-chain-contract','red-motion':'blue-motion',
    'red-guard-npc':'blue-guard-npc','red-guard-player':'blue-guard-player',
    's_r{':'s_b{','y_r{':'y_b{','RED_':'BLUE_',
    'original initial red guard restored':'authored JKA fast guard',
}
for old,new in selections.items():code=code.replace(old,new)
exec(compile(code,str(service)+' [blue selection]','exec'),globals())
if not npc_mode:
    target=repo/'src/game/clientcore/include/k1bluechain.inc'
    target.write_text((output/'blue-chain-contract.inc').read_text())
    windows=[]
    for short in directions:
        windows.append(clip_report['s_'+short]['hit_window'])
    (repo/'src/game/clientcore/include/k1bluewindows.inc').write_text(
        '// JKA fast source strike windows in animation seconds.\n'
        'constexpr float blueHitWindows[7][2]={'+','.join('{'+','.join(f'{x:.9f}f' for x in pair)+'}' for pair in windows)+'};\n')
    import runpy
    runpy.run_path(str(repo/'platforms/tools/prepare-k1-blue-spin-contract.py'),run_name='__main__')
