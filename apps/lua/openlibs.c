/*
 * luaL_openlibs() for the koppi-os port: the core plus the "safe" standard
 * libraries. No io (no writable FS path) and no debug. os is included but
 * trimmed to the read-only + exit surface (time/date/clock/getenv/exit);
 * remove/rename/tmpname/execute return failure.
 *
 * package is kept so `require` of a .lua file on the FAT volume still works;
 * its C loader fails cleanly since there is no dlopen.
 */
#include "lprefix.h"

#include <stddef.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

static const luaL_Reg loadedlibs[] = {
  {LUA_GNAME, luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_COLIBNAME, luaopen_coroutine},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
  {LUA_UTF8LIBNAME, luaopen_utf8},
  {LUA_OSLIBNAME, luaopen_os},
  {NULL, NULL}
};

LUALIB_API void luaL_openlibs (lua_State *L) {
  const luaL_Reg *lib;
  for (lib = loadedlibs; lib->func; lib++) {
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);  /* remove lib */
  }
}
