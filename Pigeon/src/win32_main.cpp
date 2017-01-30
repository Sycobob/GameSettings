#define WIN32_LEAN_AND_MEAN
// TODO: #include <minwindef.h>?
#include <Windows.h>
// TODO: Switch to WRL ComPtr
#include <atlbase.h> //CComPtr

#include "shared.hpp"
#include "notification.hpp"
#include "audio.hpp"
#include "video.hpp"

// TODO: Move to resources?
static const c16* GUIDSTR_PIGEON = L"{C1FA11EF-FC16-46DF-A268-104F59E94672}";

int CALLBACK
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, i32 nCmdShow)
{
	b32 success;
	u32 uResult;
	HRESULT hr;
	MSG msg = {};


	// TODO: Use a different animation timing method. SetTimer is not precise enough (rounds to multiples of 15.6ms)
	// TODO: Pigeon image on startup
	// TODO: Pigeon sounds
	// TODO: SetProcessDPIAware?
	// TODO: Log failures
	// Using gotos is a pretty bad idea. It skips initialization of local variables and they'll be filled with garbage.

	// TODO: Hotkey to restart
	// TODO: Sound doesn't play on most devices when cycling audio devices
	// TODO: Look for a way to start faster at login (using Startup folder seems to take quite a few seconds)
	// TODO: Integrate volume ducking?
	// https://msdn.microsoft.com/en-us/library/windows/desktop/dd940522(v=vs.85).aspx
	// TODO: Use RawInput to get hardware key so it's not Logitech/Corsair profile dependent?
	// TODO: Auto-detect headset being turned on/off
	// TODO: Test with mutliple users. Might need use Local\ namespace for the event


	// Notification
	NotificationWindow notification = {};
	{
		// NOTE: QPC and QPF are documented as not being able to fail on XP+
		LARGE_INTEGER tickFrequency = {};
		QueryPerformanceFrequency(&tickFrequency);

		f64 tickFrequencyF64 = (f64) tickFrequency.QuadPart;

		notification.windowMinWidth    = 200; // TODO: Implement
		notification.windowMaxWidth    = 600; // TODO: Implement
		notification.windowSize        = {200, 60};
		notification.windowPosition    = { 50, 60};
		notification.backgroundColor   = RGBA(16, 16, 16, 242);
		notification.textColorNormal   = RGB(255, 255, 255);
		notification.textColorError    = RGB(255, 0, 0);
		notification.textColorWarning  = RGB(255, 255, 0);
		notification.textPadding       = 20;
		notification.animShowTicks     = 0.1 * tickFrequencyF64;
		notification.animIdleTicks     = 2.0 * tickFrequencyF64;
		notification.animHideTicks     = 1.0 * tickFrequencyF64;
		notification.animUpdateMS      = 1000 / 30;
		notification.timerID           = 1;
		notification.tickFrequency     = tickFrequencyF64;

		//DEBUG
		Notify(&notification, L"Started!");
	}


	// Single Instance
	HANDLE singleInstanceEvent = CreateEventW(nullptr, false, false, GUIDSTR_PIGEON);
	if (!singleInstanceEvent) NotifyWindowsError(&notification, L"CreateEventW failed");

	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		success = SetEvent(singleInstanceEvent);
		if (!success) NotifyWindowsError(&notification, L"SetEvent failed");

		uResult = WaitForSingleObject(singleInstanceEvent, 1000);
		if (uResult == WAIT_TIMEOUT) NotifyWindowsError(&notification, L"WaitForSingleObject WAIT_TIMEOUT");
		if (uResult == WAIT_FAILED ) NotifyWindowsError(&notification, L"WaitForSingleObject WAIT_FAILED");
	}


	// Misc
	{
		// NOTE: Need for audio stuff
		hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY | COINIT_DISABLE_OLE1DDE);
		if (FAILED(hr)) NotifyWindowsError(&notification, L"CoInitializeEx failed", Error::Error, hr);

		// TODO: BELOW_NORMAL_PRIORITY_CLASS?
		success = SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);
		if (!success) NotifyWindowsError(&notification, L"CreateWindowExW failed", Error::Warning);
	}


	// Window
	{
		WNDCLASSW windowClass = {};
		windowClass.style         = 0; // TODO: CS_DROPSHADOW?
		windowClass.lpfnWndProc   = NotificationWndProc;
		windowClass.cbClsExtra    = 0;
		windowClass.cbWndExtra    = sizeof(&notification);
		windowClass.hInstance     = hInstance;
		windowClass.hIcon         = nullptr;
		windowClass.hCursor       = nullptr;
		windowClass.hbrBackground = nullptr;
		windowClass.lpszMenuName  = nullptr;
		windowClass.lpszClassName = L"Pigeon Notification Class";

		ATOM classAtom = RegisterClassW(&windowClass);
		if (classAtom == INVALID_ATOM) NotifyWindowsError(&notification, L"RegisterClassW failed");

		notification.hwnd = CreateWindowExW(
			WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
			windowClass.lpszClassName,
			L"Pigeon Notification Window",
			WS_POPUP,
			notification.windowPosition.x,
			notification.windowPosition.y,
			notification.windowSize.cx,
			notification.windowSize.cy,
			nullptr,
			nullptr,
			hInstance,
			&notification
		);
		if (notification.hwnd == INVALID_HANDLE_VALUE) NotifyWindowsError(&notification, L"CreateWindowExW failed");
	}


	// Hotkeys
	const i32           cycleAudioDeviceHotkeyID = 0;
	const i32        openPlaybackDevicesHotkeyID = 1;
	const i32           cycleRefreshRateHotkeyID = 2;
	const i32 openDisplayAdapterSettingsHotkeyID = 3;
	const i32              debug_MessageHotkeyID = 4;
	const i32              debug_WarningHotkeyID = 5;
	const i32                debug_ErrorHotkeyID = 6;

	struct Hotkey
	{
		i32 id;
		u32 modifier;
		u32 key;
	};

	Hotkey hotkeys[] = {
		{           cycleAudioDeviceHotkeyID,           0, VK_F5  },
		{        openPlaybackDevicesHotkeyID, MOD_CONTROL, VK_F5  },
		{           cycleRefreshRateHotkeyID,           0, VK_F6  },
		{ openDisplayAdapterSettingsHotkeyID, MOD_CONTROL, VK_F6  },
		{              debug_MessageHotkeyID,           0, VK_F10 },
		{              debug_WarningHotkeyID,           0, VK_F11 },
		{                debug_ErrorHotkeyID,           0, VK_F12 },
	};

	for (u8 i = 0; i < ArrayCount(hotkeys); i++)
	{
		success = RegisterHotKey(nullptr, hotkeys[i].id, MOD_WIN | MOD_NOREPEAT | hotkeys[i].modifier, hotkeys[i].key);
		if (!success) NotifyWindowsError(&notification, L"RegisterHotKey failed", Error::Warning);
	}


	// Message pump
	b32 quit = false;
	while (!quit)
	{
		// TODO: I'm unsure if this releases on ALL possible messages
		uResult = MsgWaitForMultipleObjects(1, &singleInstanceEvent, false, INFINITE, QS_ALLINPUT | QS_ALLPOSTMESSAGE);
		if (uResult == WAIT_FAILED  ) Notify(&notification, L"MsgWaitForMultipleObjects WAIT_FAILED", Error::Error);
		if (uResult == WAIT_OBJECT_0)
		{
			for (u8 i = 0; i < ArrayCount(hotkeys); i++)
			{
				success = UnregisterHotKey(nullptr, hotkeys[i].id);
				if (!success) NotifyWindowsError(&notification, L"UnregisterHotKey failed", Error::Warning);
			}
			SetEvent(singleInstanceEvent);

			notification.windowPosition.y += notification.windowSize.cy + 10;
			UpdateWindowPositionAndSize(&notification);

			// TODO: Only once
			Notify(&notification, L"There can be only one!", Error::Error);
		}

		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);

			switch (msg.message)
			{
				case WM_HOTKEY:
				{
					switch (msg.wParam)
					{
						case cycleAudioDeviceHotkeyID:
							CycleDefaultAudioDevice(&notification);
							break;

						case openPlaybackDevicesHotkeyID:
							OpenAudioPlaybackDevicesWindow();
							break;

						case cycleRefreshRateHotkeyID:
							CycleRefreshRate(&notification);
							break;

						case openDisplayAdapterSettingsHotkeyID:
							OpenDisplayAdapterSettingsWindow();
							break;

						case debug_MessageHotkeyID:
							Notify(&notification, L"DEBUG Message", Error::None);
							break;

						case debug_WarningHotkeyID:
							Notify(&notification, L"DEBUG Warning", Error::Warning);
							break;

						case debug_ErrorHotkeyID:
							Notify(&notification, L"DEBUG Error", Error::Error);
							break;
					}
					break;
				}

				case WM_QUIT:
					quit = true;
					break;

				/* TODO: When the machine wakes up from sleep it looks like QueryPerformanceCounter
				 * starts back over. This causes animations to break because they thing they have
				 * an extremely long period of time left. The result is a notification flashing in
				 * and out rapidly.
				 */
				// Expected messages
				case WM_TIMER:
				case WM_PROCESSQUEUE:
					break;

				// TODO: 29 - when installing fonts
				// TODO: WM_TIMECHANGE (30) - Got this one recently.
				// TODO: WM_KEYDOWN (256) / WM_KEYUP (257) - Somehow we're getting key messages, but only sometimes
				// TODO: Open bin, wait for overflow, open bin, stuck on message
				// TODO: FormatMessage is not giving good string translations
				default:
					if (msg.message <= WM_PROCESSQUEUE)
					{
						c16 buffer[128] = {};
						swprintf(buffer, ArrayCount(buffer), L"Unexpected message: %d\n", msg.message);
						Notify(&notification, buffer, Error::Warning);
					}
					break;
			}
		}
	}


	// Cleanup
	CoUninitialize();

	// TODO: Show remaining warnings / errors in a dialog?

	// Leak all the things!
	// (Windows destroys everything automatically)

	// NOTE: Handles are closed when process terminates.
	// Events are destroyed when the last handle is destroyed.

	// TODO: This is wrong
	return LOWORD(msg.wParam);
}