extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <shared/Messenger.h>

namespace LuaCore::Movie
{
	// Movie
	static int PlayMovie(lua_State* L)
	{
		const char* fname = lua_tostring(L, 1);
		g_config.vcr_readonly = true;
		Messenger::broadcast(Messenger::Message::ReadonlyChanged, (bool)g_config.vcr_readonly);
		std::thread([fname] { VCR::start_playback(fname); }).detach();
		return 0;
	}

	static int StopMovie(lua_State* L)
	{
		VCR::stop_all();
		return 0;
	}

	static int GetMovieFilename(lua_State* L)
	{
		if (VCR::get_task() == e_task::idle)
		{
			luaL_error(L, "No movie is currently playing");
			lua_pushstring(L, "");
		} else
		{
			lua_pushstring(L, VCR::get_path().string().c_str());
		}
		return 1;
	}

	static int GetVCRReadOnly(lua_State* L)
	{
		lua_pushboolean(L, g_config.vcr_readonly);
		return 1;
	}

	static int SetVCRReadOnly(lua_State* L)
	{
		g_config.vcr_readonly = lua_toboolean(L, 1);
		Messenger::broadcast(Messenger::Message::ReadonlyChanged, (bool)g_config.vcr_readonly);
		return 0;
	}

	static int begin_seek_to(lua_State* L)
	{
		size_t frame = lua_tointeger(L, 1);
		bool relative = lua_toboolean(L, 2);

		lua_pushinteger(L, static_cast<size_t>(VCR::begin_seek_to(frame, relative)));
		return 1;
	}

	static int get_seek_info(lua_State* L)
	{
		size_t frame = lua_tointeger(L, 1);
		bool relative = lua_toboolean(L, 2);

		const auto pair = VCR::get_seek_info(frame, relative);

		lua_newtable(L);
		lua_pushboolean(L, pair.first);
		lua_rawseti(L, -2, 1);
		lua_pushinteger(L, pair.second);
		lua_rawseti(L, -2, 2);

		return 1;
	}

	static int stop_seek(lua_State* L)
	{
		VCR::stop_seek();
		return 0;
	}

	static int is_seeking(lua_State* L)
	{
		lua_pushboolean(L, VCR::is_seeking());
		return 1;
	}
}
