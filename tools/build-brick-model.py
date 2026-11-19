#!/usr/bin/env python3
"""Build the website's TrimUI Brick in Blender, beside the Pixel 2.

blender --background --python tools/build-brick-model.py -- [--render]

Source dimensions: the published outline (73.2 wide x 109.9 tall mm; 11.8
deep above the back ledge, 19.9 below it) and the key places measured to it
from TrimUI's six-view product images, in
docs/superpowers/specs/2026-11-14-device-modular-design.md, appendix
"Measurements", the same numbers src/device/brick_model.c draws the
launcher's own Brick with. The images are reference only: nothing is traced
from them, and none of TrimUI's marks is on the model. This is a visual
reconstruction, not manufacturer CAD or a printable part.

Blender coordinates, as tools/build-pixel2-model.py's: x right, -y front,
z up, millimetres, the body's middle at the origin. glTF export converts to
y-up. The editable .blend and review renders go in build/brick-model/; the
deployment asset goes in site/models/brick.glb.

The poster is drawn at the Pixel 2 poster's scale (8.807 px a millimetre:
its 109 mm frame over 960 px), in the same pose, light and camera, so the
page can stand the two side by side, true to size.
"""
import math
import sys
import os
from pathlib import Path
import bpy
from mathutils import Vector, Matrix

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/brick-model'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 0.001
objects = []
# The studio light's strength; the area lights below draw the clean highlights
STUDIO = float(os.environ.get('STUDIO', '1.0'))

def linear(c):
    return c / 12.92 if c < .04045 else ((c + .055) / 1.055) ** 2.4

def material(name, color, metal=0, rough=.4):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = m.node_tree.nodes.get('Principled BSDF')
    rgb = tuple(linear(int(color[i:i+2],16)/255) for i in (0,2,4))
    p.inputs['Base Color'].default_value = (*rgb,1)
    p.inputs['Metallic'].default_value = metal
    p.inputs['Roughness'].default_value = rough
    m.diffuse_color = (*rgb,1)
    return m

# The purple Brick, as TrimUI's own product photo shows it: the shell
# #564f78, the keys in the same purple, F1 and F2 light grey (#cecece). Its
# shell is plastic, so no metal: a satin finish under a clear coat, which
# gives it its highlights.
def coated(m, weight=.6, rough=.12):
    p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Coat Weight'].default_value=weight
    p.inputs['Coat Roughness'].default_value=rough
    return m
shell = coated(material('Purple plastic shell', os.environ.get('BRICK_SHELL','372a4b'), 0, .36))
ledge = coated(material('Purple plastic, shaded', '31263f', 0, .4))
plastic = coated(material('Purple plastic keys', '342a48', 0, .3), .5)
pills = material('Light grey pills', 'cecece', 0, .38)
engraved = material('Key markings', '3b355a', 0, .45)
black = material('Port and socket interiors', '0b0c10', 0, .75)
silver = material('Steel port rims', 'b9bfc0', .9, .24)
glass = material('Black glass', '07080b', .05, .12)
lightbar = material('Frosted light bar', 'e9e6f2', 0, .35)

# Rounded rectangles in x/z, counterclockwise. Radii: one value or (upper, lower).
def outline(w,h,r,n=8):
    pts=[]
    top,bottom=r if isinstance(r,tuple) else (r,r)
    for cx,cz,start,radius in [(w/2-top,h/2-top,0,top),(-w/2+top,h/2-top,90,top),(-w/2+bottom,-h/2+bottom,180,bottom),(w/2-bottom,-h/2+bottom,270,bottom)]:
        for j in range(n+1):
            a=math.radians(start+j*90/n)
            pts.append((cx+radius*math.cos(a),cz+radius*math.sin(a)))
    return pts

def mesh(name, verts, faces, mat):
    me=bpy.data.meshes.new(name)
    me.from_pydata(verts,[],faces); me.update()
    ob=bpy.data.objects.new(name,me); scene.collection.objects.link(ob)
    if mat: me.materials.append(mat)
    objects.append(ob)
    return ob

def consistent(ob):
    bpy.context.view_layer.objects.active=ob; ob.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT'); ob.select_set(False)

def normals(ob):
    bpy.context.view_layer.objects.active=ob
    ob.select_set(True)
    mod=ob.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL')
    mod.keep_sharp=True; mod.weight=50
    bpy.ops.object.modifier_apply(modifier=mod.name)
    ob.select_set(False)

def profile(name,rings,mat,cx=0,cz=0):
    # Each ring is (depth y,width,height,corner radius).
    verts=[]
    for y,w,h,r in rings: verts.extend((x+cx,y,z+cz) for x,z in outline(w,h,r))
    n=len(verts)//len(rings)
    faces=[tuple(reversed(range(n))), tuple(range((len(rings)-1)*n,len(rings)*n))]
    for k in range(len(rings)-1):
        for j in range(n):
            a=k*n+j; b=k*n+(j+1)%n
            faces.append((a,b,b+n,a+n))
    ob=mesh(name,verts,faces,mat)
    consistent(ob)
    return ob

def rounded(name,w,h,depth,r,loc,mat,bevel=0):
    x,y,z=loc
    if bevel:
        b=min(bevel,depth/3,(min(r) if isinstance(r,tuple) else r)/2)
        inset=tuple(max(.05,radius-b) for radius in r) if isinstance(r,tuple) else max(.05,r-b)
        rings=[(-depth/2,w-2*b,h-2*b,inset),(-depth/2+b,w,h,r),(depth/2-b,w,h,r),(depth/2,w-2*b,h-2*b,inset)]
    else: rings=[(-depth/2,w,h,r),(depth/2,w,h,r)]
    return profile(name,[(yy+y,ww,hh,rr) for yy,ww,hh,rr in rings],mat,x,z)

def cut(ob,cutter):
    bpy.context.view_layer.objects.active=ob
    mod=ob.modifiers.new('Cut','BOOLEAN'); mod.operation='DIFFERENCE'; mod.solver='EXACT'; mod.object=cutter
    bpy.ops.object.modifier_apply(modifier=mod.name)
    objects.remove(cutter); bpy.data.objects.remove(cutter,do_unlink=True)

def joined(name,parts):
    # One cutter from many, so a row of holes is one boolean
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts: p.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    bpy.ops.object.join()
    for p in parts[1:]:
        if p in objects: objects.remove(p)
    parts[0].name=name
    parts[0].select_set(False)
    return parts[0]

def text(name,body,size,loc,mat,rot=(math.pi/2,0,0)):
    curve=bpy.data.curves.new(name,'FONT'); curve.body=body; curve.size=size
    curve.align_x='CENTER'; curve.align_y='CENTER'; curve.resolution_u=3
    ob=bpy.data.objects.new(name,curve); scene.collection.objects.link(ob)
    ob.location=loc; ob.rotation_euler=rot; curve.materials.append(mat)
    bpy.context.view_layer.objects.active=ob; ob.select_set(True)
    bpy.ops.object.convert(target='MESH'); ob.select_set(False); objects.append(ob)
    return ob

def cylinder(name,r,depth,loc,mat,axis='Y',bevel=.12):
    bpy.ops.mesh.primitive_cylinder_add(vertices=48,radius=r,depth=depth,location=loc)
    ob=bpy.context.object; ob.name=name
    if axis=='Y': ob.rotation_euler[0]=math.pi/2
    elif axis=='X': ob.rotation_euler[1]=math.pi/2
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    if mat: ob.data.materials.append(mat)
    objects.append(ob)
    if bevel:
        mod=ob.modifiers.new('Cap edge','BEVEL'); mod.width=bevel; mod.segments=3
        bpy.ops.object.modifier_apply(modifier=mod.name)
    for p in ob.data.polygons: p.use_smooth=len(p.vertices)==4
    ob.select_set(False)
    return ob

# The appendix measures from the front's top-left corner, x right, y down.
W,H=73.2,109.9
def fx(x): return x-W/2
def fz(y): return H/2-y
FRONT=-9.95          # the face
THIN_BACK=1.85       # the thin part's back: 11.8 deep
THICK_BACK=9.95      # the grip's back: 19.9 deep
LEDGE=fz(42.9)       # the top of the grip, where the shoulders stand

# One body, full depth, then the thin part's back cut away above the ledge.
# Front corners: top r 0.5, bottom r 4.1, as measured.
body=rounded('Plastic shell',W,H,THICK_BACK-FRONT,(.5,4.1),(0,(FRONT+THICK_BACK)/2,0),shell,.9)
step=rounded('Ledge cutter',W+4,H,20,.1,(0,THIN_BACK+10,LEDGE+H/2),None)
cut(body,step)
# The ledge's new edges, softened as a moulding's are
bpy.context.view_layer.objects.active=body; body.select_set(True)
mod=body.modifiers.new('Moulded edges','BEVEL'); mod.width=.35; mod.segments=3
mod.limit_method='ANGLE'; mod.angle_limit=math.radians(40)
bpy.ops.object.modifier_apply(modifier=mod.name); body.select_set(False)

# The glass: x 0.9 to 72.2, y 0.8 to 57.2, nearly the whole top of the face
gw,gh=72.2-.9,57.2-.8
gx,gz=fx((72.2+.9)/2),fz((57.2+.8)/2)
cut(body,rounded('Glass recess cutter',gw+.1,gh+.1,1.0,(.45,1.2),(gx,FRONT,gz),None))
rounded('Black glass',gw,gh,.5,(.4,1.15),(gx,FRONT+.3,gz),glass,.08)
# The active screen, 65.0 x 48.8 (3.2 in, 4:3), centred in the glass. A
# separate named mesh, so site/pixel2.js can draw the launcher on it.
display=material('Display','080b08',0,.5)
p=display.node_tree.nodes.get('Principled BSDF')
p.inputs['Emission Color'].default_value=(.01,.01,.01,1)
p.inputs['Emission Strength'].default_value=1
sw,sh=65.0,48.8; sy=FRONT+.04
screen=mesh('Screen',[(gx-sw/2,sy,gz-sh/2),(gx+sw/2,sy,gz-sh/2),(gx+sw/2,sy,gz+sh/2),(gx-sw/2,sy,gz+sh/2)],[(0,1,2,3)],display)
uv=screen.data.uv_layers.new(name='UVMap')
for i,co in enumerate([(0,0),(1,0),(1,1),(0,1)]): uv.data[i].uv=co

# The d-pad: centre (15.9, 72.3), span 19.3, arms 7.0 wide
dx,dz=fx(15.9),fz(72.3)
def crossmesh(name,half,arm,front,rear,mat,bevel=.35):
    pts=[(-arm,half),(arm,half),(arm,arm),(half,arm),(half,-arm),(arm,-arm),(arm,-half),(-arm,-half),(-arm,-arm),(-half,-arm),(-half,arm),(-arm,arm)]
    verts=[(dx+x,y,dz+z) for y in [front,rear] for x,z in pts]
    n=len(pts); faces=[tuple(range(n)),tuple(reversed(range(n,2*n)))]
    for i in range(n): faces.append((i,(i+1)%n,(i+1)%n+n,i+n))
    ob=mesh(name,verts,faces,mat)
    consistent(ob)
    if bevel:
        bpy.context.view_layer.objects.active=ob; ob.select_set(True)
        mod=ob.modifiers.new('Rounded cross','BEVEL'); mod.width=bevel; mod.segments=3
        bpy.ops.object.modifier_apply(modifier=mod.name); ob.select_set(False)
    return ob
cut(body,crossmesh('D-pad socket cutter',9.65+.45,3.5+.45,FRONT-1,FRONT+1.6,None))
crossmesh('D-pad socket',9.65+.43,3.5+.43,FRONT+1.2,FRONT+1.5,black,.2)
crossmesh('D-pad',9.65,3.5,FRONT-1.3,FRONT+1.3,plastic,.6)
for i in range(4):
    a=i*math.pi/2; pts=[(-1.2,5.6),(1.2,5.6),(0,7.4)]
    verts=[(dx+x*math.cos(a)-z*math.sin(a),FRONT-1.32,dz+x*math.sin(a)+z*math.cos(a)) for x,z in pts]
    mesh('D-pad arrow %d'%i,verts,[(0,1,2)],engraved)

# A B X Y: the diamond about (56.3, 72.1), each key 7.6 from it, 7.7 across
ax,az=fx(56.3),fz(72.1)
for label,ox,oz in [('A',7.6,0),('B',0,-7.6),('X',0,7.6),('Y',-7.6,0)]:
    x,z=ax+ox,az+oz
    cut(body,cylinder(label+' bore',3.85+.45,3,(x,FRONT,z),None,bevel=0))
    cylinder(label+' socket',3.85+.43,.3,(x,FRONT+1.4,z),black,bevel=.1)
    cylinder(label+' key',3.85,2.6,(x,FRONT-.05,z),plastic,bevel=.45)
    text(label+' marking',label,3.0,(x,FRONT-1.36,z),engraved)

# F1 and F2, pills 7.1 x 2.6 between the d-pad and the diamond
for name,x in [('F1',30.9),('F2',41.6)]:
    cut(body,rounded(name+' bore',7.1+.8,2.6+.8,3,1.7,(fx(x),FRONT,fz(63.6)),None))
    rounded(name+' socket',7.1+.75,2.6+.75,.3,1.65,(fx(x),FRONT+1.4,fz(63.6)),black)
    rounded(name,7.1,2.6,2.2,1.3,(fx(x),FRONT+.1,fz(63.6)),pills,.35)

# MENU, SELECT and START: round keys 5.4 across along the bottom
for name,x in [('MENU',26.1),('SELECT',36.1),('START',46.2)]:
    cut(body,cylinder(name+' bore',2.7+.4,3,(fx(x),FRONT,fz(101.4)),None,bevel=0))
    cylinder(name+' socket',2.7+.38,.3,(fx(x),FRONT+1.4,fz(101.4)),black,bevel=.1)
    cylinder(name+' key',2.7,2.2,(fx(x),FRONT+.15,fz(101.4)),plastic,bevel=.35)

# Speaker grilles at (11.1, 99.6) and (61.9, 99.6): two rows of holes, 7.4 wide
for gxm in [11.1,61.9]:
    holes=[]
    for row,dzr in [(0,.75),(1,-.75)]:
        count=6 if row==0 else 5
        for i in range(count):
            x=fx(gxm)-(count-1)*1.33/2+i*1.33
            holes.append(cylinder('Grille hole',.42,3,(x,FRONT,fz(99.6)+dzr),None,bevel=0))
    cut(body,joined('Grille cutter',holes))
    rounded('Grille floor',7.6,3.2,.1,.4,(fx(gxm),FRONT+1.2,fz(99.6)),black)

# Side keys stand on the thin part's sides: their middle 4.05 mm behind the
# body's, between the face and the thin part's back.
SIDE_Y=(FRONT+THIN_BACK)/2
def sidepiece(name,w,h,depth,r,side,y,z,mat,surface):
    ob=rounded(name,w,h,depth,r,(0,0,0),mat,min(.12,depth/3))
    ob.rotation_euler=(0,0,side*math.pi/2)
    ob.location=(side*surface,y,z)
    return ob
EDGE=W/2
def side_key(name,length,width,side,z,mat=plastic):
    cut(body,sidepiece(name+' bore',width+.6,length+.6,3,min(width,length)/2+.3,side,SIDE_Y,z,None,EDGE))
    sidepiece(name+' socket',width+.55,length+.55,.2,min(width,length)/2+.27,side,SIDE_Y,z,black,EDGE-1.2)
    sidepiece(name,width,length,1.8,min(width,length)/2,side,SIDE_Y,z,mat,EDGE-.1)
# Left: VOL+ and VOL-, each 7.7 long
side_key('VOL+',7.7,2.8,-1,fz(10.3))
side_key('VOL-',7.7,2.8,-1,fz(21.4))
# Right: the FN slider (10.3 long) in its slot, and POWER (7.1 across)
cut(body,sidepiece('FN slot cutter',3.0,10.3,3,1.4,1,SIDE_Y,fz(23.3),None,EDGE))
sidepiece('FN slot floor',3.0,10.3,.2,1.4,1,SIDE_Y,fz(23.3),black,EDGE-1.2)
sidepiece('FN slider',2.4,4.2,1.5,1.1,1,SIDE_Y,fz(23.3)+2.6,plastic,EDGE-.35)
side_key('POWER',7.1,3.0,1,fz(39.3))

# The shoulders stand on the ledge: L1 and R1 outside (about 17.5 wide),
# L2 and R2 inside (about 11), the USB-C housing (about 13.5) between.
LY=(THIN_BACK+THICK_BACK)/2; LD=THICK_BACK-THIN_BACK-.8
for prefix,side in [('L',-1),('R',1)]:
    rounded(prefix+'1',17.2,1.7,LD,.6,(side*(36.6-.25-17.2/2),LY,LEDGE+.75),plastic,.3)
    rounded(prefix+'2',10.6,1.9,LD-.6,.6,(side*12.3,LY,LEDGE+.85),plastic,.3)
rounded('USB-C housing',13.0,1.4,LD+.4,.5,(0,LY,LEDGE+.6),ledge,.3)

# The top edge's light bar, 37.9 wide, centred, towards the back
cut(body,rounded('Light bar cutter',38.2,2,1.3,.5,(0,THIN_BACK-1.1,H/2),None))
rounded('Light bar',37.9,.6,1.1,.45,(0,THIN_BACK-1.1,H/2-.35),lightbar)

# Bottom edge, left to right: microSD, a small hole, USB-C, a pinhole, the
# 3.5 mm jack, across the grip's depth
def bottompiece(name,w,h,depth,r,x,y,mat,z=-H/2):
    ob=rounded(name,w,h,depth,r,(0,0,0),mat,min(.08,depth/3))
    ob.rotation_euler=(math.pi/2,0,0); ob.location=(x,y,z)
    return ob
BY=(FRONT+THICK_BACK)/2
cut(body,bottompiece('MicroSD slot cutter',12,1.5,3,.6,-22.5,BY-2.5,None))
bottompiece('MicroSD slot interior',11.9,1.4,.1,.55,-22.5,BY-2.5,black,z=-H/2+1.2)
cut(body,bottompiece('USB-C cutter',8.9,3.2,3,1.5,0,BY,None))
usb=bottompiece('USB-C rim',9.4,3.7,.22,1.75,0,BY,silver)
cut(usb,bottompiece('USB-C rim hole',8.8,3.1,1,1.45,0,BY,None))
bottompiece('USB-C interior',8.75,3.05,.1,1.45,0,BY,black,z=-H/2+1.2)
bottompiece('USB-C tongue',6.4,.7,.2,.2,0,BY,plastic,z=-H/2+.5)
for x,r in [(-11.5,.6),(10.5,.4)]:
    cut(body,cylinder('Bottom hole',r,3,(x,BY,-H/2),None,'Z',0))
    cylinder('Bottom hole floor',r,.1,(x,BY,-H/2+1.3),black,'Z',0)
cut(body,cylinder('Jack cutout',1.8,3,(22.5,BY,-H/2),None,'Z',0))
jack=cylinder('Jack rim',2.3,.18,(22.5,BY,-H/2-.02),silver,'Z',.06)
cut(jack,cylinder('Jack rim hole',1.75,1,(22.5,BY,-H/2),None,'Z',0))
cylinder('Jack interior',1.75,.1,(22.5,BY,-H/2+1.3),black,'Z',0)

for ob in objects:
    if ob.type=='MESH': normals(ob)

root=bpy.data.objects.new('TrimUI Brick',None); scene.collection.objects.link(root)
for ob in objects: ob.parent=root
# The Pixel 2's studio, unchanged, so the two posters match
scene.render.engine='CYCLES'; scene.cycles.samples=32
scene.cycles.use_denoising=True
scene.render.image_settings.file_format='PNG'
scene.render.film_transparent=False
# A real studio around the product, for its reflections: Blender's own
# studio light (Poly Haven's studio_small_01, CC0), the same one the web page
# lights the 3D model with (site/models/studio.hdr)
scene.world.use_nodes=True
world_nodes=scene.world.node_tree.nodes; world_nodes.clear()
studio_image=world_nodes.new('ShaderNodeTexEnvironment')
studio_image.image=bpy.data.images.load(str(Path(bpy.utils.system_resource('DATAFILES'))/'studiolights/world/studio.exr'))
background=world_nodes.new('ShaderNodeBackground'); background.inputs[1].default_value=STUDIO
world_out=world_nodes.new('ShaderNodeOutputWorld')
scene.world.node_tree.links.new(studio_image.outputs['Color'],background.inputs['Color'])
scene.world.node_tree.links.new(background.outputs[0],world_out.inputs['Surface'])
scene.view_settings.view_transform='AgX'
scene.view_settings.look='AgX - Medium High Contrast'

def area(name,loc,power,size,scale=1):
    data=bpy.data.lights.new(name,'AREA'); data.energy=power; data.shape='RECTANGLE'; data.size=size; data.size_y=size*scale
    ob=bpy.data.objects.new(name,data); scene.collection.objects.link(ob); ob.location=loc
    ob.rotation_euler=(Vector((0,0,0))-ob.location).to_track_quat('-Z','Y').to_euler()
    return ob
area('Large softbox',(-80,-100,110),220000,90,1.5)
area('Right strip',(85,-35,35),160000,20,5)
area('Rear rim',(30,70,90),240000,65,1.5)
area('Front fill',(0,-110,-40),55000,70)
# Behind the camera: what a satin metal face mirrors back at the viewer
area('Front softbox',(0,-300,25),float(os.environ.get('FRONT','400000')),700,.45)
camdata=bpy.data.cameras.new('Review camera'); camera=bpy.data.objects.new('Review camera',camdata); scene.collection.objects.link(camera); scene.camera=camera
camdata.type='ORTHO'; camdata.ortho_scale=135
camera.location=(0,-220,0); camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'brick.blend'))
# The deployment geometry as material primitives, the screen apart
bpy.ops.object.select_all(action='DESELECT')
for ob in objects:
    if ob != screen: ob.select_set(True)
bpy.context.view_layer.objects.active=body
bpy.ops.object.join()
body.name='Housing and controls'
bpy.ops.object.select_all(action='DESELECT')
root.select_set(True); body.select_set(True); screen.select_set(True)
root.scale=(.001,)*3
(ROOT/'site/models').mkdir(parents=True,exist_ok=True)
bpy.ops.export_scene.gltf(filepath=str(ROOT/'site/models/brick.glb'),export_format='GLB',use_selection=True,export_apply=True,export_yup=True,export_cameras=False,export_lights=False)
root.scale=(1,1,1)
if '--render' in sys.argv:
    inspection=area('Inspection fill',(0,-100,0),65000,90)
    for name,loc in [('front',(0,-220,0)),('back',(0,220,0)),('left',(-220,0,0)),('right',(220,0,0)),('top',(0,0,220)),('bottom',(0,0,-220))]:
        camera.location=loc
        inspection.location=Vector(loc).normalized()*110
        inspection.rotation_euler=(-inspection.location).to_track_quat('-Z','Y').to_euler()
        camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler()
        if name in ('left','right'):
            scene.render.resolution_x=400; scene.render.resolution_y=1100; camdata.ortho_scale=124.9
        elif name in ('top','bottom'):
            scene.render.resolution_x=1000; scene.render.resolution_y=360; camdata.ortho_scale=85
            camera.rotation_euler=(0,0,0) if name=='top' else (math.pi,0,0)
        else:
            scene.render.resolution_x=800; scene.render.resolution_y=1100; camdata.ortho_scale=124.9
        scene.render.filepath=str(OUT/(name+'.png'))
        bpy.ops.render.render(write_still=True)
    inspection.hide_render=True
    # The Pixel 2 poster's pose and scale: 109 mm over its 960 px is 8.807
    # px a millimetre, so 1100 px is 124.9 mm
    root.rotation_euler=(Matrix.Rotation(-.28,4,'Z')).to_euler()
    camera.location=(0,-220,0)
    camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler()
    camdata.ortho_scale=1100*109/960
    scene.render.resolution_x=800; scene.render.resolution_y=1100
    scene.render.film_transparent=True
    nodes=display.node_tree.nodes; nodes.clear()
    image=nodes.new('ShaderNodeTexImage'); image.image=bpy.data.images.load(str(ROOT/'res/readme/brick/systems.png'))
    emission=nodes.new('ShaderNodeEmission'); output=nodes.new('ShaderNodeOutputMaterial')
    display.node_tree.links.new(image.outputs['Color'],emission.inputs['Color'])
    display.node_tree.links.new(emission.outputs[0],output.inputs['Surface'])
    scene.render.filepath=str(OUT/'hero.png')
    bpy.ops.render.render(write_still=True)
    bpy.data.images['Render Result'].save_render(str(ROOT/'site/models/brick-poster.png'))
    # The page's row of handhelds shows a quarter of it (site/devices.json)
    thumb=bpy.data.images.load(str(ROOT/'site/models/brick-poster.png'))
    thumb.scale(thumb.size[0]//4,thumb.size[1]//4)
    thumb.filepath_raw=str(ROOT/'site/models/brick-thumb.png'); thumb.file_format='PNG'; thumb.save()
print('Brick asset:',ROOT/'site/models/brick.glb')
