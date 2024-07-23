#include <view/lua/LuaConsole.h>
#include <Windows.h>
#include <commctrl.h>
#include <shared/services/FrontendService.h>
#include <view/gui/Main.h>
#include <core/r4300/tracelog.h>
#include <core/r4300/vcr.h>
#include <view/capture/EncodingManager.h>
#include <shared/Config.hpp>
#include <view/gui/features/MGECompositor.h>

#include "features/RomBrowser.hpp"
#include "features/Statusbar.hpp"

bool FrontendService::show_ask_dialog(const char* str, const char* title, bool warning, void* hwnd)
{
	if (g_config.silent_mode)
	{
		return true;
	}
	return MessageBox(static_cast<HWND>(hwnd ? hwnd : g_main_hwnd), str, title, MB_YESNO | (warning ? MB_ICONWARNING : MB_ICONQUESTION)) == IDYES;
}

void FrontendService::show_warning(const char* str, const char* title, void* hwnd)
{
	if (!g_config.silent_mode)
	{
		MessageBox(static_cast<HWND>(hwnd ? hwnd : g_main_hwnd), str, title, MB_ICONWARNING);
	}
}

void FrontendService::show_error(const char* str, const char* title, void* hwnd)
{
	if (!g_config.silent_mode)
	{
		MessageBox(static_cast<HWND>(hwnd ? hwnd : g_main_hwnd), str, title, MB_ICONERROR);
	}
}

void FrontendService::show_information(const char* str, const char* title, void* hwnd)
{
	if (!g_config.silent_mode)
	{
		MessageBox(static_cast<HWND>(hwnd ? hwnd : g_main_hwnd), str, title, MB_OK | MB_ICONINFORMATION);
	}
}

void FrontendService::show_statusbar(const char* str)
{
	Statusbar::post(str);
}

std::filesystem::path FrontendService::get_app_path()
{
	return g_app_path;
}

void FrontendService::set_default_hotkey_keys(t_config* config)
{
	config->fast_forward_hotkey.key = VK_TAB;

	config->gs_hotkey.key = 'G';

	config->speed_down_hotkey.key = VK_OEM_MINUS;

	config->speed_up_hotkey.key = VK_OEM_PLUS;

	config->frame_advance_hotkey.key = VK_OEM_5;

	config->pause_hotkey.key = VK_PAUSE;

	config->toggle_read_only_hotkey.key = 'R';
	config->toggle_read_only_hotkey.shift = true;

	config->toggle_movie_loop_hotkey.key = 'L';
	config->toggle_movie_loop_hotkey.shift = true;

	config->start_movie_playback_hotkey.key = 'P';
	config->start_movie_playback_hotkey.ctrl = true;
	config->start_movie_playback_hotkey.shift = true;

	config->start_movie_recording_hotkey.key = 'R';
	config->start_movie_recording_hotkey.ctrl = true;
	config->start_movie_recording_hotkey.shift = true;

	config->stop_movie_hotkey.key = 'C';
	config->stop_movie_hotkey.ctrl = true;
	config->stop_movie_hotkey.shift = true;

	config->take_screenshot_hotkey.key = VK_F12;

	config->play_latest_movie_hotkey.key = 'T';
	config->play_latest_movie_hotkey.ctrl = true;
	config->play_latest_movie_hotkey.shift = true;

	config->load_latest_script_hotkey.key = 'K';
	config->load_latest_script_hotkey.ctrl = true;
	config->load_latest_script_hotkey.shift = true;

	config->new_lua_hotkey.key = 'N';
	config->new_lua_hotkey.ctrl = true;

	config->close_all_lua_hotkey.key = 'W';
	config->close_all_lua_hotkey.ctrl = true;
	config->close_all_lua_hotkey.shift = true;

	config->load_rom_hotkey.key = 'O';
	config->load_rom_hotkey.ctrl = true;

	config->close_rom_hotkey.key = 'W';
	config->close_rom_hotkey.ctrl = true;

	config->reset_rom_hotkey.key = 'R';
	config->reset_rom_hotkey.ctrl = true;

	config->load_latest_rom_hotkey.key = 'O';
	config->load_latest_rom_hotkey.ctrl = true;
	config->load_latest_rom_hotkey.shift = true;

	config->fullscreen_hotkey.key = VK_RETURN;
	config->fullscreen_hotkey.alt = true;

	config->settings_hotkey.key = 'S';
	config->settings_hotkey.ctrl = true;

	config->toggle_statusbar_hotkey.key = 'S';
	config->toggle_statusbar_hotkey.alt = true;

	config->refresh_rombrowser_hotkey.key = VK_F5;
	config->refresh_rombrowser_hotkey.ctrl = true;

	config->seek_to_frame_hotkey.key = 'G';
	config->seek_to_frame_hotkey.ctrl = true;

	config->run_hotkey.key = 'P';
	config->run_hotkey.ctrl = true;

	config->cheats_hotkey.key = 'U';
	config->cheats_hotkey.ctrl = true;

	config->save_current_hotkey.key = 'I';

	config->load_current_hotkey.key = 'I';

	config->save_as_hotkey.key = 'N';
	config->save_as_hotkey.ctrl = true;

	config->load_as_hotkey.key = 'M';
	config->load_as_hotkey.ctrl = true;

	config->save_to_slot_1_hotkey.key = '1';
	config->save_to_slot_1_hotkey.shift = true;

	config->save_to_slot_2_hotkey.key = '2';
	config->save_to_slot_2_hotkey.shift = true;

	config->save_to_slot_3_hotkey.key = '3';
	config->save_to_slot_3_hotkey.shift = true;

	config->save_to_slot_4_hotkey.key = '4';
	config->save_to_slot_4_hotkey.shift = true;

	config->save_to_slot_5_hotkey.key = '5';
	config->save_to_slot_5_hotkey.shift = true;

	config->save_to_slot_6_hotkey.key = '6';
	config->save_to_slot_6_hotkey.shift = true;

	config->save_to_slot_7_hotkey.key = '7';
	config->save_to_slot_7_hotkey.shift = true;

	config->save_to_slot_8_hotkey.key = '8';
	config->save_to_slot_8_hotkey.shift = true;

	config->save_to_slot_9_hotkey.key = '9';
	config->save_to_slot_9_hotkey.shift = true;

	config->save_to_slot_10_hotkey.key = '0';
	config->save_to_slot_10_hotkey.shift = true;

	config->load_from_slot_1_hotkey.key = VK_F1;

	config->load_from_slot_2_hotkey.key = VK_F2;

	config->load_from_slot_3_hotkey.key = VK_F3;

	config->load_from_slot_4_hotkey.key = VK_F4;

	config->load_from_slot_5_hotkey.key = VK_F5;

	config->load_from_slot_6_hotkey.key = VK_F6;

	config->load_from_slot_7_hotkey.key = VK_F7;

	config->load_from_slot_8_hotkey.key = VK_F8;

	config->load_from_slot_9_hotkey.key = VK_F9;

	config->load_from_slot_10_hotkey.key = VK_F10;

	config->select_slot_1_hotkey.key =  '1';

	config->select_slot_2_hotkey.key =  '2';

	config->select_slot_3_hotkey.key =  '3';

	config->select_slot_4_hotkey.key =  '4';

	config->select_slot_5_hotkey.key =  '5';

	config->select_slot_6_hotkey.key =  '6';

	config->select_slot_7_hotkey.key =  '7';

	config->select_slot_8_hotkey.key =  '8';

	config->select_slot_9_hotkey.key =  '9';

	config->select_slot_10_hotkey.key = '0';
}

void* FrontendService::get_app_instance_handle()
{
	return g_app_instance;
}

void* FrontendService::get_main_window_handle()
{
	return g_main_hwnd;
}

void* FrontendService::get_statusbar_handle()
{
	return Statusbar::hwnd();
}

void* FrontendService::get_plugin_config_parent_handle()
{
	return g_hwnd_plug;
}

bool FrontendService::get_prefers_no_render_skip()
{
	return EncodingManager::is_capturing();
}

void FrontendService::update_screen()
{
	if (MGECompositor::available())
	{
		MGECompositor::update_screen();
	} else
	{
		updateScreen();
	}
}

void FrontendService::at_vi()
{
	EncodingManager::at_vi();
}

void FrontendService::ai_len_changed()
{
	EncodingManager::ai_len_changed();
}

std::string FrontendService::find_available_rom(const std::function<bool(const t_rom_header&)>& predicate)
{
	return Rombrowser::find_available_rom(predicate);
}

void FrontendService::mge_read_screen(void** dest, long* width, long* height)
{
	MGECompositor::read_screen(dest, width, height);
}

void FrontendService::mge_load_screen(void* data, long width, long height)
{
	MGECompositor::load_screen(data, width, height);
}


