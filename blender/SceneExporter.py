#MIT License
#
# Copyright (c) 2017 Daniel Suttor
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in all
#copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#SOFTWARE.


bl_info = {
    "name": "Scene exporter",
    "author": "Daniel Suttor",
    "location": "File->Import-Export",
    "category": "Import-Export",
    }

import bpy
import bmesh
import struct
import mathutils
import sys
import subprocess
import os
from collections import namedtuple
from array import array
from collections import defaultdict
from enum import IntEnum

class ComponentType(IntEnum):
    transform = 0
    mesh = 1
    boundingBox = 2
    camera = 3

class SceneExporter(bpy.types.Operator):
    """Export Scene"""       # tooltip for menu items and buttons
    bl_idname = "export_scene.scene"         # unique identifier for buttons, menu items to reference
    bl_label = "Export the current scene"          # display name in interface
    bl_options = {'REGISTER', 'PRESET'}

    #changes the coordinate from to left handed for rotation
    def ChangeRotation(originalRot):
        eulerAngles = originalRot.to_euler('XYZ')
        rot = mathutils.Euler((-eulerAngles.x, -eulerAngles.y, eulerAngles.z))
        return rot.to_quaternion()

    def CreateDataStrings(loc, rot, scale):
        location = '\t\tpos %f %f %f \n' %(loc.x, -loc.z, loc.y)
        rotation = '\t\trotation %f %f %f %f \n' %(rot.x, rot.z, rot.y, -rot.w)
        scaling = '\t\tscaling %f %f %f' %(scale.x, scale.z, scale.y)
        return location, rotation, scaling

    def MinVector(min, input):
        for i in range(0,3):
            if input[i] < min[i]:
                min[i] = input[i]

    def MaxVector(max, input):
        for i in range(0,3):
            if input[i] > max[i]:
                max[i] = input[i]

    def ExportTransformComponent(matrix, file):
        loc, originalRot, scale = matrix.decompose()
        rot = SceneExporter.ChangeRotation(originalRot)
        locS, rotS, scaleS = SceneExporter.CreateDataStrings(loc, rot, scale)

        file.write( '\ttransform\n'
            + locS + rotS + scaleS + '\n')

    def ExportMeshComponent(path, object, file):
        file.write( '\tpath\t' + path + object.data.name + '.ply\n')
        file.write('\tmaterialName ' + object.material_slots[0].name + '\n')
        
        #triangulate mesh first
        tempMesh = bmesh.new()
        tempMesh.from_mesh(object.data)

        bmesh.ops.triangulate(tempMesh, faces=tempMesh.faces[:], quad_method=0, ngon_method=0)

        tempMesh.to_mesh(object.data)
        tempMesh.free()

        #save without transform
        transform = object.matrix_world.copy()
        object.matrix_world = mathutils.Matrix.Identity(4)

        bpy.ops.export_mesh.ply(filepath=path+object.data.name+'.ply',
            axis_forward="Z", axis_up="-Y")

        #reset transform
        object.matrix_world = transform

    def ExportBoundingBoxComponent(object, file):
        maxFloat = sys.float_info.max
        minV = mathutils.Vector((maxFloat, maxFloat, maxFloat))
        maxV = mathutils.Vector((-maxFloat, -maxFloat, -maxFloat))

        for corner in object.bound_box:
            cornerValue = mathutils.Vector((corner[0], -corner[2], corner[1]))
            SceneExporter.MinVector(minV, cornerValue)
            SceneExporter.MaxVector(maxV, cornerValue)

        file.write( '\tboundingBox\n\t\tmin %f %f %f\n\t\tmax %f %f %f\n' %(
            minV.x, minV.y, minV.z,
            maxV.x, maxV.y, maxV.z))

    def ExportCameraComponent(file):
        file.write('%d \n' %(ComponentType.camera))

    def GetTextureImage(object):
        if len(object.material_slots) > 0:
            material = object.material_slots[0].material
            if len(material.texture_slots) > 0:
                texture = material.texture_slots[0].texture
                if hasattr(texture, 'image'):
                    return material.texture_slots[0].texture.image

    def ExportParticleComponent(object, file):
        settings = object.particle_systems[0].settings
        file.write('\tcount %d\n\tlifetime %f\n' %(settings.count, settings.lifetime))

        image = SceneExporter.GetTextureImage(object)
        if hasattr(image, 'filepath'):
            file.write('\ttextureFile ' + bpy.path.abspath(image.filepath) + '\n')

    def ExportMaterials(file):
      for mat in bpy.data.materials:
        file.write('material\n\tname ' + mat.name + '\n')
        texture = mat.active_texture
        if hasattr(texture, 'image'):
          file.write('\tbaseColorTexture ' + texture.image.filepath)
        else:
          file.write('\tbaseColorTexture NONE')
        file.write('\n\troughness %f\n\tmetalMask %d\n' %(0.5, 0))
    
    #exports the instance data of the objects
    def exportScene(scene, path, name):
        scenePath = str(path + name)
        file = open(scenePath, 'w')
        
        relativePath = os.path.relpath(path)
        for obj in scene.objects:
            if not obj.hide:
                if obj.type == 'MESH':
                    obj.select = True
                    scene.objects.active = obj
                    if len(obj.particle_systems) > 0:
                        file.write('particles\n')
                        SceneExporter.ExportParticleComponent(obj, file)
                        SceneExporter.ExportTransformComponent(obj.matrix_world, file)
                        SceneExporter.ExportBoundingBoxComponent(obj, file)
                    else:
                        file.write('mesh\n')
                        SceneExporter.ExportMeshComponent(path, obj, file)
                        SceneExporter.ExportTransformComponent(obj.matrix_world, file)
                        SceneExporter.ExportBoundingBoxComponent(obj, file)

        SceneExporter.ExportMaterials(file)
        
        file.close()

    def GetOutputName():
      blendName = bpy.path.basename(bpy.data.filepath)
      return blendName.split('.',1)[0] + ".scene"
    
    def execute(self, context):
        #location of the blend file
        path = bpy.path.abspath("//")
        if(bpy.data.is_saved == False):
            return {'FINISHED'}

        scene = context.scene
        sceneName = SceneExporter.GetOutputName()

        #export all instances of the mesh data
        SceneExporter.exportScene(scene, path, sceneName)

        return {'FINISHED'}             # operator finished successfully

def menu_func(self, context):
    self.layout.operator(SceneExporter.bl_idname, text="Scene exporter(.scene)")

def register():
    bpy.utils.register_module(__name__)
    bpy.types.INFO_MT_file_export.append(menu_func)

def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.INFO_MT_file_export.remove(menu_func)

# for testing running directly out of editor
if __name__ == "__main__":
    register()
bl_info = {
    "name": "Scene exporter",
    "author": "Daniel Suttor",
    "location": "File->Import-Export",
    "category": "Import-Export",
    }

import bpy
import bmesh
import struct
import mathutils
import sys
import subprocess
import os
from collections import namedtuple
from array import array
from collections import defaultdict
from enum import IntEnum

class ComponentType(IntEnum):
    transform = 0
    mesh = 1
    boundingBox = 2
    camera = 3

class SceneExporter(bpy.types.Operator):
    """Export Scene"""       # tooltip for menu items and buttons
    bl_idname = "export_scene.scene"         # unique identifier for buttons, menu items to reference
    bl_label = "Export the current scene"          # display name in interface
    bl_options = {'REGISTER', 'PRESET'}

    #changes the coordinate from to left handed for rotation
    def ChangeRotation(originalRot):
        eulerAngles = originalRot.to_euler('XYZ')
        rot = mathutils.Euler((-eulerAngles.x, -eulerAngles.y, eulerAngles.z))
        return rot.to_quaternion()

    def CreateDataStrings(loc, rot, scale):
        location = '\t\tpos %f %f %f \n' %(loc.x, -loc.z, loc.y)
        rotation = '\t\trotation %f %f %f %f \n' %(rot.x, rot.z, rot.y, -rot.w)
        scaling = '\t\tscaling %f %f %f' %(scale.x, scale.z, scale.y)
        return location, rotation, scaling

    def MinVector(min, input):
        for i in range(0,3):
            if input[i] < min[i]:
                min[i] = input[i]

    def MaxVector(max, input):
        for i in range(0,3):
            if input[i] > max[i]:
                max[i] = input[i]

    def ExportTransformComponent(matrix, file):
        loc, originalRot, scale = matrix.decompose()
        rot = SceneExporter.ChangeRotation(originalRot)
        locS, rotS, scaleS = SceneExporter.CreateDataStrings(loc, rot, scale)

        file.write( '\ttransform\n'
            + locS + rotS + scaleS + '\n')

    def ExportMeshComponent(path, object, file):
        file.write( '\tpath\t' + object.data.name + '.ply\n')
        file.write('\tmaterialName ' + object.material_slots[0].name + '\n')
        
        #triangulate mesh first
        tempMesh = bmesh.new()
        tempMesh.from_mesh(object.data)

        bmesh.ops.triangulate(tempMesh, faces=tempMesh.faces[:], quad_method=0, ngon_method=0)

        tempMesh.to_mesh(object.data)
        tempMesh.free()

        #save without transform
        transform = object.matrix_world.copy()
        object.matrix_world = mathutils.Matrix.Identity(4)

        bpy.ops.export_mesh.ply(filepath=path+object.data.name+'.ply',
            axis_forward="Z", axis_up="-Y")

        #reset transform
        object.matrix_world = transform

    def ExportBoundingBoxComponent(object, file):
        maxFloat = sys.float_info.max
        minV = mathutils.Vector((maxFloat, maxFloat, maxFloat))
        maxV = mathutils.Vector((-maxFloat, -maxFloat, -maxFloat))

        for corner in object.bound_box:
            cornerValue = mathutils.Vector((corner[0], -corner[2], corner[1]))
            SceneExporter.MinVector(minV, cornerValue)
            SceneExporter.MaxVector(maxV, cornerValue)

        file.write( '\tboundingBox\n\t\tmin %f %f %f\n\t\tmax %f %f %f\n' %(
            minV.x, minV.y, minV.z,
            maxV.x, maxV.y, maxV.z))

    def ExportCameraComponent(file):
        file.write('%d \n' %(ComponentType.camera))

    def GetTextureImage(object):
        if len(object.material_slots) > 0:
            material = object.material_slots[0].material
            if len(material.texture_slots) > 0:
                texture = material.texture_slots[0].texture
                if hasattr(texture, 'image'):
                    return material.texture_slots[0].texture.image

    def ExportParticleComponent(object, file):
        settings = object.particle_systems[0].settings
        file.write('\tcount %d\n\tlifetime %f\n' %(settings.count, settings.lifetime))

        image = SceneExporter.GetTextureImage(object)
        if hasattr(image, 'filepath'):
            file.write('\ttextureFile ' + bpy.path.abspath(image.filepath) + '\n')

    def ExportMaterials(file):
      for mat in bpy.data.materials:
        file.write('material\n\tname ' + mat.name + '\n')
        texture = mat.active_texture
        if hasattr(texture, 'image'):
          filepath = texture.image.filepath
          if texture.image.filepath[0] is '/':
            filepath = texture.image.filepath[2:]
          file.write('\tbaseColorTexture ' + filepath)
        else:
          file.write('\tbaseColorTexture NONE')
        file.write('\n\troughness %f\n\tmetalMask %d\n' %(0.5, 0))
    
    #exports the instance data of the objects
    def exportScene(scene, path, name):
        scenePath = str(path + name)
        file = open(scenePath, 'w')
        
        for obj in scene.objects:
            if not obj.hide:
                if obj.type == 'MESH':
                    obj.select = True
                    scene.objects.active = obj
                    if len(obj.particle_systems) > 0:
                        file.write('particles\n')
                        SceneExporter.ExportParticleComponent(obj, file)
                        SceneExporter.ExportTransformComponent(obj.matrix_world, file)
                        SceneExporter.ExportBoundingBoxComponent(obj, file)
                    else:
                        file.write('mesh\n')
                        SceneExporter.ExportMeshComponent(path, obj, file)
                        SceneExporter.ExportTransformComponent(obj.matrix_world, file)
                        SceneExporter.ExportBoundingBoxComponent(obj, file)

        SceneExporter.ExportMaterials(file)
        
        file.close()

    def GetOutputName():
      blendName = bpy.path.basename(bpy.data.filepath)
      return blendName.split('.',1)[0] + ".scene"
    
    def execute(self, context):
        #location of the blend file
        path = bpy.path.abspath("//")
        if(bpy.data.is_saved == False):
            return {'FINISHED'}

        scene = context.scene
        sceneName = SceneExporter.GetOutputName()

        #export all instances of the mesh data
        SceneExporter.exportScene(scene, path, sceneName)

        return {'FINISHED'}             # operator finished successfully

def menu_func(self, context):
    self.layout.operator(SceneExporter.bl_idname, text="Scene exporter(.scene)")

def register():
    bpy.utils.register_module(__name__)
    bpy.types.INFO_MT_file_export.append(menu_func)

def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.INFO_MT_file_export.remove(menu_func)

# for testing running directly out of editor
if __name__ == "__main__":
    register()
bl_info = {
    "name": "Scene exporter",
    "author": "Daniel Suttor",
    "location": "File->Import-Export",
    "category": "Import-Export",
    }

import bpy
import bmesh
import struct
import mathutils
import sys
import subprocess
import os
from collections import namedtuple
from array import array
from collections import defaultdict
from enum import IntEnum

class ComponentType(IntEnum):
    transform = 0
    mesh = 1
    boundingBox = 2
    camera = 3

class SceneExporter(bpy.types.Operator):
    """Export Scene"""       # tooltip for menu items and buttons
    bl_idname = "export_scene.scene"         # unique identifier for buttons, menu items to reference
    bl_label = "Export the current scene"          # display name in interface
    bl_options = {'REGISTER', 'PRESET'}

    #changes the coordinate from to left handed for rotation
    def ChangeRotation(originalRot):
        eulerAngles = originalRot.to_euler('XYZ')
        rot = mathutils.Euler((-eulerAngles.x, -eulerAngles.y, eulerAngles.z))
        return rot.to_quaternion()

    def CreateDataStrings(loc, rot, scale):
        location = '\t\tpos %f %f %f \n' %(loc.x, -loc.z, loc.y)
        rotation = '\t\trotation %f %f %f %f \n' %(rot.x, rot.z, rot.y, -rot.w)
        scaling = '\t\tscaling %f %f %f' %(scale.x, scale.z, scale.y)
        return location, rotation, scaling

    def MinVector(min, input):
        for i in range(0,3):
            if input[i] < min[i]:
                min[i] = input[i]

    def MaxVector(max, input):
        for i in range(0,3):
            if input[i] > max[i]:
                max[i] = input[i]

    def ExportTransformComponent(matrix, file):
        loc, originalRot, scale = matrix.decompose()
        rot = SceneExporter.ChangeRotation(originalRot)
        locS, rotS, scaleS = SceneExporter.CreateDataStrings(loc, rot, scale)

        file.write( '\ttransform\n'
            + locS + rotS + scaleS + '\n')

    def ExportMeshComponent(path, object, file):
        file.write( '\tpath\t' + object.data.name + '.ply\n')
        file.write('\tmaterialName ' + object.material_slots[0].name + '\n')
        
        #triangulate mesh first
        tempMesh = bmesh.new()
        tempMesh.from_mesh(object.data)

        bmesh.ops.triangulate(tempMesh, faces=tempMesh.faces[:], quad_method=0, ngon_method=0)

        tempMesh.to_mesh(object.data)
        tempMesh.free()

        #save without transform
        transform = object.matrix_world.copy()
        object.matrix_world = mathutils.Matrix.Identity(4)

        bpy.ops.export_mesh.ply(filepath=path+object.data.name+'.ply',
            axis_forward="Z", axis_up="-Y")

        #reset transform
        object.matrix_world = transform

    def ExportBoundingBoxComponent(object, file):
        maxFloat = sys.float_info.max
        minV = mathutils.Vector((maxFloat, maxFloat, maxFloat))
        maxV = mathutils.Vector((-maxFloat, -maxFloat, -maxFloat))

        for corner in object.bound_box:
            cornerValue = mathutils.Vector((corner[0], -corner[2], corner[1]))
            SceneExporter.MinVector(minV, cornerValue)
            SceneExporter.MaxVector(maxV, cornerValue)

        file.write( '\tboundingBox\n\t\tmin %f %f %f\n\t\tmax %f %f %f\n' %(
            minV.x, minV.y, minV.z,
            maxV.x, maxV.y, maxV.z))

    def ExportCameraComponent(file):
        file.write('%d \n' %(ComponentType.camera))

    def GetTextureImage(object):
        if len(object.material_slots) > 0:
            material = object.material_slots[0].material
            if len(material.texture_slots) > 0:
                texture = material.texture_slots[0].texture
                if hasattr(texture, 'image'):
                    return material.texture_slots[0].texture.image

    def ExportParticleComponent(object, file):
        settings = object.particle_systems[0].settings
        file.write('\tcount %d\n\tlifetime %f\n' %(settings.count, settings.lifetime))

        image = SceneExporter.GetTextureImage(object)
        if hasattr(image, 'filepath'):
            file.write('\ttextureFile ' + bpy.path.abspath(image.filepath) + '\n')

    def ExportMaterials(file):
      for mat in bpy.data.materials:
        file.write('material\n\tname ' + mat.name + '\n')
        texture = mat.active_texture
        if hasattr(texture, 'image'):
          filepath = texture.image.filepath
          if texture.image.filepath[0] is '/':
            filepath = texture.image.filepath[2:]
          file.write('\tbaseColorTexture ' + filepath)
        else:
          file.write('\tbaseColorTexture NONE')
        file.write('\n\troughness %f\n\tmetalMask %d\n' %(0.5, 0))
        
    def ExportSmokeComponent(object, file):
      smoke = object.modifiers['Smoke']
      file.write('\tdensity %f\n' %(smoke.domain_settings.alpha))
      
    #exports the instance data of the objects
    def exportScene(scene, path, name):
        scenePath = str(path + name)
        file = open(scenePath, 'w')
        
        for obj in scene.objects:
          if not obj.hide:
            if obj.type == 'MESH':
              obj.select = True
              scene.objects.active = obj
              if len(obj.particle_systems) > 0:
                file.write('particles\n')
                SceneExporter.ExportParticleComponent(obj, file)
              elif 'Smoke' in obj.modifiers:
                file.write('smoke\n')
                SceneExporter.ExportSmokeComponent(obj, file)
              else:
                file.write('mesh\n')
                SceneExporter.ExportMeshComponent(path, obj, file)
              SceneExporter.ExportTransformComponent(obj.matrix_world, file)
              SceneExporter.ExportBoundingBoxComponent(obj, file)  

        SceneExporter.ExportMaterials(file)
        
        file.close()

    def GetOutputName():
      blendName = bpy.path.basename(bpy.data.filepath)
      return blendName.split('.',1)[0] + ".scene"
    
    def execute(self, context):
        #location of the blend file
        path = bpy.path.abspath("//")
        if(bpy.data.is_saved == False):
            return {'FINISHED'}

        scene = context.scene
        sceneName = SceneExporter.GetOutputName()

        #export all instances of the mesh data
        SceneExporter.exportScene(scene, path, sceneName)

        return {'FINISHED'}             # operator finished successfully

def menu_func(self, context):
    self.layout.operator(SceneExporter.bl_idname, text="Scene exporter(.scene)")

def register():
    bpy.utils.register_module(__name__)
    bpy.types.INFO_MT_file_export.append(menu_func)

def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.INFO_MT_file_export.remove(menu_func)

# for testing running directly out of editor
if __name__ == "__main__":
    register()