# $Id: Makefile,v 1.1.1.1 2007/01/21 17:15:35 peter Exp $

#RELATIVEPATH=../
#include ${RELATIVEPATH}LINUX/config.make

.PHONY: all mrproper clean install uninstall install-files

OBJS =  nibread.o nibwrite.o nibscan.o nibconv.o nibrepair.o nibsrqtest.o read.o write.o gcr.o prot.o crc.o drive.o fileio.o ihs.o lz.o md5.o 
PROG = nibread nibwrite nibscan nibconv nibrepair nibsrqtest

CBM_LNX_PATH=../../../opencbm/OpenCBM/opencbm

all:
	make -f GNU/Makefile CBM_LNX_PATH=$(CBM_LNX_PATH) linux

mrproper: clean

clean:
	make -f GNU/Makefile distclean

install-files: $(PROG)
	install -m 755 -s $(PROG) $(BINDIR)
#	install -m 644 $(MAN1) $(MANDIR)

install: install-files

uninstall:
	rm -f $(BINDIR)/$(PROG)	# $(MANDIR)/$(MAN1)
