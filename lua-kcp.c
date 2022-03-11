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

static int lua_kcp_context_input(lua_State * L) {
    ikcpcb* kcp = check_kcp(L, 1);
    size_t len = 0;
    const char * data = luaL_checklstring(L, 2, &len);

    if (kcp == NULL) {
        return luaL_error(L, "error: kcp context has been release");
	}

    int ret = ikcp_input(kcp, data, len);
    lua_pushinteger(L, ret);
    return 1;
}

static const luaL_Reg lua_kcp_context_functions[] = {
    {"set_output", lua_kcp_context_set_output},
    {"update", lua_kcp_context_update},
    {"input", lua_kcp_context_input},
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
    long long conv = luaL_checkinteger(L, 1);
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