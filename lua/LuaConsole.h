#ifndef LUA_CONSOLE_H
#define LUA_CONSOLE_H

#include "include/lua.hpp"

#include <Windows.h>
#include <locale>
#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <cstdint>
#include <gdiplus.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <functional>
#include <queue>
#include <md5.h>
#include <assert.h>
#include <filesystem>
#include <mutex>
#include <stack>

#include "plugin.hpp"

typedef struct s_window_procedure_params {
	HWND wnd;
	UINT msg;
	WPARAM w_param;
	LPARAM l_param;
} t_window_procedure_params;

/**
 * \brief Creates a lua window and runs the specified script
 * \param path The script's path
 * \param minimized Whether the window starts out minimized
 */
void lua_create_and_run(const char* path, bool minimized);

/**
 * \brief Creates and shows a lua window with the specified flags
 * \param sw_flags The flags to call ShowWindow with
 * \return The window's handle
 */
HWND create_and_show_lua_window(int32_t sw_flags);

void NewLuaScript();
void LuaWindowMessage(HWND, UINT, WPARAM, LPARAM);

void ConsoleWrite(HWND wnd, const char* str);
void AtUpdateScreenLuaCallback();
void AtVILuaCallback();
void AtInputLuaCallback(int n);
void AtIntervalLuaCallback();
void AtPlayMovieLuaCallback();
void AtStopMovieLuaCallback();
void AtLoadStateLuaCallback();
void AtSaveStateLuaCallback();
void AtResetLuaCallback();
void LuaTraceLoggingPure();
void LuaTraceLoggingInterpOps();
void LuaTraceLogState();

/**
 * \brief Stops all lua scripts and closes their windows
 */
void close_all_scripts();

/**
 * \brief Stops all lua scripts
 */
void stop_all_scripts();

void instrStr1(unsigned long pc, unsigned long w, char* buffer);

static uint32_t bitmap_color_mask = RGB(255, 0, 255);

enum class Renderer {
	None,
	GDIMixed,
	Direct2D
};

class LuaEnvironment {
public:
	bool stopping = false;

	/**
	 * \brief Creates a lua environment and runs it if the operation succeeds
	 * \param path The script path
	 * \param wnd The associated window
	 * \return A pointer to a lua environment object, or NULL if the operation failed and an error string
	 */
	static std::pair<LuaEnvironment*, std::string> create(std::filesystem::path path, HWND wnd);
	static void destroy(LuaEnvironment* lua_environment);
	Renderer renderer = Renderer::GDIMixed;

	std::filesystem::path path;
	HDC dc = nullptr;
	int dc_width, dc_height = 0;
	ID2D1Factory* d2d_factory = nullptr;
	ID2D1DCRenderTarget* d2d_render_target = nullptr;
	IDWriteFactory* dw_factory = nullptr;
	std::unordered_map<uint32_t, ID2D1SolidColorBrush*> d2d_brush_cache;
	std::unordered_map<std::string, ID2D1Bitmap*> d2d_bitmap_cache;
	std::unordered_map<std::string, ID2D1BitmapRenderTarget*> d2d_bitmap_render_target;
	std::stack<ID2D1RenderTarget*> d2d_render_target_stack;

	/**
	 * \brief Destroys and stops the environment
	 */
	~LuaEnvironment();

	void create_renderer();
	void destroy_renderer();

	void draw();

	void setBrush(HBRUSH h);
	void selectBrush();
	void setPen(HPEN h);
	void selectPen();
	void setFont(HFONT h);
	void selectFont();
	void setTextColor(COLORREF c);
	void selectTextColor();
	void setBackgroundColor(COLORREF c, int mode = OPAQUE);
	void selectBackgroundColor();
	//calls all functions that lua script has defined as callbacks, reads them from registry
	//returns true at fail
	bool invoke_callbacks_with_key(std::function<int(lua_State*)> function,
								   const char* key);

	ID2D1Bitmap* get_loose_bitmap(const char* identifier) {

		// look for bitmap in the normal cache
		if (d2d_bitmap_cache.contains(identifier)) {
			return d2d_bitmap_cache[identifier];
		}

		// look for it in render target map
		if (d2d_bitmap_render_target.contains(identifier)) {
			ID2D1Bitmap* bitmap;
			d2d_bitmap_render_target[identifier]->GetBitmap(&bitmap);
			return bitmap;
		}

		return nullptr;
	}
	HWND hwnd;
	lua_State* L;


private:
	void register_functions();
	void setGDIObject(HGDIOBJ* save, HGDIOBJ newobj);
	void selectGDIObject(HGDIOBJ p);
	void deleteGDIObject(HGDIOBJ p, int stockobj);
	// gdi objects are filled in on run(), then deleted on stop()
	HBRUSH brush;
	HPEN pen;
	HFONT font;
	COLORREF col, bkcol;
	int bkmode;
};

extern bool enableTraceLog;
extern bool traceLogMode;
extern unsigned long gdiPlusToken;

/**
 * \brief The controller data at time of the last input poll
 */
extern BUTTONS last_controller_data[4];

/**
 * \brief The modified control data to be pushed the next frame
 */
extern BUTTONS new_controller_data[4];

/**
 * \brief Whether the <c>new_controller_data</c> of a controller should be pushed the next frame
 */
extern bool overwrite_controller_data[4];

extern std::map<HWND, LuaEnvironment*> hwnd_lua_map;



#endif
