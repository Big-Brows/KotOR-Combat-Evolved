# Updated for compact model loading on 2026-11-24; see docs/LOCAL_CHANGES.md.
"""Compare native model poses with compact streams and unchanged raw v0.2 resources."""
from pathlib import Path
import argparse, configparser, ctypes as c, json, os, shutil, subprocess, time
REPO=Path(__file__).resolve().parents[2]

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--package',type=Path,required=True)
    p.add_argument('--reference-profile',type=Path,required=True)
    p.add_argument('--audit-dir',type=Path,required=True)
    p.add_argument('--game-dir',type=Path,required=True)
    args=p.parse_args();audit=args.audit_dir.resolve();package=args.package.resolve()
    assert audit.is_relative_to(REPO/'platforms/out') and not audit.exists()
    subprocess.run(['git','-C',str(REPO),'check-ignore','--quiet',str(audit)],check=True)
    audit.mkdir(parents=True);reference=audit/'raw-reference';reference.mkdir()
    manifest=json.loads((package/'asset-manifest.json').read_text())
    needed=set('k1jarig k1jared k1jablue k1japurp k1jadual k1jastaff k1jadark k1jadred k1jadblue k1jaddual k1jadstaff'.split())
    table=(package/'user-data/override/k1armorrigs.2da').read_text().splitlines()
    for prefix in ['pm','pf']:
        fields=next(line.split() for line in table if len(line.split())>2 and line.split()[1].startswith(prefix))
        needed.update(fields[2:8])
    for row in manifest:
        relative=Path(row['file']);assert not relative.is_absolute() and '..' not in relative.parts
        src=args.reference_profile/relative;dst=reference/relative;dst.parent.mkdir(parents=True,exist_ok=True)
        if src.suffix in {'.mdl','.mdx'} and src.stem not in needed and row['bytes']>=1024*1024:
            continue
        # Copy the exercised rigs and small dependencies. No full 15GiB clone or
        # hard links to the preserved reference; saves/configuration are independent.
        shutil.copyfile(src,dst)
    kernel=c.WinDLL('kernel32',use_last_error=True)
    kernel.CreateFileW.argtypes=[c.c_wchar_p,c.c_uint,c.c_uint,c.c_void_p,c.c_uint,c.c_uint,c.c_void_p]
    kernel.CreateFileW.restype=c.c_void_p;kernel.CloseHandle.argtypes=[c.c_void_p]
    reports=[]
    for label,profile in [('compact',package/'user-data'),('raw',reference)]:
        config=configparser.ConfigParser()
        config.read_dict({'Display Options':{'FullScreen':'0'},'Graphics Options':{'EnableHardwareMouse':'0'},'Alias':{'SAVES':str(profile/'saves')}})
        with (profile/'swkotor.ini').open('w') as f:config.write(f)
        assert Path(config['Alias']['SAVES']).resolve()==(profile/'saves').resolve()
        lock=kernel.CreateFileW(str(profile/'launcher.lock'),0x40000000,0,None,4,0x80,None)
        assert lock!=c.c_void_p(-1).value
        try:
            env={k:v for k,v in os.environ.items() if not k.startswith(('K1_','K2_'))}
            for key in ['K2_SKIP_MOVIES','K2_HIDDEN','K1_ENGAGE_PROTOTYPE','K1_ENGAGE_SANDBOX','K1_ENGAGE_RED','K1_ENGAGE_PURPLE','K1_ENGAGE_DISMEMBERMENT','K1_ENGAGE_ANATOMICAL_RIG','K1_ENGAGE_VIEW_PITCH','K1_ENGAGE_ARENA','K1_ENGAGE_ENEMY_AI','K1_ENGAGE_DARK_JEDI','K1_PORTABLE_SMOKE','K1_PORTABLE_MODEL_TEST']:
                env[key]='1'
            env.update(K2_CLOUDSAVES='0',K2_WORKSHOP='0',K2_LOGFILE='0',K2_ASSET_DIR=str(args.game_dir.resolve()),K2_USER_DIR=str(profile),K1_ENGAGE_NPC_TYPE='dark-jedi',K1_WINDOW='1280x720',K2_INPUT='100:key:40;1200:quit:0')
            start=time.monotonic()
            with (audit/(label+'.log')).open('wb') as log:
                r=subprocess.run([str(package/'bin/k1.exe')],cwd=profile,env=env,stdout=log,stderr=log,timeout=180)
            assert r.returncode==0,r.returncode
            text=(audit/(label+'.log')).read_text(errors='replace')
            assert 'K1_PORTABLE_MODELS_PASS models=23 clips=69 player=k1japurp' in text
            assert 'K1_LAZY_MODEL_FAIL' not in text and 'K1_PORTABLE_MODELS_FAIL' not in text
            restores=text.count('K1_LAZY_MODEL_PASS')
            assert (restores>0)==(label=='compact')
            shutil.copyfile(profile/'portable-model-poses.tsv',audit/(label+'-poses.tsv'))
            reports.append(dict(mode=label,seconds=round(time.monotonic()-start,3),restores=restores))
        finally:kernel.CloseHandle(lock)
    a=(audit/'compact-poses.tsv').read_bytes();b=(audit/'raw-poses.tsv').read_bytes()
    assert a==b,'Native posed transforms differ'
    result=dict(passed=True,models=23,clips=69,pose_records=len(a.splitlines()),runs=reports)
    (audit/'verification.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
if __name__=='__main__':main()
