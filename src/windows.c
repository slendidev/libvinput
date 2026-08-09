#include "libvinput.h"

#include <math.h>
#include <stdlib.h>

#include <windows.h>
#include <winuser.h>

#define WM_QUIT_LOOP (WM_USER + 1)

// FIXME: Make this thread-safe!

typedef struct _EventListenerInternal
{
	HINSTANCE exe;
	HHOOK key_hook;
	HHOOK mouse_hook;
	DWORD thread_id;

	bool running;
	bool can_free;
} EventListenerInternal;

// Thread-Local Storage
DWORD tls_index;
DWORD tls_index_mouse_move;
DWORD tls_index_mouse_button;
DWORD tls_index_mouse_prev_x;
DWORD tls_index_mouse_prev_y;

KeyboardEvent generate_keyevent(WPARAM wparam, LPARAM lparam)
{
	(void)lparam;
	DWORD timestamp = GetTickCount(); // Milliseconds, from boot time

	KBDLLHOOKSTRUCT *keyboard = (KBDLLHOOKSTRUCT *)lparam;
	DWORD vkCode = keyboard->vkCode;

	char keychar = '\0';
	WORD scanCode = keyboard->scanCode;
	BYTE keyState[256];
	GetKeyboardState(keyState);

	int ret = ToAscii(vkCode, scanCode, keyState, (LPWORD)&keychar, 0);
	if (ret == 1) { // Success
		return (KeyboardEvent) {
			.pressed = wparam == WM_KEYDOWN,
			.keychar = keychar,
			.keycode = scanCode,
			.keysym = vkCode,
			.timestamp = timestamp,
		};
	} else { // Failure
		return (KeyboardEvent) {
			.pressed = wparam == WM_KEYDOWN,
			.keychar = '\0',
			.keycode = scanCode,
			.keysym = vkCode,
			.timestamp = timestamp,
		};
	}
}

static LRESULT CALLBACK keyboard_callback(int code, WPARAM wparam, LPARAM lparam)
{
	if (code < 0) CallNextHookEx(NULL, code, wparam, lparam);
	KeyboardCallback cb = TlsGetValue(tls_index);
	// Handle key events or call next hook in chain.
	switch (wparam) {
	case WM_KEYDOWN:
	case WM_KEYUP:
		if (cb) cb(generate_keyevent(wparam, lparam));
		break;
	}
	return CallNextHookEx(NULL, code, wparam, lparam);
}

static MouseButtonEvent generate_button_event(MouseButtonEventKind kind, WPARAM wparam)
{
	MouseButton button = -1;
	if (wparam == WM_LBUTTONUP || wparam == WM_NCLBUTTONUP || wparam == WM_LBUTTONDOWN
	    || wparam == WM_NCLBUTTONDOWN)
		button = MouseButtonLeft;
	if (wparam == WM_RBUTTONUP || wparam == WM_NCRBUTTONUP || wparam == WM_RBUTTONDOWN
	    || wparam == WM_NCRBUTTONDOWN)
		button = MouseButtonRight;
	if (wparam == WM_MBUTTONUP || wparam == WM_NCMBUTTONUP || wparam == WM_MBUTTONDOWN
	    || wparam == WM_NCMBUTTONDOWN)
		button = MouseButtonMiddle;

	return (MouseButtonEvent) {
		.button = button,
		.kind = kind,
	};
}

static MouseMoveEvent generate_move_event(LPARAM lparam)
{
	MSLLHOOKSTRUCT *evt = (MSLLHOOKSTRUCT *)lparam;

	int x_pos = evt->pt.x;
	int y_pos = evt->pt.y;

	int prev_x = (int)(uintptr_t)TlsGetValue(tls_index_mouse_prev_x);
	int prev_y = (int)(uintptr_t)TlsGetValue(tls_index_mouse_prev_y);

	TlsSetValue(tls_index_mouse_prev_x, (LPVOID)(uintptr_t)x_pos);
	TlsSetValue(tls_index_mouse_prev_y, (LPVOID)(uintptr_t)y_pos);

	float vel_x = (float)(x_pos - prev_x);
	float vel_y = (float)(y_pos - prev_y);

	return (MouseMoveEvent) {
		.x = x_pos,
		.y = y_pos,
		.velocity_x = vel_x,
		.velocity_y = vel_y,
		.velocity = sqrtf(vel_x * vel_x + vel_y * vel_y),
	};
}

static LRESULT CALLBACK mouse_callback(int code, WPARAM wparam, LPARAM lparam)
{
	if (code < 0) return CallNextHookEx(NULL, code, wparam, lparam);

	MouseMoveCallback cb = TlsGetValue(tls_index_mouse_move);
	MouseButtonCallback cb_button = TlsGetValue(tls_index_mouse_button);

	if (code == HC_ACTION) {
		if (wparam == WM_LBUTTONDOWN || wparam == WM_NCLBUTTONDOWN || wparam == WM_RBUTTONDOWN
		    || wparam == WM_NCRBUTTONDOWN || wparam == WM_MBUTTONDOWN
		    || wparam == WM_NCMBUTTONDOWN) {
			MouseButtonEvent ev = generate_button_event(MousePressEvent, wparam);
			if (ev.button != -1 && cb_button) cb_button(ev);
		} else if (wparam == WM_LBUTTONUP || wparam == WM_NCLBUTTONUP
		           || wparam == WM_RBUTTONUP || wparam == WM_NCRBUTTONUP
		           || wparam == WM_MBUTTONUP || wparam == WM_NCMBUTTONUP) {
			MouseButtonEvent ev = generate_button_event(MouseReleaseEvent, wparam);
			if (ev.button != -1 && cb_button) cb_button(ev);
		} else if (wparam == WM_NCMOUSEMOVE || wparam == WM_MOUSEMOVE) {
			if (cb) cb(generate_move_event(lparam));
		}
	}

	return CallNextHookEx(NULL, code, wparam, lparam);
}

VInputError _EventListener_init(EventListener *listener)
{
	listener->data = malloc(sizeof(EventListenerInternal));
	EventListenerInternal *data = listener->data;

	// Get module handle
	data->exe = GetModuleHandle(NULL);
	if (!data->exe) return VINPUT_WINAPI_MODULE;

	// Create keyboard hook
	data->key_hook
	    = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)keyboard_callback, data->exe, 0);
	if (!data->key_hook) return VINPUT_WINAPI_HOOK;

	tls_index = TlsAlloc();
	if (tls_index == TLS_OUT_OF_INDEXES) {
		UnhookWindowsHookEx(data->key_hook);
		return VINPUT_MALLOC;
	}

	data->mouse_hook
	    = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC)mouse_callback, data->exe, 0);
	if (!data->mouse_hook) return VINPUT_WINAPI_HOOK;

	tls_index_mouse_button = TlsAlloc();
	if (tls_index_mouse_button == TLS_OUT_OF_INDEXES) {
		UnhookWindowsHookEx(data->key_hook);
		UnhookWindowsHookEx(data->mouse_hook);
		return VINPUT_MALLOC;
	}
	tls_index_mouse_move = TlsAlloc();
	if (tls_index_mouse_move == TLS_OUT_OF_INDEXES) {
		UnhookWindowsHookEx(data->key_hook);
		UnhookWindowsHookEx(data->mouse_hook);
		return VINPUT_MALLOC;
	}
	tls_index_mouse_prev_x = TlsAlloc();
	if (tls_index_mouse_prev_x == TLS_OUT_OF_INDEXES) {
		UnhookWindowsHookEx(data->key_hook);
		UnhookWindowsHookEx(data->mouse_hook);
		return VINPUT_MALLOC;
	}
	tls_index_mouse_prev_y = TlsAlloc();
	if (tls_index_mouse_prev_y == TLS_OUT_OF_INDEXES) {
		UnhookWindowsHookEx(data->key_hook);
		UnhookWindowsHookEx(data->mouse_hook);
		return VINPUT_MALLOC;
	}

	POINT ms_pos;
	GetCursorPos(&ms_pos);
	TlsSetValue(tls_index_mouse_prev_x, (LPVOID)(uintptr_t)ms_pos.x);
	TlsSetValue(tls_index_mouse_prev_y, (LPVOID)(uintptr_t)ms_pos.y);

	listener->initialized = true;

	return VINPUT_OK;
}

VINPUT_PUBLIC VInputError EventListener2_start(EventListener *listener,
    KeyboardCallback callback, MouseButtonCallback button_callback,
    MouseMoveCallback move_callback)
{
	if (!listener->initialized) return VINPUT_UNINITIALIZED;
	EventListenerInternal *data = listener->data;

	data->running = true;
	data->can_free = false;

	data->thread_id = GetCurrentThreadId();

	TlsSetValue(tls_index, callback);
	TlsSetValue(tls_index_mouse_move, move_callback);
	TlsSetValue(tls_index_mouse_button, button_callback);

	// Propagate messages.
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) != 0 && data->running) {
		if (msg.message == WM_QUIT_LOOP) break;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	data->can_free = true;

	return VINPUT_OK;
}

VINPUT_PUBLIC VInputError EventListener_stop(EventListener *listener)
{
	if (!listener->initialized) return VINPUT_UNINITIALIZED;
	EventListenerInternal *data = listener->data;
	if (!data) return VINPUT_UNINITIALIZED;

	// Signal the message loop in EventListener2_start to exit. PostThreadMessage
	// wakes the blocking GetMessage call on the listening thread.
	data->running = false;
	PostThreadMessage(data->thread_id, WM_QUIT_LOOP, 0, 0);

	return VINPUT_OK;
}

VINPUT_PUBLIC VInputError EventListener_free(EventListener *listener)
{
	if (!listener->initialized) return VINPUT_UNINITIALIZED;
	EventListenerInternal *data = listener->data;

	data->running = false;
	PostThreadMessage(data->thread_id, WM_QUIT_LOOP, 0, 0);

	while (!data->can_free)
		; // FIXME: Do not hang the CPU like this, it's really ugly.

	UnhookWindowsHookEx(data->key_hook);
	UnhookWindowsHookEx(data->mouse_hook);
	memset(data, 0, sizeof(EventListenerInternal));

	TlsFree(tls_index);
	TlsFree(tls_index_mouse_move);
	TlsFree(tls_index_mouse_button);
	TlsFree(tls_index_mouse_prev_x);
	TlsFree(tls_index_mouse_prev_y);

	return VINPUT_OK;
}
