CC ?= gcc
CXX ?= g++
PLAT ?= none
PLATS = linux freebsd macosx

linux : PLAT = linux
macosx : PLAT = macosx

SHARED := -shared
macosx: SHARED := -bundle -undefined dynamic_lookup -all_load

linux macosx :
	$(MAKE) all SHARED="$(SHARED)"