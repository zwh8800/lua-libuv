/*
** $Id: loslib.c,v 1.57 2015/04/10 17:41:04 roberto Exp $
** Standard Operating System library
** See Copyright Notice in lua.h
*/

#define loslib_c
#define LUA_LIB

#include "lprefix.h"


#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#include <uv.h>
#include <fcntl.h>

#define UV_SERVER_META "UV_SERVER_META"


static int uv_test(lua_State *L) {
	int n = lua_gettop(L);  /* number of arguments */
	int i;
	lua_getglobal(L, "tostring");
	for (i = 1; i <= n; i++) {
		const char *s;
		size_t l;
		lua_pushvalue(L, -1);  /* function to be called */
		lua_pushvalue(L, i);   /* value to print */
		lua_call(L, 1, 1);
		s = lua_tolstring(L, -1, &l);  /* get result */
		if (s == NULL)
			return luaL_error(L, "'tostring' must return a string to 'print'");
		if (i>1) lua_writestring("\t", 1);
		lua_writestring(s, l);
		lua_pop(L, 1);  /* pop result */
	}
	lua_writeline();

	return 0;
}

static uv_loop_t *loop;
static int inited = 0;

static void init()
{
	if (inited)
		return;
	loop = uv_loop_new();
	inited = 1;
}

struct Server
{
	uv_tcp_t server;
	struct sockaddr_in bind_addr;
};

int init_server(struct Server* server)
{
	int ret;
	ret = uv_tcp_init(loop, &server->server);
	if (ret < 0)
		return ret;


}

static int uv_createServer(lua_State *L) 
{
	int ret;
	char* err_str;
	struct Server* server = malloc(sizeof(struct Server));
	if ((ret = init_server(server)) < 0)
	{
		err_str = uv_strerror(ret);
		lua_pushstring(L, err_str);
		lua_error(L);
	}

	lua_pushlightuserdata(L, server);
	luaL_setmetatable(L, UV_SERVER_META);

	return 1;
}

static int s_listen(lua_State *L)
{
	struct Server* server = lua_touserdata(L, 1);
	printf("0x%p\n", server);

	return 0;
}

static const luaL_Reg uvlib[] = {
	{ "test", uv_test },
	{ "createServer", uv_createServer },
	{ NULL, NULL }
};

static const luaL_Reg serverlib[] = {
	{ "listen", s_listen },
	{ NULL, NULL }
};

static void createmeta(lua_State *L)
{
	luaL_newmetatable(L, UV_SERVER_META);  /* create metatable for file handles */
	lua_pushvalue(L, -1);  /* push metatable */
	lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
	luaL_setfuncs(L, serverlib, 0);  /* add file methods to new metatable */
	lua_pop(L, 1);  /* pop new metatable */
}

LUAMOD_API int luaopen_uv(lua_State *L) {
	luaL_newlib(L, uvlib);
	createmeta(L);
	init();
	return 1;
}

