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

#define UV_SERVER_META "uv.Server"
#define UV_SOCKET_META "uv.Socket"

#define BACKLOG 512


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



struct Socket
{
	uv_tcp_t socket;
	lua_State *L;
	int on_data;
	int on_error;
	int on_end;
};

struct Write
{
	uv_write_t write;
	lua_State *L;
	uv_buf_t buf;
};

static void on_alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t* buf)
{
	*buf = uv_buf_init(malloc(suggested_size), suggested_size);
}

static void on_stream_read(uv_stream_t* uv_socket, ssize_t nread, const uv_buf_t* buf)
{
	int ret;
	const char* err_str;
	struct Socket* socket = uv_socket;
	lua_State *L = socket->L;

	if (nread == UV__EOF)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, socket->on_end);
		uv_close(socket, NULL);

		lua_call(L, 0, 0);
	}
	else if(nread < 0)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, socket->on_error);
		uv_close(socket, NULL);
		err_str = uv_strerror(nread);
		lua_pushinteger(L, nread);
		lua_pushstring(L, err_str);

		lua_call(L, 2, 0);
	}
	else
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, socket->on_data);
		char* str = malloc(nread + 1);
		memcpy(str, buf->base, nread);
		str[nread] = '\0';
		lua_pushstring(L, str);
		lua_call(L, 1, 0);

		free(str);
	}
error:
	free(buf->base);
}

static int socket_reg_data(struct Socket* socket, int on_data)
{
	int ret;
	lua_State *L = socket->L;
	socket->on_data = on_data;

	ret = uv_read_start(socket, on_alloc_buffer, on_stream_read);
	if (ret < 0)
	{
		uv_close(socket, NULL);
		return ret;
	}

	return 0;
}

static void socket_on_write(uv_write_t* uv_write, int status)
{
	int ret;
	const char* err_str;

	struct Write* write = uv_write;

	if (status < 0)
	{
		err_str = uv_strerror(status);
		goto error;
	}

error:
	free(write);
}

static int socket_write(struct Socket* socket, const char* data)
{
	int ret;
	lua_State *L = socket->L;
	struct Write* write = malloc(sizeof(struct Write));
	memset(write, 0, sizeof(struct Write));
	write->L = L;
	write->buf = uv_buf_init(data, strlen(data));
	
	ret = uv_write(write, socket, &write->buf, 1, socket_on_write);
	if (ret < 0)
		return ret;

	return 0;
}

static void socket_close(struct Socket* socket)
{
	uv_close(socket, NULL);
}

struct Server
{
	uv_tcp_t server;
	lua_State *L;
	struct sockaddr_in bind_addr;
	int on_connection;
};

static int server_init(struct Server* server)
{
	int ret;
	ret = uv_tcp_init(loop, server);
	if (ret < 0)
		return ret;
	return 0;
}

static void server_on_connection(uv_stream_t* uv_server, int status)
{
	int ret;
	const char* err_str;
	struct Server* server = uv_server;
	lua_State *L = server->L;

	lua_rawgeti(L, LUA_REGISTRYINDEX, server->on_connection);

	if (status < 0)
	{
		err_str = uv_strerror(status);
		lua_pushnil(L);
		lua_pushstring(L, err_str);

		lua_call(L, 2, 0);

		return;
	}
	struct Socket* socket = lua_newuserdata(L, sizeof(struct Socket));
	memset(socket, 0, sizeof(struct Socket));
	socket->L = L;

	uv_tcp_init(loop, socket);
	ret = uv_accept(server, socket);
	if (ret < 0)
	{
		uv_close(socket, NULL);
		err_str = uv_strerror(ret);
		lua_pop(L, 1);
		lua_pushnil(L);
		lua_pushstring(L, err_str);

		lua_call(L, 2, 0);
		return;
	}
	luaL_setmetatable(L, UV_SOCKET_META);

	lua_call(L, 1, 0);
}

static int server_bind(struct Server* server, const char* ip, int port, int on_connection)
{
	int ret;
	server->on_connection = on_connection;
	ret = uv_ip4_addr(ip, port, &server->bind_addr);
	if (ret < 0)
		return ret;
	ret = uv_tcp_bind(server, &server->bind_addr, 0);
	if (ret < 0)
		return ret;
	ret = uv_listen(server, BACKLOG, server_on_connection);
	if (ret < 0)
		return ret;
	return 0;
}

static int uv_loop(lua_State *L)
{
	int ret;
	const char* err_str;
	ret = uv_run(loop, UV_RUN_DEFAULT);
	if (ret < 0)
	{
		err_str = uv_strerror(ret);
		lua_pushstring(L, err_str);
		lua_error(L);
	}
	return 0;
}

static int uv_createServer(lua_State *L) 
{
	int ret;
	const char* err_str;
	struct Server* server = lua_newuserdata(L, sizeof(struct Server));
	memset(server, 0, sizeof(struct Server));
	server->L = L;
	if ((ret = server_init(server)) < 0)
	{
		err_str = uv_strerror(ret);
		lua_pushstring(L, err_str);
		lua_error(L);
	}

	luaL_setmetatable(L, UV_SERVER_META);

	return 1;
}

static int s_listen(lua_State *L)
{
	int ret;
	const char* err_str;
	struct Server* server = luaL_checkudata(L, 1, UV_SERVER_META);
	const char* ip = luaL_checkstring(L, 2);
	lua_Integer port = luaL_checkinteger(L, 3);
	luaL_checkany(L, 4);
	int on_connection = luaL_ref(L, LUA_REGISTRYINDEX);
	if ((ret = server_bind(server, ip, port, on_connection)) < 0)
	{
		err_str = uv_strerror(ret);
		lua_pushstring(L, err_str);
		lua_error(L);
	}

	return 0;
}

static int s_tostring(lua_State* L)
{
	struct Server* server = luaL_checkudata(L, 1, UV_SERVER_META);

	lua_pushfstring(L, "Server(0x%p)", server);
	return 1;
}

static int s_gc(lua_State* L)
{
	return 0;
}

static int so_onData(lua_State* L)
{
	int ret;
	const char* err_str;
	struct Socket* socket = luaL_checkudata(L, 1, UV_SOCKET_META);
	luaL_checkany(L, 2);
	int on_data = luaL_ref(L, LUA_REGISTRYINDEX);
	if ((ret = socket_reg_data(socket, on_data)) < 0)
	{
		err_str = uv_strerror(ret);
		lua_pushstring(L, err_str);
		lua_error(L);
	}

	return 0;
}

static int so_onError(lua_State* L)
{
	int ret;
	const char* err_str;
	struct Socket* socket = luaL_checkudata(L, 1, UV_SOCKET_META);
	luaL_checkany(L, 2);
	int on_error = luaL_ref(L, LUA_REGISTRYINDEX);
	socket->on_error = on_error;

	return 0;
}

static int so_onEnd(lua_State* L)
{
	int ret;
	const char* err_str;
	struct Socket* socket = luaL_checkudata(L, 1, UV_SOCKET_META);
	luaL_checkany(L, 2);
	int on_end = luaL_ref(L, LUA_REGISTRYINDEX);
	socket->on_end = on_end;

	return 0;
}

static int so_write(lua_State *L)
{
	int ret;
	const char* err_str;
	struct Socket* socket = luaL_checkudata(L, 1, UV_SOCKET_META);
	const char* data = luaL_checkstring(L, 2);
	if ((ret = socket_write(socket, data)) < 0)
	{
		err_str = uv_strerror(ret);
		lua_pushstring(L, err_str);
		lua_error(L);
	}

	return 0;
}

static int so_close(lua_State *L)
{
	struct Socket* socket = luaL_checkudata(L, 1, UV_SOCKET_META);
	socket_close(socket);
	return 0;
}

static int so_finish(lua_State *L)
{
	struct Socket* socket = luaL_checkudata(L, 1, UV_SOCKET_META);
	const char* data = luaL_checkstring(L, 2);

	lua_pushcfunction(L, so_write);
	lua_pushlightuserdata(L, socket);
	lua_pushstring(L, data);
	lua_call(L, 2, 0);

	lua_pushcfunction(L, so_close);
	lua_pushlightuserdata(L, socket);
	lua_call(L, 1, 0);

	return 0;
}

static int so_tostring(lua_State* L)
{
	struct Socket* socket = luaL_checkudata(L, 1, UV_SOCKET_META);
	lua_pushfstring(L, "Socket(0x%p)", socket);
	return 1;
}

static int so_gc(lua_State* L)
{
	return 0;
}

static const luaL_Reg uvlib[] = {
	{ "test", uv_test },
	{ "loop", uv_loop },
	{ "createServer", uv_createServer },
	{ NULL, NULL }
};

static const luaL_Reg serverlib[] = {
	{ "listen", s_listen },
	{ "__tostring", s_tostring },
	{ "__gc", s_gc },
	{ NULL, NULL }
};

static const luaL_Reg socketlib[] = {
	{ "onData", so_onData },
	{ "onError", so_onError },
	{ "onEnd", so_onEnd },
	{ "write", so_write },
	{ "close", so_close },
	{ "finish", so_finish },
	{ "__tostring", so_tostring },
	{ "__gc", so_gc },
	{ NULL, NULL }
};

static void createmeta(lua_State *L)
{
	luaL_newmetatable(L, UV_SERVER_META);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_setfuncs(L, serverlib, 0);
	lua_pop(L, 1);

	luaL_newmetatable(L, UV_SOCKET_META);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_setfuncs(L, socketlib, 0);
	lua_pop(L, 1);
}

LUAMOD_API int luaopen_uv(lua_State *L) {
	luaL_newlib(L, uvlib);
	createmeta(L);
	init();
	return 1;
}

