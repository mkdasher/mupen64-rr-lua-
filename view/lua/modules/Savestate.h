extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace LuaCore::Savestate
{
	//savestate
	//�蔲��
	static int SaveFileSavestate(lua_State* L)
	{
		savestates_do_file(lua_tostring(L, 1), e_st_job::save);
		return 0;
	}

	static int LoadFileSavestate(lua_State* L)
	{
		savestates_do_file(lua_tostring(L, 1), e_st_job::load);
		return 0;
	}
}
