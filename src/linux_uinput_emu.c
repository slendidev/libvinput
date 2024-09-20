#include "libvinput.h"

#include <ctype.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon.h>

typedef struct _EventEmulatorInternal
{
	struct xkb_context *ctx;
	struct xkb_keymap *keymap;
	struct xkb_state *state;
	int uinput_fd;
} EventEmulatorInternal;

static int _emit(int fd, int type, int code, int val)
{
	struct input_event ie;
	memset(&ie, 0, sizeof(ie));
	ie.type = type;
	ie.code = code;
	ie.value = val;
	if (write(fd, &ie, sizeof(ie)) < 0) {
		perror("Failed to emit event");
		return -1;
	}
	return 0;
}

VInputError _uinput_EventEmulator_init(EventEmulator *emulator)
{
	EventEmulatorInternal *data = malloc(sizeof(EventEmulatorInternal));
	if (!data) return VINPUT_MALLOC;

	data->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (data->ctx) {
		data->keymap
		    = xkb_keymap_new_from_names(data->ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
		if (data->keymap) {
			data->state = xkb_state_new(data->keymap);
			if (!data->state) {
				free(data);
				return VINPUT_XKB;
			}
		} else {
			free(data);
			return VINPUT_XKB;
		}
	} else {
		free(data);
		return VINPUT_XKB;
	}

	emulator->data = data;
	data->uinput_fd = open("/dev/uinput", O_WRONLY);
	if (data->uinput_fd < 0) {
		perror("Failed to open /dev/uinput");
		free(data);
		return VINPUT_NO_PERM;
	}

	static const int key_list[] = { KEY_ESC, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6,
		KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL, KEY_BACKSPACE, KEY_TAB, KEY_Q,
		KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEY_LEFTBRACE,
		KEY_RIGHTBRACE, KEY_ENTER, KEY_LEFTCTRL, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H,
		KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE, KEY_LEFTSHIFT,
		KEY_BACKSLASH, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_DOT,
		KEY_SLASH, KEY_RIGHTSHIFT, KEY_KPASTERISK, KEY_LEFTALT, KEY_SPACE, KEY_CAPSLOCK,
		KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
		KEY_NUMLOCK, KEY_SCROLLLOCK, KEY_KP7, KEY_KP8, KEY_KP9, KEY_KPMINUS, KEY_KP4, KEY_KP5,
		KEY_KP6, KEY_KPPLUS, KEY_KP1, KEY_KP2, KEY_KP3, KEY_KP0, KEY_KPDOT,
		KEY_ZENKAKUHANKAKU, KEY_102ND, KEY_F11, KEY_F12, KEY_RO, KEY_KATAKANA, KEY_HIRAGANA,
		KEY_HENKAN, KEY_KATAKANAHIRAGANA, KEY_MUHENKAN, KEY_KPJPCOMMA, KEY_KPENTER,
		KEY_RIGHTCTRL, KEY_KPSLASH, KEY_SYSRQ, KEY_RIGHTALT, KEY_LINEFEED, KEY_HOME, KEY_UP,
		KEY_PAGEUP, KEY_LEFT, KEY_RIGHT, KEY_END, KEY_DOWN, KEY_PAGEDOWN, KEY_INSERT,
		KEY_DELETE, KEY_MACRO, KEY_MUTE, KEY_VOLUMEDOWN, KEY_VOLUMEUP, KEY_POWER, KEY_KPEQUAL,
		KEY_KPPLUSMINUS, KEY_PAUSE, KEY_SCALE, KEY_KPCOMMA, KEY_HANGEUL, KEY_HANGUEL,
		KEY_HANJA, KEY_YEN, KEY_LEFTMETA, KEY_RIGHTMETA, KEY_COMPOSE, KEY_STOP, KEY_AGAIN,
		KEY_PROPS, KEY_UNDO, KEY_FRONT, KEY_COPY, KEY_OPEN, KEY_PASTE, KEY_FIND, KEY_CUT,
		KEY_HELP, KEY_MENU, KEY_CALC, KEY_SETUP, KEY_SLEEP, KEY_WAKEUP, KEY_FILE,
		KEY_SENDFILE, KEY_DELETEFILE, KEY_XFER, KEY_PROG1, KEY_PROG2, KEY_WWW, KEY_MSDOS,
		KEY_COFFEE, KEY_SCREENLOCK, KEY_ROTATE_DISPLAY, KEY_DIRECTION, KEY_CYCLEWINDOWS,
		KEY_MAIL, KEY_BOOKMARKS, KEY_COMPUTER, KEY_BACK, KEY_FORWARD, KEY_CLOSECD,
		KEY_EJECTCD, KEY_EJECTCLOSECD, KEY_NEXTSONG, KEY_PLAYPAUSE, KEY_PREVIOUSSONG,
		KEY_STOPCD, KEY_RECORD, KEY_REWIND, KEY_PHONE, KEY_ISO, KEY_CONFIG, KEY_HOMEPAGE,
		KEY_REFRESH, KEY_EXIT, KEY_MOVE, KEY_EDIT, KEY_SCROLLUP, KEY_SCROLLDOWN,
		KEY_KPLEFTPAREN, KEY_KPRIGHTPAREN, KEY_NEW, KEY_REDO, KEY_F13, KEY_F14, KEY_F15,
		KEY_F16, KEY_F17, KEY_F18, KEY_F19, KEY_F20, KEY_F21, KEY_F22, KEY_F23, KEY_F24,
		KEY_PLAYCD, KEY_PAUSECD, KEY_PROG3, KEY_PROG4, KEY_DASHBOARD, KEY_SUSPEND, KEY_CLOSE,
		KEY_PLAY, KEY_FASTFORWARD, KEY_BASSBOOST, KEY_PRINT, KEY_HP, KEY_CAMERA, KEY_SOUND,
		KEY_QUESTION, KEY_EMAIL, KEY_CHAT, KEY_SEARCH, KEY_CONNECT, KEY_FINANCE, KEY_SPORT,
		KEY_SHOP, KEY_ALTERASE, KEY_CANCEL, KEY_BRIGHTNESSDOWN, KEY_BRIGHTNESSUP, KEY_MEDIA,
		KEY_SWITCHVIDEOMODE, KEY_KBDILLUMTOGGLE, KEY_KBDILLUMDOWN, KEY_KBDILLUMUP, KEY_SEND,
		KEY_REPLY, KEY_FORWARDMAIL, KEY_SAVE, KEY_DOCUMENTS, KEY_BATTERY, KEY_BLUETOOTH,
		KEY_WLAN, KEY_UWB, KEY_UNKNOWN, KEY_VIDEO_NEXT, KEY_VIDEO_PREV, KEY_BRIGHTNESS_CYCLE,
		KEY_BRIGHTNESS_AUTO, KEY_BRIGHTNESS_ZERO, KEY_DISPLAY_OFF, KEY_WWAN, KEY_WIMAX,
		KEY_RFKILL, KEY_MICMUTE, KEY_OK, KEY_SELECT, KEY_GOTO, KEY_CLEAR, KEY_POWER2,
		KEY_OPTION, KEY_INFO, KEY_TIME, KEY_VENDOR, KEY_ARCHIVE, KEY_PROGRAM, KEY_CHANNEL,
		KEY_FAVORITES, KEY_EPG, KEY_PVR, KEY_MHP, KEY_LANGUAGE, KEY_TITLE, KEY_SUBTITLE,
		KEY_ANGLE, KEY_ZOOM, KEY_MODE, KEY_KEYBOARD, KEY_SCREEN, KEY_PC, KEY_TV, KEY_TV2,
		KEY_VCR, KEY_VCR2, KEY_SAT, KEY_SAT2, KEY_CD, KEY_TAPE, KEY_RADIO, KEY_TUNER,
		KEY_PLAYER, KEY_TEXT, KEY_DVD, KEY_AUX, KEY_MP3, KEY_AUDIO, KEY_VIDEO, KEY_DIRECTORY,
		KEY_LIST, KEY_MEMO, KEY_CALENDAR, KEY_RED, KEY_GREEN, KEY_YELLOW, KEY_BLUE,
		KEY_CHANNELUP, KEY_CHANNELDOWN, KEY_FIRST, KEY_LAST, KEY_AB, KEY_NEXT, KEY_RESTART,
		KEY_SLOW, KEY_SHUFFLE, KEY_BREAK, KEY_PREVIOUS, KEY_DIGITS, KEY_TEEN, KEY_TWEN,
		KEY_VIDEOPHONE, KEY_GAMES, KEY_ZOOMIN, KEY_ZOOMOUT, KEY_ZOOMRESET, KEY_WORDPROCESSOR,
		KEY_EDITOR, KEY_SPREADSHEET, KEY_GRAPHICSEDITOR, KEY_PRESENTATION, KEY_DATABASE,
		KEY_NEWS, KEY_VOICEMAIL, KEY_ADDRESSBOOK, KEY_MESSENGER, KEY_DISPLAYTOGGLE,
		KEY_BRIGHTNESS_TOGGLE, KEY_SPELLCHECK, KEY_LOGOFF, KEY_DOLLAR, KEY_EURO,
		KEY_FRAMEBACK, KEY_FRAMEFORWARD, KEY_CONTEXT_MENU, KEY_MEDIA_REPEAT, KEY_10CHANNELSUP,
		KEY_10CHANNELSDOWN, KEY_IMAGES, KEY_DEL_EOL, KEY_DEL_EOS, KEY_INS_LINE, KEY_DEL_LINE,
		KEY_FN, KEY_FN_ESC, KEY_FN_F1, KEY_FN_F2, KEY_FN_F3, KEY_FN_F4, KEY_FN_F5, KEY_FN_F6,
		KEY_FN_F7, KEY_FN_F8, KEY_FN_F9, KEY_FN_F10, KEY_FN_F11, KEY_FN_F12, KEY_FN_1,
		KEY_FN_2, KEY_FN_D, KEY_FN_E, KEY_FN_F, KEY_FN_S, KEY_FN_B, KEY_BRL_DOT1,
		KEY_BRL_DOT2, KEY_BRL_DOT3, KEY_BRL_DOT4, KEY_BRL_DOT5, KEY_BRL_DOT6, KEY_BRL_DOT7,
		KEY_BRL_DOT8, KEY_BRL_DOT9, KEY_BRL_DOT10, KEY_NUMERIC_0, KEY_NUMERIC_1,
		KEY_NUMERIC_2, KEY_NUMERIC_3, KEY_NUMERIC_4, KEY_NUMERIC_5, KEY_NUMERIC_6,
		KEY_NUMERIC_7, KEY_NUMERIC_8, KEY_NUMERIC_9, KEY_NUMERIC_STAR, KEY_NUMERIC_POUND,
		KEY_NUMERIC_A, KEY_NUMERIC_B, KEY_NUMERIC_C, KEY_NUMERIC_D, KEY_CAMERA_FOCUS,
		KEY_WPS_BUTTON, KEY_TOUCHPAD_TOGGLE, KEY_TOUCHPAD_ON, KEY_TOUCHPAD_OFF,
		KEY_CAMERA_ZOOMIN, KEY_CAMERA_ZOOMOUT, KEY_CAMERA_UP, KEY_CAMERA_DOWN,
		KEY_CAMERA_LEFT, KEY_CAMERA_RIGHT, KEY_ATTENDANT_ON, KEY_ATTENDANT_OFF,
		KEY_ATTENDANT_TOGGLE, KEY_LIGHTS_TOGGLE, KEY_ALS_TOGGLE, KEY_BUTTONCONFIG,
		KEY_TASKMANAGER, KEY_JOURNAL, KEY_CONTROLPANEL, KEY_APPSELECT, KEY_SCREENSAVER,
		KEY_VOICECOMMAND, KEY_BRIGHTNESS_MIN, KEY_BRIGHTNESS_MAX, KEY_KBDINPUTASSIST_PREV,
		KEY_KBDINPUTASSIST_NEXT, KEY_KBDINPUTASSIST_PREVGROUP, KEY_KBDINPUTASSIST_NEXTGROUP,
		KEY_KBDINPUTASSIST_ACCEPT, KEY_KBDINPUTASSIST_CANCEL, KEY_RIGHT_UP, KEY_RIGHT_DOWN,
		KEY_LEFT_UP, KEY_LEFT_DOWN, KEY_ROOT_MENU, KEY_MEDIA_TOP_MENU, KEY_NUMERIC_11,
		KEY_NUMERIC_12, KEY_AUDIO_DESC, KEY_3D_MODE, KEY_NEXT_FAVORITE, KEY_STOP_RECORD,
		KEY_PAUSE_RECORD, KEY_VOD, KEY_UNMUTE, KEY_FASTREVERSE, KEY_SLOWREVERSE, KEY_DATA,
		KEY_MIN_INTERESTING, BTN_MISC, BTN_0, BTN_1, BTN_2, BTN_3, BTN_4, BTN_5, BTN_6, BTN_7,
		BTN_8, BTN_9, BTN_MOUSE, BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, BTN_SIDE, BTN_EXTRA,
		BTN_FORWARD, BTN_BACK, BTN_TASK, BTN_JOYSTICK, BTN_TRIGGER, BTN_THUMB, BTN_THUMB2,
		BTN_TOP, BTN_TOP2, BTN_PINKIE, BTN_BASE, BTN_BASE2, BTN_BASE3, BTN_BASE4, BTN_BASE5,
		BTN_BASE6, BTN_DEAD, BTN_GAMEPAD, BTN_SOUTH, BTN_A, BTN_EAST, BTN_B, BTN_C, BTN_NORTH,
		BTN_X, BTN_WEST, BTN_Y, BTN_Z, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_SELECT,
		BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR, BTN_DIGI, BTN_TOOL_PEN, BTN_TOOL_RUBBER,
		BTN_TOOL_BRUSH, BTN_TOOL_PENCIL, BTN_TOOL_AIRBRUSH, BTN_TOOL_FINGER, BTN_TOOL_MOUSE,
		BTN_TOOL_LENS, BTN_TOOL_QUINTTAP, BTN_TOUCH, BTN_STYLUS, BTN_STYLUS2,
		BTN_TOOL_DOUBLETAP, BTN_TOOL_TRIPLETAP, BTN_TOOL_QUADTAP, BTN_WHEEL, BTN_GEAR_DOWN,
		BTN_GEAR_UP, BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT,
		BTN_TRIGGER_HAPPY, BTN_TRIGGER_HAPPY1, BTN_TRIGGER_HAPPY2, BTN_TRIGGER_HAPPY3,
		BTN_TRIGGER_HAPPY4, BTN_TRIGGER_HAPPY5, BTN_TRIGGER_HAPPY6, BTN_TRIGGER_HAPPY7,
		BTN_TRIGGER_HAPPY8, BTN_TRIGGER_HAPPY9, BTN_TRIGGER_HAPPY10, BTN_TRIGGER_HAPPY11,
		BTN_TRIGGER_HAPPY12, BTN_TRIGGER_HAPPY13, BTN_TRIGGER_HAPPY14, BTN_TRIGGER_HAPPY15,
		BTN_TRIGGER_HAPPY16, BTN_TRIGGER_HAPPY17, BTN_TRIGGER_HAPPY18, BTN_TRIGGER_HAPPY19,
		BTN_TRIGGER_HAPPY20, BTN_TRIGGER_HAPPY21, BTN_TRIGGER_HAPPY22, BTN_TRIGGER_HAPPY23,
		BTN_TRIGGER_HAPPY24, BTN_TRIGGER_HAPPY25, BTN_TRIGGER_HAPPY26, BTN_TRIGGER_HAPPY27,
		BTN_TRIGGER_HAPPY28, BTN_TRIGGER_HAPPY29, BTN_TRIGGER_HAPPY30, BTN_TRIGGER_HAPPY31,
		BTN_TRIGGER_HAPPY32, BTN_TRIGGER_HAPPY33, BTN_TRIGGER_HAPPY34, BTN_TRIGGER_HAPPY35,
		BTN_TRIGGER_HAPPY36, BTN_TRIGGER_HAPPY37, BTN_TRIGGER_HAPPY38, BTN_TRIGGER_HAPPY39,
		BTN_TRIGGER_HAPPY40 };

	for (size_t i = 0; i < sizeof(key_list) / sizeof(int); i++) {
		if (ioctl(data->uinput_fd, UI_SET_KEYBIT, key_list[i])) {
			fprintf(stderr, "UI_SET_KEYBIT %ld failed\n", i);
		}
	}

	if (ioctl(data->uinput_fd, UI_SET_EVBIT, EV_KEY) < 0) {
		perror("Failed to enable keyboard events");
		close(data->uinput_fd);
		free(data);
		return VINPUT_IOCTL_FAIL;
	}

	struct uinput_setup usetup;
	memset(&usetup, 0, sizeof(usetup));
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = 0x1234;
	usetup.id.product = 0x5678;
	usetup.id.version = 1;
	strcpy(usetup.name, "libvinput Virtual Keyboard");

	if (ioctl(data->uinput_fd, UI_DEV_SETUP, &usetup) < 0
	    || ioctl(data->uinput_fd, UI_DEV_CREATE) < 0) {
		perror("Failed to create uinput device");
		close(data->uinput_fd);
		free(data);
		return VINPUT_IOCTL_FAIL;
	}

	emulator->initialized = true;

	return VINPUT_OK;
}

VInputError uinput_EventEmulator_press(EventEmulator *emulator, uint16_t keysym)
{
	if (!emulator->initialized) return VINPUT_UNINITIALIZED;
	EventEmulatorInternal *data = emulator->data;
	if (_emit(data->uinput_fd, EV_KEY, keysym - 8, 1) < 0) return VINPUT_NO_PERM;
	if (_emit(data->uinput_fd, EV_SYN, SYN_REPORT, 0) < 0) return VINPUT_NO_PERM;
	usleep(500);
	return VINPUT_OK;
}

VInputError uinput_EventEmulator_release(EventEmulator *emulator, uint16_t keysym)
{
	if (!emulator->initialized) return VINPUT_UNINITIALIZED;
	EventEmulatorInternal *data = emulator->data;
	if (_emit(data->uinput_fd, EV_KEY, keysym - 8, 0) < 0) return VINPUT_NO_PERM;
	if (_emit(data->uinput_fd, EV_SYN, SYN_REPORT, 0) < 0) return VINPUT_NO_PERM;
	usleep(500);
	return VINPUT_OK;
}

typedef struct
{
	xkb_keycode_t keycode;
	bool in_shift_layer;
} KeycodeResult;

KeycodeResult keysym_to_keycode(
    struct xkb_state *state, struct xkb_keymap *keymap, xkb_keysym_t target_keysym)
{
	KeycodeResult result = { XKB_KEYCODE_INVALID, false };
	xkb_keycode_t min_keycode = xkb_keymap_min_keycode(keymap);
	xkb_keycode_t max_keycode = xkb_keymap_max_keycode(keymap);

	for (xkb_keycode_t keycode = min_keycode; keycode <= max_keycode; keycode++) {
		const xkb_keysym_t *syms;
		int num_syms = xkb_state_key_get_syms(state, keycode, &syms);

		for (int i = 0; i < num_syms; i++) {
			if (syms[i] == target_keysym) {
				result.keycode = keycode;
				result.in_shift_layer = false;
				return result;
			}
		}

		xkb_mod_index_t shift_mod = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_SHIFT);
		xkb_state_update_mask(state, (1 << shift_mod), 0, 0, 0, 0, 0);

		num_syms = xkb_state_key_get_syms(state, keycode, &syms);
		for (int i = 0; i < num_syms; i++) {
			if (syms[i] == target_keysym) {
				result.keycode = keycode;
				result.in_shift_layer = true;
				return result;
			}
		}

		xkb_state_update_mask(state, 0, 0, 0, 0, 0, 0);
	}

	return result;
}

VInputError uinput_EventEmulator_typec(EventEmulator *emulator, char ch)
{
	if (!emulator->initialized) return VINPUT_UNINITIALIZED;
	EventEmulatorInternal *data = emulator->data;

	VInputError err;

	uint16_t keysym = xkb_utf32_to_keysym((uint32_t)ch);
	KeycodeResult res = keysym_to_keycode(data->state, data->keymap, keysym);
	if (res.in_shift_layer) {
		if (_emit(data->uinput_fd, EV_KEY, KEY_RIGHTSHIFT, 1) < 0) return VINPUT_NO_PERM;
		if (_emit(data->uinput_fd, EV_SYN, SYN_REPORT, 0) < 0) return VINPUT_NO_PERM;
		usleep(500);
	}

	if ((err = uinput_EventEmulator_press(emulator, res.keycode)) != VINPUT_OK) return err;
	if ((err = uinput_EventEmulator_release(emulator, res.keycode)) != VINPUT_OK)
		return err;

	if (res.in_shift_layer) {
		usleep(500);
		if (_emit(data->uinput_fd, EV_KEY, KEY_RIGHTSHIFT, 0) < 0) return VINPUT_NO_PERM;
		if (_emit(data->uinput_fd, EV_SYN, SYN_REPORT, 0) < 0) return VINPUT_NO_PERM;
	}

	return VINPUT_OK;
}

VInputError uinput_EventEmulator_types(EventEmulator *emulator, char *buf, size_t len)
{
	if (!emulator->initialized) return VINPUT_UNINITIALIZED;
	for (size_t i = 0; i < len; i++)
		if (uinput_EventEmulator_typec(emulator, buf[i]) != VINPUT_OK) return VINPUT_NO_PERM;
	return VINPUT_OK;
}

VInputError uinput_EventEmulator_keyboard_state_get(
    EventEmulator *emulator, int **state, int *nstate)
{
	if (!emulator->initialized) return VINPUT_UNINITIALIZED;
	(void)emulator;
	(void)state;
	(void)nstate;
	fputs("Placeholder: uinput_EventEmulator_keyboard_state_get\n", stderr);
	return VINPUT_OK;
}

VInputError uinput_EventEmulator_keyboard_state_clear(EventEmulator *emulator)
{
	if (!emulator->initialized) return VINPUT_UNINITIALIZED;
	(void)emulator;
	fputs("Placeholder: uinput_EventEmulator_keyboard_state_clear\n", stderr);
	return VINPUT_OK;
}

VInputError uinput_EventEmulator_keyboard_state_set(
    EventEmulator *emulator, int *state, int nstate)
{
	if (!emulator->initialized) return VINPUT_UNINITIALIZED;
	(void)emulator;
	(void)state;
	(void)nstate;
	fputs("Placeholder: uinput_EventEmulator_keyboard_state_set\n", stderr);
	return VINPUT_OK;
}

VInputError uinput_EventEmulator_free(EventEmulator *emulator)
{
	if (!emulator->initialized) return VINPUT_UNINITIALIZED;
	EventEmulatorInternal *data = emulator->data;
	if (data) {
		if (ioctl(data->uinput_fd, UI_DEV_DESTROY) < 0) {
			perror("Failed to destroy uinput device");
		}
		close(data->uinput_fd);
		free(data);
	}
	return VINPUT_OK;
}
