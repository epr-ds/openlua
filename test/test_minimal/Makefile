TARGET = EBOOT
OBJS = main.o
TEXTBINDIR =
INCDIR =
CFLAGS = -O2 -G0 -Wall
ASFLAGS = $(CFLAGS)
CXXFLAGS = $(CFLAGS)
LDFLAGS = -G0
LIBDIR =
LIBS = -lpspctrl -lpspdisplay -lpspdebug -lpspuart
EXTRA_TARGETS = EBOOT.PBP
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
