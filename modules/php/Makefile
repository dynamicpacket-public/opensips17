# $Id$
#
# php module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=php.so
LIBS=

# define it (or export this) to the dir where xsubpp is on your system (if
# not on the default path.
#phpBINDIR=

ifeq ($(CC_NAME), gcc)
	DEFS+=-Wno-unused -Wno-redundant-decls
	DEFS+=$(shell php-config --includes)
endif

ifeq ($(phpLDOPTS),)
	LIBS+=$(shell php-config --libs)
else
	LIBS+=$(phpLDOPTS)
endif

ifeq ($(phpCCOPTS),)
	C_FLAGS+=$(shell php-config --includes)
else
	C_FLAGS+=$(shell php-config --includes)
endif


include ../../Makefile.modules

#php.d: opensipsxs.o
php.o: php.c

clean:	clean-special

.PHONY: clean-special
clean-special:
	rm -f *.o

install_module_custom:
	echo "installing php OpenSIPS packages ..."
#	mkdir -p $(modules-prefix)/$(lib-dir)/php

