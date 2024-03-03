#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>
#include <commctrl.h>
#include "RomBrowser.hpp"

#include <chrono>

#include "Statusbar.hpp"
#include "../main_win.h"
#include "../../winproject/resource.h"
#include "helpers/io_helpers.h"
#include "helpers/string_helpers.h"
#include "../r4300/r4300.h"
#include "../Config.hpp"
#include <assert.h>
#include <thread>

#include "messenger.h"

using t_rombrowser_entry = struct s_rombrowser_entry
{
	std::string path;
	size_t size;
	t_rom_header rom_header;
};

HWND rombrowser_hwnd = nullptr;
std::vector<t_rombrowser_entry*> rombrowser_entries;
int32_t rombrowser_is_loading = 0;

namespace Rombrowser
{
	std::vector<std::string> find_available_roms()
	{
		std::vector<std::string> rom_paths;

		// we aggregate all file paths and only filter them after we're done
		if (Config.is_rombrowser_recursion_enabled)
		{
			for (auto path : Config.rombrowser_rom_paths)
			{
				auto file_paths = get_files_in_subdirectories(path);
				rom_paths.insert(rom_paths.end(), file_paths.begin(),
								 file_paths.end());
			}
		} else
		{
			for (auto path : Config.rombrowser_rom_paths)
			{
				auto file_paths = get_files_with_extension_in_directory(
					path, "*");
				rom_paths.insert(rom_paths.end(), file_paths.begin(),
								 file_paths.end());
			}
		}

		std::vector<std::string> filtered_rom_paths;

		std::ranges::copy_if(rom_paths, std::back_inserter(filtered_rom_paths), [](std::string val)
		 {
		    char c_extension[260] = {0};
		    _splitpath(val.c_str(), nullptr, nullptr, nullptr, c_extension);

		    return iequals(c_extension, ".z64")
		        || iequals(c_extension, ".n64")
		        || iequals(c_extension, ".v64")
		        || iequals(c_extension, ".rom");
		 });
		return filtered_rom_paths;
	}

	int CALLBACK rombrowser_compare(LPARAM lParam1, LPARAM lParam2, LPARAM _)
	{
		auto first = rombrowser_entries[Config.rombrowser_sort_ascending
			                                ? lParam1
			                                : lParam2];
		auto second = rombrowser_entries[Config.rombrowser_sort_ascending
			                                 ? lParam2
			                                 : lParam1];

		int32_t result = 0;

		switch (Config.rombrowser_sorted_column)
		{
		case 0:
			result = first->rom_header.Country_code - second->rom_header.
				Country_code;
			break;
		case 1:
			// BUG: these are not null terminated!!!
			result = _strcmpi((const char*)first->rom_header.nom,
			                  (const char*)second->rom_header.nom);
			break;
		case 2:
			result = _strcmpi((const char*)first->path.c_str(),
			                  (const char*)second->path.c_str());
			break;
		case 3:
			result = first->size - second->size;
			break;
		default:
			assert(false);
			break;
		}

		if (result > 1)
		{
			result = 1;
		}
		if (result < -1)
		{
			result = -1;
		}

		return result;
	}

	void rombrowser_update_sort()
	{
		ListView_SortItems(rombrowser_hwnd, rombrowser_compare, nullptr);
	}

	void rombrowser_create()
	{
		assert(rombrowser_hwnd == nullptr);

		RECT rcl{}, rtool{}, rstatus{};
		GetClientRect(mainHWND, &rcl);
		GetWindowRect(Statusbar::hwnd(), &rstatus);

		rombrowser_hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
		                                 WS_TABSTOP | WS_VISIBLE | WS_CHILD |
		                                 LVS_SINGLESEL | LVS_REPORT |
		                                 LVS_SHOWSELALWAYS,
		                                 0, rtool.bottom - rtool.top,
		                                 rcl.right - rcl.left,
		                                 rcl.bottom - rcl.top - rtool.bottom +
		                                 rtool
		                                 .top - rstatus.bottom + rstatus.top,
		                                 mainHWND, (HMENU)IDC_ROMLIST,
		                                 app_instance,
		                                 NULL);

		ListView_SetExtendedListViewStyle(rombrowser_hwnd,
		                                  LVS_EX_GRIDLINES |
		                                  LVS_EX_FULLROWSELECT |
		                                  LVS_EX_DOUBLEBUFFER);

		HIMAGELIST hSmall =
			ImageList_Create(16, 16, ILC_COLORDDB | ILC_MASK, 11, 0);
		HICON hIcon;

		hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_GERMANY));
		ImageList_AddIcon(hSmall, hIcon);
		hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_USA));
		ImageList_AddIcon(hSmall, hIcon);
		hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_JAPAN));
		ImageList_AddIcon(hSmall, hIcon);
		hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_EUROPE));
		ImageList_AddIcon(hSmall, hIcon);
		hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_AUSTRALIA));
		ImageList_AddIcon(hSmall, hIcon);
		hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_ITALIA));
		ImageList_AddIcon(hSmall, hIcon);
		hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_FRANCE));
		ImageList_AddIcon(hSmall, hIcon);
		hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_SPAIN));
		ImageList_AddIcon(hSmall, hIcon);
		hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_UNKNOWN));
		ImageList_AddIcon(hSmall, hIcon);
		hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_DEMO));
		ImageList_AddIcon(hSmall, hIcon);
		hIcon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_BETA));
		ImageList_AddIcon(hSmall, hIcon);
		ListView_SetImageList(rombrowser_hwnd, hSmall, LVSIL_SMALL);

		LVCOLUMN lv_column = {0};
		lv_column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

		lv_column.iImage = 1;
		lv_column.cx = Config.rombrowser_column_widths[0];
		ListView_InsertColumn(rombrowser_hwnd, 0, &lv_column);

		lv_column.pszText = (LPTSTR)"Name";
		lv_column.cx = Config.rombrowser_column_widths[1];
		ListView_InsertColumn(rombrowser_hwnd, 1, &lv_column);

		lv_column.pszText = (LPTSTR)"Filename";
		lv_column.cx = Config.rombrowser_column_widths[2];
		ListView_InsertColumn(rombrowser_hwnd, 2, &lv_column);

		lv_column.pszText = (LPTSTR)"Size";
		lv_column.cx = Config.rombrowser_column_widths[3];
		ListView_InsertColumn(rombrowser_hwnd, 3, &lv_column);

		BringWindowToTop(rombrowser_hwnd);
	}

	int32_t rombrowser_country_code_to_image_index(uint16_t country_code)
	{
		switch (country_code & 0xFF)
		{
		case 0:
			return 9;
		case '7':
			return 10;
		case 0x44:
			return 0; // IDI_GERMANY
		case 0x45:
			return 1; // IDI_USA
		case 0x4A:
			return 2; // IDI_JAPAN
		case 0x20:
		case 0x21:
		case 0x38:
		case 0x70:
		case 0x50:
		case 0x58:
			return 3; // IDI_EUROPE
		case 0x55:
			return 4; // IDI_AUSTRALIA
		case 'I':
			return 5; // Italy
		case 0x46: // IDI_FRANCE
			return 6;
		case 'S': //SPAIN
			return 7;
		default:
			return 8; // IDI_N64CART
		}
	}

	void rombrowser_add_rom(int32_t index, t_rombrowser_entry* rombrowser_entry)
	{
		rombrowser_entries.push_back(rombrowser_entry);

		LV_ITEM lv_item = {0};

		lv_item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
		lv_item.pszText = LPSTR_TEXTCALLBACK;
		lv_item.lParam = index;
		lv_item.iItem = index;
		lv_item.iImage = rombrowser_country_code_to_image_index(
			rombrowser_entry->rom_header.Country_code);
		ListView_InsertItem(rombrowser_hwnd, &lv_item);
	}

	void rombrowser_build_impl()
	{
		auto start_time = std::chrono::high_resolution_clock::now();

		// we disable redrawing because it would repaint after every added rom otherwise,
		// which is slow and causes flicker
		SendMessage(rombrowser_hwnd, WM_SETREDRAW, FALSE, 0);

		ListView_DeleteAllItems(rombrowser_hwnd);
		for (auto entry : rombrowser_entries)
		{
			delete entry;
		}
		rombrowser_entries.clear();

		auto rom_paths = find_available_roms();

		int32_t i = 0;
		for (auto& path : rom_paths)
		{
			FILE* f = fopen(path.c_str(), "rb");

			fseek(f, 0, SEEK_END);
			uint64_t len = ftell(f);
			fseek(f, 0, SEEK_SET);

			auto rombrowser_entry = new t_rombrowser_entry;
			rombrowser_entry->path = path;
			rombrowser_entry->size = len;
			rombrowser_entry->rom_header = {};

			if (len > sizeof(t_rom_header))
			{
				auto header = (t_rom_header*)malloc(sizeof(t_rom_header));
				fread(header, sizeof(t_rom_header), 1, f);

				rom_byteswap((uint8_t*)header);

				rombrowser_entry->rom_header = *header;

				free(header);
			}

			rombrowser_add_rom(i, rombrowser_entry);


			fclose(f);

			i++;
		}
		rombrowser_update_sort();
		SendMessage(rombrowser_hwnd, WM_SETREDRAW, TRUE, 0);
		printf("Rombrowser loading took %dms\n",
		       static_cast<int>((std::chrono::high_resolution_clock::now() -
			       start_time).count() / 1'000'000));
	}

	void build()
	{
		if (rombrowser_is_loading)
		{
			return;
		}

		std::thread([]()
		{
			rombrowser_is_loading = 1;
			rombrowser_build_impl();
			rombrowser_is_loading = 0;
		}).detach();
	}

	void rombrowser_update_size()
	{
		if (emu_launched) return;

		if (!IsWindow(rombrowser_hwnd))
			return;

		// TODO: clean up this mess
		RECT rc, rc_main;
		WORD n_width, n_height;
		int32_t y = 0;
		GetClientRect(mainHWND, &rc_main);
		n_width = rc_main.right - rc_main.left;
		n_height = rc_main.bottom - rc_main.top;
		if (Statusbar::hwnd())
		{
			GetWindowRect(Statusbar::hwnd(), &rc);
			n_height -= (WORD)(rc.bottom - rc.top);
		}
		MoveWindow(rombrowser_hwnd, 0, y, n_width, n_height - y, TRUE);
	}


	void notify(LPARAM lparam)
	{
		switch (((LPNMHDR)lparam)->code)
		{
		case LVN_COLUMNCLICK:
			{
				auto lv = (LPNMLISTVIEW)lparam;

				if (Config.rombrowser_sorted_column == lv->iSubItem)
				{
					Config.rombrowser_sort_ascending ^= 1;
				}

				Config.rombrowser_sorted_column = lv->iSubItem;

				rombrowser_update_sort();
				break;
			}
		case LVN_GETDISPINFO:
			{
				NMLVDISPINFO* plvdi = (NMLVDISPINFO*)lparam;
				t_rombrowser_entry* rombrowser_entry = rombrowser_entries[plvdi
					->
					item.lParam];
				switch (plvdi->item.iSubItem)
				{
				case 1:
					strcpy(plvdi->item.pszText,
					       (const char*)rombrowser_entry->rom_header.nom);
					break;
				case 2:
					{
						char filename[MAX_PATH] = {0};
						_splitpath(rombrowser_entry->path.c_str(), NULL, NULL,
						           filename, NULL);
						strcpy(plvdi->item.pszText, filename);
						break;
					}
				case 3:
					{
						std::string size = std::to_string(
								rombrowser_entry->size / (1024 * 1024)) +
							" MB";
						strcpy(plvdi->item.pszText, size.c_str());
						break;
					}
				default:
					break;
				}

				break;
			}
			break;
		case NM_DBLCLK:
			{
				int32_t i = ListView_GetNextItem(
					rombrowser_hwnd, -1, LVNI_SELECTED);

				if (i == -1) break;

				LVITEM item = {0};
				item.mask = LVIF_PARAM;
				item.iItem = i;
				ListView_GetItem(rombrowser_hwnd, &item);
				auto path = rombrowser_entries[item.lParam]->path;
				std::thread([path] { start_rom(path); }).detach();
			}
			break;
		}
	}


	std::string find_available_rom(std::function<bool(const t_rom_header&)> predicate)
	{
		auto rom_paths = find_available_roms();
		for (auto rom_path : rom_paths)
		{
			FILE* f = fopen(rom_path.c_str(), "rb");

			fseek(f, 0, SEEK_END);
			uint64_t len = ftell(f);
			fseek(f, 0, SEEK_SET);

			if (len > sizeof(t_rom_header))
			{
				auto header = (t_rom_header*)malloc(sizeof(t_rom_header));
				fread(header, sizeof(t_rom_header), 1, f);

				rom_byteswap((uint8_t*)header);

				if (predicate(*header))
				{
					free(header);
					fclose(f);
					return rom_path;
				}

				free(header);
			}

			fclose(f);
		}

		return "";
	}

	void emu_launched_changed(std::any data)
	{
		auto value = std::any_cast<bool>(data);
		ShowWindow(rombrowser_hwnd, !value ? SW_SHOW : SW_HIDE);
	}

	void init()
	{
		rombrowser_create();
		Messenger::subscribe(Messenger::Message::EmuLaunchedChanged, emu_launched_changed);
		Messenger::subscribe(Messenger::Message::StatusbarVisibilityChanged, [] (auto _)
		{
			rombrowser_update_size();
		});
		Messenger::subscribe(Messenger::Message::SizeChanged, [] (auto _)
		{
			rombrowser_update_size();
		});
		Messenger::subscribe(Messenger::Message::ConfigSaving, [] (auto _)
		{
			for (int i = 0; i < Config.rombrowser_column_widths.size(); ++i)
			{
				Config.rombrowser_column_widths[i] = ListView_GetColumnWidth(rombrowser_hwnd, i);
			}
		});
	}
}
