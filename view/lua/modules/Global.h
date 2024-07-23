extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <Windows.h>

namespace LuaCore::Global
{
	static int ToStringExs(lua_State* L);

	static int Print(lua_State* L)
	{
		lua_pushcfunction(L, ToStringExs);
		lua_insert(L, 1);
		lua_call(L, lua_gettop(L) - 1, 1);
		GetLuaClass(L)->print(std::string(lua_tostring(L, 1)) + "\r\n");
		return 0;
	}


	static int StopScript(lua_State* L)
	{
		luaL_error(L, "Stop requested");
		return 0;
	}

	static int ToStringEx(lua_State* L)
	{
		switch (lua_type(L, -1))
		{
		case LUA_TNIL:
		case LUA_TBOOLEAN:
		case LUA_TFUNCTION:
		case LUA_TUSERDATA:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
		case LUA_TNUMBER:
			lua_getglobal(L, "tostring");
			lua_pushvalue(L, -2);
			lua_call(L, 1, 1);
			lua_insert(L, lua_gettop(L) - 1);
			lua_pop(L, 1);
			break;
		case LUA_TSTRING:
			lua_getglobal(L, "string");
			lua_getfield(L, -1, "format");
			lua_pushstring(L, "%q");
			lua_pushvalue(L, -4);
			lua_call(L, 2, 1);
			lua_insert(L, lua_gettop(L) - 2);
			lua_pop(L, 2);
			break;
		case LUA_TTABLE:
			{
				lua_pushvalue(L, -1);
				lua_rawget(L, 1);
				if (lua_toboolean(L, -1))
				{
					lua_pop(L, 2);
					lua_pushstring(L, "{...}");
					return 1;
				}
				lua_pop(L, 1);
				lua_pushvalue(L, -1);
				lua_pushboolean(L, TRUE);
				lua_rawset(L, 1);
				int isArray = 0;
				std::string s("{");
				lua_pushnil(L);
				if (lua_next(L, -2))
				{
					while (1)
					{
						lua_pushvalue(L, -2);
						if (lua_type(L, -1) == LUA_TNUMBER &&
							isArray + 1 == lua_tonumber(L, -1))
						{
							lua_pop(L, 1);
							isArray++;
						} else
						{
							isArray = -1;
							if (lua_type(L, -1) == LUA_TSTRING)
							{
								s.append(lua_tostring(L, -1));
								lua_pop(L, 1);
							} else
							{
								ToStringEx(L);
								s.append("[").append(lua_tostring(L, -1)).
								  append("]");
								lua_pop(L, 1);
							}
						}
						ToStringEx(L);
						if (isArray == -1)
						{
							s.append("=");
						}
						s.append(lua_tostring(L, -1));
						lua_pop(L, 1);
						if (!lua_next(L, -2))break;
						s.append(", ");
					}
				}
				lua_pop(L, 1);
				s.append("}");
				lua_pushstring(L, s.c_str());
				break;
			}
		}
		return 1;
	}

	static int ToStringExInit(lua_State* L)
	{
		lua_newtable(L);
		lua_insert(L, 1);
		ToStringEx(L);
		return 1;
	}

	static int ToStringExs(lua_State* L)
	{
		int len = lua_gettop(L);
		std::string str("");
		for (int i = 0; i < len; i++)
		{
			lua_pushvalue(L, 1 + i);
			if (lua_type(L, -1) != LUA_TSTRING)
			{
				ToStringExInit(L);
			}
			str.append(lua_tostring(L, -1));
			lua_pop(L, 1);
			if (i != len - 1) { str.append("\t"); }
		}
		lua_pushstring(L, str.c_str());
		return 1;
	}

	static int PrintX(lua_State* L)
	{
		int len = lua_gettop(L);
		std::string str("");
		for (int i = 0; i < len; i++)
		{
			lua_pushvalue(L, 1 + i);
			if (lua_type(L, -1) == LUA_TNUMBER)
			{
				int n = (DWORD)luaL_checknumber(L, -1);
				lua_pop(L, 1);
				lua_getglobal(L, "string");
				lua_getfield(L, -1, "format"); //string,string.format
				lua_pushstring(L, "%X"); //s,f,X
				lua_pushinteger(L, n); //s,f,X,n
				lua_call(L, 2, 1); //s,r
				lua_insert(L, lua_gettop(L) - 1); //
				lua_pop(L, 1);
			} else if (lua_type(L, -1) == LUA_TSTRING)
			{
				//do nothing
			} else
			{
				ToStringExInit(L);
			}
			str.append(lua_tostring(L, -1));
			lua_pop(L, 1);
			if (i != len - 1) { str.append("\t"); }
		}
		GetLuaClass(L)->print(str + "\r\n");
		return 1;
	}
}
