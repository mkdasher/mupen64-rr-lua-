#include "CrashHelper.h"
#include "../Main.h"
#include <Psapi.h>
#include <core/r4300/r4300.h>
#include <core/r4300/vcr.h>
#include <shared/Config.hpp>

/**
 * \brief Gets additional information about the exception address
 * \param addr The address where the exception occurred
 * \return A string containing further information about the exception address
 */
std::string get_metadata_for_exception_address(void* addr)
{
	const HANDLE h_process = GetCurrentProcess();
	DWORD cb_needed;
	HMODULE h_mods[1024];

	if (EnumProcessModules(h_process, h_mods, sizeof(h_mods), &cb_needed))
	{
		HMODULE maxbase = nullptr;
		for (int i = 0; i < (int)(cb_needed / sizeof(HMODULE)); i++)
		{
			//find closest addr
			if (h_mods[i] > maxbase && h_mods[i] < addr)
			{
				maxbase = h_mods[i];
				char modname[MAX_PATH] = {0};
				GetModuleBaseName(h_process, maxbase, modname,
				                  sizeof(modname) / sizeof(char));
			}
		}
		// Get the full path to the module's file.
		char modname[MAX_PATH];
		if (GetModuleBaseName(h_process, maxbase, modname,
		                      sizeof(modname) / sizeof(char)))
			// write the address with module
			return std::format("Address: {:#08x} (closest: {} {:#08x})", (uintptr_t)addr, modname,
			                   (uintptr_t)maxbase);
	}

	// Whatever, module search failed so just return the absolute minimum
	return std::format("Address: {:#08x}", (uintptr_t)addr);
}

std::string get_exception_code_friendly_name(const _EXCEPTION_POINTERS* exception_pointers_ptr)
{
	std::string str;
	switch (exception_pointers_ptr->ExceptionRecord->ExceptionCode)
	{
	case EXCEPTION_ACCESS_VIOLATION:
		str = "Access violation";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		str = "Attempted to access out-of-bounds array index";
	case EXCEPTION_FLT_DENORMAL_OPERAND:
		str = "An operand in floating point operation was denormal";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		str = "Float division by zero";
	case EXCEPTION_STACK_OVERFLOW:
		str = "Stack overflow";
	default:
		break;
	}

	str += std::format(" ({:#08x})", exception_pointers_ptr->ExceptionRecord->ExceptionCode);
	return str;
}


std::string CrashHelper::generate_log(const _EXCEPTION_POINTERS* exception_pointers_ptr)
{
	std::string str;

	SYSTEMTIME time;
	GetSystemTime(&time);

	str += std::string(MUPEN_VERSION) + "\n";
	str += std::format("{:02}/{:02}/{} {:02}:{:02}:{:02}\n", time.wDay, time.wMonth, time.wYear, time.wHour, time.wMinute, time.wSecond);
	str += get_metadata_for_exception_address(exception_pointers_ptr->ExceptionRecord->ExceptionAddress) + "\n";
	str += get_exception_code_friendly_name(exception_pointers_ptr) + "\n";
	str += "Video: " + g_config.selected_video_plugin + "\n";
	str += "Audio: " + g_config.selected_audio_plugin + "\n";
	str += "Input: " + g_config.selected_input_plugin + "\n";
	str += "RSP: " + g_config.selected_rsp_plugin + "\n";
	str += "VCR Task: " + std::to_string(static_cast<int>(VCR::get_task())) + "\n";
	str += "Emu Launched: " + std::to_string(emu_launched) + "\n";
	str += "------------------------------------\n\n";

	return str;
}
