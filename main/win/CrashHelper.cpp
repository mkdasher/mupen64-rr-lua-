#include "CrashHelper.h"
#include "main_win.h"
#include <vcr.h>
#include <Psapi.h>

#include "Config.hpp"


int CrashHelper::FindModuleName(char* error, void* addr, int len)
{
	HMODULE hMods[1024];
	HANDLE hProcess = GetCurrentProcess();
	DWORD cbNeeded;
	printf("addr: %p\n", addr);
	if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
	{
		HMODULE maxbase = nullptr;
		for (int i = 0; i < (int)(cbNeeded / sizeof(HMODULE)); i++)
		{
			//find closest addr
			if (hMods[i] > maxbase && hMods[i] < addr)
			{
				maxbase = hMods[i];
				char modname[MAX_PATH];
				GetModuleBaseName(hProcess, maxbase, modname,
				                  sizeof(modname) / sizeof(char));
				printf("%s: %p\n", modname, (void*)maxbase);
			}
		}
		// Get the full path to the module's file.
		char modname[MAX_PATH];
		if (GetModuleBaseName(hProcess, maxbase, modname,
		                      sizeof(modname) / sizeof(char)))
			// write the address with module
			return sprintf(error + len, "Addr:0x%p (%s 0x%p)\n", addr, modname,
			               (void*)maxbase);
	}
	return 0; //what
}

void CrashHelper::GetExceptionCodeFriendlyName(
	_EXCEPTION_POINTERS* exceptionPointersPtr, char* exceptionCodeStringPtr)
{
	switch (exceptionPointersPtr->ExceptionRecord->ExceptionCode)
	{
	case EXCEPTION_ACCESS_VIOLATION:
		strcpy(exceptionCodeStringPtr, "Access violation");
		break;
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		strcpy(exceptionCodeStringPtr,
		       "Attempted to access out-of-bounds array index");
		break;
	case EXCEPTION_FLT_DENORMAL_OPERAND:
		strcpy(exceptionCodeStringPtr,
		       "An operand in floating point operation was denormal");
		break;
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		strcpy(exceptionCodeStringPtr, "Float division by zero");
		break;
	case EXCEPTION_STACK_OVERFLOW:
		strcpy(exceptionCodeStringPtr, "Stack overflow");
		break;
	default:
		sprintf(exceptionCodeStringPtr, "%d",
		        (int)exceptionPointersPtr->ExceptionRecord->ExceptionCode);
		break;
	}
}


void CrashHelper::GenerateLog(_EXCEPTION_POINTERS* exceptionPointersPtr,
                              char* logPtr)
{
	int len = 0;

	if (exceptionPointersPtr != nullptr)
	{
		void* addr = exceptionPointersPtr->ExceptionRecord->ExceptionAddress;
		len += FindModuleName(logPtr, addr, len); //appends to error as well

		//emu info
#ifdef _DEBUG
		len += sprintf(logPtr + len, "Version: \"%s\" DEBUG\n", mupen_version);
#else
		len += sprintf(logPtr + len, "Version: \" %s \"\n", mupen_version);
#endif
		char exceptionCodeFriendly[1024] = {0};
		GetExceptionCodeFriendlyName(exceptionPointersPtr,
		                             exceptionCodeFriendly);
		len += sprintf(logPtr + len, "Exception code: %s (0x%08x)\n",
		               exceptionCodeFriendly,
		               (int)exceptionPointersPtr->ExceptionRecord->
		                                          ExceptionCode);
	} else
	{
		//emu info
#ifdef _DEBUG
		len += sprintf(logPtr + len, "Version:\"%s\" DEBUG\n", mupen_version);
#else
		len += sprintf(logPtr + len, "Version: \" %s \"\n", mupen_version);
#endif
		len += sprintf(logPtr + len,
		               "Exception code: unknown (no exception thrown, was crash log called manually?)\n");
	}
	len += sprintf(logPtr + len, "Gfx:%s\n",
	               Config.selected_video_plugin.c_str());
	len += sprintf(logPtr + len, "Input:%s\n",
	               Config.selected_input_plugin.c_str());
	len += sprintf(logPtr + len, "Audio:%s\n",
	               Config.selected_audio_plugin.c_str());
	len += sprintf(logPtr + len, "rsp:%s\n",
	               Config.selected_rsp_plugin.c_str());
	//some flags
	len += sprintf(logPtr + len, "m_task:%d\n", m_task);
	len += sprintf(logPtr + len, "emu_launched:%d\n", emu_launched);
	len += sprintf(logPtr + len, "is_capturing_avi:%d\n", vcr_is_capturing());

	strcpy(logPtr, logPtr); // ????
}
