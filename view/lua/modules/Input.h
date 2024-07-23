extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <Windows.h>
#include <view/gui/Main.h>

namespace LuaCore::Input
{
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

	static int get_keys(lua_State* L)
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
		ScreenToClient(g_main_hwnd, &mouse);
		lua_pushinteger(L, mouse.x);
		lua_setfield(L, -2, "xmouse");
		lua_pushinteger(L, mouse.y);
		lua_setfield(L, -2, "ymouse");
		lua_pushinteger(L, g_last_wheel_delta > 0 ? 1 : (g_last_wheel_delta < 0 ? -1 : 0));
		lua_setfield(L, -2, "ywmouse");
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
	static int GetKeyDifference(lua_State* L)
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


	static int LuaGetKeyNameText(lua_State* L)
	{
		char name[100] = {0};
		auto vk = luaL_checkinteger(L, 1);
		GetKeyNameText(vk, name, sizeof(name) / sizeof(name[0]));
		lua_pushstring(L, name);
		return 1;
	}
	static INT_PTR CALLBACK InputPromptProc(HWND wnd, UINT msg, WPARAM wParam,
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

	static int InputPrompt(lua_State* L)
	{
		DialogBoxParam(g_app_instance,
		               MAKEINTRESOURCE(IDD_LUAINPUTPROMPT), g_main_hwnd,
		               InputPromptProc, (LPARAM)L);
		return 1;
	}
}
