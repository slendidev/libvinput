CFLAGS = -I. -Wall -Wextra
ifeq ($(DEBUG),1)
	CFLAGS += -O0 -ggdb
else
	CFLAGS += -O3
endif

VERSION = 0x010203

SRC = src/libvinput.c

# Linux/X11/uinput+evdev
ifeq ($(shell uname), Linux)
all: wordlogger test_emu test_echo

ifeq ($(OS), MINGW)
all: wordlogger.exe test_emu.exe libvinput.dll

SRC += \
	src/windows_emu.c \
	src/windows.c

libvinput.dll: $(SRC)
	x86_64-w64-mingw32-gcc $(CFLAGS) -DVERSION=$(VERSION) -shared -o $@ $^ -DWINVER=0x0600 -luser32 -lkernel32

wordlogger.exe: wordlogger.c libvinput.dll
	x86_64-w64-mingw32-gcc $(CFLAGS) -o $@ wordlogger.c -L. -lvinput -DWINVER=0x0600 -luser32 -lkernel32

test_emu.exe: test_emu.c libvinput.dll
	x86_64-w64-mingw32-gcc $(CFLAGS) -o $@ test_emu.c -L. -lvinput -DWINVER=0x0600 -luser32 -lkernel32

test_echo.exe: test_echo.c libvinput.dll
	x86_64-w64-mingw32-gcc $(CFLAGS) -o $@ test_echo.c -L. -lvinput -DWINVER=0x0600 -luser32 -lkernel32

clean:
	rm -f wordlogger.exe
	rm -f libvinput.dll

else

SRC += \
			 src/linux_emu.c \
			 src/linux.c \
			 src/linux_x11.c \
			 src/linux_emu_x11.c \
			 src/linux_evdev.c \
			 src/linux_uinput_emu.c


libvinput.so: $(SRC)
	$(CC) $(CFLAGS) -DVERSION=$(VERSION) -fPIC -o $@ -shared $^ -lm -lX11 -lXtst $(shell pkg-config --cflags --libs libevdev) -I/usr/local/include -Isrc -L/usr/local/lib -lxdo

wordlogger: wordlogger.c libvinput.so
	$(CC) $(CFLAGS) wordlogger.c -o $@ -L. -lvinput -lX11 -lXtst -levdev -I/usr/local/include -Isrc -L/usr/local/lib -lxdo

test_emu: test_emu.c libvinput.so
	$(CC) $(CFLAGS) test_emu.c -o $@ -L. -lvinput -lX11 -lXtst -levdev -I/usr/local/include -Isrc -L/usr/local/lib -lxdo

test_echo: test_echo.c libvinput.so
	$(CC) $(CFLAGS) test_echo.c -o $@ -L. -lvinput -lX11 -lXtst -levdev -I/usr/local/include -Isrc -L/usr/local/lib -lxdo

install: libvinput.so
	rm -f /usr/local/lib/libvinput.* && install -m 777 libvinput.so /usr/local/lib && mv /usr/local/lib/libvinput.so /usr/local/lib/libvinput.so.$(VERSION) && ln -s /usr/local/lib/libvinput.so.$(VERSION) /usr/local/lib/libvinput.so
	install -m 644 src/libvinput.h /usr/local/include
	install -m 644 libvinput.pc /usr/local/lib/pkgconfig

clean:
	rm -f wordlogger
	rm -f test_emu
	rm -f libvinput.so

endif
endif

# MacOS
ifeq ($(shell uname), Darwin)
all: wordlogger_mac test_emu_mac

SRC += \
	src/macos_emu.c \
	src/macos.c

libvinput.dylib: $(SRC)
	$(CC) $(CFLAGS) -DVERSION=$(VERSION) -arch arm64 -arch x86_64 -fPIC -framework ApplicationServices -framework Carbon -o $@ -shared $^

wordlogger_mac: wordlogger.c libvinput.dylib
	$(CC) $(CFLAGS) wordlogger.c -o $@ -Isrc -L. -lvinput -Wl,-rpath,@loader_path/.

test_emu_mac: test_emu.c libvinput.dylib
	$(CC) $(CFLAGS) test_emu.c -o $@ -Isrc -L. -lvinput -Wl,-rpath,@loader_path/.

test_echo: test_echo.c libvinput.dylib
	$(CC) $(CFLAGS) test_echo.c -o $@ -Isrc -L. -lvinput -Wl,-rpath,@loader_path/.

clean:
	rm -f wordlogger_mac
	rm -f test_emu_mac
	rm -f libvinput.dylib
endif

# Windows/WINAPI
ifeq ($(OS), Windows_NT)

all: wordlogger libvinput.dll vinput.lib
	dumpbin /EXPORTS vinput.lib

libvinput.dll: libvinput.obj windows_emu.obj windows.obj
	link /DLL /OUT:libvinput.dll libvinput.obj windows_emu.obj windows.obj User32.lib Kernel32.lib

libvinput.obj: src/libvinput.c
	cl /D VERSION= $(VERSION) /LD /I src /DBUILDING_VINPUT /Fo$@ /c src\libvinput.c

windows_emu.obj: src/windows_emu.c
	cl /D VERSION= $(VERSION) /LD /I src /DBUILDING_VINPUT /Fo$@ /c src\windows_emu.c

windows.obj: src/windows.c
	cl /D VERSION= $(VERSION) /LD /I src /DBUILDING_VINPUT /Fo$@ /c src\windows.c

vinput.lib: libvinput.obj windows_emu.obj windows.obj
	lib /OUT:vinput.lib libvinput.obj windows_emu.obj windows.obj

wordlogger: wordlogger.c vinput.lib
	cl /I src /Fo$@ /Fe$@ wordlogger.c vinput.lib User32.lib Kernel32.lib

test_emu: test_emu.c vinput.lib
	cl /I src /Fo$@ /Fe$@ test_emu.c vinput.lib User32.lib Kernel32.lib

test_echo: test_echo.c vinput.lib
	cl /I src /Fo$@ /Fe$@ test_echo.c vinput.lib User32.lib Kernel32.lib

clean:
	del /F /Q libvinput.dll vinput.lib libvinput.obj windows_emu.obj windows.obj
endif

cppcheck:
	cppcheck --enable=warning --check-level=exhaustive --inconclusive --std=c99 --force --quiet -i src/ext/vec.c $(SRC) wordlogger.c test_emu.c

.PHONY: all clean cppcheck install

