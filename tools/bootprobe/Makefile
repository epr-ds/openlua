TARGET = openlua_bootprobe
OBJS = main.o

LIBS = -lpsputility -Wl,--undefined=sceUtilityGetSystemParamInt

CFLAGS = -O2 -G0 -Wall
ASFLAGS = $(CFLAGS)

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = OpenLua Boot Probe

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
