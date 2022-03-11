// kcp-lua封装, 支持多线程调用
#include <stdio.h>
#include <limits.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <string.h>
#include "ikcp.h"

#define check_kcp(L, idx)\
	*(ikcpcb**)luaL_checkudata(L, idx, "kcp_context")

struct kcp_callback
{
    lua_State *L;
    int callback;
};

static int kcp_output_callback(const char *buf, int len, ikcpcb *kcp, void *arg) {
    struct kcp_callback * c = (struct kcp_callback *)arg;
    if (c == NULL) return 0;

    lua_State* L = c->L;
    int callback = c->callback;
    if (callback != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
        lua_pushlstring(L, buf, len);
        lua_call(L, 1, 0);
    }
    return 0;
}

static int lua_kcp_context_set_output(lua_State * L) {
    lua_settop(L, 2);
    ikcpcb* kcp = check_kcp(L, 1);
    if (!lua_isfunction(L, 2)) {
        return luaL_error(L, "error: output should be a function");
    }

    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}

    int luafuncid = luaL_ref(L, LUA_REGISTRYINDEX);
    struct kcp_callback * c = (struct kcp_callback *)kcp->user;
    if (c->callback != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, c->callback);
    }
    c->callback = luafuncid;
    return 0;
}

static int lua_kcp_context_update(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);
    IUINT32 millisec = (IUINT32)luaL_checkinteger(L, 2);
    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}
    ikcp_update(kcp, millisec);
    return 0;
}

static int lua_kcp_context_check(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);
    IUINT32 current = (IUINT32)luaL_checkinteger(L, 2);

    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}

    IUINT32 nextUpdate = ikcp_check(kcp, current);
    lua_pushinteger(L, nextUpdate);
    return 1;
}

static int lua_kcp_context_input(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);
    size_t len = 0;
    const char * data = luaL_checklstring(L, 2, &len);

    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}

    if (len > LONG_MAX) {
        return luaL_error(L, "error: input size too long, more than LONG_MAX");
    }

    int ret = ikcp_input(kcp, data, (long)len);
    lua_pushinteger(L, ret);
    return 1;
}

static int lua_kcp_context_flush(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);
    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}
    ikcp_flush(kcp);
    return 0;
}

static int lua_kcp_context_recv(lua_State * L) {
    lua_settop(L, 2);
    ikcpcb* kcp = check_kcp(L, 1);
    int size = 1024;
    if (lua_isinteger(L, 2)) {
        lua_Integer tempSize = luaL_checkinteger(L, 2);
        if (tempSize > INT32_MAX) {
            return luaL_error(L, "error: buffer size to long, more than INT32_MAX");
        } else if (tempSize < 0) {
            return luaL_error(L, "error: buffer size small than 0");
        }
        size = (int)tempSize;
    }
    
    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}

    int bufferLen = sizeof(char) * size;
    char * buffer = (char *)malloc(bufferLen);
    memset(buffer, 0, bufferLen);
    int ret = ikcp_recv(kcp, buffer, bufferLen);
    lua_pushinteger(L, ret);
    if (ret >= 0) {
        lua_pushlstring(L, buffer, ret);
    } else {
        lua_pushnil(L);
    }
    free(buffer);
    return 2;
}

static int lua_kcp_context_send(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);
    size_t len = 0;
    const char * data = luaL_checklstring(L, 2, &len);

    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}

    if (len > INT32_MAX) {
        return luaL_error(L, "error: input size to long, more than INT32_MAX");
    }

    int ret = ikcp_send(kcp, data, (int)len);
    lua_pushinteger(L, ret);
    return 1;
}

static int lua_kcp_context_peeksize(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);
    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}

    int size = ikcp_peeksize(kcp);
    lua_pushinteger(L, size);
    return 1;
}

static int lua_kcp_context_setmtu(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);
    lua_Integer mtu = luaL_checkinteger(L, 2);

    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}

    if (mtu > INT32_MAX) {
        return luaL_error(L, "error: mtu too large, more than INT32_MAX");
    } else if (mtu < INT32_MIN) {
        return luaL_error(L, "error: mtu too small, less than INT32_MIN");
    }

    int ret = ikcp_setmtu(kcp, mtu);
    lua_pushinteger(L, ret);
    return 1;
}

static int lua_kcp_context_wndsize(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);
    lua_Integer sndwnd = luaL_checkinteger(L, 2);
    lua_Integer rcvwnd = luaL_checkinteger(L, 3);

    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}

    if (sndwnd > INT32_MAX) {
        return luaL_error(L, "error: sndwnd too large, more than INT32_MAX");
    } else if (sndwnd < INT32_MIN) {
        return luaL_error(L, "error: sndwnd too small, less than INT32_MAX");
    }

    if (rcvwnd > INT32_MAX) {
        return luaL_error(L, "error: rcvwnd too large, more than INT32_MAX");
    } else if (rcvwnd < INT32_MIN) {
        return luaL_error(L, "error: rcvwnd too small, less than INT32_MAX");
    }

    int ret = ikcp_wndsize(kcp, sndwnd, rcvwnd);
    lua_pushinteger(L, ret);
    return 1;
}

static int lua_kcp_context_waitsnd(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);

    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}

    int ret = ikcp_waitsnd(kcp);
    lua_pushinteger(L, ret);
    return 1;
}

static int lua_kcp_context_nodelay(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);
    int nodely = (int)luaL_checkinteger(L, 2);
    int interval = (int)luaL_checkinteger(L, 3);
    int resend = (int)luaL_checkinteger(L, 4);
    int nc = (int)luaL_checkinteger(L, 5);

    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}

    int ret = ikcp_nodelay(kcp, nodely, interval, resend, nc);
    lua_pushinteger(L, ret);
    return 1;
}

static int lua_kcp_context_minrto(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);
    lua_Integer minrto = luaL_checkinteger(L, 2);

    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}

    kcp->rx_minrto = (int)minrto;
    return 0;
}

static const luaL_Reg lua_kcp_context_functions[] = {
    {"set_output", lua_kcp_context_set_output},
    {"update", lua_kcp_context_update},
    {"input", lua_kcp_context_input},
    {"flush", lua_kcp_context_flush},
    {"recv", lua_kcp_context_recv},
    {"send", lua_kcp_context_send},
    {"check", lua_kcp_context_check},
    {"peeksize", lua_kcp_context_peeksize},
    {"setmtu", lua_kcp_context_setmtu},
    {"wndsize", lua_kcp_context_wndsize},
    {"waitsnd", lua_kcp_context_waitsnd},
    {"nodelay", lua_kcp_context_nodelay},
    {"minrto", lua_kcp_context_minrto},
    { NULL, NULL }
};



static int lua_kcp_context_gc(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);
	if (kcp == NULL) {
        return 0;
	}
    if (kcp->user != NULL) {
        struct kcp_callback * c = (struct kcp_callback *)kcp->user;
        int callback = c->callback;
        if (callback != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, callback);
        }
        free(c);
        kcp->user = NULL;
    }
    ikcp_release(kcp);
    kcp = NULL;
    return 0;
}

static int lua_kcp_context_tostring(lua_State *L)
{
    ikcpcb* kcp = check_kcp(L, 1);
    lua_pushfstring(L, "lua_kcp_context: %p", kcp);
    return 1;
}

static int lua_kcp_create_context(lua_State *L) {
    lua_Integer conv = luaL_checkinteger(L, 1);
    if (conv > UINT32_MAX) {
        return luaL_error(L, "error: context id has larger then UINT32_MAX");
    }

    struct kcp_callback * callbacks = malloc(sizeof(struct kcp_callback));
    if (callbacks == NULL) return luaL_error(L, "error: malloc kcp_callback faild");
    memset(callbacks, 0, sizeof(struct kcp_callback));
    callbacks->L = L;
    callbacks->callback = LUA_NOREF;

    ikcpcb * kcp = ikcp_create((IUINT32)conv, (void *)callbacks);
    if (kcp == NULL) {
        free(callbacks);
        return luaL_error(L, "error: fail to create kcp");
    }

    ikcp_setoutput(kcp, kcp_output_callback);

    *(ikcpcb**)lua_newuserdata(L, sizeof(void*)) = kcp;
    if (luaL_newmetatable(L, "kcp_context"))
    {
        luaL_newlib(L, lua_kcp_context_functions);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, lua_kcp_context_tostring);
        lua_setfield(L, -2, "__tostring");

        lua_pushcfunction(L, lua_kcp_context_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    return 1;
}

static const struct luaL_Reg kcp_funcs[] =
{
    {"create", lua_kcp_create_context},
    {NULL, NULL}
};

LUALIB_API int luaopen_kcp(lua_State * const L)
{
  luaL_newlib(L, kcp_funcs);
  return 1;
}