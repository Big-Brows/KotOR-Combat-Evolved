"""Finalize ignored wearable manifest and native equipment probe cases after generation."""
from pathlib import Path
import hashlib
import json

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'platforms/out/armor-rigs'
audit=json.loads((OUT/'audit.json').read_text())
path=OUT/'rig-report.json'
report=json.loads(path.read_text())
assert report['component_pass'] and len(report['mappings'])==len(audit['bodies'])==76
assert len(report['rigs'])==450
for name,sha in report['files'].items():
    assert hashlib.sha256((OUT/'assets'/name).read_bytes()).hexdigest()==sha,name
def witness(body):
    return min(audit['bodies'][body],key=lambda w:(not w.get('label','').startswith(('P_FEM_','P_MAL_')),w['appearance']))
rows=['2DA V2.0','','native appearance variant item disguise sex']
for i,body in enumerate(sorted(audit['bodies'])):
    w=witness(body)
    if 'disguise' in w:
        item=w['disguise'].removesuffix('.uti');variant=2;disguise=1
    else:
        letter=w['column'][-1];variant=ord(letter)-ord('a')+1;disguise=0
        candidates=[v for v in audit['items'] if v['bodyvar'].lower()==letter and 'disguise' not in v and v['module']=='global']
        candidates.sort(key=lambda v:(not v['resource'].endswith('01.uti'),v['resource']))
        assert letter=='a' or candidates,(body,'No native equipment witness')
        item=candidates[0]['resource'].removesuffix('.uti') if candidates and letter!='a' else '****'
    sex=int(body.startswith('pfb') or body.startswith(('p_bastila','p_juhani','p_mission')))
    rows.append(f'{i} {body} {w["appearance"]} {variant} {item} {disguise} {sex}')
table=OUT/'assets/k1armorcheck.2da';table.write_text('\n'.join(rows)+'\n',encoding='ascii')
report['files'][table.name]=hashlib.sha256(table.read_bytes()).hexdigest()
report['body_count']=76;report['style_count']=6;report['native_body']='pmbbl'
report['audit_sha256']=hashlib.sha256((OUT/'audit.json').read_bytes()).hexdigest()
report['donors']={}
profile=ROOT/'platforms/out/k1-engage-prototype/elbow-arena/profile/override'
for resource in audit['styles'].values():
    for ext in ('.mdl','.mdx'):
        name=resource+ext;report['donors'][name]=hashlib.sha256((profile/name).read_bytes()).hexdigest()
path.write_text(json.dumps(report,indent=2))
print('WEARABLE_MANIFEST_PASS bodies=76 stances=6 new_rigs=450 cases=76 files='+str(len(report['files'])))
