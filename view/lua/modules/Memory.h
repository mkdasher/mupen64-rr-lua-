extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <core/memory/memory.h>
#include <Windows.h>

namespace LuaCore::Memory
{
	static DWORD LuaCheckIntegerU(lua_State* L, int i = -1)
	{
		return (DWORD)luaL_checknumber(L, i);
	}

	static ULONGLONG LuaCheckQWord(lua_State* L, int i)
	{
		lua_pushinteger(L, 1);
		lua_gettable(L, i);
		ULONGLONG n = (ULONGLONG)LuaCheckIntegerU(L) << 32;
		lua_pop(L, 1);
		lua_pushinteger(L, 2);
		lua_gettable(L, i);
		n |= LuaCheckIntegerU(L);
		return n;
	}

	static void LuaPushQword(lua_State* L, ULONGLONG x)
	{
		lua_newtable(L);
		lua_pushinteger(L, 1);
		lua_pushinteger(L, x >> 32);
		lua_settable(L, -3);
		lua_pushinteger(L, 2);
		lua_pushinteger(L, x & 0xFFFFFFFF);
		lua_settable(L, -3);
	}

	// Read functions

	static int LuaReadByteUnsigned(lua_State* L)
	{
		UCHAR value = LoadRDRAMSafe<UCHAR>(luaL_checkinteger(L, 1));
		lua_pushinteger(L, value);
		return 1;
	}

	static int LuaReadByteSigned(lua_State* L)
	{
		CHAR value = LoadRDRAMSafe<CHAR>(luaL_checkinteger(L, 1));
		lua_pushinteger(L, value);
		return 1;
	}

	static int LuaReadWordUnsigned(lua_State* L)
	{
		USHORT value = LoadRDRAMSafe<USHORT>(luaL_checkinteger(L, 1));
		lua_pushinteger(L, value);
		return 1;
	}

	static int LuaReadWordSigned(lua_State* L)
	{
		SHORT value = LoadRDRAMSafe<SHORT>(luaL_checkinteger(L, 1));
		lua_pushinteger(L, value);
		return 1;
	}

	static int LuaReadDWorldUnsigned(lua_State* L)
	{
		ULONG value = LoadRDRAMSafe<ULONG>(luaL_checkinteger(L, 1));
		lua_pushinteger(L, value);
		return 1;
	}

	static int LuaReadDWordSigned(lua_State* L)
	{
		LONG value = LoadRDRAMSafe<LONG>(luaL_checkinteger(L, 1));
		lua_pushinteger(L, value);
		return 1;
	}

	static int LuaReadQWordUnsigned(lua_State* L)
	{
		ULONGLONG value = LoadRDRAMSafe<ULONGLONG>(luaL_checkinteger(L, 1));
		LuaPushQword(L, value);
		return 1;
	}

	static int LuaReadQWordSigned(lua_State* L)
	{
		LONGLONG value = LoadRDRAMSafe<LONGLONG>(luaL_checkinteger(L, 1));
		LuaPushQword(L, value);
		return 1;
	}

	static int LuaReadFloat(lua_State* L)
	{
		ULONG value = LoadRDRAMSafe<ULONG>(luaL_checkinteger(L, 1));
		lua_pushnumber(L, *(FLOAT*)&value);
		return 1;
	}

	static int LuaReadDouble(lua_State* L)
	{
		ULONGLONG value = LoadRDRAMSafe<ULONGLONG>(luaL_checkinteger(L, 1));
		lua_pushnumber(L, *(DOUBLE*)value);
		return 1;
	}

	// Write functions

	static int LuaWriteByteUnsigned(lua_State* L)
	{
		StoreRDRAMSafe<UCHAR>(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2));
		return 0;
	}

	static int LuaWriteWordUnsigned(lua_State* L)
	{
		StoreRDRAMSafe<USHORT>(luaL_checkinteger(L, 1),
		                       luaL_checkinteger(L, 2));
		return 0;
	}

	static int LuaWriteDWordUnsigned(lua_State* L)
	{
		StoreRDRAMSafe<ULONG>(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2));
		return 0;
	}

	static int LuaWriteQWordUnsigned(lua_State* L)
	{
		StoreRDRAMSafe<ULONGLONG>(luaL_checkinteger(L, 1), LuaCheckQWord(L, 2));
		return 0;
	}

	static int LuaWriteFloatUnsigned(lua_State* L)
	{
		FLOAT f = luaL_checknumber(L, -1);
		StoreRDRAMSafe<ULONG>(luaL_checkinteger(L, 1), *(ULONG*)&f);
		return 0;
	}

	static int LuaWriteDoubleUnsigned(lua_State* L)
	{
		DOUBLE f = luaL_checknumber(L, -1);
		StoreRDRAMSafe<ULONGLONG>(luaL_checkinteger(L, 1), *(ULONGLONG*)&f);
		return 0;
	}

	static int LuaReadSize(lua_State* L)
	{
		ULONG addr = luaL_checkinteger(L, 1);
		int size = luaL_checkinteger(L, 2);
		switch (size)
		{
		// unsigned
		case 1: lua_pushinteger(L, LoadRDRAMSafe<UCHAR>(addr));
			break;
		case 2: lua_pushinteger(L, LoadRDRAMSafe<USHORT>(addr));
			break;
		case 4: lua_pushinteger(L, LoadRDRAMSafe<ULONG>(addr));
			break;
		case 8: LuaPushQword(L, LoadRDRAMSafe<ULONGLONG>(addr));
			break;
		// signed
		case -1: lua_pushinteger(L, LoadRDRAMSafe<CHAR>(addr));
			break;
		case -2: lua_pushinteger(L, LoadRDRAMSafe<SHORT>(addr));
			break;
		case -4: lua_pushinteger(L, LoadRDRAMSafe<LONG>(addr));
			break;
		case -8: LuaPushQword(L, LoadRDRAMSafe<LONGLONG>(addr));
			break;
		default: luaL_error(L, "size must be 1, 2, 4, 8, -1, -2, -4, -8");
		}
		return 1;
	}

	static int LuaWriteSize(lua_State* L)
	{
		ULONG addr = luaL_checkinteger(L, 1);
		int size = luaL_checkinteger(L, 2);
		switch (size)
		{
		case 1: StoreRDRAMSafe<UCHAR>(addr, luaL_checkinteger(L, 3));
			break;
		case 2: StoreRDRAMSafe<USHORT>(addr, luaL_checkinteger(L, 3));
			break;
		case 4: StoreRDRAMSafe<ULONG>(addr, luaL_checkinteger(L, 3));
			break;
		case 8: StoreRDRAMSafe<ULONGLONG>(addr, LuaCheckQWord(L, 3));
			break;
		case -1: StoreRDRAMSafe<CHAR>(addr, luaL_checkinteger(L, 3));
			break;
		case -2: StoreRDRAMSafe<SHORT>(addr, luaL_checkinteger(L, 3));
			break;
		case -4: StoreRDRAMSafe<LONG>(addr, luaL_checkinteger(L, 3));
			break;
		case -8: StoreRDRAMSafe<LONGLONG>(addr, LuaCheckQWord(L, 3));
			break;
		default: luaL_error(L, "size must be 1, 2, 4, 8, -1, -2, -4, -8");
		}
		return 0;
	}

	static int LuaIntToFloat(lua_State* L)
	{
		ULONG n = luaL_checknumber(L, 1);
		lua_pushnumber(L, *(FLOAT*)&n);
		return 1;
	}

	static int LuaIntToDouble(lua_State* L)
	{
		ULONGLONG n = LuaCheckQWord(L, 1);
		lua_pushnumber(L, *(DOUBLE*)&n);
		return 1;
	}

	static int LuaFloatToInt(lua_State* L)
	{
		FLOAT n = luaL_checknumber(L, 1);
		lua_pushinteger(L, *(ULONG*)&n);
		return 1;
	}

	static int LuaDoubleToInt(lua_State* L)
	{
		DOUBLE n = luaL_checknumber(L, 1);
		LuaPushQword(L, *(ULONGLONG*)&n);
		return 1;
	}

	static int LuaQWordToNumber(lua_State* L)
	{
		ULONGLONG n = LuaCheckQWord(L, 1);
		lua_pushnumber(L, n);
		return 1;
	}

	template <typename T>
	static void PushT(lua_State* L, T value)
	{
		LuaPushIntU(L, value);
	}

	template <>
	static void PushT<ULONGLONG>(lua_State* L, ULONGLONG value)
	{
		LuaPushQword(L, value);
	}

	template <typename T, void(**readmem_func)()>
	static int ReadMemT(lua_State* L)
	{
		ULONGLONG *rdword_s = rdword, tmp, address_s = address;
		address = LuaCheckIntegerU(L, 1);
		rdword = &tmp;
		readmem_func[address >> 16]();
		PushT<T>(L, tmp);
		rdword = rdword_s;
		address = address_s;
		return 1;
	}

	template <typename T>
	static T CheckT(lua_State* L, int i)
	{
		return LuaCheckIntegerU(L, i);
	}

	template <>
	static ULONGLONG CheckT<ULONGLONG>(lua_State* L, int i)
	{
		return LuaCheckQWord(L, i);
	}

	template <typename T, void(**writemem_func)(), T& g_T>
	static int WriteMemT(lua_State* L)
	{
		ULONGLONG *rdword_s = rdword, address_s = address;
		T g_T_s = g_T;
		address = LuaCheckIntegerU(L, 1);
		g_T = CheckT<T>(L, 2);
		writemem_func[address >> 16]();
		address = address_s;
		g_T = g_T_s;
		return 0;
	}
}
