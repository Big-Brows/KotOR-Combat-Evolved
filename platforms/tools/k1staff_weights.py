"""Approved waist/chest redistribution, confined to the staff body's Torso."""
from mathutils import Vector


def apply_staff_torso_weights(model, bind_world):
    def nodes(n):return [n]+[v for c in n.children for v in nodes(c)]
    torso=next(n for n in nodes(model.root_node) if n.name=='Torso')
    spine=('pelvis_g','torso_g','torsoMid_g','torsoUpr_g')
    heights=[bind_world[n].translation.z for n in spine]
    assert all(a<b for a,b in zip(heights,heights[1:]))
    changed=[]
    for i,row in enumerate(torso.weights):
        participating={b for b,w in row if b in spine and w>1e-8}
        if not participating or participating<={'pelvis_g'} or participating<={'torsoUpr_g'}:
            continue
        fixed=[(b,w) for b,w in row if b not in spine]
        mass=sum(w for b,w in row if b in spine)
        z=(bind_world['Torso']@Vector(torso.verts[i])).z
        if z<=heights[0]:field=[(spine[0],1.)]
        elif z>=heights[-1]:field=[(spine[-1],1.)]
        else:
            j=next(j for j in range(3) if z<=heights[j+1])
            u=(z-heights[j])/(heights[j+1]-heights[j]);u=u*u*(3.-2.*u)
            field=[(spine[j],1.-u),(spine[j+1],u)]
        field=sorted(((b,w) for b,w in field if w>1e-8),key=lambda p:-p[1])[:4-len(fixed)]
        total=sum(w for _,w in field);assert total>0
        new=fixed+[(b,mass*w/total) for b,w in field]
        assert len(new)<=4 and abs(sum(w for _,w in new)-1)<1e-5
        assert dict(fixed)=={b:w for b,w in new if b not in spine}
        if dict(row)!=dict(new):changed.append(i)
        torso.weights[i]=new
    return dict(changed_torso_vertices=changed,policy='neighboring spine bind heights; preserve non-spine mass and rigid pelvis/chest attachments')
