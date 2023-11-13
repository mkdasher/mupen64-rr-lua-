#include "LuaConsole.h"
#include "Recent.h"
#include <Windows.h>
#include "win/main_win.h"
#include "../winproject/resource.h"
#include "../main/win/Config.hpp"

void lua_recent_scripts_build(const int32_t reset)
{
	const HMENU h_menu = GetMenu(main_hwnd);
	for (size_t i = 0; i < Config.recent_lua_script_paths.size(); i++)
	{
		if (Config.recent_lua_script_paths[i].empty())
		{
			continue;
		}
		DeleteMenu(h_menu, ID_LUA_RECENT + i, MF_BYCOMMAND);
	}

	if (reset)
	{
		Config.recent_lua_script_paths.clear();
	}

	HMENU h_sub_menu = GetSubMenu(h_menu, 6);
	h_sub_menu = GetSubMenu(h_sub_menu, 3);

	MENUITEMINFO menu_info = {0};
	menu_info.cbSize = sizeof(MENUITEMINFO);
	menu_info.fMask = MIIM_TYPE | MIIM_ID;
	menu_info.fType = MFT_STRING;
	menu_info.fState = MFS_ENABLED;

	for (size_t i = 0; i < Config.recent_lua_script_paths.size(); i++)
	{
		if (Config.recent_lua_script_paths[i].empty())
		{
			continue;
		}
		menu_info.dwTypeData = const_cast<LPSTR>(Config.recent_lua_script_paths[i].c_str());
		menu_info.cch = strlen(menu_info.dwTypeData);
		menu_info.wID = ID_LUA_RECENT + i;
		InsertMenuItem(h_sub_menu, i + 3, TRUE, &menu_info);
	}
}

void lua_recent_scripts_add(const std::string& path)
{
	if (Config.is_recent_scripts_frozen)
	{
		return;
	}
	if (Config.recent_lua_script_paths.size() > 5)
	{
		Config.recent_lua_script_paths.pop_back();
	}
	std::erase(Config.recent_lua_script_paths, path);
	Config.recent_lua_script_paths.insert(
		Config.recent_lua_script_paths.begin(), path);
	lua_recent_scripts_build();
}

int32_t lua_recent_scripts_run(const uint16_t menu_item_id)
{
	if (const int index = menu_item_id - ID_LUA_RECENT; index >= 0 && index < (int)Config.recent_lua_script_paths.size())
	{
		lua_create_and_run(Config.recent_lua_script_paths[index].c_str(), false);
		return 1;
	}
	return 0;
}
