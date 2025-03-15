/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include <Config.h>
#include <DialogService.h>


#include <winhttp.h>
#include <gui/Loggers.h>
#include <gui/Main.h>
#include <gui/features/UpdateChecker.h>
#include <json/json.hpp>

namespace UpdateChecker
{
    const std::wstring REPO_LATEST_RELEASE_URL = L"/repos/mkdasher/mupen64-rr-lua-/releases/latest";

    /**
     * Gets information about the latest release using the Github REST API.
     */
    std::string get_latest_release_as_json()
    {
        HINTERNET h_session = WinHttpOpen(L"Win32 Mupen64 GitHub API Client/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);

        if (!h_session)
        {
            g_view_logger->error("[UpdateChecker] WinHttpOpen failed");
            return "";
        }

        HINTERNET h_connect = WinHttpConnect(h_session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);

        if (!h_connect)
        {
            g_view_logger->error("[UpdateChecker] WinHttpConnect failed");
            WinHttpCloseHandle(h_session);
            return "";
        }

        HINTERNET h_request = WinHttpOpenRequest(h_connect, L"GET", REPO_LATEST_RELEASE_URL.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

        if (!h_request)
        {
            g_view_logger->error("[UpdateChecker] WinHttpOpenRequest failed");
            WinHttpCloseHandle(h_connect);
            WinHttpCloseHandle(h_session);
            return "";
        }

        BOOL b_results = WinHttpSendRequest(h_request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

        if (!b_results)
        {
            g_view_logger->error("[UpdateChecker] WinHttpSendRequest failed");
            WinHttpCloseHandle(h_request);
            WinHttpCloseHandle(h_connect);
            WinHttpCloseHandle(h_session);
            return "";
        }

        b_results = WinHttpReceiveResponse(h_request, NULL);

        if (!b_results)
        {
            g_view_logger->error("[UpdateChecker] WinHttpSendRequest failed");
            WinHttpCloseHandle(h_request);
            WinHttpCloseHandle(h_connect);
            WinHttpCloseHandle(h_session);
            return "";
        }

        std::stringstream response_stream;
        DWORD size = 0;
        DWORD downloaded = 0;
        char buf[4096]{};
        do
        {
            size = 0;
            if (WinHttpQueryDataAvailable(h_request, &size) && size > 0 && WinHttpReadData(h_request, buf, size, &downloaded))
            {
                response_stream.write(buf, downloaded);
            }
        }
        while (size > 0);

        WinHttpCloseHandle(h_request);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);

        return response_stream.str();
    }

    /**
     * Gets the download URL for the SSE2 build of Mupen in the provided release json.
     */
    std::string find_exe_download_url(nlohmann::json data)
    {
        const auto assets = data["assets"];

        if (!assets.is_array())
        {
            return "";
        }

        for (const auto& asset : assets)
        {
            const auto& name = asset["name"];

            if (!name.is_string())
            {
                continue;
            }

            if (!name.get<std::string>().contains("sse2"))
            {
                continue;
            }

            const auto browser_download_url = asset["browser_download_url"];

            if (!browser_download_url.is_string())
            {
                continue;
            }

            return browser_download_url.get<std::string>();
        }

        return "";
    }

    /**
     * Compares two version strings.
     *
     * Returns:
     * 1 if LHS > RHS
     * 0 if LHS = RHS
     * -1 if LHS < RHS
     */
    int version_compare(const std::wstring& version1, const std::wstring& version2)
    {
        auto split_version = [](const std::wstring& version) {
            std::vector parts(4, 0);
            const std::size_t dash_pos = version.find(L'-');
            std::wstring main_part = dash_pos != std::wstring::npos ? version.substr(0, dash_pos) : version;
            const std::wstring sub_part = dash_pos != std::wstring::npos ? version.substr(dash_pos + 1) : L"";

            std::wstringstream ss(main_part);
            for (int i = 0; i < 3 && std::getline(ss, main_part, L'.'); ++i)
            {
                parts[i] = std::stoi(main_part);
            }

            if (!sub_part.empty())
            {
                parts[3] = std::stoi(sub_part);
            }

            return parts;
        };

        const std::vector<int> parts1 = split_version(version1);
        const std::vector<int> parts2 = split_version(version2);

        for (int i = 0; i < 4; ++i)
        {
            if (parts1[i] > parts2[i])
            {
                return 1;
            }
            if (parts1[i] < parts2[i])
            {
                return -1;
            }
        }
        return 0;
    }

    /**
     * Downloads the executable from the specified release.
     */
    void download_executable(const nlohmann::json& data)
    {
        const auto download_url = find_exe_download_url(data);

        if (download_url.empty())
        {
            g_view_logger->error("[UpdateChecker] find_exe_download_url failed");
            return;
        }

        ShellExecute(0, 0, string_to_wstring(download_url).c_str(), 0, 0, SW_SHOW);
        PostMessage(g_main_hwnd, WM_CLOSE, 0, 0);
    }

    void check(bool manual)
    {
        if (!manual && !g_config.automatic_update_checking)
        {
            g_view_logger->trace("[UpdateChecker] Automatic update checking disabled. Ignoring update check.");
            return;
        }

        const auto json = get_latest_release_as_json();

        if (json.empty())
        {
            return;
        }

        nlohmann::json data = nlohmann::json::parse(json);

        const auto tag_name = data["tag_name"];

        if (!tag_name.is_string())
        {
            g_view_logger->error("[UpdateChecker] no tag_name in json response");
            return;
        }

        auto version = string_to_wstring(tag_name.get<std::string>());

        if (!manual && g_config.ignored_version == version)
        {
            g_view_logger->trace(L"[UpdateChecker] Version {} ignored by user. Skipping showing update dialog.", version);
            return;
        }

        const auto version_difference = version_compare(CURRENT_VERSION, version);

        if (version_difference != -1)
        {
            if (manual)
            {
                DialogService::show_dialog(L"You are already up-to-date.", L"Already up-to-date", fsvc_information);
            }

            return;
        }

        const auto result = DialogService::show_multiple_choice_dialog(VIEW_DLG_UPDATE_DIALOG, {L"Update now", L"Skip this version", L"Ignore"}, std::format(L"Mupen64 {} is available for download.", version).c_str(), L"Update Available", fsvc_information);

        if (result == 1)
        {
            g_config.ignored_version = version;
            return;
        }

        if (result == 2)
        {
            return;
        }

        download_executable(data);
    }
} // namespace UpdateChecker
