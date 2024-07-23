#include "PersistentPathDialog.h"
#include <shlobj.h>
#include <cstdio>
#include <shared/helpers/IOHelpers.h>
#include <shared/helpers/StringHelpers.h>
#include <shared/Config.hpp>
#include <view/helpers/WinHelpers.h>
#include <view/helpers/IOHelpers.h>

#define FAILSAFE(operation) if(FAILED(operation)) goto cleanUp

std::wstring show_persistent_open_dialog(const std::string& id, HWND hwnd,
                                         const std::wstring& filter)
{
	COMInitializer com_initializer;
	IFileDialog* pFileDialog = nullptr;
	IShellItem* pShellItem = nullptr;
	PWSTR pFilePath = nullptr;
	DWORD dwFlags;
	bool succeeded = false;

	FAILSAFE(
		CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pFileDialog)));

	FAILSAFE(pFileDialog->GetOptions(&dwFlags));


	{
		COMDLG_FILTERSPEC fileTypes[] =
		{
			{L"Supported files", filter.c_str()},
		};
		FAILSAFE(pFileDialog->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes));
	}
	IShellItem* shlPtr;

	{
		std::wstring restored_path = g_config.persistent_folder_paths.contains(id)
			                             ? g_config.persistent_folder_paths[id]
			                             : get_desktop_path();
		printf("Open dialog %s restored %ls\n", id.c_str(), restored_path.c_str());
		if (SHCreateItemFromParsingName(restored_path.c_str(), nullptr,
		                                IID_PPV_ARGS(&shlPtr)) != S_OK)
			printf(
				"Unable to create IShellItem from parsing lastPath name");
	}
	FAILSAFE(pFileDialog->SetFolder(shlPtr));
	// we need to pass null due to a shell api bug. ms has been informed
	FAILSAFE(pFileDialog->Show(nullptr));
	FAILSAFE(pFileDialog->GetResult(&pShellItem));
	FAILSAFE(pShellItem->GetDisplayName(SIGDN_FILESYSPATH, &pFilePath));

	g_config.persistent_folder_paths[id] = pFilePath;

	succeeded = true;

cleanUp:
	CoTaskMemFree(pFilePath);
	if (pShellItem) pShellItem->Release();
	if (pFileDialog) pFileDialog->Release();

	return succeeded ? g_config.persistent_folder_paths[id] : std::wstring();
}

std::wstring show_persistent_save_dialog(const std::string& id, HWND hwnd,
                                         const std::wstring& filter)
{
	COMInitializer com_initializer;
	IFileDialog* pFileDialog = nullptr;
	IShellItem* pShellItem = nullptr;
	PWSTR pFilePath = nullptr;
	DWORD dwFlags;
	bool succeeded = false;

	FAILSAFE(
		CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pFileDialog)));

	FAILSAFE(pFileDialog->GetOptions(&dwFlags));

	{
		COMDLG_FILTERSPEC fileTypes[] =
		{
			{L"Supported files", filter.c_str()},
		};
		FAILSAFE(pFileDialog->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes));
	}
	IShellItem* shlPtr;

	{
		std::wstring restored_path = g_config.persistent_folder_paths.contains(id)
			                             ? g_config.persistent_folder_paths[id]
			                             : get_desktop_path();
		printf("Save dialog %s restored %ls\n", id.c_str(), restored_path.c_str());
		if (SHCreateItemFromParsingName(restored_path.c_str(), nullptr,
		                                IID_PPV_ARGS(&shlPtr)) != S_OK)
			printf(
				"Unable to create IShellItem from parsing lastPath name");
	}

	// we are saving a file, use the filter list's first extension as the default suffix
	// because otherwise we get an extension-less file...
	{
		auto first_filter = split_wstring(filter, L";")[0];
		first_filter.erase(0, 2);
		pFileDialog->SetDefaultExtension(first_filter.c_str());
	}
	FAILSAFE(pFileDialog->SetFolder(shlPtr));
	// we need to pass null due to a shell api bug. ms has been informed
	FAILSAFE(pFileDialog->Show(nullptr));
	FAILSAFE(pFileDialog->GetResult(&pShellItem));
	FAILSAFE(pShellItem->GetDisplayName(SIGDN_FILESYSPATH, &pFilePath));

	g_config.persistent_folder_paths[id] = pFilePath;

	succeeded = true;

cleanUp:
	CoTaskMemFree(pFilePath);
	if (pShellItem) pShellItem->Release();
	if (pFileDialog) pFileDialog->Release();

	return succeeded ? g_config.persistent_folder_paths[id] : std::wstring();
}

std::wstring show_persistent_folder_dialog(const std::string& id, HWND hwnd)
{
	COMInitializer com_initializer;
	std::wstring final_path;
	IFileDialog* pfd;
	if (SUCCEEDED(
		CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pfd))))
	{
		std::wstring restored_path = g_config.persistent_folder_paths.contains(id)
			                             ? g_config.persistent_folder_paths[id]
			                             : get_desktop_path();

		PIDLIST_ABSOLUTE pidl;

		printf("Folder dialog %s restored %ls\n", id.c_str(), restored_path.c_str());
		HRESULT hresult = SHParseDisplayName(
			restored_path.c_str(), nullptr, &pidl, SFGAO_FOLDER, nullptr);
		if (SUCCEEDED(hresult))
		{
			IShellItem* psi;
			hresult = SHCreateShellItem(nullptr, nullptr, pidl, &psi);
			if (SUCCEEDED(hresult))
			{
				pfd->SetFolder(psi);
			}
			ILFree(pidl);
		}


		DWORD dwOptions;
		if (SUCCEEDED(pfd->GetOptions(&dwOptions)))
		{
			pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
		}
		// we need to pass null due to a shell api bug. ms has been informed
		if (SUCCEEDED(pfd->Show(nullptr)))
		{
			IShellItem* psi;
			if (SUCCEEDED(pfd->GetResult(&psi)))
			{
				WCHAR* tmp;
				if (SUCCEEDED(
					psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &tmp)))
				{
					final_path = tmp;
				}
				psi->Release();
			}
		}
		pfd->Release();
	}


	// HACK: fail if non-filesystem folders were picked
	if (final_path.find('{') != std::string::npos || final_path.find('}') != std::string::npos)
	{
		return L"";
	}

	if (final_path.size() > 1)
	{
		// probably valid path, store it
		g_config.persistent_folder_paths[id] = final_path;
	}

	return final_path;
}
