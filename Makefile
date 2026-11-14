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
	engine/core/color.o \
	engine/core/stb_impl.o \
	engine/net/net.o \
	engine/gfx/gfx.o \
	engine/gfx/gfx3d.o \
	engine/gfx/splash.o \
	engine/gfx/transform.o \
	engine/gfx/draw.o \
	engine/img/image.o \
	engine/img/image_mem.o \
	engine/input/buttons.o \
	engine/util/olmath.o \
	engine/util/timers.o \
	engine/font/font.o \
	engine/font/pgf.o \
	engine/snd/sound.o \
	engine/snd/minimp3_impl.o \
	engine/fs/olfs.o \
	engine/fs/files.o \
	engine/fs/ini.o \
	engine/data/json.o \
	engine/data/psmf.o \
	engine/data/pbp.o \
	engine/sys/savedata.o \
	engine/sys/battery.o \
	engine/sys/power.o \
	engine/sys/os.o \
	engine/sys/hardware.o \
	engine/sys/osk.o \
	engine/sys/game.o \
	engine/sys/cam.o \
	engine/sys/umd.o \
	engine/sys/usb.o \
	engine/sys/sio.o \
	engine/sys/ir.o \
	engine/sys/wlan.o \
	engine/sys/socket.o \
	engine/sys/http.o \
	engine/sys/ftp.o \
	engine/sys/adhoc.o \
	engine/sys/bluetooth.o \
	engine/video/pmf.o \
	engine/maps/maps.o \
	engine/3d/amg.o \
	engine/3d/cam3d.o \
	engine/3d/obj.o \
	engine/3d/texture3d.o \
	engine/3d/model3d.o \
	$(LUAOBJS)

INCDIR = . $(LUADIR) $(STBDIR)

CFLAGS = -O2 -G0 -Wall -DOPENLUA_NO_PGF
ASFLAGS = $(CFLAGS)
LDFLAGS = -L.

# Pull libcglue's sceUtility stub in with libpsputility so psp-fixup-imports
# sees all sceUtility stubs as one block instead of splitting the import table.
# libpspnet/libpspnet_apctl are appended by build.mak, and psp-gcc's specs add
# libpspnet_inet/libpspnet_resolver last.  Listing those four here as well
# would scan them twice and split their stub blocks ("stubs out of order"), so
# this list only carries the libraries the SDK does not add automatically.
LIBS = -lpspgu -lpspgum -lpspaudio -lpspmpeg -lpsppower -lpsprtc \
       -lpsputility -lpspreg -lpsphprm -lpspusbcam -lpspusb \
       -lpspumd \
       -lpspnet_adhocctl -lpspnet_adhocmatching -lpspnet_adhoc \
       -lpspwlan \
       -Wl,--undefined=sceUtilityGetSystemParamInt

EXTRA_TARGETS = EBOOT.PBP

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# Diagnostic EBOOT used to triage 8002013C on real hardware.  It is packaged
# next to the main tester so users can tell a missing library apart from a
# launcher/CFW problem.
probe:
	$(MAKE) -C tools/bootprobe

