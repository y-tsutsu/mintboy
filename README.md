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

```sh
sudo apt update
sudo apt install -y libsdl2-dev
```

Then build and run tests:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The Debian 13 package is available as `libsdl2-dev`. If SDL2 is not installed, CMake still builds the CLI fallback and tests. The `mintboy_info` executable prints ROM header information.

## SDL2 frontend

When SDL2 is available, the `mintboy` executable opens an SDL2 window:

```sh
./build/mintboy path/to/game.gb
```

On WSL, an SDL2 window requires WSLg or another X/Wayland server.

## Windows

Windows support is planned, but not the current priority. When revisiting it, vcpkg should be the simplest setup:

```powershell
vcpkg install sdl2:x64-windows
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build
ctest --test-dir build --output-on-failure
```

To disable the SDL2 frontend explicitly:

```sh
cmake -S . -B build -DMINTBOY_BUILD_SDL=OFF
```
