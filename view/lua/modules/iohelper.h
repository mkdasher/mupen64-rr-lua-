extern "C" {
#include <lib/lua/include/lua.h>
#include <lib/lua/include/lauxlib.h>
#include <lib/lua/include/lualib.h>
}
#include <view/gui/wrapper/PersistentPathDialog.h>

namespace LuaCore::IOHelper
{
	// IO
	static int LuaFileDialog(lua_State* L)
	{
		EmulationLock lock;
		auto filter = string_to_wstring(std::string(luaL_checkstring(L, 1)));
		const int32_t type = luaL_checkinteger(L, 2);

		std::wstring path;

		if (type == 0)
		{
			path = show_persistent_open_dialog("o_lua_api", mainHWND, filter);
		} else
		{
			path = show_persistent_save_dialog("s_lua_api", mainHWND, filter);
		}

		lua_pushstring(L, wstring_to_string(path).c_str());
		return 1;
	}
}
