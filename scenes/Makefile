

OS := $(shell uname)

ifeq ($(OS), Linux)
	BLENDER=../../../blender-2.80-linux-glibc217-x86_64/blender
else
	BLENDER=/Applications/Blender.app/Contents/MacOS/Blender
endif

all : \
	../dist/brunch.pnct \
	../dist/brunch.scene \
	../dist/plant.pnct \
	../dist/plant.banims \
	../dist/shower.pnct \
	../dist/shower.scene \
	../dist/rat_girl.pnct \
	../dist/rat_girl.scene


../dist/plant.pnct : plant.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- plant.blend '$@'

../dist/plant.banims : plant.blend export-bone-animations.py
	$(BLENDER) --background --python export-bone-animations.py -- '$<' 'Plant' '[0,30]Wind;[100,140]Walk' '$@'

../dist/brunch.scene : brunch.blend export-scene.py
	$(BLENDER) --background --python export-scene.py -- 'brunch.blend' '$@'

../dist/brunch.pnct : brunch.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- 'brunch.blend' '$@'

../dist/shower.scene : shower.blend export-scene.py
	$(BLENDER) --background --python export-scene.py -- 'shower.blend' '$@'

../dist/shower.pnct : shower.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- 'shower.blend' '$@'

../dist/rat_girl.pnct : rat_girl.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- 'rat_girl.blend' '$@'
../dist/rat_girl.scene : rat_girl.blend export-scene.py
	$(BLENDER) --background --python export-scene.py -- 'rat_girl.blend' '$@'

#../dist/pool.scene : pool.blend export-scene.py
#	$(BLENDER) --background --python export-scene.py -- 'pool.blend:Table 1' '$@'

#hexapod.pnct : hexapod.blend export-meshes.py
#	$(BLENDER) --background --python export-meshes.py -- hexapod.blend:Main '$@'
#hexapod.scene : hexapod.blend export-scene.py
#	$(BLENDER) --background --python export-scene.py -- hexapod.blend:Main '$@'
