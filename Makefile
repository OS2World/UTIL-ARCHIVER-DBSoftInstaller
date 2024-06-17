# Installer Makefile

CC = gcc
RM = rm
RC = rc
MAKE = make
COMPRESS = lxlite

DEFS =
LIBS =

CFLAGS = -O2 -Zomf -Zsys -Zmt -D__ST_MT_ERRNO__
LDFLAGS =
RCFLAGS = -r


OBJECTS = globals.obj install.obj uac_comm.obj uac_crc.obj \
          uac_crt.obj uac_dcpr.obj uac_sys.obj unace.obj

SOURCES = globals.c install.c uac_comm.c uac_crc.c \
          uac_crt.c uac_dcpr.c uac_sys.c unace.c

all: sfx.exe Include

$(OBJECTS):
	$(CC) $(CFLAGS) -c $<	

Include: include/Makefile Makefile
#	cd include
#	$(MAKE) -f Makefile all

sfx.exe:  $(OBJECTS)
	$(RC) $(RCFLAGS) install.rc
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEFS) -o sfx.exe $(OBJECTS) install.def install.res
	$(COMPRESS) sfx.exe

clean: 
	$(RM) $(OBJECTS) install.exe install.res sfx.exe

globals.obj: globals.c globals.h acestruc.h unace.h
install.obj: install.c install.h 
uac_comm.obj: uac_comm.c globals.h uac_dcpr.h uac_comm.h
uac_crc.obj: uac_crc.c uac_crc.h
uac_crt.obj: uac_crt.c os.h attribs.h globals.h uac_crt.h uac_sys.h
uac_dcpr.obj: uac_dcpr.c os.h globals.h portable.h uac_comm.h uac_crc.h \
	      uac_dcpr.h uac_sys.h
uac_sys.obj: uac_sys.c os.h globals.h uac_sys.h
unace.obj: unace.c os.h globals.h portable.h uac_comm.h uac_crc.h uac_crt.h \
           uac_dcpr.h uac_sys.h
install.res: install.rc install.h

