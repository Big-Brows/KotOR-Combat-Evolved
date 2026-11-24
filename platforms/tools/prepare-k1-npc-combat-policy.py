"""Resolve approved menu variants to native source identities; never edit UTCs."""
from pathlib import Path
import json
import hashlib
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools"))
from gffdump import gff
from k1_spawn_menu_catalog import list_ids

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'platforms/out/unnamed-melee-npcs'
rules=json.loads(Path(__file__).with_name('k1-npc-combat-rules.json').read_text())
report=json.loads((OUT/'weapon-menu-report.json').read_text())
catalog=json.loads((OUT/'catalog-report.json').read_text())
sources={s['spawn_resource']:s for g in report['groups'] for v in g['variants'] for s in v['sources']}
roster={s['spawn_resource']:s for s in json.loads((OUT/'spawn-menu-roster.json').read_text())}
policies={}
for alias,s in roster.items():
    hp=s['hp']
    tier=next(t for t in rules['hp_tiers'] if hp>=t['min_hp'] and (t['max_hp'] is None or hp<=t['max_hp']))
    policies[alias]={'poise':tier['poise'],'parry':tier['parry'],'stance':'Yellow'}
for g in report['groups']:
    for v in g['variants']:
        for s in v['sources']:
            policies[s['spawn_resource']]['stance']=rules['faction_stances'].get(g['faction'],'Yellow')
targets=0
for rule in rules['overrides']:
    matches=[g for g in report['groups'] if g['foundation']==rule['npc'] and ' + '.join(g['equipped_weapons']).lower()==rule['weapon'].lower()]
    assert len(matches)==1,rule
    g=matches[0]
    variants=rule['variants'] or list(range(1,len(g['variants'])+1))
    for number in variants:
        assert 1<=number<=len(g['variants']),rule
        targets+=1
        for s in g['variants'][number-1]['sources']:
            p=policies[s['spawn_resource']]
            for key in ('parry','stance'):
                if rule[key] is not None:p[key]=rule[key]
rows=['2DA V2.0','','alias module source tag hp level poise parry stance gender cls right left']
story={}
legacy={}
for c in catalog['coverage']:
    alias=c['template'];s=roster[alias];p=policies[alias]
    source=c['source'][:-4]
    module=c['module'][:-2] if c['module'].endswith('_s') else c['module']
    identity=(module.lower(),source.lower(),c['tag'].lower(),s['hp'],s['level'])
    assert identity not in story or story[identity]==p,('Ambiguous story identity',identity,p,story.get(identity))
    story[identity]=p
    raw=(OUT/'assets'/(alias+'.utc')).read_bytes();native=gff(raw)
    equipped={slot:gff((OUT/'assets'/(item['EquippedRes']+'.uti')).read_bytes())['BaseItem'] for slot,item in zip(list_ids(raw,'Equip_ItemList'),native.get('Equip_ItemList',[]))}
    gender=native.get('Gender',0);cls=native['ClassList'][0]['Class'];right=equipped.get(16,-1);left=equipped.get(32,-1)
    legacy_key=(module.lower(),c['tag'].lower(),s['hp'],s['level'],gender,cls,right,left)
    assert legacy_key not in legacy or legacy[legacy_key]==p,('Ambiguous old-save policy',legacy_key)
    legacy[legacy_key]=p
    rows.append(f'{len(rows)-3} {alias} {module} {source} "{c["tag"]}" {s["hp"]} {s["level"]} {p["poise"]} {p["parry"]} '+str({'Yellow':0,'Red':1,'Blue':2}[p['stance']])+f' {gender} {cls} {right} {left}')
destination=OUT/'assets/k1npccombat.2da'
subprocess.run(['git','-C',str(ROOT),'check-ignore','--quiet',str(destination)],check=True)
destination.write_text('\n'.join(rows)+'\n',encoding='ascii')
catalog['files'][destination.name]=hashlib.sha256(destination.read_bytes()).hexdigest()
(OUT/'catalog-report.json').write_text(json.dumps(catalog,indent=2))
(OUT/'combat-policy-report.json').write_text(json.dumps({'resolved_override_variants':targets,'policies':policies,'story_identities':len(story)},indent=2))
print('NPC_COMBAT_POLICY_PASS',len(policies),'sources',targets,'override variants',len(story),'story identities')
