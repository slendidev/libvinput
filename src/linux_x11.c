#include "libvinput.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/extensions/XKB.h>
#include <X11/extensions/record.h>

#define XK_MISCELLANY
#include <X11/keysymdef.h>

typedef struct _EventListenerInternal
{
	Display *dpy, *dpy_datalink;
	XkbDescPtr keyboard_map;
	XRecordContext context;

	KeyboardCallback callback;
	MouseMoveCallback callback_mouse_move;
	MouseButtonCallback callback_mouse_button;

	KeyboardModifiers modifiers;
} EventListenerInternal;

VInputError _x11_EventListener_init(EventListener *listener)
{
	listener->data = malloc(sizeof(EventListenerInternal));
	EventListenerInternal *data = listener->data;

	VInputError result = VINPUT_OK;

	memset(&data->modifiers, 0, sizeof(KeyboardModifiers));

	// Open X11 display connection
	data->dpy = XOpenDisplay(NULL);
	if (!data->dpy) return VINPUT_X11_DISPLAY;

	// Get keyboard map
	data->keyboard_map = XkbGetMap(data->dpy, XkbAllClientInfoMask, XkbUseCoreKbd);
	if (!data->keyboard_map) {
		result = VINPUT_X11_XKB_GET_MAP;
		goto cleanup;
	}

	// Prepare XRecord to listen to global events
	XRecordClientSpec clients = XRecordAllClients;
	XRecordRange *range = XRecordAllocRange();
	if (range == 0) {
		result = VINPUT_X11_RANGE_ALLOC;
		goto cleanup;
	}

	memset(range, 0, sizeof(XRecordRange));
	// We only want KeyPresses and KeyReleases
	range->device_events.first = KeyPress;
	range->device_events.last = MotionNotify;

	// Create the context
	data->context = XRecordCreateContext(data->dpy, 0, &clients, 1, &range, 1);
	if (!data->context) {
		XFree(range);
		result = VINPUT_X11_XRECORD_CONTEXT;
		goto cleanup;
	}

	XFree(range);

	XSync(data->dpy, True);

	// Open a second display for datalink, this is what keeps events coming
	data->dpy_datalink = XOpenDisplay(NULL);
	if (!data->dpy_datalink) {
		result = VINPUT_X11_DISPLAY_DATALINK;
		goto cleanup;
	}

	listener->initialized = true;

cleanup:
	if (result != VINPUT_OK) EventListener_free(listener);
	return result;
}

// https://stackoverflow.com/a/10233743
KeySym keycode_to_keysym(
    EventListenerInternal *data, KeyCode keycode, unsigned int event_mask)
{
	KeySym keysym = NoSymbol;

	if (data->keyboard_map) {
		unsigned char info = XkbKeyGroupInfo(data->keyboard_map, keycode);
		unsigned int num_groups = XkbKeyNumGroups(data->keyboard_map, keycode);

		unsigned int group = 0x00;
		switch (XkbOutOfRangeGroupAction(info)) {
		case XkbRedirectIntoRange:
			group = XkbOutOfRangeGroupInfo(info);
			if (group >= num_groups) { group = 0; }
			break;

		case XkbClampIntoRange: group = num_groups - 1; break;
		case XkbWrapIntoRange:
		default:
			if (num_groups != 0) { group %= num_groups; }
			break;
		}

		XkbKeyTypePtr key_type = XkbKeyKeyType(data->keyboard_map, keycode, group);
		unsigned int active_mods = event_mask & key_type->mods.mask;

		int i, level = 0;
		for (i = 0; i < key_type->map_count; i++) {
			if (key_type->map[i].active && key_type->map[i].mods.mask == active_mods) {
				level = key_type->map[i].level;
			}
		}

		keysym = XkbKeySymEntry(data->keyboard_map, keycode, level, group);
	}

	return keysym;
}

KeyboardEvent xevent_to_key_event(
    EventListenerInternal *data_, XRecordInterceptData *data)
{
	// Get time in seconds and nanoseconds
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		KeyboardEvent evt = { 0 };
		evt.modifiers = data_->modifiers;
		return evt;
	}
	size_t timestamp = ts.tv_nsec / 1000000;

	xEvent *event = (xEvent *)data->data;
	KeySym keysym = keycode_to_keysym(data_, event->u.u.detail, 0);
	// If keysym is not ascii, set keychar to 0.
	char ch = keysym & 0xff;
	if ((keysym & 0xff00) > 0) ch = '\0';
	if (keysym == XK_Return)
		ch = '\n';
	else if (keysym == XK_Linefeed)
		ch = '\r';
	else if (keysym == XK_BackSpace)
		ch = '\b';

	KeyboardEvent final_event = {
		.pressed = false,
		.keychar = ch,
		.keycode = event->u.u.detail,
		.keysym = keysym,
		.modifiers = data_->modifiers,
		.timestamp = timestamp,
	};

	switch (event->u.u.type) {
	case KeyPress: final_event.pressed = true; break;
	case KeyRelease: final_event.pressed = false; break;
	default: return (KeyboardEvent) { 0 };
	}

	switch (final_event.keycode) {
	case XK_Shift_L: data_->modifiers.left_shift = final_event.pressed; break;
	case XK_Shift_R: data_->modifiers.right_shift = final_event.pressed; break;
	case XK_Control_L: data_->modifiers.left_control = final_event.pressed; break;
	case XK_Control_R: data_->modifiers.right_control = final_event.pressed; break;
	case XK_Alt_L: data_->modifiers.left_alt = final_event.pressed; break;
	case XK_Alt_R: data_->modifiers.right_alt = final_event.pressed; break;
	case XK_Meta_L: data_->modifiers.left_meta = final_event.pressed; break;
	case XK_Meta_R: data_->modifiers.right_meta = final_event.pressed; break;
	case XK_Super_L: data_->modifiers.left_super = final_event.pressed; break;
	case XK_Super_R: data_->modifiers.right_super = final_event.pressed; break;
	case XK_Hyper_L: data_->modifiers.left_hyper = final_event.pressed; break;
	case XK_Hyper_R: data_->modifiers.right_hyper = final_event.pressed; break;
	}

	final_event.modifiers = data_->modifiers;

	return final_event;
}

MouseButtonEvent xevent_to_mouse_button_event(
    EventListenerInternal *data_, XRecordInterceptData *data)
{
	(void)data_;
	xEvent *event = (xEvent *)data->data;

	MouseButtonEvent final_event = {
		.button = MouseButtonLeft,
		.kind = MousePressEvent,
	};

	switch (event->u.u.detail) {
	case Button1: final_event.button = MouseButtonLeft; break;
	case Button2: final_event.button = MouseButtonMiddle; break;
	case Button3: final_event.button = MouseButtonRight; break;
	default: final_event.button = MouseButtonLeft; break;
	}

	switch (event->u.u.type) {
	case ButtonPress: final_event.kind = MousePressEvent; break;
	case ButtonRelease: final_event.kind = MouseReleaseEvent; break;
	default: break;
	}

	return final_event;
}

MouseMoveEvent xevent_to_mouse_move_event(
    EventListenerInternal *data_, XRecordInterceptData *data)
{
	(void)data_;
	xEvent *event = (xEvent *)data->data;

	int x_pos = event->u.keyButtonPointer.rootX;
	int y_pos = event->u.keyButtonPointer.rootY;
	static __thread int prev_x = 0;
	static __thread int prev_y = 0;

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

void xrecord_callback(XPointer incoming, XRecordInterceptData *data)
{
	EventListenerInternal *data_ = (EventListenerInternal *)incoming;
	if (data->category == XRecordFromServer) {
		if (data_->callback && (data->data[0] == KeyPress || data->data[0] == KeyRelease))
			data_->callback(xevent_to_key_event(data_, data));
		else if (data_->callback_mouse_button
		         && (data->data[0] == ButtonPress || data->data[0] == ButtonRelease))
			data_->callback_mouse_button(xevent_to_mouse_button_event(data_, data));
		else if (data_->callback_mouse_move && data->data[0] == MotionNotify)
			data_->callback_mouse_move(xevent_to_mouse_move_event(data_, data));
	}

	XRecordFreeData(data);
}

VInputError x11_EventListener2_start(EventListener *listener, KeyboardCallback callback,
    MouseButtonCallback button_callback, MouseMoveCallback move_callback)
{
	if (!listener->initialized) return VINPUT_UNINITIALIZED;
	EventListenerInternal *data = listener->data;
	data->callback = callback;
	data->callback_mouse_move = move_callback;
	data->callback_mouse_button = button_callback;

	// Start listening for events
	if (!XRecordEnableContext(
	        data->dpy_datalink, data->context, xrecord_callback, (XPointer)data))
		return VINPUT_X11_ENABLE_XRECORD;

	return VINPUT_OK;
}

VInputError x11_EventListener_stop(EventListener *listener)
{
	if (!listener->initialized) return VINPUT_UNINITIALIZED;
	EventListenerInternal *data = listener->data;
	if (!data) return VINPUT_UNINITIALIZED;

	// Disabling the context makes the blocking XRecordEnableContext() call in
	// x11_EventListener2_start return. We must issue this on the control display
	// (data->dpy), NOT the datalink display, which is busy inside
	// XRecordEnableContext on the listening thread.
	if (data->context) {
		XRecordDisableContext(data->dpy, data->context);
		XFlush(data->dpy);
	}

	return VINPUT_OK;
}

VInputError x11_EventListener_free(EventListener *listener)
{
	if (!listener->initialized) return VINPUT_UNINITIALIZED;
	EventListenerInternal *data = listener->data;

	if (data) {
		if (data->context) XRecordDisableContext(data->dpy_datalink, data->context);
		if (data->dpy_datalink) XCloseDisplay(data->dpy_datalink);
		if (data->context) XRecordFreeContext(data->dpy, data->context);
		if (data->keyboard_map)
			XkbFreeClientMap(data->keyboard_map, XkbAllClientInfoMask, true);
		if (data->dpy) XCloseDisplay(data->dpy);
		memset(data, 0, sizeof(EventListenerInternal));
		free(data);
	}

	return VINPUT_OK;
}
