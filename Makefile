include platform.mk

KCP_ROOT ?= kcp
LUA_INCLUDE ?= /opt/homebrew/include/lua5.4

all:
	$(CXX) $(CFLAGS) -O2 -fPIC -c lua-kcp.cpp -o lua-kcp.o -I ${KCP_ROOT} -I ${LUA_INCLUDE}
	$(CC) $(CFLAGS) -O2 -fPIC -c ${KCP_ROOT}/ikcp.c -o ikcp.o -I ${KCP_ROOT}
	$(CXX) $(SHARED) -o kcp.so lua-kcp.o \
						ikcp.o