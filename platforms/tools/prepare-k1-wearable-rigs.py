"""Audit owned K1 wearables; generate six body-specific player stance rigs in Blender.

Run with --audit using the existing Python interpreter, or without it in Blender.
Outputs and extracted source remain ignored. Existing authored stances are inputs.
"""
from pathlib import Path
import importlib.util
import json
import sys
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'platforms/out/armor-rigs'
STYLES = {'single':'k1jarig','red':'k1jared','blue':'k1jablue',
          'purple':'k1japurp','dual':'k1jadual','staff':'k1jastaff'}


def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def audit():
    subprocess.run(['git','-C',str(ROOT),'check-ignore','--quiet',str(OUT/'audit.json')],check=True)
    OUT.mkdir(exist_ok=True)
    owned=load('owned_wearables',ROOT/'platforms/out/k1-engage-prototype/dark-jedi/inspect-owned.py')
    appearances=owned.twoda(owned.resource('appearance',2017))
    bases=owned.twoda(owned.resource('baseitems',2017))
    armor_bases={k:v for k,v in bases.items() if int(v.get('equipableslots') or '0',0)&2 and v.get('droidorhuman')=='1'}
    # All six PC sex/size combinations, plus humanoid party members' unique clothes.
    rows={k:v for k,v in appearances.items() if v.get('modeltype')=='B' and
          (v.get('label','').startswith(('P_FEM_','P_MAL_','Party_NPC_')))}
    bodies={}
    for row,a in rows.items():
        for letter in 'abcdefghij':
            body=a.get('model'+letter,'').lower()
            if body:bodies.setdefault(body,[]).append({'appearance':int(row),'column':'model'+letter,'label':a['label']})
    items=[];disguises={}
    archives=[('global',{n+'.uti':owned.resource(n,t) for n,t in owned.entries if t==2025})]
    archives += [(p.stem,owned.read_archive(p)) for p in sorted((owned.game/'modules').glob('*_s.rim'))]
    for module,archive in archives:
        for name,raw in archive.items():
            if not name.endswith('.uti'):continue
            data=owned.gff(raw);base=armor_bases.get(str(data.get('BaseItem')))
            if not base:continue
            item={'module':module,'resource':name,'base':base['label'],'bodyvar':base['bodyvar'],
                  'texture':data.get('TextureVar'),'model':data.get('ModelVariation')}
            for prop in data.get('PropertiesList',[]):
                if prop.get('PropertyName')!=59:continue
                index=str(prop.get('Subtype'));a=appearances.get(index,{})
                body=a.get('modelb','').lower() if a.get('modeltype')=='B' else a.get('race','').lower()
                if body:
                    bodies.setdefault(body,[]).append({'disguise':name,'appearance':int(index),'label':a.get('label','')})
                    disguises[body]=a.get('label','');item['disguise']=int(index)
            items.append(item)
    missing=[n for n in bodies if (n,2002) not in owned.entries or (n,3008) not in owned.entries]
    assert not missing,('Missing native wearable models',missing)
    report={'bodies':dict(sorted(bodies.items())),'items':items,'disguises':disguises,
            'styles':STYLES,'body_count':len(bodies),'item_instances':len(items)}
    (OUT/'audit.json').write_text(json.dumps(report,indent=2))
    print('WEARABLE_AUDIT',len(bodies),'bodies',len(items),'item instances','disguises',disguises,flush=True)
    return report


if __name__=='__main__':
    report=audit()
    if '--audit' not in sys.argv:
        rig=load('wearable_native_transfer',ROOT/'platforms/tools/prepare-k1-native-npc-rigs.py')
        rig.OUT=OUT
        rig.main({'bodies':set(report['bodies'])-{'pmbbl'},'donors':STYLES,
                  'prefix':'k1a','identity':'wearable-player/','table':'k1armorrigs',
                  'extra_mappings':[{'native':'pmbbl',**STYLES}]})
