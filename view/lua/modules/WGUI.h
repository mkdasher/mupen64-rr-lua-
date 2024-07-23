#include "shared/services/FrontendService.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <Windows.h>

#include "LuaConsole.h"
#include <view/gui/Main.h>
#include <view/helpers/WinHelpers.h>

namespace LuaCore::Wgui
{
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

	static int GetGUIInfo(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		RECT rect;
		GetClientRect(g_main_hwnd, &rect);

		lua_newtable(L);
		lua_pushinteger(L, rect.right - rect.left);
		lua_setfield(L, -2, "width");
		lua_pushinteger(L, rect.bottom - rect.top);
		lua_setfield(L, -2, "height");
		return 1;
	}

	static int ResizeWindow(lua_State* L)
	{
		assert(is_on_gui_thread());

		LuaEnvironment* lua = GetLuaClass(L);

		RECT clientRect, wndRect;
		GetWindowRect(g_main_hwnd, &wndRect);
		GetClientRect(g_main_hwnd, &clientRect);
		wndRect.bottom -= wndRect.top;
		wndRect.right -= wndRect.left;
		int w = luaL_checkinteger(L, 1),
		    h = luaL_checkinteger(L, 2);
		SetWindowPos(g_main_hwnd, 0, 0, 0,
		             w + (wndRect.right - clientRect.right),
		             h + (wndRect.bottom - clientRect.bottom),
		             SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);


		// we need to recreate the renderer to accomodate for size changes (this cant be done in-place)
		lua->destroy_renderer();
		lua->create_renderer();

		return 0;
	}

	static COLORREF StrToColorA(const char* s, bool alpha = false, COLORREF def = 0)
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

	static COLORREF StrToColor(const char* s, bool alpha = false, COLORREF def = 0)
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

	static int set_brush(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		if (lua->brush)
		{
			DeleteObject(lua->brush);
		}

		const char* s = lua_tostring(L, 1);
		if (lstrcmpi(s, "null") == 0)
			lua->brush = (HBRUSH)GetStockObject(NULL_BRUSH);
		else
			lua->brush = CreateSolidBrush(StrToColor(s));

		return 0;
	}

	static int set_pen(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		if (lua->pen)
		{
			DeleteObject(lua->pen);
		}

		const char* s = lua_tostring(L, 1);
		int width = luaL_optnumber(L, 2, 1);

		if (lstrcmpi(s, "null") == 0)
			lua->pen = (HPEN)GetStockObject(NULL_PEN);
		else
			lua->pen = CreatePen(PS_SOLID, width, StrToColor(s));

		return 0;
	}

	static int set_text_color(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		lua->col = StrToColor(lua_tostring(L, 1));
		return 0;
	}

	static int SetBackgroundColor(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		const char* s = lua_tostring(L, 1);
		if (lstrcmpi(s, "null") == 0)
			lua->bkmode = TRANSPARENT;
		else
		{
			lua->bkcol = StrToColor(s);
			lua->bkmode = OPAQUE;
		}

		return 0;
	}

	static int SetFont(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		LOGFONT font = {0};

		if (lua->font)
		{
			DeleteObject(lua->font);
		}

		auto font_size = luaL_checknumber(L, 1);
		const char* font_name = luaL_optstring(L, 2, "MS Gothic");
		const char* style = luaL_optstring(L, 3, "");

		// set the size of the font
		font.lfHeight = -MulDiv(font_size,
		                        GetDeviceCaps(lua->gdi_back_dc, LOGPIXELSY), 72);
		lstrcpyn(font.lfFaceName, font_name, LF_FACESIZE);
		font.lfCharSet = DEFAULT_CHARSET;
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
		lua->font = CreateFontIndirect(&font);
		return 0;
	}

	static int LuaTextOut(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		SetBkMode(lua->gdi_back_dc, lua->bkmode);
		SetBkColor(lua->gdi_back_dc, lua->bkcol);
		SetTextColor(lua->gdi_back_dc, lua->col);
		SelectObject(lua->gdi_back_dc, lua->font);

		int x = luaL_checknumber(L, 1);
		int y = luaL_checknumber(L, 2);
		const char* text = lua_tostring(L, 3);

		::TextOut(lua->gdi_back_dc, x, y, text, lstrlen(text));
		return 0;
	}

	static bool GetRectLua(lua_State* L, int idx, RECT* rect)
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

	static int GetTextExtent(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		const char* string = luaL_checkstring(L, 1);

		SelectObject(lua->gdi_back_dc, lua->font);

		SIZE size = {0};
		GetTextExtentPoint32(lua->gdi_back_dc, string, strlen(string), &size);

		lua_newtable(L);
		lua_pushinteger(L, size.cx);
		lua_setfield(L, -2, "width");
		lua_pushinteger(L, size.cy);
		lua_setfield(L, -2, "height");
		return 1;
	}

	static int LuaDrawText(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		SetBkMode(lua->gdi_back_dc, lua->bkmode);
		SetBkColor(lua->gdi_back_dc, lua->bkcol);
		SetTextColor(lua->gdi_back_dc, lua->col);
		SelectObject(lua->gdi_back_dc, lua->font);

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
		::DrawText(lua->gdi_back_dc, lua_tostring(L, 1), -1, &rect, format);
		return 0;
	}

	static int LuaDrawTextAlt(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		SetBkMode(lua->gdi_back_dc, lua->bkmode);
		SetBkColor(lua->gdi_back_dc, lua->bkcol);
		SetTextColor(lua->gdi_back_dc, lua->col);
		SelectObject(lua->gdi_back_dc, lua->font);

		RECT rect = {0};
		LPSTR string = (LPSTR)lua_tostring(L, 1);
		UINT format = luaL_checkinteger(L, 2);
		rect.left = luaL_checkinteger(L, 3);
		rect.top = luaL_checkinteger(L, 4);
		rect.right = luaL_checkinteger(L, 5);
		rect.bottom = luaL_checkinteger(L, 6);

		DrawTextEx(lua->gdi_back_dc, string, -1, &rect, format, NULL);
		return 0;
	}

	static int DrawRect(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		int left = luaL_checknumber(L, 1);
		int top = luaL_checknumber(L, 2);
		int right = luaL_checknumber(L, 3);
		int bottom = luaL_checknumber(L, 4);
		int cornerW = luaL_optnumber(L, 5, 0);
		int cornerH = luaL_optnumber(L, 6, 0);

		SelectObject(lua->gdi_back_dc, lua->brush);
		SelectObject(lua->gdi_back_dc, lua->pen);
		RoundRect(lua->gdi_back_dc, left, top, right, bottom, cornerW, cornerH);
		return 0;
	}

	static int LuaLoadImage(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		std::wstring path = string_to_wstring(luaL_checkstring(L, 1));

		auto img = new Gdiplus::Bitmap(path.c_str());
		if (!img || img->GetLastStatus())
		{
			luaL_error(L, "Couldn't find image '%s'", path.c_str());
			return 0;
		}

		lua->image_pool_index++;
		lua->image_pool[lua->image_pool_index] = img;

		lua_pushinteger(L, lua->image_pool_index);
		return 1;
	}

	static int DeleteImage(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		size_t key = luaL_checkinteger(L, 1);

		if (key == 0)
		{
			printf("Deleting all images\n");
			for (auto& val : lua->image_pool | std::views::values)
			{
				delete val;
			}
			lua->image_pool.clear();
		} else
		{
			if (!lua->image_pool.contains(key))
			{
				luaL_error(L, "Argument #1: Image index doesn't exist");
				return 0;
			}

			delete lua->image_pool[key];
			lua->image_pool[key] = nullptr;
			lua->image_pool.erase(key);
		}
		return 0;
	}

	static int DrawImage(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		size_t key = luaL_checkinteger(L, 1);

		if (!lua->image_pool.contains(key))
		{
			luaL_error(L, "Argument #1: Image index doesn't exist");
			return 0;
		}

		// Gets the number of arguments
		unsigned int args = lua_gettop(L);

		Gdiplus::Graphics gfx(lua->gdi_back_dc);
		Gdiplus::Bitmap* img = lua->image_pool[key];

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


	static int LoadScreen(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		// Copy screen into the loadscreen dc
		auto dc = GetDC(g_main_hwnd);
		BitBlt(lua->loadscreen_dc, 0, 0, lua->dc_size.width, lua->dc_size.height, dc, 0,
			   0, SRCCOPY);
		ReleaseDC(g_main_hwnd, dc);

		Gdiplus::Bitmap* out = new Gdiplus::Bitmap(lua->loadscreen_bmp, nullptr);

		lua->image_pool_index++;
		lua->image_pool[lua->image_pool_index] = out;

		lua_pushinteger(L, lua->image_pool_index);
		return 1;
	}

	static int LoadScreenReset(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		lua->destroy_loadscreen();
		lua->create_loadscreen();
		return 0;
	}

	static int GetImageInfo(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);
		size_t key = luaL_checkinteger(L, 1);

		if (!lua->image_pool.contains(key))
		{
			luaL_error(L, "Argument #1: Image index doesn't exist");
			return 0;
		}

		Gdiplus::Bitmap* img = lua->image_pool[key];

		lua_newtable(L);
		lua_pushinteger(L, img->GetWidth());
		lua_setfield(L, -2, "width");
		lua_pushinteger(L, img->GetHeight());
		lua_setfield(L, -2, "height");

		return 1;
	}

	//1st arg is table of points
	//2nd arg is color #xxxxxxxx
	static int FillPolygonAlpha(lua_State* L)
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

		Gdiplus::Graphics gfx(lua->gdi_back_dc);
		Gdiplus::SolidBrush brush(Gdiplus::Color(
			luaL_checkinteger(L, 2), luaL_checkinteger(L, 3),
			luaL_checkinteger(L, 4), luaL_checkinteger(L, 5)));
		gfx.FillPolygon(&brush, pts.data(), n);

		return 0;
	}


	static int FillEllipseAlpha(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		int x = luaL_checknumber(L, 1);
		int y = luaL_checknumber(L, 2);
		int w = luaL_checknumber(L, 3);
		int h = luaL_checknumber(L, 4);
		const char* col = luaL_checkstring(L, 5); //color string

		Gdiplus::Graphics gfx(lua->gdi_back_dc);
		Gdiplus::SolidBrush brush(Gdiplus::Color(StrToColorA(col, true)));

		gfx.FillEllipse(&brush, x, y, w, h);

		return 0;
	}

	static int FillRectAlpha(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		int x = luaL_checknumber(L, 1);
		int y = luaL_checknumber(L, 2);
		int w = luaL_checknumber(L, 3);
		int h = luaL_checknumber(L, 4);
		const char* col = luaL_checkstring(L, 5); //color string

		Gdiplus::Graphics gfx(lua->gdi_back_dc);
		Gdiplus::SolidBrush brush(Gdiplus::Color(StrToColorA(col, true)));

		gfx.FillRectangle(&brush, x, y, w, h);

		return 0;
	}

	static int FillRect(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		COLORREF color = RGB(
			luaL_checknumber(L, 5),
			luaL_checknumber(L, 6),
			luaL_checknumber(L, 7)
		);
		COLORREF colorold = SetBkColor(lua->gdi_back_dc, color);
		RECT rect;
		rect.left = luaL_checknumber(L, 1);
		rect.top = luaL_checknumber(L, 2);
		rect.right = luaL_checknumber(L, 3);
		rect.bottom = luaL_checknumber(L, 4);
		ExtTextOut(lua->gdi_back_dc, 0, 0, ETO_OPAQUE, &rect, "", 0, 0);
		SetBkColor(lua->gdi_back_dc, colorold);
		return 0;
	}

	static int DrawEllipse(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		SelectObject(lua->gdi_back_dc, lua->brush);
		SelectObject(lua->gdi_back_dc, lua->pen);

		int left = luaL_checknumber(L, 1);
		int top = luaL_checknumber(L, 2);
		int right = luaL_checknumber(L, 3);
		int bottom = luaL_checknumber(L, 4);

		::Ellipse(lua->gdi_back_dc, left, top, right, bottom);
		return 0;
	}

	static int DrawPolygon(lua_State* L)
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
		SelectObject(lua->gdi_back_dc, lua->brush);
		SelectObject(lua->gdi_back_dc, lua->pen);
		::Polygon(lua->gdi_back_dc, p, n);
		return 0;
	}

	static int DrawLine(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		SelectObject(lua->gdi_back_dc, lua->pen);
		::MoveToEx(lua->gdi_back_dc, luaL_checknumber(L, 1), luaL_checknumber(L, 2),
		           NULL);
		::LineTo(lua->gdi_back_dc, luaL_checknumber(L, 3), luaL_checknumber(L, 4));
		return 0;
	}

	static int SetClip(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);


		auto rgn = CreateRectRgn(luaL_checkinteger(L, 1),
		                         luaL_checkinteger(L, 2),
		                         luaL_checkinteger(L, 1) +
		                         luaL_checkinteger(L, 3),
		                         luaL_checkinteger(L, 2) + luaL_checkinteger(
			                         L, 4));
		SelectClipRgn(lua->gdi_back_dc, rgn);
		DeleteObject(rgn);
		return 0;
	}

	static int ResetClip(lua_State* L)
	{
		LuaEnvironment* lua = GetLuaClass(L);

		SelectClipRgn(lua->gdi_back_dc, NULL);
		return 0;
	}
}
