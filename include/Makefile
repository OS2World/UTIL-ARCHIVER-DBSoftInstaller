# Installer Makefile

CC = gcc
RM = rm -f
RC = rc
MAKE = make
COMPRESS = lxlite

DEFS =
LIBS =

CFLAGS = -O2 -Zomf -Zsys -Zmt -D__ST_MT_ERRNO__
LDFLAGS =
RCFLAGS = -r


OBJECTS = globals.obj include.obj uac_comm.obj uac_crc.obj \
          uac_crt.obj uac_dcpr.obj uac_sys.obj unace.obj

SOURCES = globals.c include.c uac_comm.c uac_crc.c \
          uac_crt.c uac_dcpr.c uac_sys.c unace.c

all: Include.exe

$(OBJECTS):
	$(CC) $(CFLAGS) -c $<	

Include.exe:  $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEFS) -o ../include.exe $(OBJECTS)

clean: 
	$(RM) $(OBJECTS) ../include.exe

globals.obj: globals.c globals.h acestruc.h unace.h
include.obj: include.c 
uac_comm.obj: uac_comm.c globals.h uac_dcpr.h uac_comm.h
uac_crc.obj: uac_crc.c uac_crc.h
uac_crt.obj: uac_crt.c os.h attribs.h globals.h uac_crt.h uac_sys.h
uac_dcpr.obj: uac_dcpr.c os.h globals.h portable.h uac_comm.h uac_crc.h \
	      uac_dcpr.h uac_sys.h
uac_sys.obj: uac_sys.c os.h globals.h uac_sys.h
unace.obj: unace.c os.h globals.h portable.h uac_comm.h uac_crc.h uac_crt.h \
           uac_dcpr.h uac_sys.h
