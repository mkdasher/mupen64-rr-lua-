#include <view/lua/LuaConsole.h>
#include <Windows.h>
#include <commctrl.h>
#include <shared/services/FrontendService.h>
#include <view/gui/main_win.h>
#include <core/r4300/tracelog.h>
#include <core/r4300/vcr.h>
#include <view/capture/EncodingManager.h>
#include <shared/Config.hpp>
#include <shared/services/MGECompositor.h>

#include "features/RomBrowser.hpp"
#include "features/Statusbar.hpp"

bool confirm_user_exit()
{
	if (Config.silent_mode)
	{
		return true;
	}

	int res = 0;
	int warnings = 0;

	std::string final_message;
	if (VCR::get_task() == e_task::recording)
	{
		final_message.append("Movie recording ");
		warnings++;
	}
	if (EncodingManager::is_capturing())
	{
		if (warnings > 0) { final_message.append(","); }
		final_message.append(" AVI capture ");
		warnings++;
	}
	if (tracelog::active())
	{
		if (warnings > 0) { final_message.append(","); }
		final_message.append(" Trace logging ");
		warnings++;
	}
	final_message.
		append("is running. Are you sure you want to stop emulation?");
	if (warnings > 0)
		res = MessageBox(mainHWND, final_message.c_str(), "Stop emulation?",
		                 MB_YESNO | MB_ICONWARNING);

	return res == IDYES || warnings == 0;
}

bool show_ask_dialog(const char* str, const char* title, bool warning, void* hwnd)
{
	if (Config.silent_mode)
	{
		return true;
	}
	return MessageBox(static_cast<HWND>(hwnd ? hwnd : mainHWND), str, title, MB_YESNO | (warning ? MB_ICONWARNING : MB_ICONQUESTION)) == IDYES;
}

void show_warning(const char* str, const char* title, void* hwnd)
{
	if (!Config.silent_mode)
	{
		MessageBox(static_cast<HWND>(hwnd ? hwnd : mainHWND), str, title, MB_ICONWARNING);
	}
}

void show_error(const char* str, const char* title, void* hwnd)
{
	if (!Config.silent_mode)
	{
		MessageBox(static_cast<HWND>(hwnd ? hwnd : mainHWND), str, title, MB_ICONERROR);
	}
}

void show_information(const char* str, const char* title, void* hwnd)
{
	if (!Config.silent_mode)
	{
		MessageBox(static_cast<HWND>(hwnd ? hwnd : mainHWND), str, title, MB_OK | MB_ICONINFORMATION);
	}
}

void statusbar_post(const std::string& text)
{
	Statusbar::post(text);
}

bool is_on_gui_thread()
{
	return GetCurrentThreadId() == g_ui_thread_id;
}

std::filesystem::path get_app_path()
{
	return app_path;
}

void set_default_hotkey_keys(CONFIG* config)
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

void* get_app_instance_handle()
{
	return app_instance;
}

void* get_main_window_handle()
{
	return mainHWND;
}

void* get_statusbar_handle()
{
	return Statusbar::hwnd();
}

void* get_plugin_config_parent_handle()
{
	return hwnd_plug;
}

bool get_prefers_no_render_skip()
{
	return EncodingManager::is_capturing();
}

void at_vi()
{
	EncodingManager::at_vi();
}

std::string find_available_rom(const std::function<bool(const t_rom_header&)>& predicate)
{
	return Rombrowser::find_available_rom(predicate);
}

