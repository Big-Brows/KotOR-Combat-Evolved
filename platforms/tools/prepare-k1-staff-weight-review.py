"""Isolated waist/chest skin-weight trial on the unchanged stock-JKA poses."""
from pathlib import Path
import copy
import hashlib
import json
import os
import shutil

import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree

repo = Path(__file__).resolve().parents[2]
root = repo/'platforms/out/k1-engage-prototype/staff-swing-review'
dest = root/os.environ.get('K1_STAFF_WEIGHT_ATTEMPT','weights-02')
assert not dest.exists(), 'Preserve prior trials.'
dest.mkdir()
shutil.copy2(__file__,dest/'proposal-recipe.py')
os.environ['K1_STAFF_REVIEW_ATTEMPT']='stock-reset-01'
diagnostic = root/'check-connected.py'
ns = {'__file__':str(diagnostic)}
exec(compile(diagnostic.read_text().split('report={};events={}')[0],str(diagnostic),'exec'),ns)
nn,rest,meshes = ns['nn'],ns['rest'],ns['meshes']
skin,worlds,candidate,displayed = (ns[n] for n in ('skin','worlds','candidate','displayed'))
lookup={n.name:n for n in nn}
spine=('pelvis_g','torso_g','torsoMid_g','torsoUpr_g')
heights=[rest[b].translation.z for b in spine]
assert all(a<b for a,b in zip(heights,heights[1:]))
original={n.name:copy.deepcopy(n.weights) for n in nn if n.nodetype=='SKIN'}
trial=copy.deepcopy(original)
# Discover seam indices on this actual, partitioned mesh. Historical unsplit
# Torso indices are not valid for the current Torso/TorsoLegs pair.
points=[(name,i,rest[name]@Vector(v)) for name in original for i,v in enumerate(lookup[name].verts)]
tree=KDTree(len(points))
for k,(_,_,p) in enumerate(points):tree.insert(p,k)
tree.balance()
seams=[]
for k,(name,i,p) in enumerate(points):
    for _,j,_ in tree.find_range(p,1e-5):
        other,index,_=points[j]
        if j>k and name!=other:seams.append(dict(a=[name,i],b=[other,index]))
seam_arm={(mesh,i) for p in seams for mesh,i in (p['a'],p['b']) if mesh in ('RArm','LArm')}
changed=[]
for name,rows in original.items():
    for i,row in enumerate(rows):
        if name not in ('Torso','TorsoLegs') and (name,i) not in seam_arm:
            continue
        participating={b for b,w in row if b in spine and w>1e-8}
        # Preserve rigid pelvis/chest attachments, including backpack and belt.
        if not participating or participating <= {'pelvis_g'} or participating <= {'torsoUpr_g'}:
            continue
        fixed=[(b,w) for b,w in row if b not in spine]
        mass=sum(w for b,w in row if b in spine)
        z=(rest[name]@Vector(lookup[name].verts[i])).z
        if z<=heights[0]:
            field=[(spine[0],1.)]
        elif z>=heights[-1]:
            field=[(spine[-1],1.)]
        else:
            j=next(j for j in range(3) if z<=heights[j+1])
            u=(z-heights[j])/(heights[j+1]-heights[j])
            u=u*u*(3.-2.*u)
            field=[(spine[j],1.-u),(spine[j+1],u)]
        field=sorted(((b,w) for b,w in field if w>1e-8),key=lambda p:-p[1])[:4-len(fixed)]
        total=sum(w for _,w in field)
        assert total>0
        new=fixed+[(b,mass*w/total) for b,w in field]
        assert abs(sum(w for _,w in new)-sum(w for _,w in row))<1e-7
        assert dict(fixed)=={b:w for b,w in new if b not in spine}
        trial[name][i]=new
        if dict(row)!=dict(new):changed.append((name,i))

# Close actual native arm/torso seams using identical weights on both sides.
for pair in seams:
    a,i=pair['a'];b,j=pair['b']
    if dict(trial[a][i])!=dict(trial[b][j]):
        # The same field must produce the same distribution at a shared point.
        assert (rest[a]@Vector(lookup[a].verts[i])-rest[b]@Vector(lookup[b].verts[j])).length<1e-5
        olda,oldb=dict(original[a][i]),dict(original[b][j])
        assert max(abs(olda.get(k,0)-oldb.get(k,0)) for k in olda.keys()|oldb.keys())<1e-5
        trial[a][i]=copy.deepcopy(trial[b][j])
        if (a,i) not in changed:changed.append((a,i))

palettes={name:len({b for row in rows for b,w in row if w>1e-8}) for name,rows in trial.items()}
assert max(palettes.values())<=16,palettes
assert all(len(row)<=4 and abs(sum(w for _,w in row)-1)<1e-5 for rows in trial.values() for row in rows)

def install(weights):
    for name,rows in weights.items():
        m=meshes[name]
        bones=sorted({b for row in rows for b,w in row if w>0})
        ids=np.zeros((len(rows),4),dtype=int);values=np.zeros((len(rows),4))
        for i,row in enumerate(rows):
            for j,(b,w) in enumerate(row):ids[i,j]=bones.index(b);values[i,j]=w
        m.update(bones=bones,ids=ids,weights=values)

report={}
for after,weights in ((False,original),(True,trial)):
    install(weights)
    for clip,d in ns['proposals'].items():
        for walking in (False,True):
            label=clip+('/walk' if walking else '/stand')
            edge_max=0.;stretch_energy=0.;area_min=1e9;seam_error=0.
            for k in range(round(d['length']*60)+1):
                t=k/60;w=worlds(displayed(candidate(clip,t),t,walking))
                points={name:skin(w,name) for name in meshes}
                for name in ('Torso','TorsoLegs'):
                    p=points[name];m=meshes[name];e=m['edges'];f=m['faces']
                    assert np.all(np.isfinite(p))
                    extension=np.linalg.norm(p[e[:,1]]-p[e[:,0]],axis=1)-m['edge_rest']
                    edge_max=max(edge_max,float(max(extension)))
                    stretch_energy+=float(np.sum(np.maximum(extension,0)**2))
                    area=np.linalg.norm(np.cross(p[f[:,1]]-p[f[:,0]],p[f[:,2]]-p[f[:,0]]),axis=1)
                    area_min=min(area_min,float(min(area[m['valid_faces']]/m['area_rest'][m['valid_faces']])))
                for pair in seams:
                    a,i=pair['a'];b,j=pair['b']
                    seam_error=max(seam_error,float(np.linalg.norm(points[a][i]-points[b][j])))
            report.setdefault(label,{})['trial' if after else 'original']=dict(max_extra_edge_m=edge_max,positive_edge_energy=stretch_energy,min_face_area_ratio=area_min,seam_error_m=seam_error)
            print('WEIGHT_SKIN_MEASURE',label,after,report[label]['trial' if after else 'original'],flush=True)

baseline=root/'stock-reset-01'
for name in ('poses.json','generation.json'):
    shutil.copy2(baseline/name,dest/name)
generation=json.loads((dest/'generation.json').read_text())
generation.update(comparison_baseline='stock-reset-01',skin_weight_trial='skin-weights.json',scope='weights only; unchanged stock-JKA poses; preview only')
payload=dict(weights=trial,original_weights=original,bind_points={n.name:[list(rest[n.name]@Vector(v)) for v in n.verts] for n in nn if n.nodetype=='SKIN'})
(dest/'skin-weights.json').write_text(json.dumps(payload))
(dest/'generation.json').write_text(json.dumps(generation,indent=2))
(dest/'weight-checks.json').write_text(json.dumps(dict(changed_vertices=changed,palettes=palettes,seams=seams,joint_heights=heights,poses_exact=hashlib.sha256((dest/'poses.json').read_bytes()).hexdigest()==hashlib.sha256((baseline/'poses.json').read_bytes()).hexdigest(),measurements=report),indent=2))
print('WEIGHT_TRIAL_READY',len(changed),palettes,flush=True)
