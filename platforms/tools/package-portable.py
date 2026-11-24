# Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
# Updated for KotOR:CE v0.2 on 2026-11-24; see docs/LOCAL_CHANGES.md.
"""Create an ignored, explicit-file portable candidate; never publishes or ships raw game files.
Run with the Python interpreter already configured in the developer CMake cache.
The owned-data patch API is documented at learn.microsoft.com/windows/win32/devnotes/msdelta.
"""
from pathlib import Path
import argparse, ctypes as c, hashlib, json, os, re, shutil, struct, subprocess, zipfile

REPO = Path(__file__).resolve().parents[2]
def digest(data): return hashlib.sha256(data).hexdigest()
def private(data):
    hits = []
    for tag, text in [('profile', os.environ.get('USERPROFILE', '')), ('machine', os.environ.get('COMPUTERNAME', '')), ('user', os.environ.get('USERNAME', ''))]:
        if len(text) < 4: continue
        for value in {text, text.replace('\\', '/')}:
            for encoding in ('utf-8', 'utf-16le'):
                if value.lower().encode(encoding) in data.lower(): hits.append(tag)
    # K12's Wine discovery contains the generic string Z:/Users/ with no name.
    # Reject only populated user-directory paths, in either PE string encoding.
    if re.search(rb'[A-Za-z]:[\\/]Users[\\/][^\x00\r\n]', data, re.I): hits.append('user-path')
    if re.search(rb'[A-Za-z]\x00:\x00[\\/]\x00U\x00s\x00e\x00r\x00s\x00[\\/]\x00[^\x00]', data, re.I): hits.append('user-path-wide')
    return sorted(set(hits))

class Input(c.Structure):
    _fields_ = [('start', c.c_void_p), ('size', c.c_size_t), ('editable', c.c_int)]
class Output(c.Structure):
    _fields_ = [('start', c.c_void_p), ('size', c.c_size_t)]
api = c.WinDLL('msdelta.dll', use_last_error=True)
api.CreateDeltaB.argtypes = [c.c_longlong]*3 + [Input]*5 + [c.c_void_p, c.c_uint, c.POINTER(Output)]
api.CreateDeltaB.restype = c.c_int
api.ApplyDeltaB.argtypes = [c.c_longlong, Input, Input, c.POINTER(Output)]
api.ApplyDeltaB.restype = c.c_int
api.DeltaFree.argtypes = [c.c_void_p]
def delta_part(base, target):
    a, b = c.create_string_buffer(base), c.create_string_buffer(target)
    source, dest = Input(c.addressof(a),len(base),0), Input(c.addressof(b),len(target),0)
    out = Output()
    if not api.CreateDeltaB(1,0,0,source,dest,Input(),Input(),Input(),None,32,c.byref(out)): raise c.WinError(c.get_last_error())
    try: patch = c.string_at(out.start,out.size)
    finally: api.DeltaFree(out.start)
    p = c.create_string_buffer(patch); restored = Output()
    if not api.ApplyDeltaB(0,source,Input(c.addressof(p),len(patch),0),c.byref(restored)): raise c.WinError(c.get_last_error())
    try: assert c.string_at(restored.start,restored.size) == target
    finally: api.DeltaFree(restored.start)
    return patch

def delta(base, target):
    # MSDelta rejects large single targets; bound memory and preserve empty files.
    parts=[delta_part(base,target[i:i+16*1024*1024]) for i in range(0,len(target),16*1024*1024)]
    return b'KDP1'+struct.pack('<I',len(parts))+b''.join(struct.pack('<I',len(p))+p for p in parts)

def key_source(game, resource, restype):
    raw = (game/'chitin.key').read_bytes()
    count, resources, files_at, keys_at = struct.unpack_from('<4I',raw,8)
    for i in range(resources):
        name, kind, identity = struct.unpack_from('<16sHI',raw,keys_at+i*22)
        if name.rstrip(b'\0').decode('ascii').lower() != resource or kind != restype: continue
        _, offset, size, _ = struct.unpack_from('<IIHH',raw,files_at+(identity>>20)*12)
        name = raw[offset:offset+size].rstrip(b'\0').decode('ascii').replace('\\','/')
        with (game/name).open('rb') as f:
            head=f.read(20); table=struct.unpack_from('<I',head,16)[0]
            f.seek(table+(identity&0xfffff)*16)
            _, position, size, _ = struct.unpack('<4I',f.read(16))
            f.seek(position); data=f.read(size)
        return 'K',name,position,data
    raise ValueError('Missing original resource '+resource)

def package_release(args):
    """Extend a previous verified release; reuse unchanged bounded data patches."""
    output=args.output.resolve()
    assert output.is_relative_to(REPO/'platforms/out') and not output.exists()
    subprocess.run(['git','-C',str(REPO),'check-ignore','--quiet',str(output)],check=True)
    old=zipfile.ZipFile(args.reuse_release); prefix=old.namelist()[0].split('/')[0]+'/'
    source_rows=[l.split('\t') for l in old.read(prefix+'data/sources.tsv').decode().splitlines() if l.strip()]
    bases=[]
    for kind,name,offset,size,expected in source_rows:
        path=(args.game_dir if kind=='K' else args.jka_dir)/name
        with path.open('rb') as f:f.seek(int(offset));data=f.read(int(size))
        assert digest(data)==expected, 'Previous release input version mismatch'
        bases.append(((kind,name,int(offset),int(size),expected),data))
    previous={r[0]:r for l in old.read(prefix+'data/patches.tsv').decode().splitlines() if l.strip() for r in [l.split('\t')]}
    profile=REPO/'platforms/out/k1-engage-prototype/elbow-arena/profile'
    names={Path(n).name for n in previous if n.startswith('override/')}; native={}; expected={}
    for folder,report in [('unnamed-melee-npcs','catalog-report.json'),('unnamed-melee-npcs','rig-report.json'),('armor-rigs','rig-report.json'),('npc-dismemberment','runtime-report.json')]:
        record=json.loads((REPO/'platforms/out'/folder/report).read_text())
        assert record.get('component_pass') or record.get('native_data_pass')
        names.update(record['files']);expected.update(record['files'])
        for rig in record.get('rigs',[]):native[rig['resource']]=rig['native']
    for folder in ['purple-stance','red-stance','red-stance/npc','blue-stance','blue-stance/npc','dual-stance','dual-stance/npc']:
        record=json.loads((REPO/'platforms/out'/folder/'generation-report.json').read_text())
        assert record['component_pass'];resource=record['resource']
        for ext in ['mdl','mdx']:names.add(resource+'.'+ext);expected[resource+'.'+ext]=record[ext+'_sha256']
        for name,value in record.get('extra_assets',{}).items():
            if Path(name).suffix in ['.utc','.uti','.2da']:names.add(name);expected[name]=value
    names.add('k1npcglow.tga')
    for row in (profile/'override/k1npccuts.2da').read_text().splitlines()[3:]:
        cells=row.split()
        for index in [5,6]:native[cells[index]]=cells[2] if cells[2]!='****' else cells[1]
    targets=[('override/'+name,profile/'override'/name) for name in sorted(names)]
    targets += [(name,profile/name) for name in previous if not name.startswith('override/')]
    output.mkdir();(output/'data').mkdir();(output/'bin').mkdir()
    def add_base(value):
        kind,name,offset,data=value;record=(kind,name,offset,len(data),digest(data))
        for i,(r,_) in enumerate(bases):
            if r==record:return i
        bases.append((record,data));return len(bases)-1
    source_cache={};report=[];rows=[];reused=0
    for number,(name,path) in enumerate(targets):
        assert path.is_file(), 'Missing prepared asset: '+name
        data=path.read_bytes();assert not private(data), 'Private target: '+name
        value=digest(data)
        if path.name in expected:assert value==expected[path.name], 'Prepared asset hash mismatch: '+path.name
        prior=previous.get(name)
        if prior and value==prior[3]:
            index=int(prior[1]);patch=old.read(prefix+'data/'+prior[2]);reused+=1
        else:
            resource=native.get(path.stem,'pmbbl');kind={'mdl':2002,'mdx':3008}.get(path.suffix[1:])
            if kind:
                key=(resource,kind)
                if key not in source_cache:source_cache[key]=add_base(key_source(args.game_dir,resource,kind))
                index=source_cache[key]
            else:index=int(prior[1]) if prior else int(previous['modules/k1arena.mod'][1])
            patch=delta(bases[index][1],data)
        # Apply old patches too: reuse is accepted only after exact target reconstruction.
        assert patch[:4]==b'KDP1';cursor=8;restored=[]
        for _ in range(struct.unpack_from('<I',patch,4)[0]):
            size=struct.unpack_from('<I',patch,cursor)[0];cursor+=4;chunk=patch[cursor:cursor+size];cursor+=size
            a=c.create_string_buffer(bases[index][1]);b=c.create_string_buffer(chunk);dest=Output()
            assert api.ApplyDeltaB(0,Input(c.addressof(a),len(bases[index][1]),0),Input(c.addressof(b),len(chunk),0),c.byref(dest))
            try:restored.append(c.string_at(dest.start,dest.size))
            finally:api.DeltaFree(dest.start)
        assert cursor==len(patch) and b''.join(restored)==data
        patch_name=f'{number:04d}.delta';(output/'data'/patch_name).write_bytes(patch)
        rows.append('\t'.join([name,str(index),patch_name,value]));report.append(dict(file=name,sha256=value,bytes=len(data),patch_bytes=len(patch)))
        if number%50==0:print('PATCH',number+1,len(targets),'reused',reused,flush=True)
    (output/'data/sources.tsv').write_text('\n'.join('\t'.join(map(str,b[0])) for b in bases)+'\n',encoding='utf-8')
    (output/'data/patches.tsv').write_text('\n'.join(rows)+'\n',encoding='utf-8')
    for name in old.namelist():
        relative=name[len(prefix):]
        if relative.startswith('licenses/') or relative=='NOTICE.txt':
            dest=output/relative;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(old.read(name))
    readme=old.read(prefix+'README.txt').decode('utf-8').replace('0.1','0.2')
    readme=readme.replace('double-bladed','Purple single-saber').replace('double bladed','Purple single-saber')
    (output/'README.txt').write_text(readme,encoding='utf-8')
    shutil.copy2(REPO/'platforms/out/launcher-build/KotOR-CE.exe',output/'KotOR-CE.exe')
    shutil.copy2(REPO/'platforms/out/k1-private-pc/windows/k1.exe',output/'bin/k1.exe')
    (output/'asset-manifest.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for file in output.rglob('*'):
        if file.is_file():assert not private(file.read_bytes()) and file.suffix.lower()!='.pdb', 'Private package content (values suppressed)'
    (REPO/'platforms/out/release-v0.2/package-result.json').write_text(json.dumps(dict(targets=len(targets),reused_patches=reused,privacy_pass=True),indent=2))
    archive=output.with_name(output.name+'.zip');assert not archive.exists()
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for file in sorted(output.rglob('*')):
            if file.is_file():
                info=zipfile.ZipInfo(output.name+'/'+file.relative_to(output).as_posix(),(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16;z.writestr(info,file.read_bytes())
    print('PACKAGE_PASS',len(targets),'reused',reused,flush=True)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--game-dir',type=Path,required=True)
    parser.add_argument('--jka-dir',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--reuse-release',type=Path)
    args=parser.parse_args()
    if args.reuse_release:return package_release(args)
    output=args.output.resolve()
    assert output.is_relative_to(REPO/'platforms/out')
    subprocess.run(['git','-C',str(REPO),'check-ignore','--quiet',str(output)],check=True)
    assert not output.exists(), 'Choose a new package directory; preserve previous candidates.'
    output.mkdir(parents=True); (output/'data').mkdir(); (output/'bin').mkdir()
    profile=REPO/'platforms/out/k1-engage-prototype/elbow-arena/profile'
    bases=[]; rows=[]; report=[]
    def base(kind,name,offset,data):
        record=(kind,name,offset,len(data),digest(data))
        if record not in [b[0] for b in bases]: bases.append((record,data))
        return [b[0] for b in bases].index(record)
    native={}
    for resource in ('pmbbl','n_darkjedim','n_sithsoldier'):
        for ext,kind in [('mdl',2002),('mdx',3008)]:
            try: native[resource,ext]=base(*key_source(args.game_dir,resource,kind))
            except ValueError:
                if resource=='n_sithsoldier': continue
                raise
    default=base('K','modules/tar_m02ae.rim',0,(args.game_dir/'modules/tar_m02ae.rim').read_bytes())
    # Require the actual owned JKA animation, marks and audio archive entries.
    jka_indices=[]
    for archive,entry in [('assets1.pk3','models/players/_humanoid/_humanoid.gla'),('assets1.pk3','gfx/damage/rivetmark.tga'),('assets0.pk3','sound/weapons/saber/saberhitwall1.mp3')]:
        path=args.jka_dir/'base'/archive
        with zipfile.ZipFile(path) as z:
            info=z.getinfo(entry)
        with path.open('rb') as f:
            f.seek(info.header_offset); head=f.read(30)
            n,e=struct.unpack_from('<HH',head,26); offset=info.header_offset+30+n+e
            f.seek(offset); data=f.read(info.compress_size)
        jka_indices.append(base('J','base/'+archive,offset,data))
    # Preserve the accepted installed assets exactly. Do not copy diagnostics,
    # settings, personal saves, screenshots or the rest of the development profile.
    allowed=re.compile(r'^(?:k1(?:j\w+|d[psd]\w+|dark\w+|cuts|yellow|fnt\w+)|jkcap(?:gl)?|jkscorch[123]|(?:dialogfont|fnt_)\w+|ios_button_\w+|(?:black|white|start)button_\w+)\.(?:mdl|mdx|tpc|tga|txi|wav|utc|2da)$')
    assets=sorted((profile/'override').iterdir())
    assert len(assets)==177 and all(allowed.fullmatch(p.name) for p in assets)
    targets=[('override/'+p.name,p) for p in assets]
    targets += [('modules/k1arena.mod',profile/'modules/k1arena.mod')]
    targets += [('jka-marks/'+n,profile/'jka-marks'/n) for n in ('rivet.tga','glow.tga')]
    targets += [('saves/000003 - Game2/'+n,profile/'saves/000003 - Game2'/n) for n in ('GLOBALVARS.res','PARTYTABLE.res','SAVEGAME.sav','savenfo.res')]
    for number,(name,path) in enumerate(targets):
        data=path.read_bytes(); assert not private(data), 'Private content in target '+name
        ext=path.suffix[1:]; resource='pmbbl'
        if path.stem.startswith(('k1dd','k1jadark','k1jadstaff')): resource='n_darkjedim'
        elif path.stem.startswith(('k1ds','k1jasith')): resource='n_sithsoldier'
        index=native.get((resource,ext),default)
        if name.startswith('jka-marks/') or path.stem.startswith('jkscorch'): index=jka_indices[1 if name.startswith('jka-marks/') else 2]
        patch=delta(bases[index][1],data)
        patch_name=f'{number:04d}.delta'
        (output/'data'/patch_name).write_bytes(patch)
        rows.append('\t'.join([name,str(index),patch_name,digest(data)]))
        report.append(dict(file=name,sha256=digest(data),bytes=len(data),patch_bytes=len(patch)))
        print('PATCH',number+1,len(targets),name,len(patch),flush=True)
    (output/'data/sources.tsv').write_text('\n'.join('\t'.join(map(str,b[0])) for b in bases)+'\n',encoding='utf-8')
    (output/'data/patches.tsv').write_text('\n'.join(rows)+'\n',encoding='utf-8')
    shutil.copy2(REPO/'platforms/out/launcher-build/KotOR-CE.exe',output/'KotOR-CE.exe')
    shutil.copy2(REPO/'platforms/out/k1-private-pc/windows/k1.exe',output/'bin/k1.exe')
    (output/'asset-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    # Only installed MSVC redistributables; never arbitrary DLLs from PATH.
    where=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
    vs=Path(subprocess.check_output([str(where),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip())
    crt=sorted((vs/'VC/Redist/MSVC').glob('*/x64/Microsoft.VC143.CRT'))[-1]
    for dll in crt.glob('*.dll'): shutil.copy2(dll,output/'bin'/dll.name)
    notices=output/'licenses'; notices.mkdir()
    shutil.copy2(REPO/'LICENSE',notices/'K12-LICENSE.txt')
    shutil.copy2(REPO/'README.md',notices/'KotOR-CE-README.md')
    shutil.copy2(REPO/'platforms/out/k1-engage-prototype/hd-fonts/downloads/OFL.txt',notices/'KMRP-font-OFL.txt')
    for source,name in [('third_party/openjk/COPYING','OpenJK-COPYING.txt'),('platforms/third_party/miniaudio/LICENSE','miniaudio-LICENSE.txt'),('platforms/video/k2bink/LICENSE.unbikit.md','k2bink-LICENSE.md')]:
        shutil.copy2(REPO/source,notices/name)
    # Preserve bundled dependency notices, including SPDX/license text in source files.
    roots=[REPO/'platforms/out/k1-host-pc/_deps/k2sdl2-src',REPO/'platforms/out/k1-host-pc/_deps/k2freetype-src',REPO/'platforms/out/k1-host-pc/_deps/k2zlib-src',REPO/'platforms/out/_deps-k1/miniz-src',REPO/'platforms/out/_deps-k1/7zip-src',REPO/'platforms/audio',REPO/'platforms/bink']
    for root in roots:
        for file in root.rglob('*'):
            if (file.name.lower().startswith(('license','copying','notice','authors')) or file.name.lower() in ('ftl.txt','gplv2.txt')) and file.is_file():
                dest=notices/root.name/file.relative_to(root);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,dest)
    (output/'README.txt').write_text('KotOR:CE portable candidate\n\nExtract the complete folder to a writable location and double-click KotOR-CE.exe.\nOwned KotOR 1 (Steam/GOG) and Jedi Academy are required. Select folders if asked.\nSetup verifies the source data version before applying the arena patches locally.\nSettings, logs and saves are in user-data beside the launcher. Keep that folder\nwhen moving your copy; exclude it when sharing a fresh package.\n\nCurrent arena: double-bladed player; press 0 for Spawn Menu. Font issues remain.\nThis candidate has not been tested with an actual GOG installation or on another PC.\nGame-derived patches and third-party notices need final review before publication.\nNo permission to publish is implied by creating this local candidate.\n',encoding='utf-8')
    failures=[]
    for file in output.rglob('*'):
        if file.is_file():
            hits=private(file.read_bytes())
            if file.suffix.lower()=='.pdb': hits.append('private-symbols')
            if hits: failures.append(dict(file=str(file.relative_to(output)),categories=hits))
    audit=REPO/'platforms/out/launcher-audit'; (audit/'package-scan.json').write_text(json.dumps(dict(passed=not failures,failures=failures,targets=len(targets)),indent=2))
    assert not failures, 'Privacy scan failed; see ignored package-scan.json (values suppressed)'
    archive=output.with_name(output.name+'.zip')
    assert not archive.exists(), 'Preserve existing archives.'
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for file in sorted(output.rglob('*')):
            if file.is_file():
                info=zipfile.ZipInfo(output.name+'/'+file.relative_to(output).as_posix(),(1980,1,1,0,0,0))
                info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
                z.writestr(info,file.read_bytes())
    print('PACKAGE_PASS',len(targets),'patched assets',flush=True)
if __name__=='__main__': main()
