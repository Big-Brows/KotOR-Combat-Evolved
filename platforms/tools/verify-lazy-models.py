# Updated for compact model loading on 2026-11-24; see docs/LOCAL_CHANGES.md.
"""Exact-byte stream checks and malformed-input tests in ignored local storage."""
from pathlib import Path
import argparse, hashlib, importlib.util, json, shutil, subprocess, struct, time
REPO=Path(__file__).resolve().parents[2]
def sha(b): return hashlib.sha256(b).hexdigest()

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--package',type=Path,required=True)
    p.add_argument('--audit-dir',type=Path,required=True)
    p.add_argument('--fixtures-only',action='store_true')
    args=p.parse_args(); package=args.package.resolve(); audit=args.audit_dir.resolve()
    assert audit.is_relative_to(REPO/'platforms/out') and not audit.exists()
    audit.mkdir(parents=True)
    exe=REPO/'platforms/out/launcher-build/lazy-model-check.exe'
    checks=[]
    def run(arguments,expected=0):
        start=time.monotonic()
        with (audit/'streams.log').open('ab') as log:
            result=subprocess.run([str(exe),*map(str,arguments)],stdout=log,stderr=log,timeout=1800)
        assert result.returncode==expected,(result.returncode,expected)
        checks.append(dict(exit=result.returncode,seconds=round(time.monotonic()-start,3)))
    manifest=json.loads((package/'asset-manifest.json').read_text())
    models=[r for r in manifest if r['file'].endswith(('.mdl','.mdx'))]
    for row in models:
        b=(package/'user-data'/row['file']).read_bytes()
        fields=b.decode('ascii').splitlines()
        assert len(b)==200 and fields[0]=='KLM1' and fields[3]==row['sha256']
    if not args.fixtures_only:
        run(['--all',package/'user-data/override'])
    # Multi-chunk and empty targets, plus ordinary-file pass-through.
    spec=importlib.util.spec_from_file_location('pack',REPO/'platforms/tools/package-portable.py')
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    store=audit/'.k1models';store.mkdir()
    base=b'owned-data-test-base'*4096
    for label,target in [('multi',bytes(range(256))*(65536+17)),('empty',b'')]:
        patch=m.delta(base,target)
        (store/(sha(base)+'.base')).write_bytes(base)
        patch_path=store/(sha(patch)+'.delta');patch_path.write_bytes(patch)
        descriptor=audit/(label+'.mdl')
        valid=('KLM1\n'+sha(base)+'\n'+sha(patch)+'\n'+sha(target)+'\n').encode('ascii')
        descriptor.write_bytes(valid); reference=audit/(label+'.raw');reference.write_bytes(target)
        run([descriptor,reference]);run([reference,reference])
        if label=='multi':
            bad=[valid[:-1],valid+b'x',valid[:5]+b'../'+valid[8:],valid[:-65]+b'0'*64+b'\n']
            for contents in bad:
                descriptor.write_bytes(contents);run([descriptor],1)
            descriptor.write_bytes(valid)
            patch_path.write_bytes(patch[:-1]+bytes([patch[-1]^1]));run([descriptor],1)
            patch_path.write_bytes(patch)
            base_path=store/(sha(base)+'.base');base_path.write_bytes(base+b'x');run([descriptor],1)
            base_path.write_bytes(base)
            # Invalid container count with a correct content hash must still fail.
            corrupt=b'KDP1'+struct.pack('<I',33)
            (store/(sha(corrupt)+'.delta')).write_bytes(corrupt)
            descriptor.write_bytes(('KLM1\n'+sha(base)+'\n'+sha(corrupt)+'\n'+sha(target)+'\n').encode('ascii'))
            run([descriptor],1);descriptor.write_bytes(valid)
    total=sum(f.stat().st_size for f in (package/'user-data').rglob('*') if f.is_file())
    report=dict(model_count=len(models),all_model_streams_checked=not args.fixtures_only,raw_model_bytes=sum(r['bytes'] for r in models),
                installed_generated_bytes=total,checks=checks,passed=True)
    (audit/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print('LAZY_MODEL_VERIFICATION_PASS',len(models),'installed_generated_bytes',total)
if __name__=='__main__': main()
