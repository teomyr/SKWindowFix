#include "skse/PluginAPI.h"
#include "Version.h"
#include <Windows.h>

IDebugLog gLog("skwindowfix.log");

bool scheduleActivateApp = false;

LRESULT CALLBACK CallWndProcRet(int code, WPARAM wParam, LPARAM lParam) {
	// Called after a window message has been processed by the game

	if (code == HC_ACTION) {
		CWPRETSTRUCT *msg = (CWPRETSTRUCT *)lParam;
		int cursorVisibility = 0;
		DWORD gainingProcessId = 0;

		switch (msg->message) {
			case WM_ACTIVATEAPP:
				// Usually sent first by Windows when switching to or from the
				// Skyrim application.
				// The game, however, seems to expect this message *after* the
				// individual WM_ACTIVATE message for the game window.
				_DMESSAGE("WM_ACTIVATEAPP, active = %d", msg->wParam);

				// If this is an activating message, schedule WM_ACTIVATEAPP to
				// be sent again after WM_ACTIVATE.
				scheduleActivateApp = (msg->wParam == 1);

				break;
			case WM_ACTIVATE:
				// Usually sent second by windows when the game window is being
				// activated.
				_DMESSAGE("WM_ACTIVATE, active = %d", msg->wParam);

				if (scheduleActivateApp && (msg->wParam == WA_ACTIVE
					|| msg->wParam == WA_CLICKACTIVE)) {

					// Now that the window is active, send WM_ACTIVATEAPP again
					// to activate the application itself.
					_VMESSAGE("Sending WM_ACTIVATEAPP to restore game");

					scheduleActivateApp = false;
					SendMessage(msg->hwnd, WM_ACTIVATEAPP, true, NULL);
				}

				break;
			case WM_SETFOCUS:
				_DMESSAGE("WM_SETFOCUS");

				// Hide the cursor upon gaining focus

				do {
					cursorVisibility = ShowCursor(false);
				} while (cursorVisibility >= 0);

				break;
			case WM_KILLFOCUS:
				_DMESSAGE("WM_KILLFOCUS");

				// Show the cursor upon losing focus to another process

				if (msg->wParam != NULL) {
					GetWindowThreadProcessId((HWND)(msg->wParam),
						&gainingProcessId);
				}

				if (gainingProcessId != GetCurrentProcessId()) {
					do {
						cursorVisibility = ShowCursor(true);
					} while (cursorVisibility < 0);
				}

				break;
			default:
				break;
		}
	}

	return CallNextHookEx(NULL, code, wParam, lParam);
}

bool SKSEPlugin_Query(const SKSEInterface *skse, PluginInfo *info) {
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "SKWindowFix";
	info->version = SKWINDOWFIX_VERSION;

	if (skse->isEditor) {
		return false;
	}

	return true;
}

bool SKSEPlugin_Load(const SKSEInterface *skse) {
	_MESSAGE("SKWindowFix version %d loaded", SKWINDOWFIX_VERSION);

	DWORD threadID = GetCurrentThreadId();

	_MESSAGE("Installing hook for thread 0x%08X", threadID);

	HHOOK hookResult = SetWindowsHookEx(WH_CALLWNDPROCRET, CallWndProcRet,
		NULL, threadID);
	
	if (hookResult == NULL) {
		LPSTR buffer = NULL;

		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER
			| FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
			(LPSTR)(&buffer), 0, NULL);

		_ERROR("Failed to install hook (error code 0x%X): %s", GetLastError(),
			buffer);
		
		LocalFree(buffer);

		return false;
	} else {
		_MESSAGE("Hook installed successfully, proc address is 0x%08X",
			hookResult);

		return true;
	}
}
