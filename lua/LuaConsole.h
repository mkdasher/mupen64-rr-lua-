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
#include "vcr_compress.h"

typedef struct s_window_procedure_params {
	HWND wnd;
	UINT msg;
	WPARAM w_param;
	LPARAM l_param;
} t_window_procedure_params;

/**
 * \brief Initializes the lua subsystem
 */
void lua_init();

/**
 * \brief Exits the lua subsystem
 */
void lua_exit();

/**
 * \brief Creates a lua window and runs the specified script
 * \param path The script's path
 */
void lua_create_and_run(const char* path);

/**
 * \brief Creates a lua window
 * \return The window's handle
 */
HWND lua_create();

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
static const char* const REG_LUACLASS = "C";
static const char* const REG_ATUPDATESCREEN = "S";
static const char* const REG_ATVI = "V";
static const char* const REG_ATINPUT = "I";
static const char* const REG_ATSTOP = "T";
static const char* const REG_SYNCBREAK = "B";
static const char* const REG_READBREAK = "R";
static const char* const REG_WRITEBREAK = "W";
static const char* const REG_WINDOWMESSAGE = "M";
static const char* const REG_ATINTERVAL = "N";
static const char* const REG_ATPLAYMOVIE = "PM";
static const char* const REG_ATSTOPMOVIE = "SM";
static const char* const REG_ATLOADSTATE = "LS";
static const char* const REG_ATSAVESTATE = "SS";
static const char* const REG_ATRESET = "RE";

struct EmulationLock
{
	EmulationLock();
	~EmulationLock();
};

class LuaEnvironment {
public:
	/**
	 * \brief Creates a lua environment and runs it if the operation succeeds
	 * \param path The script path
	 * \param wnd The associated window
	 * \return A pointer to a lua environment object, or NULL if the operation failed and an error string
	 */
	static std::pair<LuaEnvironment*, std::string> create(std::filesystem::path path, HWND wnd);

	/**
	 * \brief Stops, destroys and removes a lua environment from the environment map
	 * \param lua_environment The lua environment to destroy
	 */
	static void destroy(LuaEnvironment* lua_environment);

	// The path to the current lua script
	std::filesystem::path path;

	// The DC for GDI/GDI+ drawings
	HDC gdi_dc = nullptr;

	// The DC for D2D drawings
	HDC d2d_dc = nullptr;

	// Dimensions of both DCs
	int dc_width, dc_height = 0;

	// The Direct2D factory, whose lifetime is the renderer's
	ID2D1Factory* d2d_factory = nullptr;

	// The Direct2D render target, which points to the d2d_dc
	ID2D1DCRenderTarget* d2d_render_target = nullptr;

	// The DirectWrite factory, whose lifetime is the renderer's
	IDWriteFactory* dw_factory = nullptr;

	// The cache for DirectWrite text layouts
	std::unordered_map<uint64_t, IDWriteTextLayout*> dw_text_layouts;

	// The stack of render targets. The top is used for D2D calls.
	std::stack<ID2D1RenderTarget*> d2d_render_target_stack;

	// The pool of GDI+ images.
	// The user receives handles, which are indicies into this array.
	// BUG: When deleting an image, all handles after it become invalid. The user is not aware of this happening, and it affects global state (can be caused by libraries)
	std::vector<Gdiplus::Bitmap*> image_pool;

	bool LoadScreenInitialized = false;
	// LoadScreen variables
	HDC hwindowDC, hsrcDC;
	t_window_info windowSize{};
	HBITMAP hbitmap;

	HBRUSH brush;
	HPEN pen;
	HFONT font;
	COLORREF col, bkcol;
	int bkmode;

	/**
	 * \brief Destroys and stops the environment
	 */
	~LuaEnvironment();

	void create_renderer();
	void destroy_renderer();


	/**
	 * \brief Draws to the environment's DC
	 */
	void draw();

	//calls all functions that lua script has defined as callbacks, reads them from registry
	//returns true at fail
	bool invoke_callbacks_with_key(std::function<int(lua_State*)> function,
								   const char* key);

	// Deletes all the variables used in LoadScreen (avoid memory leaks)
	void LoadScreenDelete();

	// Initializes everything needed for LoadScreen
	void LoadScreenInit();
	HWND hwnd;
	lua_State* L;



private:
	void register_functions();

};

extern uint64_t inputCount;

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

/**
 * \brief Gets the lua environment associated to a lua state
 * \param L The lua state
 */
extern LuaEnvironment* GetLuaClass(lua_State* L);

/**
 * \brief Registers a function with a key to a lua state
 * \param L The lua state
 * \param key The function's key
 */
int RegisterFunction(lua_State* L, const char* key);

/**
 * \brief Unregisters a function with a key to a lua state
* \param L The lua state
 * \param key The function's key
 */
void UnregisterFunction(lua_State* L, const char* key);

#endif
