#ifndef LIBVINPUT_H
#define LIBVINPUT_H

#if defined _WIN32 || defined __CYGWIN__
#	ifdef BUILDING_VINPUT
#		define VINPUT_PUBLIC __declspec(dllexport)
#	else
#		define VINPUT_PUBLIC __declspec(dllimport)
#	endif
#else
#	ifdef BUILDING_VINPUT
#		define VINPUT_PUBLIC __attribute__((visibility("default")))
#	else
#		define VINPUT_PUBLIC
#	endif
#endif

#undef VINPUT_PUBLIC
#define VINPUT_PUBLIC

#if defined(_MSC_VER)
#	define VINPUT_DEPRECATED(message) __declspec(deprecated(message))
#elif defined(__GNUC__) || defined(__clang__)
#	define VINPUT_DEPRECATED(message) __attribute__((deprecated(message)))
#else
#	define VINPUT_DEPRECATED(message)
#endif

// If you wish to use the old names, you need to define "#define LIBVINPUT_OLD_NAMES"
// before including this header.

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VINPUT_VERSION_MAJOR(x) (((x) & 0xff0000) >> 16)
#define VINPUT_VERSION_MINOR(x) (((x) & 0xff00) >> 8)
#define VINPUT_VERSION_PATCH(x) ((x) & 0xff)

// Currently, only used by the EventListener.
typedef struct _KeyboardModifiers
{
	bool left_control : 1;
	bool right_control : 1;
	bool left_shift : 1;
	bool right_shift : 1;
	bool left_alt : 1;    // On Mac, this is the Option key
	bool right_alt : 1;   // On Mac, this is the Option key
	bool left_meta : 1;   // On Mac, this is the Command key
	bool right_meta : 1;  // On Mac, this is the Command key
	bool left_super : 1;  // On Mac, this is the Fn key
	bool right_super : 1; // On Mac, this is the Fn key
	bool left_hyper : 1;
	bool right_hyper : 1;
} KeyboardModifiers;

typedef struct _KeyboardEvent
{
	bool pressed;
	char keychar; // The ASCII character of the key, 0 if unavailable
	uint16_t keycode;
	uint16_t keysym; // On X11, the KeySym, on windows, the Virtual Key code

	KeyboardModifiers modifiers;

	size_t timestamp; // Timestamp of event, in milliseconds
} KeyboardEvent;

typedef enum _MouseButton
{
	MouseButtonLeft = 0,
	MouseButtonRight,
	MouseButtonMiddle,
} MouseButton;

typedef enum _MouseButtonEventKind
{
	MousePressEvent,
	MouseReleaseEvent,
} MouseButtonEventKind;

typedef struct _MouseButtonEvent
{
	MouseButton button;
	MouseButtonEventKind kind;
} MouseButtonEvent;

typedef struct _MouseMoveEvent
{
	unsigned int x, y;
	float velocity_x, velocity_y, velocity;
} MouseMoveEvent;

typedef struct _EventListener
{
	bool listen_keyboard;
	bool initialized;
	void *data; // Internal data, do not use
	// V2
	bool listen_mouse_button;
	bool listen_mouse_move;
} EventListener;

typedef struct _EventEmulator
{
	bool initialized;
	void *data; // Internal data, do not use
} EventEmulator;

typedef enum _VInputError
{
	VINPUT_OK = 0,

	VINPUT_UNINITIALIZED,
	VINPUT_MALLOC,

	// WINAPI
	VINPUT_WINAPI_MODULE,
	VINPUT_WINAPI_HOOK,
	VINPUT_WINAPI_SENDINPUT,
	VINPUT_WINAPI_VKKEYSCANA,
	VINPUT_WINAPI_GETKEYBOARDSTATE,

	// X11
	VINPUT_X11_DISPLAY,
	VINPUT_X11_XKB_GET_MAP,
	VINPUT_X11_RANGE_ALLOC,
	VINPUT_X11_XRECORD_CONTEXT,
	VINPUT_X11_DISPLAY_DATALINK,
	VINPUT_X11_ENABLE_XRECORD,

	// EVDEV+UINPUT
	VINPUT_XKB,
	VINPUT_DEV_INPUT_DIR,
	VINPUT_NO_PERM,
	VINPUT_IOCTL_FAIL,

	VINPUT_XDO_NEW,

	// MACOS
	VINPUT_MAC_TAP,
	VINPUT_MAC_EVENT_CREATION,
	VINPUT_MAC_INVALID_KEY,
} VInputError;

typedef void (*KeyboardCallback)(KeyboardEvent);
typedef void (*MouseButtonCallback)(MouseButtonEvent);
typedef void (*MouseMoveCallback)(MouseMoveEvent);

#ifdef LIBVINPUT_OLD_NAMES
typedef EventListener Listener;
typedef EventEmulator Emulator;
#endif

VINPUT_PUBLIC bool VInput_modifier_pressed_except_shift(KeyboardModifiers modifiers);

VINPUT_PUBLIC uint32_t VInput_version(void);
VINPUT_PUBLIC char const *VInput_error_get_message(VInputError error);

// Create a EventListener, does not allocate memory for the listener.
VINPUT_PUBLIC VInputError EventListener_create(EventListener *listener,
    bool listen_keyboard) VINPUT_DEPRECATED("Use EventListener2_create instead");
VINPUT_PUBLIC VInputError EventListener2_create(EventListener *listener,
    bool listen_keyboard, bool listen_mouse_button, bool listen_mouse_move);
// Make a Listener start listening. This is a blocking call.
VINPUT_PUBLIC VInputError EventListener_start(EventListener *listener,
    KeyboardCallback callback) VINPUT_DEPRECATED("Use EventListener2_start instead");
VINPUT_PUBLIC VInputError EventListener2_start(EventListener *listener,
    KeyboardCallback callback, MouseButtonCallback button_callback,
    MouseMoveCallback move_callback);
// Stop a running Listener, causing the blocking EventListener_start /
// EventListener2_start call to return. This is safe to call from a different
// thread than the one that called start (e.g. from a GUI thread). It is a no-op
// if the listener is not currently running.
VINPUT_PUBLIC VInputError EventListener_stop(EventListener *listener);
// Free up internal data in the Listener.
VINPUT_PUBLIC VInputError EventListener_free(EventListener *listener);

// Create an EventEmulator, does not allocate memory for the emulator.
VINPUT_PUBLIC VInputError EventEmulator_create(EventEmulator *emulator);
// Get current keyboard state, modifiers only, allocates memory
VINPUT_PUBLIC VInputError EventEmulator_keyboard_state_get(
    EventEmulator *emulator, int **state, int *nstate);
// Clear current keyboard state, modifiers only
VINPUT_PUBLIC VInputError EventEmulator_keyboard_state_clear(EventEmulator *emulator);
// Set current keyboard state, modifiers only, clears current keyboard state
VINPUT_PUBLIC VInputError EventEmulator_keyboard_state_set(
    EventEmulator *emulator, int *state, int nstate);
VINPUT_PUBLIC VInputError EventEmulator_press(EventEmulator *emulator, uint16_t keysym);
VINPUT_PUBLIC VInputError EventEmulator_release(EventEmulator *emulator, uint16_t keysym);
// Type out an ASCII character.
VINPUT_PUBLIC VInputError EventEmulator_typec(EventEmulator *emulator, char ch);
// Type out a string of ASCII characters.
VINPUT_PUBLIC VInputError EventEmulator_types(
    EventEmulator *emulator, char *buf, size_t len);
// Free up internal data in the Emulator.
VINPUT_PUBLIC VInputError EventEmulator_free(EventEmulator *emulator);

#ifdef LIBVINPUT_OLD_NAMES
#	define Listener_create EventListener_create
#	define Listener_start EventListener_start
#	define Listener_free EventListener_free

#	define Emulator_create EventEmulator_create
#	define Emulator_keyboard_state_get EventEmulator_keyboard_state_get
#	define Emulator_keyboard_state_clear EventEmulator_keyboard_state_clear
#	define Emulator_keyboard_state_set EventEmulator_keyboard_state_set
#	define Emulator_press EventEmulator_press
#	define Emulator_release EventEmulator_release
#	define Emulator_typec EventEmulator_typec
#	define Emulator_types EventEmulator_types
#	define Emulator_free EventEmulator_free
#endif

#ifdef __cplusplus
}
#endif

#endif // LIBVINPUT_H
