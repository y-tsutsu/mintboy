# mintboy

mintboy is a C++20 Game Boy emulator project.

The current codebase has the first core pieces in place:

- ROM loading and Game Boy header parsing
- A minimal memory map for ROM, WRAM, HRAM, and interrupt enable
- A small CPU skeleton with a few opcodes
- A lightweight test binary
- An optional SDL2 frontend

## Build on Debian/WSL

Install the SDL2 development package:

```console
$ sudo apt update
$ sudo apt install -y libsdl2-dev
```

Then build and run tests:

```console
$ cmake -S . -B build
$ cmake --build build
$ ctest --test-dir build --output-on-failure
```

The Debian 13 package is available as `libsdl2-dev`. If SDL2 is not installed, CMake still builds the CLI fallback and tests. The `mintboy_info` executable prints ROM header information.

## SDL2 frontend

When SDL2 is available, the `mintboy` executable opens an SDL2 window:

```console
$ ./build/mintboy path/to/game.gb
```

On WSL, an SDL2 window requires WSLg or another X/Wayland server.

Keyboard controls:

- D-pad: Arrow keys
- A: Z or A
- B: X or S
- Select: Backspace
- Start: Enter
- Quit: Escape

SDL2 game controllers are also supported when the controller is visible to the OS:

- D-pad or left stick: D-pad
- B or Y: A
- A or X: B
- Back: Select
- Start: Start

On WSLg, if Z/X/A/S do not work, switch the IME/input mode with the Hankaku/Zenkaku key.
WSL may not expose USB controllers to Linux by default. If the controller is not listed under `/dev/input`, use the keyboard controls or run the emulator on Windows once the Windows build is revisited.

## Build on Windows with MSYS2 and vcpkg

These steps assume MSYS2, Git, and CMake are already installed, and that the
MSYS2 MinGW compiler tools are available on `PATH`.

Install vcpkg under the ignored `build/` directory:

```console
$ git clone https://github.com/microsoft/vcpkg.git build\vcpkg
$ .\build\vcpkg\bootstrap-vcpkg.bat
$ .\build\vcpkg\vcpkg.exe install sdl2:x64-mingw-dynamic
```

Then configure, build, and run tests:

```console
$ cmake -S . -B build\windows-mingw -G "MinGW Makefiles" ^
  -DCMAKE_TOOLCHAIN_FILE="%CD%\build\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic ^
  -DCMAKE_BUILD_TYPE=Release
$ cmake --build build\windows-mingw --parallel
$ ctest --test-dir build\windows-mingw --output-on-failure
```

Run the SDL2 frontend from the build directory so `SDL2.dll` can be found:

```console
$ .\build\windows-mingw\mintboy.exe path\to\game.gb
```

To trace keyboard and controller input while testing:

```console
$ set MINTBOY_TRACE_INPUT=1
$ .\build\windows-mingw\mintboy.exe path\to\game.gb
```

To disable the SDL2 frontend explicitly:

```console
$ cmake -S . -B build -DMINTBOY_BUILD_SDL=OFF
```
