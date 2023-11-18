#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>
#include <commctrl.h>
#include "RomBrowser.hpp"

#include <chrono>

#include "Statusbar.hpp"
#include "Toolbar.hpp"
#include "../main_win.h"
#include "../../winproject/resource.h"
#include "helpers/io_helpers.h"
#include "helpers/string_helpers.h"
#include "../Config.hpp"
#include <cassert>
#include <thread>

DWORD id;
HWND rombrowser_hwnd = nullptr;
std::vector<t_rombrowser_entry*> rombrowser_entries;
int32_t rombrowser_is_loading = 0;

int CALLBACK rombrowser_compare(const LPARAM l_param1, const LPARAM l_param2, LPARAM _) {
	const auto first = rombrowser_entries[Config.rombrowser_sort_ascending ? l_param1 : l_param2];
	const auto second = rombrowser_entries[Config.rombrowser_sort_ascending ? l_param2 : l_param1];

	int32_t result = 0;

	switch (Config.rombrowser_sorted_column) {
		case 0:
			result = first->rom_header.Country_code - second->rom_header.Country_code;
			break;
		case 1:
			// BUG: these are not null terminated!!!
			result = _strcmpi((const char*)first->rom_header.nom, (const char*)second->rom_header.nom);
			break;
		case 2:
			result = _strcmpi((const char*)first->path.c_str(), (const char*)second->path.c_str());
			break;
		case 3:
			result = (int32_t)(first->size - second->size);
			break;
		default:
			assert(false);
			break;
	}

	if (result > 1) {
		result = 1;
	}
	if (result < -1) {
		result = -1;
	}

	return result;
}
void rombrowser_update_sort() {
	ListView_SortItems(rombrowser_hwnd, rombrowser_compare, nullptr);
}

void rombrowser_create()
{
	assert(rombrowser_hwnd == nullptr);

	RECT rcl, rtool, rstatus;
	GetClientRect(main_hwnd, &rcl);
	GetWindowRect(toolbar_hwnd, &rtool);
	GetWindowRect(statusbar_hwnd, &rstatus);

	rombrowser_hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
	                                 WS_TABSTOP | WS_VISIBLE | WS_CHILD |
	                                 LVS_SINGLESEL | LVS_REPORT |
	                                 LVS_SHOWSELALWAYS,
	                                 0, rtool.bottom - rtool.top,
	                                 rcl.right - rcl.left,
	                                 rcl.bottom - rcl.top - rtool.bottom + rtool
	                                 .top - rstatus.bottom + rstatus.top,
	                                 main_hwnd, (HMENU)IDC_ROMLIST,
	                                 app_instance,
	nullptr);

	ListView_SetExtendedListViewStyle(rombrowser_hwnd,
	                                  LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT |
	                                  LVS_EX_DOUBLEBUFFER);

	const HIMAGELIST h_small =
		ImageList_Create(16, 16, ILC_COLORDDB | ILC_MASK, 11, 0);

	HICON h_icon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_GERMANY));
	ImageList_AddIcon(h_small, h_icon);
	h_icon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_USA));
	ImageList_AddIcon(h_small, h_icon);
	h_icon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_JAPAN));
	ImageList_AddIcon(h_small, h_icon);
	h_icon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_EUROPE));
	ImageList_AddIcon(h_small, h_icon);
	h_icon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_AUSTRALIA));
	ImageList_AddIcon(h_small, h_icon);
	h_icon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_ITALIA));
	ImageList_AddIcon(h_small, h_icon);
	h_icon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_FRANCE));
	ImageList_AddIcon(h_small, h_icon);
	h_icon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_SPAIN));
	ImageList_AddIcon(h_small, h_icon);
	h_icon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_UNKNOWN));
	ImageList_AddIcon(h_small, h_icon);
	h_icon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_DEMO));
	ImageList_AddIcon(h_small, h_icon);
	h_icon = LoadIcon(app_instance, MAKEINTRESOURCE(IDI_BETA));
	ImageList_AddIcon(h_small, h_icon);
	ListView_SetImageList(rombrowser_hwnd, h_small, LVSIL_SMALL);

	LVCOLUMN lv_column = {0};
	lv_column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

	lv_column.iImage = 1;
	lv_column.cx = Config.rombrowser_column_widths[0];
	ListView_InsertColumn(rombrowser_hwnd, 0, &lv_column);

	lv_column.pszText = const_cast<LPTSTR>("Name");
	lv_column.cx = Config.rombrowser_column_widths[1];
	ListView_InsertColumn(rombrowser_hwnd, 1, &lv_column);

	lv_column.pszText = const_cast<LPTSTR>("Filename");
	lv_column.cx = Config.rombrowser_column_widths[2];
	ListView_InsertColumn(rombrowser_hwnd, 2, &lv_column);

	lv_column.pszText = const_cast<LPTSTR>("Size");
	lv_column.cx = Config.rombrowser_column_widths[3];
	ListView_InsertColumn(rombrowser_hwnd, 3, &lv_column);

	BringWindowToTop(rombrowser_hwnd);
}

int32_t rombrowser_country_code_to_image_index(const uint16_t country_code)
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

void rombrowser_add_rom(const int32_t index, t_rombrowser_entry* rombrowser_entry)
{
	LV_ITEM lv_item = {0};

	lv_item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
	lv_item.pszText = LPSTR_TEXTCALLBACK;
	lv_item.lParam = index;
	lv_item.iItem = index;
	lv_item.iImage = rombrowser_country_code_to_image_index(
		rombrowser_entry->rom_header.Country_code);
	ListView_InsertItem(rombrowser_hwnd, &lv_item);
	rombrowser_entries.push_back(rombrowser_entry);
}
void rombrowser_build_impl()
{
	const auto start_time = std::chrono::high_resolution_clock::now();

	// we disable redrawing because it would repaint after every added rom otherwise,
	// which is slow and causes flicker
	SendMessage(rombrowser_hwnd, WM_SETREDRAW, FALSE, 0);

	ListView_DeleteAllItems(rombrowser_hwnd);
	for (const auto entry : rombrowser_entries)
	{
		delete entry;
	}
	rombrowser_entries.clear();

	std::vector<std::string> rom_paths;

	// we aggregate all file paths and only filter them after we're done
	if (Config.is_rombrowser_recursion_enabled)
	{
		for (auto& path : Config.rombrowser_rom_paths)
		{
			auto file_paths = get_files_in_subdirectories(path + "\\");
			rom_paths.insert(rom_paths.end(), file_paths.begin(),
			                 file_paths.end());
		}
	} else
	{
		for (auto& path : Config.rombrowser_rom_paths)
		{
			auto file_paths = get_files_with_extension_in_directory(
				path + "\\", "*");
			rom_paths.insert(rom_paths.end(), file_paths.begin(),
			                 file_paths.end());
		}
	}

	std::vector<std::string> filtered_rom_paths;

	std::ranges::copy_if(rom_paths,
	                     std::back_inserter(filtered_rom_paths), [](const std::string& val)
	                     {
		                     char c_extension[260] = {0};
		                     _splitpath(val.c_str(), nullptr, nullptr, nullptr, c_extension);

		                     const auto extension = std::string(c_extension);
		                     return is_case_insensitive_equal(extension, ".z64") ||
			                     is_case_insensitive_equal(extension, ".n64") ||
			                     is_case_insensitive_equal(
				                     extension, ".v64")
			                     || is_case_insensitive_equal(extension, ".rom");
	                     });


	int32_t i = 0;
	for (auto& path : filtered_rom_paths)
	{
		FILE* f = fopen(path.c_str(), "rb");

		fseek(f, 0, SEEK_END);
		const uint64_t len = ftell(f);
		fseek(f, 0, SEEK_SET);

		if (len > sizeof(t_rom_header))
		{
			const auto header = (t_rom_header*)malloc(sizeof(t_rom_header));
			fread(header, sizeof(t_rom_header), 1, f);

			rom_byteswap((uint8_t*)header);

			const auto rombrowser_entry = new t_rombrowser_entry;
			rombrowser_entry->rom_header = *header;
			rombrowser_entry->path = path;
			rombrowser_entry->size = (unsigned int)(len);

			rombrowser_add_rom(i, rombrowser_entry);
			free(header);
		}


		fclose(f);

		i++;
	}
	rombrowser_update_sort();
	SendMessage(rombrowser_hwnd, WM_SETREDRAW, TRUE, 0);
	printf("Rombrowser loading took %dms\n", static_cast<int>((std::chrono::high_resolution_clock::now() - start_time).count() / 1'000'000));
}
void rombrowser_build()
{
	if (rombrowser_is_loading)
	{
		return;
	}

	std::thread([] ()
	{
		rombrowser_is_loading = 1;
		rombrowser_build_impl();
		rombrowser_is_loading = 0;
	}).detach();
}

void rombrowser_set_visibility(const int32_t is_visible)
{
	ShowWindow(rombrowser_hwnd, is_visible ? SW_SHOW : SW_HIDE);
}

void rombrowser_update_size()
{
	if (emu_launched) return;

	if (!IsWindow(rombrowser_hwnd))
		return;

	// TODO: clean up this mess
	RECT rc, rc_main;
	int32_t y = 0;
	GetClientRect(main_hwnd, &rc_main);
	const WORD n_width = (WORD)(rc_main.right - rc_main.left);
	WORD n_height = (WORD)(rc_main.bottom - rc_main.top);
	if (statusbar_hwnd)
	{
		GetWindowRect(statusbar_hwnd, &rc);
		n_height -= (WORD)(rc.bottom - rc.top);
	}
	if (toolbar_hwnd)
	{
		GetWindowRect(toolbar_hwnd, &rc);
		y += (WORD)(rc.bottom - rc.top);
	}
	MoveWindow(rombrowser_hwnd, 0, y, n_width, n_height - y, TRUE);
}



void rombrowser_notify(const LPARAM lparam)
{
	switch (LPNMHDR(lparam)->code)
	{
		case LVN_COLUMNCLICK:
		{
			const auto lv = (LPNMLISTVIEW)lparam;

			if (Config.rombrowser_sorted_column == lv->iSubItem) {
				Config.rombrowser_sort_ascending ^= 1;
			}

			Config.rombrowser_sorted_column = lv->iSubItem;

			rombrowser_update_sort();
			break;
		}
	case LVN_GETDISPINFO:
		{
			NMLVDISPINFO* plvdi = (NMLVDISPINFO*)lparam;
			const t_rombrowser_entry* rombrowser_entry = rombrowser_entries[plvdi->
			                                                                item.lParam];
			switch (plvdi->item.iSubItem)
			{
			case 1:
				strcpy(plvdi->item.pszText, (const char*)rombrowser_entry->rom_header.nom);
				break;
			case 2:
			{
				char filename[MAX_PATH] = {0};
				_splitpath(rombrowser_entry->path.c_str(), nullptr, nullptr,
						   filename, nullptr);
				strcpy(plvdi->item.pszText, filename);
				break;
			}
			case 3:
			{
				const std::string size = std::to_string(rombrowser_entry->size / (1024 * 1024)) +
					" MB";
				strcpy(plvdi->item.pszText, size.c_str());
				break;
			}
			default:
				break;
			}

			break;
		}
	case NM_DBLCLK:
		{
			const auto i = ListView_GetNextItem(rombrowser_hwnd, -1, LVNI_SELECTED);

			if (i == -1) break;

			LVITEM item = {0};
			item.mask = LVIF_PARAM;
			item.iItem = i;
			ListView_GetItem(rombrowser_hwnd, &item);
			strcpy(rom_path, rombrowser_entries[item.lParam]->path.c_str());
			CreateThread(nullptr, 0, start_rom, nullptr, 0, &id);
		}
		break;
	default: ;
	}
}
