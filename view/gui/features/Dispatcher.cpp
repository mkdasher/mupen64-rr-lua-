#include "Dispatcher.h"

#include <assert.h>
#include <mutex>
#include <queue>
#include <Windows.h>
#include "../Main.h"
#include <shared/services/FrontendService.h>

namespace Dispatcher
{
	std::queue<std::function<void()>> funcs;
	std::mutex dispatcher_mutex;

	void invoke(const std::function<void()>& func)
	{
		if (is_on_gui_thread())
		{
			func();
			return;
		}

		std::lock_guard lock(dispatcher_mutex);
		funcs.push(func);
		SendMessage(g_main_hwnd, WM_EXECUTE_DISPATCHER, 0, 0);
	}

	void execute()
	{
		while (!funcs.empty())
		{
			funcs.front()();
			funcs.pop();
		}
	}
}
