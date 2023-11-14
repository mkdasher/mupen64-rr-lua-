/*

*/


#include "LuaConsole.h"
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <string>
#include <map>
#include <list>
#include <filesystem>
#include "../../winproject/resource.h"
#include "win/main_win.h"
#include "win/Config.hpp"
#include "include/lua.hpp"
#include "../memory/memory.h"
#include "../r4300/r4300.h"
#include "../r4300/recomp.h"
#include "../main/plugin.hpp"
#include "../main/disasm.h"
#include "../main/vcr.h"
#include "../main/savestates.h"
#include "../main/win/configdialog.h"
#include "../main/win/wrapper/PersistentPathDialog.h"
#include "../main/vcr_compress.h"
#include <vcr.h>
#include <gdiplus.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <functional>
#include <queue>
#include <md5.h>
#include <assert.h>

#include "Recent.h"
#include "helpers/string_helpers.h"
#include "win/features/Statusbar.hpp"

#include <Windows.h>
#pragma comment(lib, "lua54.lib")

extern unsigned long op;
extern void (*interp_ops[64])(void);
extern int m_current_vi;
extern long m_current_sample;
extern int fast_memory;
void SYNC();
void NOTCOMPILED();
void timer_init();
inline void TraceLoggingBufFlush();
extern void (__cdecl*CaptureScreen)(char* Directory); // for lua screenshot

std::vector<std::string> recent_closed_lua;

BUTTONS last_controller_data[4];
BUTTONS new_controller_data[4];
bool overwrite_controller_data[4];

bool enableTraceLog;
bool traceLogMode;
bool gdiPlusInitialized = false;
ULONG_PTR gdiPlusToken;
// LoadScreen variables
HDC hwindowDC, hsrcDC;
t_window_info windowSize{};
HBITMAP hbitmap;
bool LoadScreenInitialized = false;
std::map<HWND, LuaEnvironment*> hwnd_lua_map;
t_window_procedure_params window_proc_params = {0};
// Deletes all the variables used in LoadScreen (avoid memory leaks)
void LoadScreenDelete()
{
	ReleaseDC(main_hwnd, hwindowDC);
	DeleteDC(hsrcDC);

	LoadScreenInitialized = false;
}

// Initializes everything needed for LoadScreen
void LoadScreenInit()
{
	if (LoadScreenInitialized) LoadScreenDelete();

	hwindowDC = GetDC(main_hwnd);
	// Create a handle to the main window Device Context
	hsrcDC = CreateCompatibleDC(hwindowDC); // Create a DC to copy the screen to

	get_window_info(main_hwnd, windowSize);
	windowSize.height -= 1; // ¯\_(ツ)_/¯
	printf("LoadScreen Size: %d x %d\n", windowSize.width, windowSize.height);

	// create an hbitmap
	hbitmap = CreateCompatibleBitmap(hwindowDC, windowSize.width,
	                                 windowSize.height);

	LoadScreenInitialized = true;
}

#define DEBUG_GETLASTERROR 0


	RECT InitalWindowRect[3] = {0};
	HANDLE TraceLogFile;

	std::vector<Gdiplus::Bitmap*> image_pool;

	unsigned inputCount = 0;

	int getn(lua_State*);


	//improved debug print from stackoverflow, now shows function info
#ifdef _DEBUG
	static void stackDump(lua_State* L)
	{
		int i = lua_gettop(L);
		printf(" ----------------  Stack Dump ----------------\n");
		while (i)
		{
			int t = lua_type(L, i);
			switch (t)
			{
			case LUA_TSTRING:
				printf("%d:`%s'", i, lua_tostring(L, i));
				break;
			case LUA_TBOOLEAN:
				printf("%d: %s", i, lua_toboolean(L, i) ? "true" : "false");
				break;
			case LUA_TNUMBER:
				printf("%d: %g", i, lua_tonumber(L, i));
				break;
			case LUA_TFUNCTION:
				lua_Debug ar;
				lua_getstack(L, 0, &ar);
				lua_pushvalue(L, -1);
				lua_getinfo(L, ">S", &ar);
				printf("%d: %s %p, type: %s", i, "function at",
				       lua_topointer(L, -1), ar.what);
				break;
			default: printf("%d: %s", i, lua_typename(L, t));
				break;
			}
			printf("\n");
			i--;
		}
		printf("--------------- Stack Dump Finished ---------------\n");
	}
#endif
	void ASSERT(bool e, const char* s = "assert")
	{
		if (!e)
		{
			DebugBreak();
			printf("Lua assert failed: %s\n", s);
		}
	}

	struct EmulationLock
	{
		EmulationLock()
		{
			printf("Emulation Lock\n");
			pause_emu(FALSE);
		}

		~EmulationLock()
		{
			resume_emu(FALSE);
			printf("Emulation Unlock\n");
		}
	};

	int AtPanic(lua_State* L);
	extern const luaL_Reg globalFuncs[];
	extern const luaL_Reg emuFuncs[];
	extern const luaL_Reg wguiFuncs[];
	extern const luaL_Reg d2dFuncs[];
	extern const luaL_Reg memoryFuncs[];
	extern const luaL_Reg inputFuncs[];
	extern const luaL_Reg joypadFuncs[];
	extern const luaL_Reg movieFuncs[];
	extern const luaL_Reg savestateFuncs[];
	extern const luaL_Reg ioHelperFuncs[];
	extern const luaL_Reg aviFuncs[];
	extern const char* const REG_ATSTOP;
	int AtStop(lua_State* L);

	const char* const REG_LUACLASS = "C";
	const char* const REG_ATUPDATESCREEN = "S";
	const char* const REG_ATVI = "V";
	const char* const REG_ATINPUT = "I";
	const char* const REG_ATSTOP = "T";
	const char* const REG_SYNCBREAK = "B";
	const char* const REG_READBREAK = "R";
	const char* const REG_WRITEBREAK = "W";
	const char* const REG_WINDOWMESSAGE = "M";
	const char* const REG_ATINTERVAL = "N";
	const char* const REG_ATPLAYMOVIE = "PM";
	const char* const REG_ATSTOPMOVIE = "SM";
	const char* const REG_ATLOADSTATE = "LS";
	const char* const REG_ATSAVESTATE = "SS";
	const char* const REG_ATRESET = "RE";

	int AtUpdateScreen(lua_State* L);

	int AtPanic(lua_State* L)
	{
		printf("Lua panic: %s\n", lua_tostring(L, -1));
		MessageBox(main_hwnd, lua_tostring(L, -1), "Lua Panic", 0);
		return 0;
	}

	extern const char* const REG_WINDOWMESSAGE;
	int AtWindowMessage(lua_State* L);

	void invoke_callbacks_with_key_on_all_instances(
		std::function<int(lua_State*)> function, const char* key)
	{
		for (auto pair : hwnd_lua_map)
		{
			pair.second->invoke_callbacks_with_key(function, key);
		}
	}


	void checkGDIPlusInitialized()
	{
		if (gdiPlusInitialized)
			return;
		printf("Initializing GDI+\n");
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		Gdiplus::GdiplusStartup(&gdiPlusToken, &gdiplusStartupInput, NULL);
		gdiPlusInitialized = true;
	}

	void SizingControl(HWND wnd, RECT* p, int x, int y, int w, int h)
	{
		SetWindowPos(wnd, NULL, p->left + x, p->top + y,
		             p->right - p->left + w, p->bottom - p->top + h,
		             SWP_NOZORDER);
	}

	void SizingControls(HWND wnd, WORD width, WORD height)
	{
		int xa = width - (InitalWindowRect[0].right - InitalWindowRect[0].left),
		    ya = height - (InitalWindowRect[0].bottom - InitalWindowRect[0].
			    top);
		SizingControl(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
		              &InitalWindowRect[1], 0, 0, xa, 0);
		SizingControl(GetDlgItem(wnd, IDC_TEXTBOX_LUACONSOLE),
		              &InitalWindowRect[2], 0, 0, xa, ya);
	}

	void GetInitalWindowRect(HWND wnd)
	{
		GetClientRect(wnd, &InitalWindowRect[0]);
		GetWindowRect(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
		              &InitalWindowRect[1]);
		GetWindowRect(GetDlgItem(wnd, IDC_TEXTBOX_LUACONSOLE),
		              &InitalWindowRect[2]);
		MapWindowPoints(NULL, wnd, (LPPOINT)&InitalWindowRect[1], 2 * 2);
	}

	BOOL WmCommand(HWND wnd, WORD id, WORD code, HWND control);

	INT_PTR CALLBACK DialogProc(HWND wnd, UINT msg, WPARAM wParam,
	                            LPARAM lParam)
	{
		switch (msg)
		{
		case WM_INITDIALOG:
			{
				checkGDIPlusInitialized();

				if (InitalWindowRect[0].right == 0)
				{
					GetInitalWindowRect(wnd);
				}
				SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
				              Config.lua_script_path.c_str());
				return TRUE;
			}
		case WM_CLOSE:
			DestroyWindow(wnd);
			return TRUE;
		case WM_DESTROY:
			{
				if (hwnd_lua_map.contains(wnd)) {
					LuaEnvironment::destroy(hwnd_lua_map[wnd]);
				}
				return TRUE;
			}
		case WM_COMMAND:
			return WmCommand(wnd, LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
		case WM_SIZE:
			SizingControls(wnd, LOWORD(lParam), HIWORD(lParam));
			if (wParam == SIZE_MINIMIZED) SetFocus(main_hwnd);
			break;
		}
		return FALSE;
	}

	void SetButtonState(HWND wnd, bool state)
	{
		if (!IsWindow(wnd)) return;
		const HWND state_button = GetDlgItem(wnd, IDC_BUTTON_LUASTATE);
		const HWND stop_button = GetDlgItem(wnd, IDC_BUTTON_LUASTOP);
		if (state)
		{
			SetWindowText(state_button, "Restart");
			EnableWindow(stop_button, TRUE);
		} else
		{
			SetWindowText(state_button, "Run");
			EnableWindow(stop_button, FALSE);
		}
	}

	BOOL WmCommand(HWND wnd, WORD id, WORD code, HWND control)
	{
		switch (id)
		{
		case IDC_BUTTON_LUASTATE:
			{
				char path[MAX_PATH] = {0};
				GetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
				              path, MAX_PATH);

				// if already running, delete and erase it (we dont want to overwrite the environment without properly disposing it)
				if (hwnd_lua_map.contains(wnd)) {
					LuaEnvironment::destroy(hwnd_lua_map[wnd]);
				}

				// now spool up a new one
				auto status = LuaEnvironment::create(path, wnd);
				lua_recent_scripts_add(path);
				if (status.first == nullptr) {
					// failed, we give user some info and thats it
					ConsoleWrite(wnd, status.second.c_str());
				} else {
					// it worked, we can set up associations and sync ui state
					hwnd_lua_map[wnd] = status.first;
					SetButtonState(wnd, true);
				}

				return TRUE;
			}
		case IDC_BUTTON_LUASTOP:
			{
				if (hwnd_lua_map.contains(wnd)) {
					LuaEnvironment::destroy(hwnd_lua_map[wnd]);
					SetButtonState(wnd, false);
				}
				return TRUE;
			}
		case IDC_BUTTON_LUABROWSE:
			{
				auto path = show_persistent_open_dialog("o_lua", wnd, L"*.lua");

				if (path.empty())
				{
					break;
				}

				SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH), wstring_to_string(path).c_str());
				return TRUE;
			}
		case IDC_BUTTON_LUAEDIT:
			{
				CHAR buf[MAX_PATH];
				GetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
				              buf, MAX_PATH);
				if (buf == NULL || buf[0] == '\0')
					/* || strlen(buf)>MAX_PATH*/
					return FALSE;
				// previously , clicking edit with empty path will open current directory in explorer, which is very bad

				ShellExecute(0, 0, buf, 0, 0, SW_SHOW);
				return TRUE;
			}
		case IDC_BUTTON_LUACLEAR:
			if (GetAsyncKeyState(VK_MENU))
			{
				// clear path
				SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH), "");
				return TRUE;
			}

			SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUACONSOLE), "");
			return TRUE;
		}
		return FALSE;
	}

	HWND create_and_show_lua_window(int32_t sw_flags)
	{
		HWND hwnd = CreateDialogParam(app_instance,
		                             MAKEINTRESOURCE(IDD_LUAWINDOW), main_hwnd,
		                             DialogProc,
		                             NULL);
		ShowWindow(hwnd, sw_flags);
		return hwnd;
	}

	void ConsoleWrite(HWND wnd, const char* str)
	{
		HWND console = GetDlgItem(wnd, IDC_TEXTBOX_LUACONSOLE);

		int length = GetWindowTextLength(console);
		if (length >= 0x7000)
		{
			SendMessage(console, EM_SETSEL, 0, length / 2);
			SendMessage(console, EM_REPLACESEL, false, (LPARAM)"");
			length = GetWindowTextLength(console);
		}
		SendMessage(console, EM_SETSEL, length, length);
		SendMessage(console, EM_REPLACESEL, false, (LPARAM)str);
	}

	LRESULT CALLBACK LuaGUIWndProc(HWND wnd, UINT msg, WPARAM wParam,
	                               LPARAM lParam)
	{
		switch (msg)
		{
		case WM_CREATE:
		case WM_DESTROY:
			return 0;
		}
		return DefWindowProc(wnd, msg, wParam, lParam);
	}


	void RecompileNextAll();
	void RecompileNow(ULONG);

	void TraceLogStart(const char* name, BOOL append = FALSE)
	{
		if (TraceLogFile = CreateFile(name, GENERIC_WRITE,
		                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		                              append ? OPEN_ALWAYS : CREATE_ALWAYS,
		                              FILE_ATTRIBUTE_NORMAL |
		                              FILE_FLAG_SEQUENTIAL_SCAN, NULL))
		{
			if (append)
			{
				SetFilePointer(TraceLogFile, 0, NULL, FILE_END);
			}
			enableTraceLog = true;
			if (interpcore == 0)
			{
				RecompileNextAll();
				RecompileNow(PC->addr);
			}
			MENUITEMINFO mii = {};
			mii.cbSize = sizeof(MENUITEMINFO);
			mii.fMask = MIIM_STRING;
			mii.dwTypeData = (LPTSTR)"Stop &Trace Logger";
			SetMenuItemInfo(GetMenu(main_hwnd), ID_TRACELOG,
			                FALSE, &mii);
		}
	}

	void TraceLogStop()
	{
		enableTraceLog = false;
		CloseHandle(TraceLogFile);
		TraceLogFile = NULL;
		MENUITEMINFO mii = {};
		mii.cbSize = sizeof(MENUITEMINFO);
		mii.fMask = MIIM_STRING;
		mii.dwTypeData = (LPTSTR)"Start &Trace Logger";
		SetMenuItemInfo(GetMenu(main_hwnd), ID_TRACELOG,
		                FALSE, &mii);
		TraceLoggingBufFlush();
	}


	LuaEnvironment* GetLuaClass(lua_State* L)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, REG_LUACLASS);
		auto lua = (LuaEnvironment*)lua_topointer(L, -1);
		lua_pop(L, 1);
		return lua;
	}

	void SetLuaClass(lua_State* L, void* lua)
	{
		lua_pushlightuserdata(L, lua);
		lua_setfield(L, LUA_REGISTRYINDEX, REG_LUACLASS);
		//lua_pop(L, 1); //iteresting, it worked before
	}

	int GetErrorMessage(lua_State* L)
	{
		return 1;
	}

	int getn(lua_State* L)
	{
		lua_pushinteger(L, luaL_len(L, -1));
		return 1;
	}


	//lua�̕⏕�֐��Ƃ�
	DWORD LuaCheckIntegerU(lua_State* L, int i = -1)
	{
		return (DWORD)luaL_checknumber(L, i);
	}

	ULONGLONG LuaCheckQWord(lua_State* L, int i)
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

	void LuaPushQword(lua_State* L, ULONGLONG x)
	{
		lua_newtable(L);
		lua_pushinteger(L, 1);
		lua_pushinteger(L, x >> 32);
		lua_settable(L, -3);
		lua_pushinteger(L, 2);
		lua_pushinteger(L, x & 0xFFFFFFFF);
		lua_settable(L, -3);
	}

	int RegisterFunction(lua_State* L, const char* key)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, key);
		if (lua_isnil(L, -1))
		{
			lua_pop(L, 1);
			lua_newtable(L);
			lua_setfield(L, LUA_REGISTRYINDEX, key);
			lua_getfield(L, LUA_REGISTRYINDEX, key);
		}
		int i = luaL_len(L, -1) + 1;
		lua_pushinteger(L, i);
		lua_pushvalue(L, -3); //
		lua_settable(L, -3);
		lua_pop(L, 1);
		return i;
	}

	void UnregisterFunction(lua_State* L, const char* key)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, key);
		if (lua_isnil(L, -1))
		{
			lua_pop(L, 1);
			lua_newtable(L); //�Ƃ肠����
		}
		int n = luaL_len(L, -1);
		for (LUA_INTEGER i = 0; i < n; i++)
		{
			lua_pushinteger(L, 1 + i);
			lua_gettable(L, -2);
			if (lua_rawequal(L, -1, -3))
			{
				lua_pop(L, 1);
				lua_getglobal(L, "table");
				lua_getfield(L, -1, "remove");
				lua_pushvalue(L, -3);
				lua_pushinteger(L, 1 + i);
				lua_call(L, 2, 0);
				lua_pop(L, 2);
				return;
			}
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
		lua_pushfstring(L, "unregisterFunction(%s): not found function",
		                key);
		lua_error(L);
	}


	//������ւ񂩂�֐�
	int ToStringExs(lua_State* L);

	int Print(lua_State* L)
	{
		lua_pushcfunction(L, ToStringExs);
		lua_insert(L, 1);
		lua_call(L, lua_gettop(L) - 1, 1);
		const char* str = lua_tostring(L, 1);
		HWND wnd = GetLuaClass(L)->hwnd;
		ConsoleWrite(wnd, str);
		ConsoleWrite(wnd, "\r\n");
		return 0;
	}

	int StopScript(lua_State* L)
	{
		// FIXME: this isn't the right way to do this, but it's roughly equivalent to the old method
		// it's better to force an error and kill the environment indirectly
		HWND wnd = GetLuaClass(L)->hwnd;
		LuaEnvironment::destroy(hwnd_lua_map[wnd]);
		return 0;
	}

	int ToStringEx(lua_State* L)
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

	int ToStringExInit(lua_State* L)
	{
		lua_newtable(L);
		lua_insert(L, 1);
		ToStringEx(L);
		return 1;
	}

	int ToStringExs(lua_State* L)
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

	int PrintX(lua_State* L)
	{
		int len = lua_gettop(L);
		std::string str("");
		for (int i = 0; i < len; i++)
		{
			lua_pushvalue(L, 1 + i);
			if (lua_type(L, -1) == LUA_TNUMBER)
			{
				int n = LuaCheckIntegerU(L, -1);
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
		ConsoleWrite(GetLuaClass(L)->hwnd, str.append("\r\n").c_str());
		return 1;
	}

	int LuaIntToFloat(lua_State* L)
	{
		ULONG n = luaL_checknumber(L, 1);
		lua_pushnumber(L, *(FLOAT*)&n);
		return 1;
	}

	int LuaIntToDouble(lua_State* L)
	{
		ULONGLONG n = LuaCheckQWord(L, 1);
		lua_pushnumber(L, *(DOUBLE*)&n);
		return 1;
	}

	int LuaFloatToInt(lua_State* L)
	{
		FLOAT n = luaL_checknumber(L, 1);
		lua_pushinteger(L, *(ULONG*)&n);
		return 1;
	}

	int LuaDoubleToInt(lua_State* L)
	{
		DOUBLE n = luaL_checknumber(L, 1);
		LuaPushQword(L, *(ULONGLONG*)&n);
		return 1;
	}

	int LuaQWordToNumber(lua_State* L)
	{
		ULONGLONG n = LuaCheckQWord(L, 1);
		lua_pushnumber(L, n);
		return 1;
	}

	//memory
	unsigned char* const rdramb = (unsigned char*)rdram;
	const unsigned long AddrMask = 0x7FFFFF;

	template <typename T>
	ULONG ToAddr(ULONG addr)
	{
		return sizeof(T) == 4
			       ? addr
			       : sizeof(T) == 2
			       ? addr ^ S16
			       : sizeof(T) == 1
			       ? addr ^ S8
			       : throw"ToAddr: sizeof(T)";
	}

	template <typename T>
	T LoadRDRAMSafe(unsigned long addr)
	{
		return *((T*)(rdramb + ((ToAddr<T>(addr) & AddrMask))));
	}

	template <typename T>
	void StoreRDRAMSafe(unsigned long addr, T value)
	{
		*((T*)(rdramb + ((ToAddr<T>(addr) & AddrMask)))) = value;
	}

	// Read functions

	int LuaReadByteUnsigned(lua_State* L)
	{
		UCHAR value = LoadRDRAMSafe<UCHAR>(luaL_checkinteger(L, 1));
		lua_pushinteger(L, value);
		return 1;
	}

	int LuaReadByteSigned(lua_State* L)
	{
		CHAR value = LoadRDRAMSafe<CHAR>(luaL_checkinteger(L, 1));
		lua_pushinteger(L, value);
		return 1;
	}

	int LuaReadWordUnsigned(lua_State* L)
	{
		USHORT value = LoadRDRAMSafe<USHORT>(luaL_checkinteger(L, 1));
		lua_pushinteger(L, value);
		return 1;
	}

	int LuaReadWordSigned(lua_State* L)
	{
		SHORT value = LoadRDRAMSafe<SHORT>(luaL_checkinteger(L, 1));
		lua_pushinteger(L, value);
		return 1;
	}

	int LuaReadDWorldUnsigned(lua_State* L)
	{
		ULONG value = LoadRDRAMSafe<ULONG>(luaL_checkinteger(L, 1));
		lua_pushinteger(L, value);
		return 1;
	}

	int LuaReadDWordSigned(lua_State* L)
	{
		LONG value = LoadRDRAMSafe<LONG>(luaL_checkinteger(L, 1));
		lua_pushinteger(L, value);
		return 1;
	}

	int LuaReadQWordUnsigned(lua_State* L)
	{
		ULONGLONG value = LoadRDRAMSafe<ULONGLONG>(luaL_checkinteger(L, 1));
		LuaPushQword(L, value);
		return 1;
	}

	int LuaReadQWordSigned(lua_State* L)
	{
		LONGLONG value = LoadRDRAMSafe<LONGLONG>(luaL_checkinteger(L, 1));
		LuaPushQword(L, value);
		return 1;
	}

	int LuaReadFloat(lua_State* L)
	{
		ULONG value = LoadRDRAMSafe<ULONG>(luaL_checkinteger(L, 1));
		lua_pushnumber(L, *(FLOAT*)&value);
		return 1;
	}

	int LuaReadDouble(lua_State* L)
	{
		ULONGLONG value = LoadRDRAMSafe<ULONGLONG>(luaL_checkinteger(L, 1));
		lua_pushnumber(L, *(DOUBLE*)value);
		return 1;
	}

	// Write functions

	int LuaWriteByteUnsigned(lua_State* L)
	{
		StoreRDRAMSafe<UCHAR>(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2));
		return 0;
	}

	int LuaWriteWordUnsigned(lua_State* L)
	{
		StoreRDRAMSafe<USHORT>(luaL_checkinteger(L, 1),
		                       luaL_checkinteger(L, 2));
		return 0;
	}

	int LuaWriteDWordUnsigned(lua_State* L)
	{
		StoreRDRAMSafe<ULONG>(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2));
		return 0;
	}

	int LuaWriteQWordUnsigned(lua_State* L)
	{
		StoreRDRAMSafe<ULONGLONG>(luaL_checkinteger(L, 1), LuaCheckQWord(L, 2));
		return 0;
	}

	int LuaWriteFloatUnsigned(lua_State* L)
	{
		FLOAT f = luaL_checknumber(L, -1);
		StoreRDRAMSafe<ULONG>(luaL_checkinteger(L, 1), *(ULONG*)&f);
		return 0;
	}

	int LuaWriteDoubleUnsigned(lua_State* L)
	{
		DOUBLE f = luaL_checknumber(L, -1);
		StoreRDRAMSafe<ULONGLONG>(luaL_checkinteger(L, 1), *(ULONGLONG*)&f);
		return 0;
	}

	int LuaReadSize(lua_State* L)
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

	int LuaWriteSize(lua_State* L)
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

	// 000000 | 0000 0000 0000 000 | stype(5) = 10101 |001111
	const ULONG BREAKPOINTSYNC_MAGIC_STYPE = 0x15;
	const ULONG BREAKPOINTSYNC_MAGIC = 0x0000000F |
		(BREAKPOINTSYNC_MAGIC_STYPE << 6);

	void PureInterpreterCoreCheck(lua_State* L)
	{
		if (!(dynacore == 0 && interpcore == 1))
		{
			luaL_error(L, "this function works only in pure interpreter core"
			           "(Menu->Option->Settings->General->CPU Core->Pure Interpreter)");
		}
	}

	void InterpreterCoreCheck(lua_State* L, const char* s = "")
	{
		if (dynacore)
		{
			luaL_error(
				L, "this function%s works only in (pure) interpreter core"
				"(Menu->Option->Settings->General->CPU Core->Interpreter or Pure Interpreter)",
				s);
		}
	}

	void Recompile(ULONG);

	unsigned long PAddr(unsigned long addr)
	{
		if (addr >= 0x80000000 && addr < 0xC0000000)
		{
			return addr;
		} else
		{
			return virtual_to_physical_address(addr, 2);
		}
	}

	void RecompileNow(ULONG addr)
	{
		//NOTCOMPILED���B�����ɃR���p�C�����ʂ�ops�Ȃǂ��~�������Ɏg��
		if ((addr >> 16) == 0xa400)
			recompile_block((long*)SP_DMEM, blocks[0xa4000000 >> 12], addr);
		else
		{
			unsigned long paddr = PAddr(addr);
			if (paddr)
			{
				if ((paddr & 0x1FFFFFFF) >= 0x10000000)
				{
					recompile_block(
						(long*)rom + ((((paddr - (addr - blocks[addr >> 12]->
							start)) & 0x1FFFFFFF) - 0x10000000) >> 2),
						blocks[addr >> 12], addr);
				} else
				{
					recompile_block(
						(long*)(rdram + (((paddr - (addr - blocks[addr >> 12]->
							start)) & 0x1FFFFFFF) >> 2)),
						blocks[addr >> 12], addr);
				}
			}
		}
	}

	void Recompile(ULONG addr)
	{
		//jump_to���
		//���ʂɃ��R���p�C���������Ƃ��͂���ł���
		ULONG block = addr >> 12;
		ULONG paddr = PAddr(addr);
		if (!blocks[block])
		{
			blocks[block] = (precomp_block*)malloc(sizeof(precomp_block));
			actual = blocks[block];
			blocks[block]->code = NULL;
			blocks[block]->block = NULL;
			blocks[block]->jumps_table = NULL;
		}
		blocks[block]->start = addr & ~0xFFF;
		blocks[block]->end = (addr & ~0xFFF) + 0x1000;
		init_block(
			(long*)(rdram + (((paddr - (addr - blocks[block]->start)) &
				0x1FFFFFFF) >> 2)),
			blocks[block]);
	}

	void RecompileNext(ULONG addr)
	{
		//jump_to�̎�(�u���b�N��܂������W�����v)�Ƀ`�F�b�N�����H
		//������u���b�N�𒼂��ɏC�����������ȊO�͂���ł���
		invalid_code[addr >> 12] = 1;
	}

	void RecompileNextAll()
	{
		memset(invalid_code, 1, 0x100000);
	}

	template <typename T>
	void PushT(lua_State* L, T value)
	{
		LuaPushIntU(L, value);
	}

	template <>
	void PushT<ULONGLONG>(lua_State* L, ULONGLONG value)
	{
		LuaPushQword(L, value);
	}

	template <typename T, void(**readmem_func)()>
	int ReadMemT(lua_State* L)
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
	T CheckT(lua_State* L, int i)
	{
		return LuaCheckIntegerU(L, i);
	}

	template <>
	ULONGLONG CheckT<ULONGLONG>(lua_State* L, int i)
	{
		return LuaCheckQWord(L, i);
	}

	template <typename T, void(**writemem_func)(), T& g_T>
	int WriteMemT(lua_State* L)
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

	int GetSystemMetricsLua(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		lua_pushinteger(L, GetSystemMetrics(luaL_checkinteger(L, 1)));

		return 1;
	}

	int IsMainWindowInForeground(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		lua_pushboolean(
			L, GetForegroundWindow() == main_hwnd || GetActiveWindow() ==
			main_hwnd);
		return 1;
	}

#pragma region Legacy GDI & GDI+
	//Gui
	//�v���O�C���ɕ�����Ă邩�玩�R�ɏo���Ȃ��H
	//�Ƃ������E�B���h�E�ɒ��ڏ����Ă���
	//�Ƃ肠������������E�B���h�E�ɒ�������
	typedef struct COLORNAME
	{
		const char* name;
		COLORREF value;
	} COLORNAME;

	const int hexTable[256] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
		0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0,
	};
	const COLORNAME colors[] = {
		{"white", 0xFFFFFFFF},
		{"black", 0xFF000000},
		{"clear", 0x00000000},
		{"gray", 0xFF808080},
		{"red", 0xFF0000FF},
		{"orange", 0xFF0080FF},
		{"yellow", 0xFF00FFFF},
		{"chartreuse", 0xFF00FF80},
		{"green", 0xFF00FF00},
		{"teal", 0xFF80FF00},
		{"cyan", 0xFFFFFF00},
		{"blue", 0xFFFF0000},
		{"purple", 0xFFFF0080},
		{NULL}
	};

	COLORREF StrToColorA(const char* s, bool alpha = false, COLORREF def = 0)
	{
		if (s[0] == '#')
		{
			int l = lstrlen(s);
			if (l == 4)
			{
				return (hexTable[s[1]] * 0x10 + hexTable[s[1]]) |
					((hexTable[s[2]] * 0x10 + hexTable[s[2]]) << 8) |
					((hexTable[s[3]] * 0x10 + hexTable[s[3]]) << 16) |
					(alpha ? 0xFF000000 : 0);
			} else if (alpha && l == 5)
			{
				return (hexTable[s[1]] * 0x10 + hexTable[s[1]]) |
					((hexTable[s[2]] * 0x10 + hexTable[s[2]]) << 8) |
					((hexTable[s[3]] * 0x10 + hexTable[s[3]]) << 16) |
					((hexTable[s[4]] * 0x10 + hexTable[s[4]]) << 24);
			} else if (l == 7)
			{
				return (hexTable[s[1]] * 0x10 + hexTable[s[2]]) |
					((hexTable[s[3]] * 0x10 + hexTable[s[4]]) << 8) |
					((hexTable[s[5]] * 0x10 + hexTable[s[6]]) << 16) |
					(alpha ? 0xFF000000 : 0);
			} else if (alpha && l == 9)
			{
				return (hexTable[s[1]] * 0x10 + hexTable[s[2]]) |
					((hexTable[s[3]] * 0x10 + hexTable[s[4]]) << 8) |
					((hexTable[s[5]] * 0x10 + hexTable[s[6]]) << 16) |
					((hexTable[s[7]] * 0x10 + hexTable[s[8]]) << 24);
			}
		} else
		{
			const COLORNAME* p = colors;
			do
			{
				if (lstrcmpi(s, p->name) == 0)
				{
					return (alpha ? 0xFFFFFFFF : 0xFFFFFF) & p->value;
				}
			}
			while ((++p)->name);
		}
		return (alpha ? 0xFF000000 : 0x00000000) | def;
	}

	COLORREF StrToColor(const char* s, bool alpha = false, COLORREF def = 0)
	{
		COLORREF c = StrToColorA(s, alpha, def);
		/*
			if((c&0xFFFFFF) == LUADC_BG_COLOR) {
				return LUADC_BG_COLOR_A|(alpha?0xFF000000:0);
			}else {
				return c;
			}
		*/
		return c;
	}

	//wgui
	int SetBrush(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		const char* s = lua_tostring(L, 1);
		if (lstrcmpi(s, "null") == 0)
			lua->setBrush((HBRUSH)GetStockObject(NULL_BRUSH));
		else
			lua->setBrush(::CreateSolidBrush(StrToColor(s)));
		return 0;
	}

	int SetPen(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		const char* s = lua_tostring(L, 1);
		if (lstrcmpi(s, "null") == 0)
			lua->setPen((HPEN)GetStockObject(NULL_PEN));
		else
			// optional pen width defaults to 1
			lua->setPen(
				::CreatePen(PS_SOLID, luaL_optnumber(L, 2, 1), StrToColor(s)));
		return 0;
	}

	int SetTextColor(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		lua->setTextColor(StrToColor(lua_tostring(L, 1)));
		return 0;
	}

	int SetBackgroundColor(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		const char* s = lua_tostring(L, 1);
		if (lstrcmpi(s, "null") == 0)
			lua->setBackgroundColor(0, TRANSPARENT);
		else
			lua->setBackgroundColor(StrToColor(s));
		return 0;
	}

	int SetFont(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		LOGFONT font = {0};

		// set the size of the font
		font.lfHeight = -MulDiv(luaL_checknumber(L, 1),
		                        GetDeviceCaps(lua->dc, LOGPIXELSY), 72);
		lstrcpyn(font.lfFaceName, luaL_optstring(L, 2, "MS Gothic"),
		         LF_FACESIZE);
		font.lfCharSet = DEFAULT_CHARSET;
		const char* style = luaL_optstring(L, 3, "");
		for (const char* p = style; *p; p++)
		{
			switch (*p)
			{
			case 'b': font.lfWeight = FW_BOLD;
				break;
			case 'i': font.lfItalic = TRUE;
				break;
			case 'u': font.lfUnderline = TRUE;
				break;
			case 's': font.lfStrikeOut = TRUE;
				break;
			case 'a': font.lfQuality = ANTIALIASED_QUALITY;
				break;
			}
		}
		GetLuaClass(L)->setFont(CreateFontIndirect(&font));
		return 0;
	}

	int LuaTextOut(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		lua->selectTextColor();
		lua->selectBackgroundColor();
		lua->selectFont();

		int x = luaL_checknumber(L, 1);
		int y = luaL_checknumber(L, 2);
		const char* text = lua_tostring(L, 3);

		::TextOut(lua->dc, x, y, text, lstrlen(text));
		return 0;
	}

	bool GetRectLua(lua_State* L, int idx, RECT* rect)
	{
		switch (lua_type(L, idx))
		{
		case LUA_TTABLE:
			lua_getfield(L, idx, "l");
			rect->left = lua_tointeger(L, -1);
			lua_pop(L, 1);
			lua_getfield(L, idx, "t");
			rect->top = lua_tointeger(L, -1);
			lua_pop(L, 1);
			lua_getfield(L, idx, "r");
			if (lua_isnil(L, -1))
			{
				lua_pop(L, 1);
				lua_getfield(L, idx, "w");
				if (lua_isnil(L, -1))
				{
					return false;
				}
				rect->right = rect->left + lua_tointeger(L, -1);
				lua_pop(L, 1);
				lua_getfield(L, idx, "h");
				rect->bottom = rect->top + lua_tointeger(L, -1);
				lua_pop(L, 1);
			} else
			{
				rect->right = lua_tointeger(L, -1);
				lua_pop(L, 1);
				lua_getfield(L, idx, "b");
				rect->bottom = lua_tointeger(L, -1);
				lua_pop(L, 1);
			}
			return true;
		default:
			return false;
		}
	}

	int GetTextExtent(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		const char* string = luaL_checkstring(L, 1);
		SIZE size = {0};
		GetTextExtentPoint32(lua->dc, string, strlen(string), &size);
		lua_newtable(L);
		lua_pushinteger(L, size.cx);
		lua_setfield(L, -2, "width");
		lua_pushinteger(L, size.cy);
		lua_setfield(L, -2, "height");
		return 1;
	}

	int LuaDrawText(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		lua->selectTextColor();
		lua->selectBackgroundColor();
		lua->selectFont();
		RECT rect = {0};
		UINT format = DT_NOPREFIX | DT_WORDBREAK;
		if (!GetRectLua(L, 2, &rect))
		{
			format |= DT_NOCLIP;
		}
		if (!lua_isnil(L, 3))
		{
			const char* p = lua_tostring(L, 3);
			for (; p && *p; p++)
			{
				switch (*p)
				{
				case 'l': format |= DT_LEFT;
					break;
				case 'r': format |= DT_RIGHT;
					break;
				case 't': format |= DT_TOP;
					break;
				case 'b': format |= DT_BOTTOM;
					break;
				case 'c': format |= DT_CENTER;
					break;
				case 'v': format |= DT_VCENTER;
					break;
				case 'e': format |= DT_WORD_ELLIPSIS;
					break;
				case 's': format |= DT_SINGLELINE;
					break;
				case 'n': format &= ~DT_WORDBREAK;
					break;
				}
			}
		}
		::DrawText(lua->dc, lua_tostring(L, 1), -1, &rect, format);
		return 0;
	}

	int LuaDrawTextAlt(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		lua->selectTextColor();
		lua->selectBackgroundColor();
		lua->selectFont();

		RECT rect = {0};
		LPSTR string = (LPSTR)lua_tostring(L, 1);
		UINT format = luaL_checkinteger(L, 2);
		rect.left = luaL_checkinteger(L, 3);
		rect.top = luaL_checkinteger(L, 4);
		rect.right = luaL_checkinteger(L, 5);
		rect.bottom = luaL_checkinteger(L, 6);

		DrawTextEx(lua->dc, string, -1, &rect, format, NULL);
		return 0;
	}

	int DrawRect(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		int left = luaL_checknumber(L, 1);
		int top = luaL_checknumber(L, 2);
		int right = luaL_checknumber(L, 3);
		int bottom = luaL_checknumber(L, 4);
		int cornerW = luaL_optnumber(L, 5, 0);
		int cornerH = luaL_optnumber(L, 6, 0);

		lua->selectBrush();
		lua->selectPen();
		::RoundRect(lua->dc, left, top, right, bottom, cornerW, cornerH);
		return 0;
	}

	int LuaLoadImage(lua_State* L)
	{
		const char* path = luaL_checkstring(L, 1);
		int output_size = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
		wchar_t* pathw = (wchar_t*)malloc(output_size * sizeof(wchar_t));
		int size = MultiByteToWideChar(CP_ACP, 0, path, -1, pathw, output_size);

		printf("LoadImage: %ws\n", pathw);
		Gdiplus::Bitmap* img = new Gdiplus::Bitmap(pathw);
		free(pathw);

		if (img->GetLastStatus())
		{
			luaL_error(L, "Couldn't find image '%s'", path);
			return 0;
		}
		image_pool.push_back(img);
		lua_pushinteger(L, image_pool.size()); //return the identifier (index+1)
		return 1;
	}

	int DeleteImage(lua_State* L)
	{
		// Clears one or all images from imagePool
		unsigned int clearIndex = luaL_checkinteger(L, 1);
		if (clearIndex == 0)
		{
			// If clearIndex is 0, clear all images
			printf("Deleting all images\n");
			for (auto x : image_pool)
			{
				delete x;
			}
			image_pool.clear();
		} else
		{
			// If clear index is not 0, clear 1 image
			if (clearIndex <= image_pool.size())
			{
				printf("Deleting image index %d (%d in lua)\n", clearIndex - 1,
				       clearIndex);
				delete image_pool[clearIndex - 1];
				image_pool.erase(image_pool.begin() + clearIndex - 1);
			} else
			{
				// Error if the image doesn't exist
				luaL_error(L, "Argument #1: Invalid image index");
				return 0;
			}
		}
		return 0;
	}

	int DrawImage(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		unsigned int imgIndex = luaL_checkinteger(L, 1) - 1; // because lua

		// Error if the image doesn't exist
		if (imgIndex > image_pool.size() - 1)
		{
			luaL_error(L, "Argument #1: Invalid image index");
			return 0;
		}

		// Gets the number of arguments
		unsigned int args = lua_gettop(L);

		Gdiplus::Graphics gfx(lua->dc);
		Gdiplus::Bitmap* img = image_pool[imgIndex];

		// Original DrawImage
		if (args == 3)
		{
			int x = luaL_checknumber(L, 2);
			int y = luaL_checknumber(L, 3);

			gfx.DrawImage(img, x, y); // Gdiplus::Image *image, int x, int y
			return 0;
		} else if (args == 4)
		{
			int x = luaL_checknumber(L, 2);
			int y = luaL_checknumber(L, 3);
			float scale = luaL_checknumber(L, 4);
			if (scale == 0) return 0;

			// Create a Rect at x and y and scale the image's width and height
			Gdiplus::Rect dest(x, y, img->GetWidth() * scale,
			                   img->GetHeight() * scale);

			gfx.DrawImage(img, dest);
			// Gdiplus::Image *image, const Gdiplus::Rect &rect
			return 0;
		}
		// In original DrawImage
		else if (args == 5)
		{
			int x = luaL_checknumber(L, 2);
			int y = luaL_checknumber(L, 3);
			int w = luaL_checknumber(L, 4);
			int h = luaL_checknumber(L, 5);
			if (w == 0 or h == 0) return 0;

			Gdiplus::Rect dest(x, y, w, h);

			gfx.DrawImage(img, dest);
			// Gdiplus::Image *image, const Gdiplus::Rect &rect
			return 0;
		} else if (args == 10)
		{
			int x = luaL_checknumber(L, 2);
			int y = luaL_checknumber(L, 3);
			int w = luaL_checknumber(L, 4);
			int h = luaL_checknumber(L, 5);
			int srcx = luaL_checknumber(L, 6);
			int srcy = luaL_checknumber(L, 7);
			int srcw = luaL_checknumber(L, 8);
			int srch = luaL_checknumber(L, 9);
			float rotate = luaL_checknumber(L, 10);
			if (w == 0 or h == 0 or srcw == 0 or srch == 0) return 0;
			bool shouldrotate = ((int)rotate % 360) != 0;
			// Only rotate if the angle isn't a multiple of 360 Modulo only works with int

			Gdiplus::Rect dest(x, y, w, h);

			// Rotate
			if (shouldrotate)
			{
				Gdiplus::PointF center(x + (w / 2), y + (h / 2));
				// The center of dest
				Gdiplus::Matrix matrix;
				matrix.RotateAt(rotate, center);
				// rotate "rotate" number of degrees around "center"
				gfx.SetTransform(&matrix);
			}

			gfx.DrawImage(img, dest, srcx, srcy, srcw, srch,
			              Gdiplus::UnitPixel);
			// Gdiplus::Image *image, const Gdiplus::Rect &destRect, int srcx, int srcy, int srcheight, Gdiplus::srcUnit

			if (shouldrotate) gfx.ResetTransform();
			return 0;
		}
		luaL_error(L, "Incorrect number of arguments");
		return 0;
	}

	int Screenshot(lua_State* L)
	{
		CaptureScreen((char*)luaL_checkstring(L, 1));
		return 0;
	}

	int LoadScreen(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		if (!LoadScreenInitialized)
		{
			luaL_error(
				L, "LoadScreen not initialized! Something has gone wrong.");
			return 0;
		}
		// set the selected object of hsrcDC to hbwindow
		SelectObject(hsrcDC, hbitmap);
		// copy from the window device context to the bitmap device context
		BitBlt(hsrcDC, 0, 0, windowSize.width, windowSize.height, hwindowDC, 0,
		       0, SRCCOPY);

		Gdiplus::Bitmap* out = new Gdiplus::Bitmap(hbitmap, nullptr);

		image_pool.push_back(out);

		lua_pushinteger(L, image_pool.size());

		return 1;
	}

	int LoadScreenReset(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		LoadScreenInit();
		return 0;
	}

	int GetImageInfo(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		unsigned int imgIndex = luaL_checkinteger(L, 1) - 1;

		if (imgIndex > image_pool.size() - 1)
		{
			luaL_error(L, "Argument #1: Invalid image index");
			return 0;
		}

		Gdiplus::Bitmap* img = image_pool[imgIndex];

		lua_newtable(L);
		lua_pushinteger(L, img->GetWidth());
		lua_setfield(L, -2, "width");
		lua_pushinteger(L, img->GetHeight());
		lua_setfield(L, -2, "height");

		return 1;
	}

	//1st arg is table of points
	//2nd arg is color #xxxxxxxx
	int FillPolygonAlpha(lua_State* L)
	{
		//Get lua instance stored in script class
		LuaEnvironment* lua = GetLuaClass(L);


		//stack should look like
		//--------
		//2: color string
		//--------
		//1: table of points
		//--------
		//assert that first argument is table
		luaL_checktype(L, 1, LUA_TTABLE);

		int n = luaL_len(L, 1); //length of the table, doesnt modify stack
		if (n > 255)
		{
			//hard cap, the vector can handle more but dont try
			lua_pushfstring(L, "wgui.polygon: too many points (%d > %d)",
			                n, 255);
			return lua_error(L);
		}

		std::vector<Gdiplus::PointF> pts(n); //list of points that make the poly

		//do n times
		for (int i = 0; i < n; i++)
		{
			//push current index +1 because lua
			lua_pushinteger(L, i + 1);
			//get index i+1 from table at the bottom of the stack (index 1 is bottom, 2 is next etc, -1 is top)
			//pops the index and places the element inside table on top, which again is a table [x,y]
			lua_gettable(L, 1);
			//make sure its a table
			luaL_checktype(L, -1, LUA_TTABLE);
			//push '1'
			lua_pushinteger(L, 1);
			//get index 1 from table that is second from top, because '1' is on top right now
			//then remove '1' and put table contents, its the X coord
			lua_gettable(L, -2);
			//read it
			pts[i].X = lua_tointeger(L, -1);
			//remove X coord
			lua_pop(L, 1);
			//push '2'
			lua_pushinteger(L, 2);
			//same thing
			lua_gettable(L, -2);
			pts[i].Y = lua_tointeger(L, -1);
			lua_pop(L, 2);
			//now stack again has only table at the bottom and color string on top, repeat
		}

		Gdiplus::Graphics gfx(lua->dc);
		Gdiplus::SolidBrush brush(Gdiplus::Color(
			luaL_checkinteger(L, 2), luaL_checkinteger(L, 3),
			luaL_checkinteger(L, 4), luaL_checkinteger(L, 5)));
		gfx.FillPolygon(&brush, pts.data(), n);

		return 0;
	}


	int FillEllipseAlpha(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		int x = luaL_checknumber(L, 1);
		int y = luaL_checknumber(L, 2);
		int w = luaL_checknumber(L, 3);
		int h = luaL_checknumber(L, 4);
		const char* col = luaL_checkstring(L, 5); //color string

		Gdiplus::Graphics gfx(lua->dc);
		Gdiplus::SolidBrush brush(Gdiplus::Color(StrToColorA(col, true)));

		gfx.FillEllipse(&brush, x, y, w, h);

		return 0;
	}

	int FillRectAlpha(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		int x = luaL_checknumber(L, 1);
		int y = luaL_checknumber(L, 2);
		int w = luaL_checknumber(L, 3);
		int h = luaL_checknumber(L, 4);
		const char* col = luaL_checkstring(L, 5); //color string

		Gdiplus::Graphics gfx(lua->dc);
		Gdiplus::SolidBrush brush(Gdiplus::Color(StrToColorA(col, true)));

		gfx.FillRectangle(&brush, x, y, w, h);

		return 0;
	}

	int FillRect(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		COLORREF color = RGB(
			luaL_checknumber(L, 5),
			luaL_checknumber(L, 6),
			luaL_checknumber(L, 7)
		);
		COLORREF colorold = SetBkColor(lua->dc, color);
		RECT rect;
		rect.left = luaL_checknumber(L, 1);
		rect.top = luaL_checknumber(L, 2);
		rect.right = luaL_checknumber(L, 3);
		rect.bottom = luaL_checknumber(L, 4);
		ExtTextOut(lua->dc, 0, 0, ETO_OPAQUE, &rect, "", 0, 0);
		SetBkColor(lua->dc, colorold);
		return 0;
	}

	int DrawEllipse(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		lua->selectBrush();
		lua->selectPen();

		int left = luaL_checknumber(L, 1);
		int top = luaL_checknumber(L, 2);
		int right = luaL_checknumber(L, 3);
		int bottom = luaL_checknumber(L, 4);

		::Ellipse(lua->dc, left, top, right, bottom);
		return 0;
	}

	int DrawPolygon(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		POINT p[0x100];
		luaL_checktype(L, 1, LUA_TTABLE);
		int n = luaL_len(L, 1);
		if (n >= sizeof(p) / sizeof(p[0]))
		{
			lua_pushfstring(L, "wgui.polygon: too many points (%d < %d)",
			                sizeof(p) / sizeof(p[0]), n);
			return lua_error(L);
		}
		for (int i = 0; i < n; i++)
		{
			lua_pushinteger(L, i + 1);
			lua_gettable(L, 1);
			luaL_checktype(L, -1, LUA_TTABLE);
			lua_pushinteger(L, 1);
			lua_gettable(L, -2);
			p[i].x = lua_tointeger(L, -1);
			lua_pop(L, 1);
			lua_pushinteger(L, 2);
			lua_gettable(L, -2);
			p[i].y = lua_tointeger(L, -1);
			lua_pop(L, 2);
		}
		lua->selectBrush();
		lua->selectPen();
		::Polygon(lua->dc, p, n);
		return 0;
	}

	int DrawLine(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		lua->selectPen();
		::MoveToEx(lua->dc, luaL_checknumber(L, 1), luaL_checknumber(L, 2),
		           NULL);
		::LineTo(lua->dc, luaL_checknumber(L, 3), luaL_checknumber(L, 4));
		return 0;
	}

	int SetClip(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		auto rgn = CreateRectRgn(luaL_checkinteger(L, 1),
		                         luaL_checkinteger(L, 2),
		                         luaL_checkinteger(L, 1) +
		                         luaL_checkinteger(L, 3),
		                         luaL_checkinteger(L, 2) + luaL_checkinteger(
			                         L, 4));
		SelectClipRgn(lua->dc, rgn);
		DeleteObject(rgn);
		return 0;
	}

	int ResetClip(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		SelectClipRgn(lua->dc, NULL);
		return 0;
	}
#pragma endregion


	uint32_t get_hash(D2D1::ColorF color)
	{
		ASSERT(color.r >= 0.0f && color.r <= 1.0f);
		ASSERT(color.g >= 0.0f && color.g <= 1.0f);
		ASSERT(color.b >= 0.0f && color.b <= 1.0f);
		ASSERT(color.a >= 0.0f && color.a <= 1.0f);

		uint32_t r = static_cast<uint32_t>(color.r * 255.0f);
		uint32_t g = static_cast<uint32_t>(color.g * 255.0f);
		uint32_t b = static_cast<uint32_t>(color.b * 255.0f);
		uint32_t a = static_cast<uint32_t>(color.a * 255.0f);

		return (r << 24) | (g << 16) | (b << 8) | a;
	}

	ID2D1SolidColorBrush* d2d_get_cached_brush(LuaEnvironment* lua,
	                                           D2D1::ColorF color)
	{
		uint32_t key = get_hash(color);

		if (!lua->d2d_brush_cache.contains(key))
		{
			printf("Creating ID2D1SolidColorBrush (%f, %f, %f, %f) = %d\n",
			       color.r, color.g, color.b, color.a, key);

			ID2D1SolidColorBrush* brush;
			lua->d2d_render_target_stack.top()->CreateSolidColorBrush(
				color,
				&brush
			);
			lua->d2d_brush_cache[key] = brush;
		}

		return lua->d2d_brush_cache[key];
	}

#define D2D_GET_RECT(L, idx) D2D1::RectF( \
	luaL_checknumber(L, idx), \
	luaL_checknumber(L, idx + 1), \
	luaL_checknumber(L, idx + 2), \
	luaL_checknumber(L, idx + 3) \
)

#define D2D_GET_COLOR(L, idx) D2D1::ColorF( \
	luaL_checknumber(L, idx), \
	luaL_checknumber(L, idx + 1), \
	luaL_checknumber(L, idx + 2), \
	luaL_checknumber(L, idx + 3) \
)

#define D2D_GET_POINT(L, idx) D2D1_POINT_2F{ \
	.x = (float)luaL_checknumber(L, idx), \
	.y = (float)luaL_checknumber(L, idx + 1) \
}

#define D2D_GET_ELLIPSE(L, idx) D2D1_ELLIPSE{ \
	.point = D2D_GET_POINT(L, idx), \
	.radiusX = (float)luaL_checknumber(L, idx + 2), \
	.radiusY = (float)luaL_checknumber(L, idx + 3) \
}

#define D2D_GET_ROUNDED_RECT(L, idx) D2D1_ROUNDED_RECT( \
	D2D_GET_RECT(L, idx), \
	luaL_checknumber(L, idx + 5), \
	luaL_checknumber(L, idx + 6) \
)

	int LuaD2DFillRectangle(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_RECT_F rectangle = D2D_GET_RECT(L, 1);
		D2D1::ColorF color = D2D_GET_COLOR(L, 5);

		ID2D1SolidColorBrush* brush = d2d_get_cached_brush(lua, color);

		lua->d2d_render_target_stack.top()->FillRectangle(&rectangle, brush);

		return 0;
	}

	int LuaD2DDrawRectangle(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_RECT_F rectangle = D2D_GET_RECT(L, 1);
		D2D1::ColorF color = D2D_GET_COLOR(L, 5);
		float thickness = luaL_checknumber(L, 9);

		ID2D1SolidColorBrush* brush = d2d_get_cached_brush(lua, color);

		lua->d2d_render_target_stack.top()->DrawRectangle(&rectangle, brush, thickness);

		return 0;
	}

	int LuaD2DFillEllipse(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_ELLIPSE ellipse = D2D_GET_ELLIPSE(L, 1);
		D2D1::ColorF color = D2D_GET_COLOR(L, 5);

		ID2D1SolidColorBrush* brush = d2d_get_cached_brush(lua, color);

		lua->d2d_render_target_stack.top()->FillEllipse(&ellipse, brush);

		return 0;
	}

	int LuaD2DDrawEllipse(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_ELLIPSE ellipse = D2D_GET_ELLIPSE(L, 1);
		D2D1::ColorF color = D2D_GET_COLOR(L, 5);
		float thickness = luaL_checknumber(L, 9);

		ID2D1SolidColorBrush* brush = d2d_get_cached_brush(lua, color);

		lua->d2d_render_target_stack.top()->DrawEllipse(&ellipse, brush, thickness);

		return 0;
	}

	int LuaD2DDrawLine(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_POINT_2F point_a = D2D_GET_POINT(L, 1);
		D2D1_POINT_2F point_b = D2D_GET_POINT(L, 3);
		D2D1::ColorF color = D2D_GET_COLOR(L, 5);
		float thickness = luaL_checknumber(L, 9);

		ID2D1SolidColorBrush* brush = d2d_get_cached_brush(lua, color);

		lua->d2d_render_target_stack.top()->DrawLine(point_a, point_b, brush, thickness);

		return 0;
	}


int LuaD2DDrawText(lua_State* L)
{
	LuaEnvironment* lua = GetLuaClass(L);

	D2D1_RECT_F rectangle = D2D_GET_RECT(L, 1);
	auto color = D2D_GET_COLOR(L, 5);

	// we would get tons of near-misses otherwise
	rectangle.left = (int)rectangle.left;
	rectangle.top = (int)rectangle.top;
	rectangle.right = (int)rectangle.right;
	rectangle.bottom = (int)rectangle.bottom;
	auto text = std::string(luaL_checkstring(L, 9));
	auto font_name = std::string(luaL_checkstring(L, 10));
	auto font_size = static_cast<float>(luaL_checknumber(L, 11));
	auto font_weight = static_cast<int>(luaL_checknumber(L, 12));
	auto font_style = static_cast<int>(luaL_checkinteger(L, 13));
	auto horizontal_alignment = static_cast<int>(luaL_checkinteger(L, 14));
	auto vertical_alignment = static_cast<int>(luaL_checkinteger(L, 15));

	ID2D1SolidColorBrush* brush = d2d_get_cached_brush(lua, color);

	int options = luaL_checkinteger(L, 16);


	IDWriteTextFormat* text_format;

	lua->dw_factory->CreateTextFormat(
		string_to_wstring(font_name).c_str(),
		nullptr,
		static_cast<DWRITE_FONT_WEIGHT>(font_weight),
		static_cast<DWRITE_FONT_STYLE>(font_style),
		DWRITE_FONT_STRETCH_NORMAL,
		font_size,
		L"",
		&text_format
	);

	text_format->SetTextAlignment(
		static_cast<DWRITE_TEXT_ALIGNMENT>(horizontal_alignment));
	text_format->SetParagraphAlignment(
		static_cast<DWRITE_PARAGRAPH_ALIGNMENT>(vertical_alignment));

	IDWriteTextLayout* text_layout;

	auto wtext = string_to_wstring(text);
	lua->dw_factory->CreateTextLayout(wtext.c_str(), wtext.length(),
	                                  text_format,
	                                  rectangle.right - rectangle.left,
	                                  rectangle.bottom - rectangle.top,
	                                  &text_layout);

	lua->d2d_render_target_stack.top()->DrawTextLayout({
			.x = rectangle.left,
			.y = rectangle.top,
		}, text_layout, brush,
		static_cast<
			D2D1_DRAW_TEXT_OPTIONS>(
			options));

	text_format->Release();
	text_layout->Release();
	return 0;
}

	int LuaD2DSetTextAntialiasMode(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		float mode = luaL_checkinteger(L, 1);
		lua->d2d_render_target_stack.top()->SetTextAntialiasMode(
			(D2D1_TEXT_ANTIALIAS_MODE)mode);
		return 0;
	}

	int LuaD2DSetAntialiasMode(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		float mode = luaL_checkinteger(L, 1);
		lua->d2d_render_target_stack.top()->SetAntialiasMode((D2D1_ANTIALIAS_MODE)mode);
		return 0;
	}

	int LuaD2DGetTextSize(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		std::wstring text = string_to_wstring(std::string(luaL_checkstring(L, 1)));
		std::string font_name = std::string(luaL_checkstring(L, 2));
		float font_size = luaL_checknumber(L, 3);
		float max_width = luaL_checknumber(L, 4);
		float max_height = luaL_checknumber(L, 5);

		IDWriteTextFormat* text_format;

		lua->dw_factory->CreateTextFormat(
			string_to_wstring(font_name).c_str(),
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			font_size,
			L"",
			&text_format
		);

		IDWriteTextLayout* text_layout;

		lua->dw_factory->CreateTextLayout(text.c_str(), text.length(),
		                                  text_format, max_width, max_height,
		                                  &text_layout);

		DWRITE_TEXT_METRICS text_metrics;
		text_layout->GetMetrics(&text_metrics);

		lua_newtable(L);
		lua_pushinteger(L, text_metrics.widthIncludingTrailingWhitespace);
		lua_setfield(L, -2, "width");
		lua_pushinteger(L, text_metrics.height);
		lua_setfield(L, -2, "height");

		text_format->Release();
		text_layout->Release();

		return 1;
	}

	int LuaD2DPushClip(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_RECT_F rectangle = D2D_GET_RECT(L, 1);

		lua->d2d_render_target_stack.top()->PushAxisAlignedClip(
			rectangle,
			D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
		);

		return 0;
	}

	int LuaD2DPopClip(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		lua->d2d_render_target_stack.top()->PopAxisAlignedClip();

		return 0;
	}

	int LuaD2DFillRoundedRectangle(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_ROUNDED_RECT rounded_rectangle = D2D_GET_ROUNDED_RECT(L, 1);
		D2D1::ColorF color = D2D_GET_COLOR(L, 7);

		ID2D1SolidColorBrush* brush = d2d_get_cached_brush(lua, color);

		lua->d2d_render_target_stack.top()->FillRoundedRectangle(&rounded_rectangle, brush);

		return 0;
	}

	int LuaD2DDrawRoundedRectangle(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_ROUNDED_RECT rounded_rectangle = D2D_GET_ROUNDED_RECT(L, 1);
		D2D1::ColorF color = D2D_GET_COLOR(L, 7);
		float thickness = luaL_checknumber(L, 11);

		ID2D1SolidColorBrush* brush = d2d_get_cached_brush(lua, color);

		lua->d2d_render_target_stack.top()->DrawRoundedRectangle(
			&rounded_rectangle, brush, thickness);

		return 0;
	}

	int LuaD2DLoadImage(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		std::string path(luaL_checkstring(L, 1));
		std::string identifier(luaL_checkstring(L, 2));

		if (lua->d2d_bitmap_cache.contains(identifier))
		{
			lua->d2d_bitmap_cache[identifier]->Release();
		}

		IWICImagingFactory* pIWICFactory = NULL;
		IWICBitmapDecoder* pDecoder = NULL;
		IWICBitmapFrameDecode* pSource = NULL;
		IWICFormatConverter* pConverter = NULL;
		ID2D1Bitmap* bmp = NULL;

		CoCreateInstance(
			CLSID_WICImagingFactory,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pIWICFactory)
		);

		HRESULT hr = pIWICFactory->CreateDecoderFromFilename(
			string_to_wstring(path).c_str(),
			NULL,
			GENERIC_READ,
			WICDecodeMetadataCacheOnLoad,
			&pDecoder
		);

		if (!SUCCEEDED(hr))
		{
			printf("D2D image fail HRESULT %d\n", hr);
			pIWICFactory->Release();
			return 0;
		}

		pIWICFactory->CreateFormatConverter(&pConverter);
		pDecoder->GetFrame(0, &pSource);
		pConverter->Initialize(
			pSource,
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			NULL,
			0.0f,
			WICBitmapPaletteTypeMedianCut
		);

		lua->d2d_render_target_stack.top()->CreateBitmapFromWicBitmap(
			pConverter,
			NULL,
			&bmp
		);

		pIWICFactory->Release();
		pDecoder->Release();
		pSource->Release();
		pConverter->Release();

		lua->d2d_bitmap_cache[identifier] = bmp;

		return 0;
	}

	int LuaD2DFreeImage(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		std::string identifier(luaL_checkstring(L, 1));
		lua->d2d_bitmap_cache[identifier]->Release();
		lua->d2d_bitmap_cache.erase(identifier);
		return 0;
	}

	int LuaD2DDrawImage(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		D2D1_RECT_F destination_rectangle = D2D_GET_RECT(L, 1);
		D2D1_RECT_F source_rectangle = D2D_GET_RECT(L, 5);
		std::string identifier(luaL_checkstring(L, 9));
		float opacity = luaL_checknumber(L, 10);
		int interpolation = luaL_checkinteger(L, 11);

		ID2D1Bitmap* bitmap = lua->get_loose_bitmap(identifier.c_str());

		lua->d2d_render_target_stack.top()->DrawBitmap(
			bitmap,
			destination_rectangle,
			opacity,
			(D2D1_BITMAP_INTERPOLATION_MODE)interpolation,
			source_rectangle
		);

		return 0;
	}

	int LuaD2DGetImageInfo(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		std::string identifier(luaL_checkstring(L, 1));
		ID2D1Bitmap* bitmap = lua->get_loose_bitmap(identifier.c_str());

		if (bitmap == nullptr) {
			luaL_error(L, "Bitmap doesnt exist");
		} else {
			D2D1_SIZE_U size = bitmap->GetPixelSize();
			lua_newtable(L);
			lua_pushinteger(L, size.width);
			lua_setfield(L, -2, "width");
			lua_pushinteger(L, size.height);
			lua_setfield(L, -2, "height");
		}

		return 1;
	}

	int LuaD2DCreateRenderTarget(lua_State* L) {
		LuaEnvironment* lua = GetLuaClass(L);

		float width = luaL_checknumber(L, 1);
		float height = luaL_checknumber(L, 2);

		ID2D1BitmapRenderTarget* bitmap_render_target;
		lua->d2d_render_target_stack.top()->CreateCompatibleRenderTarget(D2D1::SizeF(width, height), &bitmap_render_target);

		std::string unique_key = "rt_" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
		lua->d2d_bitmap_render_target[unique_key] = bitmap_render_target;

		lua_pushstring(L, unique_key.c_str());

		return 1;
	}

	int LuaD2DDestroyRenderTarget(lua_State* L) {
		LuaEnvironment* lua = GetLuaClass(L);

		std::string key = luaL_checkstring(L, 1);

		if (lua->d2d_bitmap_render_target.contains(key)) {
			lua->d2d_bitmap_render_target[key]->Release();
			lua->d2d_bitmap_render_target.erase(key);
		}

		return 0;
	}

	int LuaD2DBeginRenderTarget(lua_State* L) {
		LuaEnvironment* lua = GetLuaClass(L);

		std::string key = luaL_checkstring(L, 1);

		if (lua->d2d_bitmap_render_target.contains(key)) {
			lua->d2d_render_target_stack.push(lua->d2d_bitmap_render_target[key]);
			// we need to clear the brush cache because brushes are rt-scoped
			lua->d2d_brush_cache.clear();
			lua->d2d_bitmap_render_target[key]->BeginDraw();
		}

		return 0;
	}

	int LuaD2DEndRenderTarget(lua_State* L) {
		LuaEnvironment* lua = GetLuaClass(L);

		std::string key = luaL_checkstring(L, 1);

		if (lua->d2d_bitmap_render_target.contains(key)) {
			lua->d2d_bitmap_render_target[key]->EndDraw();
			lua->d2d_render_target_stack.pop();
		}

		return 0;
	}

#undef D2D_GET_RECT
#undef D2D_GET_COLOR
#undef D2D_GET_POINT
#undef D2D_GET_ELLIPS
#undef D2D_GET_ROUNDED_RECT

	int GetGUIInfo(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		RECT rect;
		GetClientRect(main_hwnd, &rect);

		lua_newtable(L);
		lua_pushinteger(L, rect.right - rect.left);
		lua_setfield(L, -2, "width");
		lua_pushinteger(L, rect.bottom - rect.top);
		lua_setfield(L, -2, "height");
		return 1;
	}

	int ResizeWindow(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		RECT clientRect, wndRect;
		GetWindowRect(main_hwnd, &wndRect);
		GetClientRect(main_hwnd, &clientRect);
		wndRect.bottom -= wndRect.top;
		wndRect.right -= wndRect.left;
		int w = luaL_checkinteger(L, 1),
		    h = luaL_checkinteger(L, 2);
		SetWindowPos(main_hwnd, 0, 0, 0,
		             w + (wndRect.right - clientRect.right),
		             h + (wndRect.bottom - clientRect.bottom),
		             SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);


		// we need to recreate the renderer to accomodate for size changes (this cant be done in-place)
		lua->destroy_renderer();
		lua->create_renderer();

		return 0;
	}

	//emu
	int ConsoleWriteLua(lua_State* L)
	{
		ConsoleWrite(GetLuaClass(L)->hwnd, lua_tostring(L, 1));
		return 0;
	}

	int DebugviewWrite(lua_State* L)
	{
		printf("%s\n", (char*)lua_tostring(L, 1));
		return 0;
	}

	int StatusbarWrite(lua_State* L)
	{
		statusbar_post_text(std::string(lua_tostring(L, 1)));
		return 0;
	}

	int AtUpdateScreen(lua_State* L)
	{
		return lua_pcall(L, 0, 0, 0);
	}

	int AtVI(lua_State* L)
	{
		return lua_pcall(L, 0, 0, 0);
	}

	static int current_input_n;

	int AtInput(lua_State* L)
	{
		lua_pushinteger(L, current_input_n);
		return lua_pcall(L, 1, 0, 0);
	}

	int AtStop(lua_State* L)
	{
		return lua_pcall(L, 0, 0, 0);
	}

	int AtWindowMessage(lua_State* L)
	{
		lua_pushinteger(L, (unsigned)window_proc_params.wnd);
		lua_pushinteger(L, window_proc_params.msg);
		lua_pushinteger(L, window_proc_params.w_param);
		lua_pushinteger(L, window_proc_params.l_param);

		return lua_pcall(L, 4, 0, 0);
	}

	int RegisterUpdateScreen(lua_State* L)
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

	int RegisterVI(lua_State* L)
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

	int RegisterInput(lua_State* L)
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

	int RegisterStop(lua_State* L)
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

	int RegisterWindowMessage(lua_State* L)
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

	int RegisterInterval(lua_State* L)
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

	int RegisterPlayMovie(lua_State* L)
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

	int RegisterStopMovie(lua_State* L)
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

	int RegisterLoadState(lua_State* L)
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

	int RegisterSaveState(lua_State* L)
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

	int RegisterReset(lua_State* L)
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

	//generic function used for all of the At... callbacks, calls function from stack top.
	int CallTop(lua_State* L)
	{
		return lua_pcall(L, 0, 0, 0);
	}

	int GetVICount(lua_State* L)
	{
		lua_pushinteger(L, m_current_vi);
		return 1;
	}

	int GetSampleCount(lua_State* L)
	{
		lua_pushinteger(L, vcr_current_sample);
		return 1;
	}

	int GetInputCount(lua_State* L)
	{
		lua_pushinteger(L, inputCount);
		return 1;
	}


	int GetMupenVersion(lua_State* L)
	{
		int type = luaL_checknumber(L, 1);
		const char* version;
		// 0 = name + version number
		// 1 = version number
		version = mupen_version;
		if (type > 0)
			version = {&mupen_version[strlen("Mupen 64 ")]};


		lua_pushstring(L, version);
		return 1;
	}

	int GetVCRReadOnly(lua_State* L)
	{
		lua_pushboolean(L, vcr_get_read_only());
		return 1;
	}

	int SetGFX(lua_State* L)
	{
		// stub for now
		return 0;
	}

	// calling this during a drawing section will crash the application
	int SetRenderer(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		lua->renderer = static_cast<Renderer>(luaL_checknumber(L, 1));
		lua->destroy_renderer();
		lua->create_renderer();

		return 1;
	}

	int LuaPlaySound(lua_State* L)
	{
		PlaySound(luaL_checkstring(L, 1), NULL, SND_FILENAME | SND_ASYNC);
		return 1;
	}

	int GetAddress(lua_State* L)
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
		return 0;
	}

	int EmuPause(lua_State* L)
	{
		if (!lua_toboolean(L, 1))
		{
			pause_emu(FALSE);
		} else
		{
			resume_emu(TRUE);
		}
		return 0;
	}

	int GetEmuPause(lua_State* L)
	{
		lua_pushboolean(L, emu_paused);
		return 1;
	}

	int GetSpeed(lua_State* L)
	{
		lua_pushinteger(L, Config.fps_modifier);
		return 1;
	}

	int SetSpeed(lua_State* L)
	{
		Config.fps_modifier = luaL_checkinteger(L, 1);
		timer_init();
		return 0;
	}

	int SetSpeedMode(lua_State* L)
	{
		if (!strcmp(luaL_checkstring(L, 1), "normal")) {
			Config.fps_modifier = 100;
		} else {
			Config.fps_modifier = 10000;
		}
		return 0;
	}

	// Movie
	int PlayMovie(lua_State* L)
	{
		const char* fname = lua_tostring(L, 1);
		vcr_set_read_only(true);
		vcr_start_playback(fname, false);
		return 0;
	}

	int StopMovie(lua_State* L)
	{
		vcr_stop_playback(true);
		return 0;
	}

	int GetMovieFilename(lua_State* L)
	{
		if (vcr_is_starting() || vcr_is_playing())
		{
			lua_pushstring(L, movie_path.string().c_str());
		} else
		{
			luaL_error(L, "No movie is currently playing");
			lua_pushstring(L, "");
		}
		return 1;
	}

	//savestate
	//�蔲��
	int SaveFileSavestate(lua_State* L)
	{
		savestates_do(lua_tostring(L, 1), e_st_job::save);
		return 0;
	}

	int LoadFileSavestate(lua_State* L)
	{
		savestates_do(lua_tostring(L, 1), e_st_job::load);
		return 0;
	}

	// IO
	int LuaFileDialog(lua_State* L)
	{
		EmulationLock lock;
		auto filter = string_to_wstring(std::string(luaL_checkstring(L, 1)));
		const int32_t type = luaL_checkinteger(L, 2);

		std::wstring path;

		if (type == 0)
		{
			path = show_persistent_open_dialog("o_lua_api", main_hwnd, filter);
		} else
		{
			path = show_persistent_save_dialog("o_lua_api", main_hwnd, filter);
		}

		lua_pushstring(L, wstring_to_string(path).c_str());
		return 1;
	}

	BOOL fileexists(const char* path)
	{
		struct stat buffer;
		return (stat(path, &buffer) == 0);
	}

	// AVI
	int StartCapture(lua_State* L)
	{
		const char* fname = lua_tostring(L, 1);
		if (!vcr_is_capturing())
			vcr_start_capture(fname, false);
		else
			luaL_error(
				L,
				"Tried to start AVI capture when one was already in progress");
		return 0;
	}

	int StopCapture(lua_State* L)
	{
		if (vcr_is_capturing())
			vcr_stop_capture();
		else
			luaL_error(L, "Tried to end AVI capture when none was in progress");
		return 0;
	}

	extern void LuaDCUpdate(int redraw);


	//input

	const char* KeyName[256] =
	{
		NULL, "leftclick", "rightclick", NULL,
		"middleclick", NULL, NULL, NULL,
		"backspace", "tab", NULL, NULL,
		NULL, "enter", NULL, NULL,
		"shift", "control", "alt", "pause", // 0x10
		"capslock", NULL, NULL, NULL,
		NULL, NULL, NULL, "escape",
		NULL, NULL, NULL, NULL,
		"space", "pageup", "pagedown", "end", // 0x20
		"home", "left", "up", "right",
		"down", NULL, NULL, NULL,
		NULL, "insert", "delete", NULL,
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
		NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		"A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
		"K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
		"U", "V", "W", "X", "Y", "Z",
		NULL, NULL, NULL, NULL, NULL,
		"numpad0", "numpad1", "numpad2", "numpad3", "numpad4", "numpad5",
		"numpad6", "numpad7", "numpad8", "numpad9",
		"numpad*", "numpad+",
		NULL,
		"numpad-", "numpad.", "numpad/",
		"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11",
		"F12",
		"F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22",
		"F23", "F24",
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		"numlock", "scrolllock",
		NULL, // 0x92
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, // 0xB9
		"semicolon", "plus", "comma", "minus",
		"period", "slash", "tilde",
		NULL, // 0xC1
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, // 0xDA
		"leftbracket", "backslash", "rightbracket", "quote",
	};

	int GetKeys(lua_State* L)
	{
		lua_newtable(L);
		for (int i = 1; i < 255; i++)
		{
			const char* name = KeyName[i];
			if (name)
			{
				int active;
				if (i == VK_CAPITAL || i == VK_NUMLOCK || i == VK_SCROLL)
					active = GetKeyState(i) & 0x01;
				else
					active = GetAsyncKeyState(i) & 0x8000;
				if (active)
				{
					lua_pushboolean(L, true);
					lua_setfield(L, -2, name);
				}
			}
		}

		POINT mouse;
		GetCursorPos(&mouse);
		ScreenToClient(main_hwnd, &mouse);
		lua_pushinteger(L, mouse.x);
		lua_setfield(L, -2, "xmouse");
		lua_pushinteger(L, mouse.y);
		lua_setfield(L, -2, "ymouse");
		return 1;
	}

	/*
		local oinp
		emu.atvi(function()
			local inp = input.get()
			local dinp = input.diff(inp, oinp)
			...
			oinp = inp
		end)
	*/
	int GetKeyDifference(lua_State* L)
	{
		if (lua_isnil(L, 1))
		{
			lua_newtable(L);
			lua_insert(L, 1);
		}
		luaL_checktype(L, 2, LUA_TTABLE);
		lua_newtable(L);
		lua_pushnil(L);
		while (lua_next(L, 1))
		{
			lua_pushvalue(L, -2);
			lua_gettable(L, 2);
			if (lua_isnil(L, -1))
			{
				lua_pushvalue(L, -3);
				lua_pushboolean(L, 1);
				lua_settable(L, 3);
			}
			lua_pop(L, 2);
		}
		return 1;
	}

	int LuaGetKeyNameText(lua_State* L)
	{
		char name[100] = {0};
		auto vk = luaL_checkinteger(L, 1);
		GetKeyNameText(vk, name, sizeof(name) / sizeof(name[0]));
		lua_pushstring(L, name);
		return 1;
	}

	int LuaMapVirtualKeyEx(lua_State* L)
	{
		auto u_code = luaL_checkinteger(L, 1);
		auto u_map_type = luaL_checkinteger(L, 2);
		lua_pushinteger(
			L, MapVirtualKeyEx(u_code, u_map_type, GetKeyboardLayout(0)));
		return 1;
	}

	INT_PTR CALLBACK InputPromptProc(HWND wnd, UINT msg, WPARAM wParam,
	                                 LPARAM lParam)
	{
		static lua_State* L;
		switch (msg)
		{
		case WM_INITDIALOG:
			{
				L = (lua_State*)lParam;
				std::string str(luaL_optstring(L, 2, ""));
				SetWindowText(wnd,
				              luaL_optstring(L, 1, "input:"));
				std::string::size_type p = 0;
				while ((p = str.find('\n', p)) != std::string::npos)
				{
					str.replace(p, 1, "\r\n");
					p += 2;
				}
				SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUAPROMPT),
				              str.c_str());
				SetFocus(GetDlgItem(wnd, IDC_TEXTBOX_LUAPROMPT));
				break;
			}
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDOK:
				{
					HWND inp = GetDlgItem(wnd, IDC_TEXTBOX_LUAPROMPT);
					int size = GetWindowTextLength(inp) + 1;
					char* buf = new char[size];
					GetWindowText(inp, buf, size);
					std::string str(buf);
					delete[] buf;
					std::string::size_type p = 0;
					while ((p = str.find("\r\n", p)) != std::string::npos)
					{
						str.replace(p, 2, "\n");
						p += 1;
					}
					lua_pushstring(L, str.c_str());
					EndDialog(wnd, 0);
					break;
				}
			case IDCANCEL:
				lua_pushnil(L);
				EndDialog(wnd, 1);
				break;
			}
			break;
		}
		return FALSE;
	}

	int InputPrompt(lua_State* L)
	{
		DialogBoxParam(app_instance,
		               MAKEINTRESOURCE(IDD_LUAINPUTPROMPT), main_hwnd,
		               InputPromptProc, (LPARAM)L);
		return 1;
	}

	//joypad
	int lua_get_joypad(lua_State* L)
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

	int lua_set_joypad(lua_State* L)
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

	const luaL_Reg globalFuncs[] = {
		{"print", Print},
		{"printx", PrintX},
		{"tostringex", ToStringExs},
		{"stop", StopScript},
		{NULL, NULL}
	};
	//�G���Ȋ֐�
	const luaL_Reg emuFuncs[] = {
		{"console", ConsoleWriteLua},
		{"debugview", DebugviewWrite},
		{"statusbar", StatusbarWrite},

		{"atvi", RegisterVI},
		{"atupdatescreen", RegisterUpdateScreen},
		{"atinput", RegisterInput},
		{"atstop", RegisterStop},
		{"atwindowmessage", RegisterWindowMessage},
		{"atinterval", RegisterInterval},
		{"atplaymovie", RegisterPlayMovie},
		{"atstopmovie", RegisterStopMovie},
		{"atloadstate", RegisterLoadState},
		{"atsavestate", RegisterSaveState},
		{"atreset", RegisterReset},

		{"framecount", GetVICount},
		{"samplecount", GetSampleCount},
		{"inputcount", GetInputCount},

		{"getversion", GetMupenVersion},

		{"pause", EmuPause},
		{"getpause", GetEmuPause},
		{"getspeed", GetSpeed},
		{"speed", SetSpeed},
		{"speedmode", SetSpeedMode},
		{"setgfx", SetGFX},

		{"getaddress", GetAddress},

		{"isreadonly", GetVCRReadOnly},

		{"getsystemmetrics", GetSystemMetricsLua},
		// irrelevant to core but i dont give a
		{"ismainwindowinforeground", IsMainWindowInForeground},

		{"screenshot", Screenshot},
		{"set_renderer", SetRenderer},
		{"play_sound", LuaPlaySound},
		{NULL, NULL}
	};
	const luaL_Reg memoryFuncs[] = {
		// memory conversion functions
		{"inttofloat", LuaIntToFloat},
		{"inttodouble", LuaIntToDouble},
		{"floattoint", LuaFloatToInt},
		{"doubletoint", LuaDoubleToInt},
		{"qwordtonumber", LuaQWordToNumber},

		// word = 2 bytes
		// reading functions
		{"readbytesigned", LuaReadByteSigned},
		{"readbyte", LuaReadByteUnsigned},
		{"readwordsigned", LuaReadWordSigned},
		{"readword", LuaReadWordUnsigned},
		{"readdwordsigned", LuaReadDWordSigned},
		{"readdword", LuaReadDWorldUnsigned},
		{"readqwordsigned", LuaReadQWordSigned},
		{"readqword", LuaReadQWordUnsigned},
		{"readfloat", LuaReadFloat},
		{"readdouble", LuaReadDouble},
		{"readsize", LuaReadSize},

		// writing functions
		// all of these are assumed to be unsigned
		{"writebyte", LuaWriteByteUnsigned},
		{"writeword", LuaWriteWordUnsigned},
		{"writedword", LuaWriteDWordUnsigned},
		{"writeqword", LuaWriteQWordUnsigned},
		{"writefloat", LuaWriteFloatUnsigned},
		{"writedouble", LuaWriteDoubleUnsigned},

		{"writesize", LuaWriteSize},

		{NULL, NULL}
	};

	//winAPI GDI�֐���
	const luaL_Reg wguiFuncs[] = {
		{"setbrush", SetBrush},
		{"setpen", SetPen},
		{"setcolor", SetTextColor},
		{"setbk", SetBackgroundColor},
		{"setfont", SetFont},
		{"text", LuaTextOut},
		{"drawtext", LuaDrawText},
		{"drawtextalt", LuaDrawTextAlt},
		{"gettextextent", GetTextExtent},
		{"rect", DrawRect},
		{"fillrect", FillRect},
		/*<GDIPlus>*/
		// GDIPlus functions marked with "a" suffix
		{"fillrecta", FillRectAlpha},
		{"fillellipsea", FillEllipseAlpha},
		{"fillpolygona", FillPolygonAlpha},
		{"loadimage", LuaLoadImage},
		{"deleteimage", DeleteImage},
		{"drawimage", DrawImage},
		{"loadscreen", LoadScreen},
		{"loadscreenreset", LoadScreenReset},
		{"getimageinfo", GetImageInfo},
		/*</GDIPlus*/
		{"ellipse", DrawEllipse},
		{"polygon", DrawPolygon},
		{"line", DrawLine},
		{"info", GetGUIInfo},
		{"resize", ResizeWindow},
		{"setclip", SetClip},
		{"resetclip", ResetClip},
		{NULL, NULL}
	};

	const luaL_Reg d2dFuncs[] = {
		{"fill_rectangle", LuaD2DFillRectangle},
		{"draw_rectangle", LuaD2DDrawRectangle},
		{"fill_ellipse", LuaD2DFillEllipse},
		{"draw_ellipse", LuaD2DDrawEllipse},
		{"draw_line", LuaD2DDrawLine},
		{"draw_text", LuaD2DDrawText},
		{"get_text_size", LuaD2DGetTextSize},
		{"push_clip", LuaD2DPushClip},
		{"pop_clip", LuaD2DPopClip},
		{"fill_rounded_rectangle", LuaD2DFillRoundedRectangle},
		{"draw_rounded_rectangle", LuaD2DDrawRoundedRectangle},
		{"load_image", LuaD2DLoadImage},
		{"free_image", LuaD2DFreeImage},
		{"draw_image", LuaD2DDrawImage},
		{"get_image_info", LuaD2DGetImageInfo},
		{"set_text_antialias_mode", LuaD2DSetTextAntialiasMode},
		{"set_antialias_mode", LuaD2DSetAntialiasMode},
		{"create_render_target", LuaD2DCreateRenderTarget},
		{"destroy_render_target", LuaD2DDestroyRenderTarget},
		{"begin_render_target", LuaD2DBeginRenderTarget},
		{"end_render_target", LuaD2DEndRenderTarget},
		{NULL, NULL}
	};

	const luaL_Reg inputFuncs[] = {
		{"get", GetKeys},
		{"diff", GetKeyDifference},
		{"prompt", InputPrompt},
		{"get_key_name_text", LuaGetKeyNameText},
		{"map_virtual_key_ex", LuaMapVirtualKeyEx},
		{NULL, NULL}
	};
	const luaL_Reg joypadFuncs[] = {
		{"get", lua_get_joypad},
		{"set", lua_set_joypad},
		{"register", RegisterInput},
		{"count", GetInputCount},
		{NULL, NULL}
	};

	const luaL_Reg movieFuncs[] = {
		{"playmovie", PlayMovie},
		{"stopmovie", StopMovie},
		{"getmoviefilename", GetMovieFilename},
		{NULL, NULL}
	};

	const luaL_Reg savestateFuncs[] = {
		{"savefile", SaveFileSavestate},
		{"loadfile", LoadFileSavestate},
		{NULL, NULL}
	};
	const luaL_Reg ioHelperFuncs[] = {
		{"filediag", LuaFileDialog},
		{NULL, NULL}
	};
	const luaL_Reg aviFuncs[] = {
		{"startcapture", StartCapture},
		{"stopcapture", StopCapture},
		{NULL, NULL}
	};



void NewLuaScript()
{
	create_and_show_lua_window(SW_SHOW);
}

void dummy_function()
{
}

void lua_create_and_run(const char* path, bool minimized)
{
	printf("Creating lua window...\n");
	auto hwnd = create_and_show_lua_window(minimized ? SW_SHOWMINNOACTIVE : SW_SHOW);

	printf("Setting path...\n");
	// set the textbox content to match the path
	SetWindowText(GetDlgItem(hwnd, IDC_TEXTBOX_LUASCRIPTPATH), path);

	printf("Simulating run button click...\n");
	// click run button
	SendMessage(hwnd, WM_COMMAND,
		            MAKEWPARAM(IDC_BUTTON_LUASTATE, BN_CLICKED),
		            (LPARAM)GetDlgItem(hwnd, IDC_BUTTON_LUASTATE));
}


void AtUpdateScreenLuaCallback()
{
	for (auto& pair : hwnd_lua_map) {
		pair.second->draw();
	}
}

void AtVILuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		AtVI, REG_ATVI);
}

void AtInputLuaCallback(int n)
{
	current_input_n = n;
	invoke_callbacks_with_key_on_all_instances(
		AtInput, REG_ATINPUT);
	inputCount++;
}

void AtIntervalLuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		CallTop, REG_ATINTERVAL);
}

void AtPlayMovieLuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		CallTop, REG_ATPLAYMOVIE);
}

void AtStopMovieLuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		CallTop, REG_ATSTOPMOVIE);
}

void AtLoadStateLuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		CallTop, REG_ATLOADSTATE);
}

void AtSaveStateLuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		CallTop, REG_ATSAVESTATE);
}

//called after reset, when emulation ready
void AtResetLuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		CallTop, REG_ATRESET);
}

//�Ƃ肠����lua�ɓ���Ƃ�
char traceLoggingBuf[0x10000];
char* traceLoggingPointer = traceLoggingBuf;

inline void TraceLoggingBufFlush()
{
	DWORD writeen;
	WriteFile(TraceLogFile,
	          traceLoggingBuf, traceLoggingPointer - traceLoggingBuf, &writeen,
	          NULL);
	traceLoggingPointer = traceLoggingBuf;
}

inline void TraceLoggingWriteBuf()
{
	const char* const buflength = traceLoggingBuf + sizeof(traceLoggingBuf) -
		512;
	if (traceLoggingPointer >= buflength)
	{
		TraceLoggingBufFlush();
	}
}

void instrStr2(r4300word pc, r4300word w, char* p1)
{
	char*& p = p1;
	INSTDECODE decode;
	//little endian
#define HEX8(n) 	*(r4300word*)p = n; p += 4
	DecodeInstruction(w, &decode);
	HEX8(pc);
	HEX8(w);
	INSTOPERAND& o = decode.operand;
	//n�͌ォ��f�R�[�h����΂킩��
#define REGCPU(n) \
	HEX8(reg[n])
#define REGCPU2(n,m) \
		REGCPU(n);\
		REGCPU(m);
	//10�i��
#define REGFPU(n) \
	HEX8(*(r4300word*)reg_cop1_simple[n])
#define REGFPU2(n,m) REGFPU(n);REGFPU(m)
#define NONE *(r4300word*)p=0;p+=4
#define NONE2 NONE;NONE

	switch (decode.format)
	{
	case INSTF_NONE:
		NONE2;
		break;
	case INSTF_J:
	case INSTF_0BRANCH:
		NONE2;
		break;
	case INSTF_LUI:
		NONE2;
		break;
	case INSTF_1BRANCH:
	case INSTF_JR:
	case INSTF_ISIGN:
	case INSTF_IUNSIGN:
		REGCPU(o.i.rs);
		NONE;
		break;
	case INSTF_2BRANCH:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_ADDRW:
		HEX8(reg[o.i.rs] + (r4300halfsigned)o.i.immediate);
		REGCPU(o.i.rt);
		break;
	case INSTF_ADDRR:
		HEX8(reg[o.i.rs] + (r4300halfsigned)o.i.immediate);
		NONE;
		break;
	case INSTF_LFW:
		HEX8(reg[o.lf.base] + (r4300halfsigned)o.lf.offset);
		REGFPU(o.lf.ft);
		break;
	case INSTF_LFR:
		HEX8(reg[o.lf.base] + (r4300halfsigned)o.lf.offset);
		NONE;
		break;
	case INSTF_R1:
		REGCPU(o.r.rd);
		NONE;
		break;
	case INSTF_R2:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_R3:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_MTC0:
	case INSTF_MTC1:
	case INSTF_SA:
		REGCPU(o.r.rt);
		NONE;
		break;
	case INSTF_R2F:
		REGFPU(o.cf.fs);
		NONE;
		break;
	case INSTF_R3F:
	case INSTF_C:
		REGFPU2(o.cf.fs, o.cf.ft);
		break;
	case INSTF_MFC0:
		NONE2;
		break;
	case INSTF_MFC1:
		REGFPU(((FPUREG)o.r.rs));
		NONE;
		break;
	}
	p1[strlen(p1)] = '\0';
#undef HEX8
#undef REGCPU
#undef REGFPU
#undef REGCPU2
#undef REGFPU2
}

void instrStr1(unsigned long pc, unsigned long w, char* p1)
{
	char*& p = p1;
	INSTDECODE decode;
	const char* const x = "0123456789abcdef";
#define HEX8(n) 	p[0] = x[(r4300word)(n)>>28&0xF];\
	p[1] = x[(r4300word)(n)>>24&0xF];\
	p[2] = x[(r4300word)(n)>>20&0xF];\
	p[3] = x[(r4300word)(n)>>16&0xF];\
	p[4] = x[(r4300word)(n)>>12&0xF];\
	p[5] = x[(r4300word)(n)>>8&0xF];\
	p[6] = x[(r4300word)(n)>>4&0xF];\
	p[7] = x[(r4300word)(n)&0xF];\
	p+=8;

	DecodeInstruction(w, &decode);
	HEX8(pc);
	*(p++) = ':';
	*(p++) = ' ';
	HEX8(w);
	*(p++) = ' ';
	const char* ps = p;
	if (w == 0x00000000)
	{
		*(p++) = 'n';
		*(p++) = 'o';
		*(p++) = 'p';
	} else
	{
		for (const char* q = GetOpecodeString(&decode); *q; q++)
		{
			*(p++) = *q;
		}
		*(p++) = ' ';
		p = GetOperandString(p, &decode, pc);
	}
	for (int i = p - ps + 3; i < 24; i += 4)
	{
		*(p++) = '\t';
	}
	*(p++) = ';';
	INSTOPERAND& o = decode.operand;
#define REGCPU(n) if((n)!=0){\
			for(const char *l = CPURegisterName[n]; *l; l++){\
				*(p++) = *l;\
			}\
			*(p++) = '=';\
			HEX8(reg[n]);\
	}
#define REGCPU2(n,m) \
		REGCPU(n);\
		if((n)!=(m)&&(m)!=0){C;REGCPU(m);}
	//10�i��
#define REGFPU(n) *(p++)='f';\
			*(p++)=x[(n)/10];\
			*(p++)=x[(n)%10];\
			*(p++) = '=';\
			p+=sprintf(p,"%f",*reg_cop1_simple[n])
#define REGFPU2(n,m) REGFPU(n);\
		if((n)!=(m)){C;REGFPU(m);}
#define C *(p++) = ','

	if (delay_slot)
	{
		*(p++) = '#';
	}
	switch (decode.format)
	{
	case INSTF_NONE:
		break;
	case INSTF_J:
	case INSTF_0BRANCH:
		break;
	case INSTF_LUI:
		break;
	case INSTF_1BRANCH:
	case INSTF_JR:
	case INSTF_ISIGN:
	case INSTF_IUNSIGN:
		REGCPU(o.i.rs);
		break;
	case INSTF_2BRANCH:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_ADDRW:
		REGCPU(o.i.rt);
		if (o.i.rt != 0) { C; }
	case INSTF_ADDRR:
		*(p++) = '@';
		*(p++) = '=';
		HEX8(reg[o.i.rs] + (r4300halfsigned)o.i.immediate);
		break;
	case INSTF_LFW:
		REGFPU(o.lf.ft);
		C;
	case INSTF_LFR:
		*(p++) = '@';
		*(p++) = '=';
		HEX8(reg[o.lf.base] + (r4300halfsigned)o.lf.offset);
		break;
	case INSTF_R1:
		REGCPU(o.r.rd);
		break;
	case INSTF_R2:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_R3:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_MTC0:
	case INSTF_MTC1:
	case INSTF_SA:
		REGCPU(o.r.rt);
		break;
	case INSTF_R2F:
		REGFPU(o.cf.fs);
		break;
	case INSTF_R3F:
	case INSTF_C:
		REGFPU2(o.cf.fs, o.cf.ft);
		break;
	case INSTF_MFC0:
		break;
	case INSTF_MFC1:
		REGFPU(((FPUREG)o.r.rs));
		break;
	}
	p1[strlen(p1)] = '\0';
#undef HEX8
#undef REGCPU
#undef REGFPU
#undef REGCPU2
#undef REGFPU2
#undef C
}

void TraceLogging(r4300word pc, r4300word w)
{
	char*& p = traceLoggingPointer;
	INSTDECODE decode;
	const char* const x = "0123456789abcdef";
#define HEX8(n) 	p[0] = x[(r4300word)(n)>>28&0xF];\
	p[1] = x[(r4300word)(n)>>24&0xF];\
	p[2] = x[(r4300word)(n)>>20&0xF];\
	p[3] = x[(r4300word)(n)>>16&0xF];\
	p[4] = x[(r4300word)(n)>>12&0xF];\
	p[5] = x[(r4300word)(n)>>8&0xF];\
	p[6] = x[(r4300word)(n)>>4&0xF];\
	p[7] = x[(r4300word)(n)&0xF];\
	p+=8;

	DecodeInstruction(w, &decode);
	HEX8(pc);
	*(p++) = ':';
	*(p++) = ' ';
	HEX8(w);
	*(p++) = ' ';
	const char* ps = p;
	if (w == 0x00000000)
	{
		*(p++) = 'n';
		*(p++) = 'o';
		*(p++) = 'p';
	} else
	{
		for (const char* q = GetOpecodeString(&decode); *q; q++)
		{
			*(p++) = *q;
		}
		*(p++) = ' ';
		p = GetOperandString(p, &decode, pc);
	}
	for (int i = p - ps + 3; i < 24; i += 4)
	{
		*(p++) = '\t';
	}
	*(p++) = ';';
	INSTOPERAND& o = decode.operand;
#define REGCPU(n) if((n)!=0){\
			for(const char *l = CPURegisterName[n]; *l; l++){\
				*(p++) = *l;\
			}\
			*(p++) = '=';\
			HEX8(reg[n]);\
	}
#define REGCPU2(n,m) \
		REGCPU(n);\
		if((n)!=(m)&&(m)!=0){C;REGCPU(m);}
	//10�i��
#define REGFPU(n) *(p++)='f';\
			*(p++)=x[(n)/10];\
			*(p++)=x[(n)%10];\
			*(p++) = '=';\
			p+=sprintf(p,"%f",*reg_cop1_simple[n])
#define REGFPU2(n,m) REGFPU(n);\
		if((n)!=(m)){C;REGFPU(m);}
#define C *(p++) = ','

	if (delay_slot)
	{
		*(p++) = '#';
	}
	switch (decode.format)
	{
	case INSTF_NONE:
		break;
	case INSTF_J:
	case INSTF_0BRANCH:
		break;
	case INSTF_LUI:
		break;
	case INSTF_1BRANCH:
	case INSTF_JR:
	case INSTF_ISIGN:
	case INSTF_IUNSIGN:
		REGCPU(o.i.rs);
		break;
	case INSTF_2BRANCH:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_ADDRW:
		REGCPU(o.i.rt);
		if (o.i.rt != 0) { C; }
	case INSTF_ADDRR:
		*(p++) = '@';
		*(p++) = '=';
		HEX8(reg[o.i.rs] + (r4300halfsigned)o.i.immediate);
		break;
	case INSTF_LFW:
		REGFPU(o.lf.ft);
		C;
	case INSTF_LFR:
		*(p++) = '@';
		*(p++) = '=';
		HEX8(reg[o.lf.base] + (r4300halfsigned)o.lf.offset);
		break;
	case INSTF_R1:
		REGCPU(o.r.rd);
		break;
	case INSTF_R2:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_R3:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_MTC0:
	case INSTF_MTC1:
	case INSTF_SA:
		REGCPU(o.r.rt);
		break;
	case INSTF_R2F:
		REGFPU(o.cf.fs);
		break;
	case INSTF_R3F:
	case INSTF_C:
		REGFPU2(o.cf.fs, o.cf.ft);
		break;
	case INSTF_MFC0:
		break;
	case INSTF_MFC1:
		REGFPU(((FPUREG)o.r.rs));
		break;
	}
	*(p++) = '\n';

	TraceLoggingWriteBuf();
#undef HEX8
#undef REGCPU
#undef REGFPU
#undef REGCPU2
#undef REGFPU2
#undef C
}

void TraceLoggingBin(r4300word pc, r4300word w)
{
	char*& p = traceLoggingPointer;
	INSTDECODE decode;
	//little endian
#define HEX8(n) 	*(r4300word*)p = n; p += 4

	DecodeInstruction(w, &decode);
	HEX8(pc);
	HEX8(w);
	INSTOPERAND& o = decode.operand;
	//n�͌ォ��f�R�[�h����΂킩��
#define REGCPU(n) \
	HEX8(reg[n])
#define REGCPU2(n,m) \
		REGCPU(n);\
		REGCPU(m);
	//10�i��
#define REGFPU(n) \
	HEX8(*(r4300word*)reg_cop1_simple[n])
#define REGFPU2(n,m) REGFPU(n);REGFPU(m)
#define NONE *(r4300word*)p=0;p+=4
#define NONE2 NONE;NONE

	switch (decode.format)
	{
	case INSTF_NONE:
		NONE2;
		break;
	case INSTF_J:
	case INSTF_0BRANCH:
		NONE2;
		break;
	case INSTF_LUI:
		NONE2;
		break;
	case INSTF_1BRANCH:
	case INSTF_JR:
	case INSTF_ISIGN:
	case INSTF_IUNSIGN:
		REGCPU(o.i.rs);
		NONE;
		break;
	case INSTF_2BRANCH:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_ADDRW:
		HEX8(reg[o.i.rs] + (r4300halfsigned)o.i.immediate);
		REGCPU(o.i.rt);
		break;
	case INSTF_ADDRR:
		HEX8(reg[o.i.rs] + (r4300halfsigned)o.i.immediate);
		NONE;
		break;
	case INSTF_LFW:
		HEX8(reg[o.lf.base] + (r4300halfsigned)o.lf.offset);
		REGFPU(o.lf.ft);
		break;
	case INSTF_LFR:
		HEX8(reg[o.lf.base] + (r4300halfsigned)o.lf.offset);
		NONE;
		break;
	case INSTF_R1:
		REGCPU(o.r.rd);
		NONE;
		break;
	case INSTF_R2:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_R3:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_MTC0:
	case INSTF_MTC1:
	case INSTF_SA:
		REGCPU(o.r.rt);
		NONE;
		break;
	case INSTF_R2F:
		REGFPU(o.cf.fs);
		NONE;
		break;
	case INSTF_R3F:
	case INSTF_C:
		REGFPU2(o.cf.fs, o.cf.ft);
		break;
	case INSTF_MFC0:
		NONE2;
		break;
	case INSTF_MFC1:
		REGFPU(((FPUREG)o.r.rs));
		NONE;
		break;
	}
	TraceLoggingWriteBuf();
#undef HEX8
#undef REGCPU
#undef REGFPU
#undef REGCPU2
#undef REGFPU2
}

void LuaTraceLoggingPure()
{
	if (!traceLogMode)
	{
		TraceLogging(interp_addr, op);
	} else
	{
		TraceLoggingBin(interp_addr, op);
	}
}

void LuaTraceLoggingInterpOps()
{
	if (enableTraceLog)
	{
		if (!traceLogMode)
		{
			TraceLogging(PC->addr, PC->src);
		} else
		{
			TraceLoggingBin(PC->addr, PC->src);
		}
	}
	PC->s_ops();
}


void LuaTraceLogState()
{
	if (!enableTraceLog) return;
	EmulationLock lock;

	auto path = show_persistent_save_dialog("o_tracelog", main_hwnd, L"*.log");

	if (path.size() == 0)
	{
		return;
	}

	TraceLogStart(wstring_to_string(path).c_str());
}

void close_all_scripts() {
	assert(IsGUIThread(false));

	// we mutate the map's nodes while iterating, so we have to make a copy
	auto copy = std::map(hwnd_lua_map);
	for (auto pair : copy) {
		SendMessage(pair.first, WM_CLOSE, 0, 0);
	}
	assert(hwnd_lua_map.empty());
}

void stop_all_scripts()
{
	assert(IsGUIThread(false));
	// we mutate the map's nodes while iterating, so we have to make a copy
	auto copy = std::map(hwnd_lua_map);
	for (auto pair : copy) {
		SendMessage(pair.first, WM_COMMAND,
					MAKEWPARAM(IDC_BUTTON_LUASTOP, BN_CLICKED),
					(LPARAM)GetDlgItem(pair.first, IDC_BUTTON_LUASTOP));

	}
	assert(hwnd_lua_map.empty());
}


void LuaWindowMessage(HWND wnd, UINT msg, WPARAM w, LPARAM l)
{
	window_proc_params = {
		.wnd = wnd,
		.msg = msg,
		.w_param = w,
		.l_param = l
	};
	invoke_callbacks_with_key_on_all_instances(AtWindowMessage, REG_WINDOWMESSAGE);
}

void LuaEnvironment::create_renderer()
{
	if (dc != nullptr)
	{
		return;
	}
	printf("Creating multi-target renderer for Lua...\n");

	RECT window_rect;
	GetClientRect(main_hwnd, &window_rect);
	dc_width = window_rect.right;
	dc_height = window_rect.bottom;

	// we create a bitmap with the main window's size and point our dc to it
	HDC main_dc = GetDC(main_hwnd);
	dc = CreateCompatibleDC(main_dc);
	HBITMAP bmp = CreateCompatibleBitmap(
		main_dc, window_rect.right, window_rect.bottom);
	SelectObject(dc, bmp);
	ReleaseDC(main_hwnd, main_dc);

	D2D1_RENDER_TARGET_PROPERTIES props =
		D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(
				DXGI_FORMAT_B8G8R8A8_UNORM,
				D2D1_ALPHA_MODE_PREMULTIPLIED));

	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
					  &d2d_factory);
	d2d_factory->CreateDCRenderTarget(&props, &d2d_render_target);
	DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(dw_factory),
		reinterpret_cast<IUnknown**>(&dw_factory)
	);

	RECT dc_rect = {0, 0, dc_width, dc_height};
	d2d_render_target->BindDC(dc, &dc_rect);
	d2d_render_target_stack.push(d2d_render_target);
}

void LuaEnvironment::destroy_renderer()
{
	if (!dc)
	{
		return;
	}

	printf("Destroying Lua renderer...\n");

	d2d_factory->Release();
	d2d_render_target->Release();

	for (auto const& [_, val] : d2d_brush_cache) {
		val->Release();
	}
	d2d_brush_cache.clear();

	for (auto const& [_, val] : d2d_bitmap_cache) {
		val->Release();
	}
	d2d_bitmap_cache.clear();

	for (auto const& [_, val] : d2d_bitmap_render_target) {
		val->Release();
	}
	d2d_bitmap_render_target.clear();

	while (!d2d_render_target_stack.empty()) {
		d2d_render_target_stack.pop();
	}

	ReleaseDC(main_hwnd, dc);
	dc = NULL;
	d2d_factory = NULL;
	d2d_render_target = NULL;
}

void LuaEnvironment::draw() {

	if (this == nullptr || !dc)
	{
		return;
	}

	// no renderer mode should only get callback and no other operations
	if (renderer == Renderer::None) {
		invoke_callbacks_with_key(AtUpdateScreen, REG_ATUPDATESCREEN);
		return;
	}

	if (renderer == Renderer::Direct2D) {
		d2d_render_target->BeginDraw();
		d2d_render_target->SetTransform(D2D1::Matrix3x2F::Identity());
		d2d_render_target->Clear(D2D1::ColorF(bitmap_color_mask));
	}

	invoke_callbacks_with_key(AtUpdateScreen, REG_ATUPDATESCREEN);

	if (renderer == Renderer::Direct2D) {
		d2d_render_target->EndDraw();
	}

	HDC main_dc = GetDC(main_hwnd);

	TransparentBlt(main_dc, 0, 0, dc_width, dc_height, dc, 0, 0,
		dc_width, dc_height, bitmap_color_mask);

	if (renderer == Renderer::GDIMixed) {
		RECT rect = { 0, 0, this->dc_width, this->dc_height};
		HBRUSH brush = CreateSolidBrush(bitmap_color_mask);
		FillRect(dc, &rect, brush);
		DeleteObject(brush);
	}

	ReleaseDC(main_hwnd, main_dc);
}


void LuaEnvironment::destroy(LuaEnvironment* lua_environment) {
	hwnd_lua_map.erase(lua_environment->hwnd);
	delete lua_environment;
}

std::pair<LuaEnvironment*, std::string> LuaEnvironment::create(std::filesystem::path path, HWND wnd)
{
	auto lua_environment = new LuaEnvironment();

	lua_environment->hwnd = wnd;
	lua_environment->path = path;

	lua_environment->brush = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
	lua_environment->pen = static_cast<HPEN>(GetStockObject(BLACK_PEN));
	lua_environment->font = static_cast<HFONT>(GetStockObject(SYSTEM_FONT));
	lua_environment->col = lua_environment->bkcol = 0;
	lua_environment->bkmode = TRANSPARENT;
	lua_environment->L = luaL_newstate();
	lua_atpanic(lua_environment->L, AtPanic);
	SetLuaClass(lua_environment->L, lua_environment);
	lua_environment->register_functions();

	// all lua scripts run with legacy renderer until otherwise stated
	lua_environment->create_renderer();

	bool error = luaL_dofile(lua_environment->L, lua_environment->path.string().c_str());

	std::string error_str;
	if (error)
	{
		error_str = lua_tostring(lua_environment->L, -1);
		delete lua_environment;
		lua_environment = nullptr;
	}

	return std::make_pair(lua_environment, error_str);
}

LuaEnvironment::~LuaEnvironment() {
	invoke_callbacks_with_key(AtStop, REG_ATSTOP);
	deleteGDIObject(brush, WHITE_BRUSH);
	deleteGDIObject(pen, BLACK_PEN);
	deleteGDIObject(font, SYSTEM_FONT);
	lua_close(L);
	L = NULL;
	for (auto x : image_pool) {
		delete x;
	}
	image_pool.clear();
	SetButtonState(hwnd, false);
	this->destroy_renderer();
	printf("Lua destroyed\n");
}

void LuaEnvironment::setBrush(HBRUSH h) {
	setGDIObject((HGDIOBJ*)&brush, h);
}

void LuaEnvironment::selectBrush() {
	selectGDIObject(brush);
}

void LuaEnvironment::setPen(HPEN h) {
	setGDIObject((HGDIOBJ*)&pen, h);
}

void LuaEnvironment::selectPen() {
	selectGDIObject(pen);
}

void LuaEnvironment::setFont(HFONT h) {
	setGDIObject((HGDIOBJ*)&font, h);
}

void LuaEnvironment::selectFont() {
	selectGDIObject(font);
}

void LuaEnvironment::setTextColor(COLORREF c) {
	col = c;
}

void LuaEnvironment::selectTextColor() {
	::SetTextColor(this->dc, col);
}

void LuaEnvironment::setBackgroundColor(COLORREF c, int mode) {
	bkcol = c;
	bkmode = mode;
}

void LuaEnvironment::selectBackgroundColor() {
	::SetBkMode(this->dc, bkmode);
	::SetBkColor(this->dc, bkcol);
}

//calls all functions that lua script has defined as callbacks, reads them from registry
//returns true at fail
bool LuaEnvironment::invoke_callbacks_with_key(std::function<int(lua_State*)> function,
							   const char* key) {
	assert(IsGUIThread(false));
	lua_getfield(L, LUA_REGISTRYINDEX, key);
	//shouldn't ever happen but could cause kernel panic
	if lua_isnil(L, -1) {
		lua_pop(L, 1);
		return false;
	}
	int n = luaL_len(L, -1);
	for (LUA_INTEGER i = 0; i < n; i++) {
		lua_pushinteger(L, 1 + i);
		lua_gettable(L, -2);
		if (function(L)) {\
			const char* str = lua_tostring(L, -1);
			ConsoleWrite(hwnd, str);
			ConsoleWrite(hwnd, "\r\n");
			printf("Lua error: %s\n", str);
			return true;
		}
	}
	lua_pop(L, 1);
	return false;
}

void register_as_package(lua_State* lua_state, const char* name, const luaL_Reg regs[]) {
	if (name == nullptr)
	{
		const luaL_Reg* p = regs;
		do {
			lua_register(lua_state, p->name, p->func);
		} while ((++p)->func);
		return;
	}
	luaL_newlib(lua_state, regs);
	lua_setglobal(lua_state, name);
}

void LuaEnvironment::register_functions() {
	luaL_openlibs(L);

	register_as_package(L, nullptr, globalFuncs);
	register_as_package(L, "emu", emuFuncs);
	register_as_package(L, "memory", memoryFuncs);
	register_as_package(L, "wgui", wguiFuncs);
	register_as_package(L, "d2d", d2dFuncs);
	register_as_package(L, "input", inputFuncs);
	register_as_package(L, "joypad", joypadFuncs);
	register_as_package(L, "movie", movieFuncs);
	register_as_package(L, "savestate", savestateFuncs);
	register_as_package(L, "iohelper", ioHelperFuncs);
	register_as_package(L, "avi", aviFuncs);

	//this makes old scripts backward compatible, new syntax for table length is '#'
	lua_getglobal(L, "table");
	lua_pushcfunction(L, getn);
	lua_setfield(L, -2, "getn");
	lua_pop(L, 1);
}

void LuaEnvironment::setGDIObject(HGDIOBJ* save, HGDIOBJ newobj) {
	if (*save)
		::DeleteObject(*save);
	DEBUG_GETLASTERROR;
	*save = newobj;
}

void LuaEnvironment::selectGDIObject(HGDIOBJ p) {
	SelectObject(this->dc, p);
	DEBUG_GETLASTERROR;
}

void LuaEnvironment::deleteGDIObject(HGDIOBJ p, int stockobj) {
	SelectObject(this->dc, GetStockObject(stockobj));
	DeleteObject(p);
}
