#include "libvinput.h"

#include <stdlib.h>
#include <string.h>

bool is_x11(void)
{
	char *env = getenv("XDG_SESSION_TYPE");
	if (!env) return false;
	return strcmp(env, "x11") == 0;
}

extern VInputError _x11_EventListener_init(EventListener *listener);
extern VInputError x11_EventListener2_start(EventListener *listener,
    KeyboardCallback callback, MouseButtonCallback button_callback,
    MouseMoveCallback move_callback);
extern VInputError x11_EventListener_stop(EventListener *listener);
extern VInputError x11_EventListener_free(EventListener *listener);

extern VInputError _evdev_EventListener_init(EventListener *listener);
extern VInputError evdev_EventListener2_start(EventListener *listener,
    KeyboardCallback callback, MouseButtonCallback button_callback,
    MouseMoveCallback move_callback);
extern VInputError evdev_EventListener_stop(EventListener *listener);
extern VInputError evdev_EventListener_free(EventListener *listener);

VInputError _EventListener_init(EventListener *listener)
{
	if (is_x11())
		return _x11_EventListener_init(listener);
	else
		return _evdev_EventListener_init(listener);
}

VInputError EventListener2_start(EventListener *listener, KeyboardCallback callback,
    MouseButtonCallback button_callback, MouseMoveCallback move_callback)
{
	if (is_x11())
		return x11_EventListener2_start(listener, callback, button_callback, move_callback);
	else
		return evdev_EventListener2_start(listener, callback, button_callback, move_callback);
}

VInputError EventListener_stop(EventListener *listener)
{
	if (is_x11())
		return x11_EventListener_stop(listener);
	else
		return evdev_EventListener_stop(listener);
}

VInputError EventListener_free(EventListener *listener)
{
	if (is_x11())
		return x11_EventListener_free(listener);
	else
		return evdev_EventListener_free(listener);
}
