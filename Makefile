
#TARGET_ENV=GNU
TARGET_ENV=LINUX
#TARGET_ENV=WSDK

all:
	make -f $(TARGET_ENV)/Makefile

clean:
	make -f $(TARGET_ENV)/Makefile clean

install:
	make -f $(TARGET_ENV)/Makefile install

uninstall:
	make -f $(TARGET_ENV)/Makefile uninstall

.PHONY: all clean install uninstall
