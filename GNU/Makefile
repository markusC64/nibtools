#
# Makefile for building various nibtools executables
#
# Build targets are:  dos(djgpp) win32(cygwin) linux
#

# If building for DOS, change this to the path to your DJGPP install
DJGPP_PATH=	c:/djgpp

# To build standalone 32-bit binaries for Windows using Cygwin, you'll need this
#CC=i686-w64-mingw32-gcc
# To build standalone 64-bit binaries for Windows using Cygwin, you'll need this
#CC=x86_64-w64-mingw32-gcc
# Stadard BSD/Linux/Mac should be default
CC=gcc

# If building for Linux or Windows, change this to the base path to your
# opencbm includes and libraries
CBM_LNX_PATH=	/usr/local
#CBM_WIN_PATH=	c:\cbm4win
CBM_WIN_PATH=	../

# Default compile flags.  Be careful with optimization (-O2)
CFLAGS=${WARNS}
#CFLAGS+=-DSVN=$(shell svn info | grep Revision | sed 's/Revision: //') 
#CFLAGS+=-DSVN=$(shell svnversion | sed -e 's/.*://' -e 's/[A-Z]*$//')

# cl65 defs, contributed by Ullrich von Bassewitz <uz(at)musoftware(dot)de>
# (cc65 >= 2.6 required)
CA65        = ca65
LD65        = ld65
CA65_FLAGS  =  --feature pc_assignment

# No user-configurable parts below

.SUFFIXES: .asm .bin .inc
.PHONY: linux

usage:
	@echo Please specify a target: dos win32 linux clean distclean

# Arch-specific targets
dos:
	${MAKE} DJGPP="${DJGPP_PATH}\djgpp.env" \
		CC="${DJGPP_PATH}/bin/gcc" \
		ARCH_OBJ="cbm.o kernel.o nibtls_rt.o" \
		CFLAGS="-I include/DOS/ $(CFLAGS)" \
		EXE=".exe" \
		-f GNU/Makefile \
		nibconv nibscan nibrepair nibread nibwrite

linux:
	${MAKE} CFLAGS="-I include/LINUX/ -I ${CBM_LNX_PATH}/include ${CFLAGS}  -std=c99 -D_BSD_SOURCE" \
		LDFLAGS="-L${CBM_LNX_PATH}/lib -lopencbm" \
		-f GNU/Makefile \
		nibconv nibscan nibrepair nibread nibwrite nibsrqtest

win32:
	${MAKE} CFLAGS="-I include/WINDOWS/ -I ${CBM_WIN_PATH}/include -D WIN32 ${CFLAGS} -std=c99" \
		LDFLAGS="-L${CBM_WIN_PATH}/bin/i386/ -lopencbm" \
		EXE=".exe" \
		-f GNU/Makefile \
		nibconv nibscan nibrepair nibread nibwrite nibsrqtest

win64:
	${MAKE} CFLAGS="-I include/WINDOWS/ -I ${CBM_WIN_PATH}/include -D WIN32 ${CFLAGS} -std=c99" \
		LDFLAGS="-L${CBM_WIN_PATH}/bin/amd64/ -lopencbm" \
		EXE=".exe" \
		-f GNU/Makefile \
		nibconv nibscan nibrepair nibread nibwrite nibsrqtest

# Warning level.  Don't reduce, fix your new code instead.
WARNS= -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wpointer-arith \
       -Wextra -Wconversion -pedantic \
       -Werror-implicit-function-declaration \
       -Wsign-conversion
#       -Werror=format-security


# Common objects
OBJ=gcr.o prot.o fileio.o crc.o md5.o lz.o

# Objects for just drive access
NIBREAD_OBJ=nibread.o read.o drive.o ihs.o
NIBWRITE_OBJ=nibwrite.o write.o drive.o ihs.o
NIBSRQTEST_OBJ=nibsrqtest.o drive.o

NIBTOOLS_BIN=nibtools_1541.inc nibtools_1571.inc nibtools_1541_ihs.inc nibtools_1571_ihs.inc nibtools_1571_srq.inc nibtools_1571_srq_test.inc

# All programs to build
PROG=nibread nibwrite nibscan nibconv nibrepair nibsrqtest

buildall: ${PROG}

nibread: ${OBJ} ${NIBREAD_OBJ} ${NIBTOOLS_BIN} ${ARCH_OBJ}
	${CC} -o nibread$(EXE) ${OBJ} ${NIBREAD_OBJ} ${ARCH_OBJ} ${LDFLAGS}

nibsrqtest: ${NIBSRQTEST_OBJ} ${NIBTOOLS_BIN} ${ARCH_OBJ}
	${CC} -o nibsrqtest$(EXE) ${NIBSRQTEST_OBJ} ${ARCH_OBJ} ${LDFLAGS}

nibwrite: ${OBJ} ${NIBWRITE_OBJ} ${NIBTOOLS_BIN} ${ARCH_OBJ}
	${CC} -o nibwrite$(EXE) ${OBJ} ${NIBWRITE_OBJ} ${ARCH_OBJ} ${LDFLAGS}

nibread.c: ${NIBTOOLS_BIN} 

nibsrqtest.c: ${NIBTOOLS_BIN} 

nibwrite.c: ${NIBTOOLS_BIN} 

nibconv: ${OBJ} nibconv.o
	${CC} -o nibconv$(EXE) nibconv.o ${OBJ}

nibrepair: ${OBJ} nibrepair.o
	${CC} -o nibrepair$(EXE) nibrepair.o ${OBJ}

nibscan: ${OBJ} nibscan.o
	${CC} -o nibscan$(EXE) nibscan.o ${OBJ}

clean:
	${RM} *.o ${MNIB_BIN} *.bin *.inc nib*.exe

distclean: clean
	${RM} ${PROG} *.exe

drive.o: nibtools_1541.inc nibtools_1541_ihs.inc nibtools_1571.inc nibtools_1571_ihs.inc nibtools_1571_srq.inc nibtools_1571_srq_test.inc

*.o: gcr.h nibtools.h

nibtools_1541.bin: nibtools_15x1.asm
	$(CA65) $(CA65_FLAGS) -D DRIVE=1541 -o $*.tmp $<
	$(LD65) -o $@ --target none $*.tmp && rm -f $*.tmp

nibtools_1571.bin: nibtools_15x1.asm
	$(CA65) $(CA65_FLAGS) -D DRIVE=1571 -o $*.tmp $<
	$(LD65) -o $@ --target none $*.tmp && rm -f $*.tmp

nibtools_1541_ihs.bin: nibtools_15x1_ihs.asm
	$(CA65) $(CA65_FLAGS) -D DRIVE=1541 -o $*.tmp $<
	$(LD65) -o $@ --target none $*.tmp && rm -f $*.tmp

nibtools_1571_ihs.bin: nibtools_15x1_ihs.asm
	$(CA65) $(CA65_FLAGS) -D DRIVE=1571 -o $*.tmp $<
	$(LD65) -o $@ --target none $*.tmp && rm -f $*.tmp

nibtools_1571_srq.bin: nibtools_1571_srq.asm
	$(CA65) $(CA65_FLAGS) -D DRIVE=1571 -o $*.tmp $<
	$(LD65) -o $@ --target none $*.tmp && rm -f $*.tmp

nibtools_1571_srq_test.bin: nibtools_1571_srq_test.asm
	$(CA65) $(CA65_FLAGS) -D DRIVE=1571 -o $*.tmp $<
	$(LD65) -o $@ --target none $*.tmp && rm -f $*.tmp

.bin.inc:
	test -s $< && od -txC -v -An $< | \
	sed 's/\([0-9a-f]\{2\}\) */0x\1,/g; $$s/,$$//' > $@
