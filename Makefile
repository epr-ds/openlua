TARGET = openlua
PSP_EBOOT_TITLE = OpenLua

LUADIR = third_party/lua/src
STBDIR = third_party/stb

LUAOBJS = \
	$(LUADIR)/lapi.o \
	$(LUADIR)/lauxlib.o \
	$(LUADIR)/lbaselib.o \
	$(LUADIR)/lcode.o \
	$(LUADIR)/ldblib.o \
	$(LUADIR)/ldebug.o \
	$(LUADIR)/ldo.o \
	$(LUADIR)/ldump.o \
	$(LUADIR)/lfunc.o \
	$(LUADIR)/lgc.o \
	$(LUADIR)/linit.o \
	$(LUADIR)/liolib.o \
	$(LUADIR)/llex.o \
	$(LUADIR)/lmathlib.o \
	$(LUADIR)/lmem.o \
	$(LUADIR)/loadlib.o \
	$(LUADIR)/lobject.o \
	$(LUADIR)/lopcodes.o \
	$(LUADIR)/loslib.o \
	$(LUADIR)/lparser.o \
	$(LUADIR)/lstate.o \
	$(LUADIR)/lstring.o \
	$(LUADIR)/lstrlib.o \
	$(LUADIR)/ltable.o \
	$(LUADIR)/ltablib.o \
	$(LUADIR)/ltm.o \
	$(LUADIR)/lundump.o \
	$(LUADIR)/lvm.o \
	$(LUADIR)/lzio.o

OBJS = engine/core/main.o \
	engine/core/olreg.o \
	engine/core/stb_impl.o \
	engine/gfx/gfx.o \
	engine/gfx/gfx3d.o \
	engine/gfx/splash.o \
	engine/gfx/transform.o \
	engine/gfx/draw.o \
	engine/img/image.o \
	engine/input/buttons.o \
	engine/util/olmath.o \
	engine/util/timers.o \
	engine/font/font.o \
	engine/font/pgf.o \
	engine/snd/sound.o \
	engine/fs/olfs.o \
	engine/fs/files.o \
	engine/fs/ini.o \
	engine/data/json.o \
	engine/data/psmf.o \
	engine/sys/savedata.o \
	engine/sys/battery.o \
	engine/sys/power.o \
	engine/sys/os.o \
	engine/sys/hardware.o \
	engine/video/pmf.o \
	engine/maps/maps.o \
	engine/3d/amg.o \
	engine/3d/cam3d.o \
	engine/3d/obj.o \
	engine/3d/texture3d.o \
	engine/3d/model3d.o \
	$(LUAOBJS)

INCDIR = . $(LUADIR) $(STBDIR)

CFLAGS = -O2 -G0 -Wall
ASFLAGS = $(CFLAGS)
LDFLAGS = -L.

# Pull libcglue's sceUtility stub in with libpsputility so psp-fixup-imports
# sees all sceUtility stubs as one block instead of splitting the import table.
LIBS = -lpspgu -lpspgum -lpspaudio -lpspmpeg -lpsppower -lpsprtc -lpspwlan \
       -lpsputility -lpspreg -lpsphprm -lpspfont \
       -Wl,--undefined=sceUtilityGetSystemParamInt

EXTRA_TARGETS = EBOOT.PBP

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
