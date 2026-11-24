"""Repair action dropdown bindings in NEW sword editor copies; preserve all keys.

Run with installed Blender background. Never overwrites delivered editing files.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import bpy

repo=Path(__file__).resolve().parents[2]
source=repo/'platforms/out/sword-stance-edit-pack'
out=source/'slot-fixed'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'report.json')],check=True)
out.mkdir(exist_ok=True)
assert not list(out.glob('*.blend')),'Preserve existing repaired editor files.'

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def key_signature():
    return {a.name:[(c.data_path,c.array_index,[(tuple(k.co),k.interpolation,tuple(k.handle_left),tuple(k.handle_right)) for k in c.keyframe_points])
        for l in a.layers for s in l.strips for bag in s.channelbags for c in bag.fcurves] for a in bpy.data.actions}
results=[]
for path in sorted(source.glob('*_sword_animations.blend')):
    original_hash=digest(path)
    bpy.ops.wm.open_mainfile(filepath=str(path))
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    before=key_signature()
    initial_action=rig.animation_data.action
    for a in bpy.data.actions:
        for slot in a.slots:slot.identifier='OB'+rig.name
    rig.animation_data.action_slot=initial_action.slots[0]
    dest=out/path.name
    bpy.ops.wm.save_as_mainfile(filepath=str(dest))
    bpy.ops.wm.open_mainfile(filepath=str(dest))
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    assert before==key_signature(),(path.name,'keys changed')
    actions=list(bpy.data.actions)
    selected=0
    for a in actions+list(reversed(actions)):
        # Deliberately assign only the action, as the dropdown does. No explicit
        # slot assignment: the previous exporter test hid the real UI failure.
        rig.animation_data.action=a
        assert rig.animation_data.action_slot==a.slots[0],(path.name,a.name,'slot missing')
        bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
        start={b.name:b.matrix.copy() for b in rig.pose.bones}
        last=float(a.get('last_frame',a.frame_range[1]))
        mid=1+(last-1)*.5
        bpy.context.scene.frame_set(int(mid),subframe=mid-int(mid));bpy.context.view_layer.update()
        if a.name.startswith('Attack ') or a.name.startswith('02 Guard walking'):
            delta=max(abs(b.matrix[i][j]-start[b.name][i][j]) for b in rig.pose.bones for i in range(4) for j in range(4))
            assert delta>1e-5,(path.name,a.name,'pose did not animate')
        selected+=1
    assert digest(path)==original_hash,'Source editor changed'
    results.append({'file':path.name,'source_sha256':original_hash,'keys_unchanged':True,'automatic_slot_switches':selected})
    print('ACTION_SLOT_REPAIR_PASS',path.name,selected,flush=True)
assert len(results)==6
(out/'report.json').write_text(json.dumps({'files':results,'source_files_unchanged':True},indent=2))
print('SWORD_ACTION_SLOTS_PASS',sum(r['automatic_slot_switches'] for r in results),flush=True)
