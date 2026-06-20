# mintboy

mintboy is a C++20 Game Boy emulator project.

The current codebase has the first core pieces in place:

- ROM loading and Game Boy header parsing
- A minimal memory map for ROM, WRAM, HRAM, and interrupt enable
- A small CPU skeleton with a few opcodes
- A lightweight test binary
- An optional SDL2 frontend

## Build on Debian/WSL

Install Ninja, SDL2, and toml++:

```console
$ sudo apt update
$ sudo apt install -y ninja-build libsdl2-dev libtomlplusplus-dev
```

Then build and run tests:

```console
$ cmake --preset debug
$ cmake --build --preset debug --parallel
$ ctest --preset debug
```

The build output is written to the ignored `build/` directory. The Debian 13 packages are available as `ninja-build`, `libsdl2-dev`, and `libtomlplusplus-dev`. If SDL2 is not installed, CMake still builds the CLI fallback and tests. The `mintboy_info` executable prints ROM header information, including the DMG/CGB compatibility flag.

If `build/` already exists from an older Makefiles-based configuration, delete and recreate it before switching to the Ninja presets.

## Headless smoke test

The `mintboy_headless` executable runs a ROM without opening an SDL2 window. It is useful for quick regression checks while emulator accuracy is still evolving:

```console
$ ./build/mintboy_headless path/to/test.gb 60
```

The optional frame count defaults to 60. The command fails if the CPU hits an invalid or unimplemented opcode, and otherwise prints the final framebuffer hash, CPU PC, and serial output. This is useful for Blargg-style test ROMs that report `Passed` through the Game Boy serial port.

Blargg's Game Boy test ROMs can be used as an external test suite:

- GitHub mirror: https://github.com/retrio/gb-test-roms
- Upstream archive noted by the mirror: http://blargg.8bitalley.com/parodius/gb-tests/

Keep these ROMs outside the repository history, for example under the ignored `rom/tests/` directory:

```console
$ mkdir -p rom/tests
$ git clone https://github.com/retrio/gb-test-roms.git rom/tests/gb-test-roms
$ ./build/mintboy_headless "rom/tests/gb-test-roms/cpu_instrs/individual/01-special.gb" 600
```

The test ROM collection is intentionally not vendored or added as a git submodule here because its licensing terms are not clearly documented in the mirror.

When the ROMs are present at `rom/tests/gb-test-roms`, CMake also registers selected Blargg CPU, timing, HALT bug, and DMG sound ROMs as optional CTest cases. Re-run CMake after cloning the ROMs so the tests are discovered:

```console
$ cmake --preset debug
$ ctest --preset debug
```

## References

The emulator implementation is cross-checked against public Game Boy hardware references and emulator test discussions:

- Pan Docs: https://gbdev.io/pandocs/
- Blargg's Game Boy test ROMs mirror: https://github.com/retrio/gb-test-roms
- Blargg test discussion on nesdev: https://forums.nesdev.org/viewtopic.php?t=13730

Pan Docs is the primary hardware behavior reference. The Blargg ROMs and related discussions are used as executable compatibility checks and as context when a test covers obscure timing behavior.

## SDL2 frontend

When SDL2 is available, the `mintboy` executable opens an SDL2 window:

```console
$ ./build/mintboy path/to/game.gb
```

You can also create a local `mintboy.toml` next to the executable or in the current working directory and launch `mintboy` without a ROM argument. Relative ROM paths are resolved from the config file location. Command-line arguments override the config file:

```toml
[rom]
path = "rom/tetris.gb"

[input]
swap_controller_ab = false
trace_input = false

[video]
window_scale = 4

[audio]
enabled = true
volume = 0.5
```

`mintboy.toml` is ignored by git. Copy `mintboy.example.toml` as a starting point if needed.

On WSL, an SDL2 window requires WSLg or another X/Wayland server.

Audio output is experimental but supports the DMG square, wave, and noise channels. `audio.volume` accepts values from `0.0` to `1.0`.

Battery-backed cartridge RAM is saved next to the ROM as `game.sav` and loaded automatically on the next run. Save files are ignored by git.

mintboy currently runs in DMG mode. DMG/CGB-compatible ROMs may run using their DMG behavior, but CGB-only ROMs are not supported yet.

Keyboard controls:

- D-pad: Arrow keys
- A: Z or A
- B: X or S
- Select: Backspace
- Start: Enter
- Quit: Escape

Debug controls:

- Pause: P
- Frame step: .
- Speed 1x: 1
- Speed 2x: 2
- Speed 0.5x: 3

SDL2 game controllers are also supported when the controller is visible to the OS:

- D-pad or left stick: D-pad
- A or X: A
- B or Y: B
- Back: Select
- Start: Start

Set `MINTBOY_SWAP_CONTROLLER_AB=1` to swap the controller A/B mapping for SNES-style layouts. Environment variables override `mintboy.toml`.

On WSLg, if Z/X/A/S do not work, switch the IME/input mode with the Hankaku/Zenkaku key.
WSL may not expose USB controllers to Linux by default. If the controller is not listed under `/dev/input`, use the keyboard controls or run the emulator on Windows once the Windows build is revisited.

## Build on Windows with MSYS2 and vcpkg

These steps assume MSYS2, Git, CMake, and Ninja are already installed, and that the MSYS2 MinGW compiler tools are available on `PATH`.

Install vcpkg under the ignored `build/` directory, then install the runtime dependencies:

```console
$ git clone https://github.com/microsoft/vcpkg.git build\vcpkg
$ .\build\vcpkg\bootstrap-vcpkg.bat
$ .\build\vcpkg\vcpkg.exe install sdl2:x64-mingw-dynamic tomlplusplus:x64-mingw-dynamic
```

The `windows-mingw` preset expects vcpkg at `build\vcpkg`. If you delete `build\`, reinstall vcpkg before configuring again.

Then configure, build, and run tests:

```console
$ cmake --preset windows-mingw
$ cmake --build --preset windows-mingw --parallel
$ ctest --preset windows-mingw
```

Run the SDL2 frontend from the build directory so `SDL2.dll` can be found:

```console
$ .\build\mintboy.exe path\to\game.gb
```

To trace keyboard and controller input while testing:

```console
$ set MINTBOY_TRACE_INPUT=1
$ .\build\mintboy.exe path\to\game.gb
```

To disable the SDL2 frontend explicitly:

```console
$ cmake --preset windows-mingw -DMINTBOY_BUILD_SDL=OFF
```
