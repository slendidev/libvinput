#include <stdio.h>
#ifdef _WIN32
#	include <windows.h>
#else
#	include <unistd.h>
#endif

#include "src/libvinput.h"

static void demo_sleep(void)
#ifdef _WIN32
{ Sleep(1000); }
#else
{ sleep(1); }
#endif

int main(void)
{
	EventEmulator emu;
	VInputError err = EventEmulator_create(&emu);
	if (err) {
		printf("Error: %s\n", VInput_error_get_message(err));
		return 1;
	}

	puts("5");
	demo_sleep();
	puts("4");
	demo_sleep();
	puts("3");
	demo_sleep();
	puts("2");
	demo_sleep();
	puts("1");
	demo_sleep();

	err = EventEmulator_typec(&emu, 'H');
	if (err) {
		printf("Error: %s\n", VInput_error_get_message(err));
		return 1;
	}
	err = EventEmulator_typec(&emu, 'i');
	if (err) {
		printf("Error: %s\n", VInput_error_get_message(err));
		return 1;
	}
	err = EventEmulator_types(&emu, "\nHello world!", 13);
	if (err) {
		printf("Error: %s\n", VInput_error_get_message(err));
		return 1;
	}
}
