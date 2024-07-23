extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace LuaCore::Joypad
{
	static int lua_get_joypad(lua_State* L)
	{
		int i = luaL_optinteger(L, 1, 1) - 1;
		if (i < 0 || i >= 4)
		{
			luaL_error(L, "port: 1-4");
		}
		lua_newtable(L);
#define A(a,s) lua_pushboolean(L,last_controller_data[i].a);lua_setfield(L, -2, s)
		A(R_DPAD, "right");
		A(L_DPAD, "left");
		A(D_DPAD, "down");
		A(U_DPAD, "up");
		A(START_BUTTON, "start");
		A(Z_TRIG, "Z");
		A(B_BUTTON, "B");
		A(A_BUTTON, "A");
		A(R_CBUTTON, "Cright");
		A(L_CBUTTON, "Cleft");
		A(D_CBUTTON, "Cdown");
		A(U_CBUTTON, "Cup");
		A(R_TRIG, "R");
		A(L_TRIG, "L");
#undef A
		lua_pushinteger(L, last_controller_data[i].X_AXIS);
		lua_setfield(L, -2, "Y");
		lua_pushinteger(L, last_controller_data[i].Y_AXIS);
		lua_setfield(L, -2, "X");
		return 1;
	}

	static int lua_set_joypad(lua_State* L)
	{
		int a_2 = 2;
		int i;
		if (lua_type(L, 1) == LUA_TTABLE)
		{
			a_2 = 1;
			i = 0;
		} else
		{
			i = luaL_optinteger(L, 1, 1) - 1;
		}
		if (i < 0 || i >= 4)
		{
			luaL_error(L, "control: 1-4");
		}
		lua_pushvalue(L, a_2);
#define A(a,s) lua_getfield(L, -1, s);new_controller_data[i].a=lua_toboolean(L,-1);lua_pop(L,1)
		A(R_DPAD, "right");
		A(L_DPAD, "left");
		A(D_DPAD, "down");
		A(U_DPAD, "up");
		A(START_BUTTON, "start");
		A(Z_TRIG, "Z");
		A(B_BUTTON, "B");
		A(A_BUTTON, "A");
		A(R_CBUTTON, "Cright");
		A(L_CBUTTON, "Cleft");
		A(D_CBUTTON, "Cdown");
		A(U_CBUTTON, "Cup");
		A(R_TRIG, "R");
		A(L_TRIG, "L");
		lua_getfield(L, -1, "Y");
		new_controller_data[i].X_AXIS = lua_tointeger(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, -1, "X");
		new_controller_data[i].Y_AXIS = lua_tointeger(L, -1);
		lua_pop(L, 1);
		overwrite_controller_data[i] = true;
#undef A
		return 1;
	}
}
