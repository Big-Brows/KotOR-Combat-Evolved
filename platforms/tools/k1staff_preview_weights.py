"""Apply an isolated native weight trial to the existing Blender preview mesh."""
import json

import bpy
from mathutils import Vector
from mathutils.kdtree import KDTree


class PreviewSkinWeights:
    def __init__(self, path, loaded_trial=False):
        self.data=json.loads(path.read_text())
        self.mapping={}
        self.maximum_bind_error=0.
        self.maximum_old_weight_error=0.
        for name,points in self.data['bind_points'].items():
            obj=bpy.data.objects[name]
            tree=KDTree(len(points))
            for i,p in enumerate(points):tree.insert(Vector(p),i)
            tree.balance()
            mapping=[]
            for vertex in obj.data.vertices:
                p=obj.matrix_world@vertex.co
                groups={obj.vertex_groups[g.group].name:g.weight for g in vertex.groups if g.weight>1e-8}
                matches=[]
                for _,i,distance in tree.find_range(p,1e-5):
                    original=dict(self.data['weights' if loaded_trial else 'original_weights'][name][i])
                    error=max(abs(groups.get(b,0)-original.get(b,0)) for b in groups.keys()|original.keys())
                    matches.append((error,distance,i))
                assert matches,(name,vertex.index,'no native bind vertex')
                error,distance,i=min(matches)
                assert error<1e-5,(name,vertex.index,error)
                self.maximum_bind_error=max(self.maximum_bind_error,distance)
                self.maximum_old_weight_error=max(self.maximum_old_weight_error,error)
                mapping.append(i)
            self.mapping[name]=mapping

    def apply(self, trial):
        field='weights' if trial else 'original_weights'
        for name,mapping in self.mapping.items():
            obj=bpy.data.objects[name]
            for vertex,i in zip(obj.data.vertices,mapping):
                new=self.data[field][name][i]
                for group in list(vertex.groups):
                    obj.vertex_groups[group.group].remove([vertex.index])
                for bone,weight in new:
                    group=obj.vertex_groups.get(bone)
                    if group is None:group=obj.vertex_groups.new(name=bone)
                    group.add([vertex.index],weight,'REPLACE')
        bpy.context.view_layer.update()
