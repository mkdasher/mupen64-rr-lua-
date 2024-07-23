extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <Windows.h>

#include "LuaConsole.h"
#include <view/gui/Main.h>
#include <core/memory/memory.h>
#include <core/memory/pif.h>
#include <core/r4300/timers.h>
#include <view/gui/features/Statusbar.hpp>

extern int m_current_vi;
extern long m_current_sample;

namespace LuaCore::Emu
{
	static int GetVICount(lua_State* L)
	{
		lua_pushinteger(L, m_current_vi);
		return 1;
	}

	static int GetSampleCount(lua_State* L)
	{
		lua_pushinteger(L, m_current_sample);
		return 1;
	}

	static int GetInputCount(lua_State* L)
	{
		lua_pushinteger(L, inputCount);
		return 1;
	}

	static int RegisterUpdateScreen(lua_State* L)
	{
		if (lua_toboolean(L, 2))
		{
			lua_pop(L, 1);
			UnregisterFunction(L, REG_ATUPDATESCREEN);
		} else
		{
			if (lua_gettop(L) == 2)
				lua_pop(L, 1);
			RegisterFunction(L, REG_ATUPDATESCREEN);
		}
		return 0;
	}

	static int RegisterAtDrawD2D(lua_State* L)
	{
		if (lua_toboolean(L, 2))
		{
			lua_pop(L, 1);
			UnregisterFunction(L, REG_ATDRAWD2D);
		} else
		{
			if (lua_gettop(L) == 2)
				lua_pop(L, 1);
			RegisterFunction(L, REG_ATDRAWD2D);
		}
		return 0;
	}

	static int RegisterVI(lua_State* L)
	{
		if (lua_toboolean(L, 2))
		{
			lua_pop(L, 1);
			UnregisterFunction(L, REG_ATVI);
		} else
		{
			if (lua_gettop(L) == 2)
				lua_pop(L, 1);
			RegisterFunction(L, REG_ATVI);
		}
		return 0;
	}

	static int RegisterInput(lua_State* L)
	{
		if (lua_toboolean(L, 2))
		{
			lua_pop(L, 1);
			UnregisterFunction(L, REG_ATINPUT);
		} else
		{
			if (lua_gettop(L) == 2)
				lua_pop(L, 1);
			RegisterFunction(L, REG_ATINPUT);
		}
		return 0;
	}

	static int RegisterStop(lua_State* L)
	{
		if (lua_toboolean(L, 2))
		{
			lua_pop(L, 1);
			UnregisterFunction(L, REG_ATSTOP);
		} else
		{
			if (lua_gettop(L) == 2)
				lua_pop(L, 1);
			RegisterFunction(L, REG_ATSTOP);
		}
		return 0;
	}

	static int RegisterWindowMessage(lua_State* L)
	{
		if (lua_toboolean(L, 2))
		{
			lua_pop(L, 1);
			UnregisterFunction(L, REG_WINDOWMESSAGE);
		} else
		{
			if (lua_gettop(L) == 2)
				lua_pop(L, 1);
			RegisterFunction(L, REG_WINDOWMESSAGE);
		}
		return 0;
	}

	static int RegisterInterval(lua_State* L)
	{
		if (lua_toboolean(L, 2))
		{
			lua_pop(L, 1);
			UnregisterFunction(L, REG_ATINTERVAL);
		} else
		{
			if (lua_gettop(L) == 2)
				lua_pop(L, 1);
			RegisterFunction(L, REG_ATINTERVAL);
		}
		return 0;
	}

	static int RegisterPlayMovie(lua_State* L)
	{
		if (lua_toboolean(L, 2))
		{
			lua_pop(L, 1);
			UnregisterFunction(L, REG_ATPLAYMOVIE);
		} else
		{
			if (lua_gettop(L) == 2)
				lua_pop(L, 1);
			RegisterFunction(L, REG_ATPLAYMOVIE);
		}
		return 0;
	}

	static int RegisterStopMovie(lua_State* L)
	{
		if (lua_toboolean(L, 2))
		{
			lua_pop(L, 1);
			UnregisterFunction(L, REG_ATSTOPMOVIE);
		} else
		{
			if (lua_gettop(L) == 2)
				lua_pop(L, 1);
			RegisterFunction(L, REG_ATSTOPMOVIE);
		}
		return 0;
	}

	static int RegisterLoadState(lua_State* L)
	{
		if (lua_toboolean(L, 2))
		{
			lua_pop(L, 1);
			UnregisterFunction(L, REG_ATLOADSTATE);
		} else
		{
			if (lua_gettop(L) == 2)
				lua_pop(L, 1);
			RegisterFunction(L, REG_ATLOADSTATE);
		}
		return 0;
	}

	static int RegisterSaveState(lua_State* L)
	{
		if (lua_toboolean(L, 2))
		{
			lua_pop(L, 1);
			UnregisterFunction(L, REG_ATSAVESTATE);
		} else
		{
			if (lua_gettop(L) == 2)
				lua_pop(L, 1);
			RegisterFunction(L, REG_ATSAVESTATE);
		}
		return 0;
	}

	static int RegisterReset(lua_State* L)
	{
		if (lua_toboolean(L, 2))
		{
			lua_pop(L, 1);
			UnregisterFunction(L, REG_ATRESET);
		} else
		{
			if (lua_gettop(L) == 2)
				lua_pop(L, 1);
			RegisterFunction(L, REG_ATRESET);
		}
		return 0;
	}

	static int Screenshot(lua_State* L)
	{
		CaptureScreen((char*)luaL_checkstring(L, 1));
		return 0;
	}

	static int IsMainWindowInForeground(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		lua_pushboolean(
			L, GetForegroundWindow() == g_main_hwnd || GetActiveWindow() ==
			g_main_hwnd);
		return 1;
	}

	static int LuaPlaySound(lua_State* L)
	{
		PlaySound(luaL_checkstring(L, 1), NULL, SND_FILENAME | SND_ASYNC);
		return 1;
	}

	static int SetGFX(lua_State* L)
	{
		// stub for now
		return 0;
	}

	static int EmuPause(lua_State* L)
	{
		if (!lua_toboolean(L, 1))
		{
			pause_emu();
		} else
		{
			resume_emu();
		}
		return 0;
	}

	static int GetEmuPause(lua_State* L)
	{
		lua_pushboolean(L, emu_paused);
		return 1;
	}

	static int GetSpeed(lua_State* L)
	{
		lua_pushinteger(L, g_config.fps_modifier);
		return 1;
	}

	static int SetSpeed(lua_State* L)
	{
		g_config.fps_modifier = luaL_checkinteger(L, 1);
		timer_init(g_config.fps_modifier, &ROM_HEADER);
		return 0;
	}

	static int GetFastForward(lua_State* L)
	{
		lua_pushboolean(L, fast_forward);
		return 1;
	}

	static int SetFastForward(lua_State* L)
	{
		fast_forward = lua_toboolean(L, 1);
		return 0;
	}

	static int SetSpeedMode(lua_State* L)
	{
		if (!strcmp(luaL_checkstring(L, 1), "normal"))
		{
			g_config.fps_modifier = 100;
		} else
		{
			g_config.fps_modifier = 10000;
		}
		return 0;
	}

	static int GetAddress(lua_State* L)
	{
		struct NameAndVariable
		{
			const char* name;
			void* pointer;
		};
#define A(n) {#n, &n}
#define B(n) {#n, n}
		const NameAndVariable list[] = {
			A(rdram),
			A(rdram_register),
			A(MI_register),
			A(pi_register),
			A(sp_register),
			A(rsp_register),
			A(si_register),
			A(vi_register),
			A(ri_register),
			A(ai_register),
			A(dpc_register),
			A(dps_register),
			B(SP_DMEM),
			B(PIF_RAM),
			{NULL, NULL}
		};
#undef A
#undef B
		const char* s = lua_tostring(L, 1);
		for (const NameAndVariable* p = list; p->name; p++)
		{
			if (lstrcmpi(p->name, s) == 0)
			{
				lua_pushinteger(L, (unsigned)p->pointer);
				return 1;
			}
		}
		luaL_error(L, "Invalid variable name. (%s)", s);
		return 0;
	}

	static int GetMupenVersion(lua_State* L)
	{
		int type = luaL_optnumber(L, 1, 0);
		const char* version;
		// 0 = name + version number
		// 1 = version number
		version = MUPEN_VERSION;
		if (type > 0)
			version = {&MUPEN_VERSION[strlen("Mupen 64 ")]};


		lua_pushstring(L, version);
		return 1;
	}

	//emu
	static int ConsoleWriteLua(lua_State* L)
	{
		GetLuaClass(L)->print(lua_tostring(L, 1));
		return 0;
	}

	static int StatusbarWrite(lua_State* L)
	{
		Statusbar::post(std::string(lua_tostring(L, 1)));
		return 0;
	}
}
