# Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
# Updated for KotOR:CE v0.2 on 2026-11-24; see docs/LOCAL_CHANGES.md.
# Updated for compact model loading on 2026-11-24; see docs/LOCAL_CHANGES.md.
"""Focused portable integration checks. Writes only beneath ignored project output."""
from pathlib import Path
import argparse, configparser, ctypes as c, hashlib, importlib.util, json, os, shutil, subprocess, time
REPO=Path(__file__).resolve().parents[2]
def sha(data): return hashlib.sha256(data).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--audit-dir',type=Path,default=REPO/'platforms/out/launcher-audit');p.add_argument('--package',type=Path,required=True);p.add_argument('--game-dir',type=Path,required=True);p.add_argument('--jka-dir',type=Path,required=True);args=p.parse_args()
 package=args.package.resolve();audit=args.audit_dir.resolve();assert audit.is_relative_to(REPO/'platforms/out');audit.mkdir(parents=True,exist_ok=True);root=audit/'relocation tests';assert not root.exists()
 assert package.is_relative_to(REPO/'platforms/out') and root.is_relative_to(REPO/'platforms/out')
 root.mkdir();work=root/'first location';shutil.copytree(package,work,ignore=shutil.ignore_patterns('user-data'))
 checks=[]
 def run(options,expected=0):
  start=time.monotonic();r=subprocess.run([str(work/'KotOR-CE.exe'),*options],cwd=root,timeout=1800)
  assert r.returncode==expected,(options,r.returncode,expected)
  checks.append(dict(check=' '.join(options[:1]),exit=r.returncode,seconds=round(time.monotonic()-start,2)))
 def originals():
  files=[*args.game_dir.glob('*.ini')]
  saves=args.game_dir/'saves'
  if saves.exists():files += [f for f in saves.rglob('*') if f.is_file()]
  return {str(f.relative_to(args.game_dir)):sha(f.read_bytes()) for f in files}
 before=originals()
 run(['--detect'])
 run(['--detect','--game-dir',str(root/'missing')],4)
 run(['--detect','--jka-dir',str(root/'missing')],5)
 wrong=root/'wrong-game';(wrong/'data').mkdir(parents=True);(wrong/'modules').mkdir();(wrong/'streamvoice').mkdir();(wrong/'chitin.key').write_bytes(b'')
 run(['--detect','--game-dir',str(wrong)],4)
 run(['--prepare-only'])
 manifest=json.loads((work/'asset-manifest.json').read_text())
 assert len(manifest)>184 and any(f['file']=='override/k1npcs.2da' for f in manifest)
 compact=(work/'data/lazy-models-v1').exists()
 for f in manifest:
  data=(work/'user-data'/f['file']).read_bytes()
  if compact and f['file'].endswith(('.mdl','.mdx')):
   fields=data.decode('ascii').splitlines()
   assert len(data)==200 and fields[0]=='KLM1' and fields[3]==f['sha256']
  else:assert sha(data)==f['sha256']
 checks.append(dict(check='all_reconstructed_assets',count=len(manifest),passed=True))
 save=work/'user-data/saves/000003 - Game2/savenfo.res';data=save.read_bytes();save.write_bytes(data+b'PORTABLE_SAVE_SENTINEL')
 run(['--prepare-only']);assert save.read_bytes()==data+b'PORTABLE_SAVE_SENTINEL';save.write_bytes(data)
 checks.append(dict(check='existing_save_preserved',passed=True))
 source=work/'data/sources.tsv';data=source.read_bytes();rows=data.decode().splitlines();fields=rows[0].split('\t');fields[-1]='0'*64;rows[0]='\t'.join(fields);source.write_text('\n'.join(rows)+'\n')
 run(['--prepare-only'],11);source.write_bytes(data)
 immutable=work/'user-data/override/k1cuts.2da';immutable.unlink()
 patch_name=next(row.split('\t')[2] for row in (work/'data/patches.tsv').read_text().splitlines() if row.startswith('override/k1cuts.2da\t'))
 patch=work/'data'/patch_name;patch_bytes=patch.read_bytes();patch.write_bytes(b'KDP1')
 run(['--prepare-only'],11);patch.write_bytes(patch_bytes)
 run(['--prepare-only']);expected=next(f['sha256'] for f in manifest if f['file']=='override/k1cuts.2da');assert sha(immutable.read_bytes())==expected
 checks.append(dict(check='repair_missing_asset',passed=True))
 kernel=c.WinDLL('kernel32',use_last_error=True);kernel.CreateFileW.argtypes=[c.c_wchar_p,c.c_uint,c.c_uint,c.c_void_p,c.c_uint,c.c_uint,c.c_void_p];kernel.CreateFileW.restype=c.c_void_p;kernel.CloseHandle.argtypes=[c.c_void_p]
 handle=kernel.CreateFileW(str(work/'user-data/launcher.lock'),0x40000000,0,None,3,0x80,None)
 assert handle!=c.c_void_p(-1).value
 try:run(['--prepare-only'],7)
 finally:kernel.CloseHandle(handle)
 game=work/'bin/k1.exe';hidden=work/'bin/k1.test-hold';game.rename(hidden)
 try:run(['--smoke'],8)
 finally:hidden.rename(game)
 moved=root/'moved location';assert work.is_relative_to(root) and moved.is_relative_to(root);work.rename(moved);work=moved
 for i in range(2):
  run(['--smoke'])
  text=(work/'user-data/game.log').read_text(errors='replace')
  assert 'K2_WINDOW_PRODUCT title=KotOR:CE' in text
  assert 'K1_PORTABLE_PURPLE_PASS rig=k1japurp rate=1.3' in text
  if compact:
   assert 'K1_PORTABLE_MODELS_PASS models=23 clips=69 player=k1japurp' in text and 'K1_LAZY_MODEL_PASS' in text
  assert 'K1_SANDBOX_READY initial_npcs=0 loadout=1' in text and 'K1_FIXTURE_EQUIP_PASS' in text
  assert not __import__('re').search(r'K1_\w+_FAIL',text)
  config=configparser.ConfigParser();config.read(work/'user-data/swkotor.ini')
  assert Path(config['Alias']['SAVES']).resolve()==(work/'user-data/saves').resolve()
 checks.append(dict(check='relocated_repeat_startup_and_save_alias',passed=True))
 assert originals()==before
 checks.append(dict(check='original_configs_and_saves_unchanged',files=len(before),passed=True))
 spec=importlib.util.spec_from_file_location('package',REPO/'platforms/tools/package-portable.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 binaries=[]
 for file in [package/'KotOR-CE.exe',package/'bin/k1.exe']:
  d=file.read_bytes();assert not m.private(d)
  import struct
  pe=struct.unpack_from('<I',d,60)[0];opt=pe+24;rva,size=struct.unpack_from('<II',d,opt+112+48);count=struct.unpack_from('<H',d,pe+6)[0];base=opt+struct.unpack_from('<H',d,pe+20)[0];types=[]
  for n in range(count):
   virtual,address,raw,off=struct.unpack_from('<4I',d,base+n*40+8)
   if address<=rva<address+max(virtual,raw):
    debug=off+rva-address
    types=[struct.unpack_from('<I',d,debug+n*28+12)[0] for n in range(size//28)]
  assert 2 not in types, 'Private CodeView/PDB record'
  binaries.append(dict(file=str(file.relative_to(package)),sha256=sha(d),bytes=len(d),debug_types=types,personal_identifiers=False))
 result=dict(passed=True,checks=checks,binaries=binaries)
 (audit/'verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
