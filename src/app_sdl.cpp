#include "mintboy/cartridge.hpp"
#include "mintboy/cpu.hpp"
#include "mintboy/memory.hpp"

#include <SDL.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
    constexpr int WindowScale = 4;
    constexpr int CyclesPerFrame = 70224;

    bool TraceInputEnabled()
    {
        return std::getenv("MINTBOY_TRACE_INPUT") != nullptr;
    }

    class Sdl
    {
    public:
        Sdl()
        {
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
            {
                throw std::runtime_error(SDL_GetError());
            }
            SDL_GameControllerEventState(SDL_ENABLE);
        }

        ~Sdl()
        {
            SDL_Quit();
        }

        Sdl(const Sdl &) = delete;
        Sdl &operator=(const Sdl &) = delete;
    };

    class Window
    {
    public:
        explicit Window(const std::string &title)
        {
            window_ = SDL_CreateWindow(
                title.c_str(),
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                mintboy::Memory::ScreenWidth * WindowScale,
                mintboy::Memory::ScreenHeight * WindowScale,
                SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
            if (window_ == nullptr)
            {
                throw std::runtime_error(SDL_GetError());
            }

            renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
            if (renderer_ == nullptr)
            {
                throw std::runtime_error(SDL_GetError());
            }

            texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, mintboy::Memory::ScreenWidth, mintboy::Memory::ScreenHeight);
            if (texture_ == nullptr)
            {
                throw std::runtime_error(SDL_GetError());
            }
        }

        ~Window()
        {
            SDL_DestroyTexture(texture_);
            SDL_DestroyRenderer(renderer_);
            SDL_DestroyWindow(window_);
        }

        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

        void Present(const mintboy::Memory::Framebuffer &pixels)
        {
            SDL_UpdateTexture(texture_, nullptr, pixels.data(), mintboy::Memory::ScreenWidth * static_cast<int>(sizeof(std::uint32_t)));
            SDL_RenderClear(renderer_);
            SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
            SDL_RenderPresent(renderer_);
        }

    private:
        SDL_Window *window_ = nullptr;
        SDL_Renderer *renderer_ = nullptr;
        SDL_Texture *texture_ = nullptr;
    };

    class Controller
    {
    public:
        Controller()
        {
            OpenFirstAvailable();
        }

        ~Controller()
        {
            Close();
        }

        Controller(const Controller &) = delete;
        Controller &operator=(const Controller &) = delete;

        void HandleEvent(const SDL_Event &event)
        {
            if (event.type == SDL_CONTROLLERDEVICEADDED && controller_ == nullptr)
            {
                Open(event.cdevice.which);
                return;
            }

            if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller_ != nullptr && event.cdevice.which == instance_id_)
            {
                Close();
                OpenFirstAvailable();
            }
        }

        [[nodiscard]] bool Right() const
        {
            return Button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || Axis(SDL_CONTROLLER_AXIS_LEFTX) > AxisThreshold;
        }

        [[nodiscard]] bool Left() const
        {
            return Button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || Axis(SDL_CONTROLLER_AXIS_LEFTX) < -AxisThreshold;
        }

        [[nodiscard]] bool Up() const
        {
            return Button(SDL_CONTROLLER_BUTTON_DPAD_UP) || Axis(SDL_CONTROLLER_AXIS_LEFTY) < -AxisThreshold;
        }

        [[nodiscard]] bool Down() const
        {
            return Button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || Axis(SDL_CONTROLLER_AXIS_LEFTY) > AxisThreshold;
        }

        [[nodiscard]] bool A() const
        {
            return Button(SDL_CONTROLLER_BUTTON_B) || Button(SDL_CONTROLLER_BUTTON_Y);
        }

        [[nodiscard]] bool B() const
        {
            return Button(SDL_CONTROLLER_BUTTON_A) || Button(SDL_CONTROLLER_BUTTON_X);
        }

        [[nodiscard]] bool Select() const
        {
            return Button(SDL_CONTROLLER_BUTTON_BACK);
        }

        [[nodiscard]] bool Start() const
        {
            return Button(SDL_CONTROLLER_BUTTON_START);
        }

    private:
        static constexpr Sint16 AxisThreshold = 16000;

        void OpenFirstAvailable()
        {
            const int joystick_count = SDL_NumJoysticks();
            for (int index = 0; index < joystick_count; ++index)
            {
                if (SDL_IsGameController(index) == SDL_TRUE && Open(index))
                {
                    return;
                }
            }

            if (TraceInputEnabled())
            {
                std::cerr << "controller unavailable joysticks=" << joystick_count << '\n';
            }
        }

        bool Open(int device_index)
        {
            SDL_GameController *controller = SDL_GameControllerOpen(device_index);
            if (controller == nullptr)
            {
                if (TraceInputEnabled())
                {
                    std::cerr << "controller open failed index=" << device_index << " error=" << SDL_GetError() << '\n';
                }
                return false;
            }

            Close();
            controller_ = controller;
            SDL_Joystick *joystick = SDL_GameControllerGetJoystick(controller_);
            instance_id_ = joystick == nullptr ? -1 : SDL_JoystickInstanceID(joystick);

            if (TraceInputEnabled())
            {
                const char *name = SDL_GameControllerName(controller_);
                std::cerr << "controller opened index=" << device_index << " name=" << (name == nullptr ? "" : name) << '\n';
            }
            return true;
        }

        void Close()
        {
            if (controller_ == nullptr)
            {
                return;
            }

            if (TraceInputEnabled())
            {
                std::cerr << "controller closed instance=" << instance_id_ << '\n';
            }
            SDL_GameControllerClose(controller_);
            controller_ = nullptr;
            instance_id_ = -1;
        }

        [[nodiscard]] bool Button(SDL_GameControllerButton button) const
        {
            return controller_ != nullptr && SDL_GameControllerGetButton(controller_, button) != 0;
        }

        [[nodiscard]] Sint16 Axis(SDL_GameControllerAxis axis) const
        {
            return controller_ == nullptr ? 0 : SDL_GameControllerGetAxis(controller_, axis);
        }

        SDL_GameController *controller_ = nullptr;
        SDL_JoystickID instance_id_ = -1;
    };

    void SyncJoypad(mintboy::Memory &memory, const Controller &controller)
    {
        static std::uint16_t last_trace_state = 0xFFFF;
        SDL_PumpEvents();
        const std::uint8_t *keys = SDL_GetKeyboardState(nullptr);

        const bool right = keys[SDL_SCANCODE_RIGHT] != 0 || controller.Right();
        const bool left = keys[SDL_SCANCODE_LEFT] != 0 || controller.Left();
        const bool up = keys[SDL_SCANCODE_UP] != 0 || controller.Up();
        const bool down = keys[SDL_SCANCODE_DOWN] != 0 || controller.Down();
        const bool a = keys[SDL_SCANCODE_Z] != 0 || keys[SDL_SCANCODE_A] != 0 || controller.A();
        const bool b = keys[SDL_SCANCODE_X] != 0 || keys[SDL_SCANCODE_S] != 0 || controller.B();
        const bool select = keys[SDL_SCANCODE_BACKSPACE] != 0 || controller.Select();
        const bool start = keys[SDL_SCANCODE_RETURN] != 0 || controller.Start();

        memory.SetJoypadButton(mintboy::Memory::JoypadButton::Right, right);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::Left, left);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::Up, up);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::Down, down);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::A, a);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::B, b);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::Select, select);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::Start, start);

        if (TraceInputEnabled())
        {
            std::uint16_t trace_state = 0;
            trace_state |= right ? 1 << 0 : 0;
            trace_state |= left ? 1 << 1 : 0;
            trace_state |= up ? 1 << 2 : 0;
            trace_state |= down ? 1 << 3 : 0;
            trace_state |= a ? 1 << 4 : 0;
            trace_state |= b ? 1 << 5 : 0;
            trace_state |= select ? 1 << 6 : 0;
            trace_state |= start ? 1 << 7 : 0;
            if (trace_state == last_trace_state)
            {
                return;
            }
            last_trace_state = trace_state;

            std::cerr << "input state"
                      << " right=" << right
                      << " left=" << left
                      << " up=" << up
                      << " down=" << down
                      << " a=" << a
                      << " b=" << b
                      << " select=" << select
                      << " start=" << start
                      << '\n';
        }
    }

    bool PollEvents(Controller &controller)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0)
        {
            controller.HandleEvent(event);

            if (TraceInputEnabled() && (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP))
            {
                std::cerr << "input event"
                          << " type=" << (event.type == SDL_KEYDOWN ? "down" : "up")
                          << " scancode=" << SDL_GetScancodeName(event.key.keysym.scancode)
                          << " key=" << SDL_GetKeyName(event.key.keysym.sym)
                          << " repeat=" << static_cast<int>(event.key.repeat)
                          << '\n';
            }

            if (event.type == SDL_QUIT)
            {
                return true;
            }

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            {
                return true;
            }
        }

        return false;
    }
}

int main(int argc, char **argv)
{
    SDL_SetMainReady();

    if (argc != 2)
    {
        std::cerr << "usage: mintboy <rom.gb>\n";
        return 2;
    }

    try
    {
        mintboy::Cartridge cartridge = mintboy::Cartridge::LoadFromFile(argv[1]);
        mintboy::Memory memory(cartridge);
        mintboy::Cpu cpu(memory);

        const Sdl sdl;
        const std::string rom_title = cartridge.Title().empty()
                                          ? std::filesystem::path(argv[1]).filename().string()
                                          : cartridge.Title();
        Window window("mintboy - " + rom_title);
        Controller controller;

        bool running = true;
        bool cpu_running = true;
        while (running)
        {
            running = !PollEvents(controller);
            SyncJoypad(memory, controller);

            int cycles = 0;
            while (cycles < CyclesPerFrame && cpu_running)
            {
                try
                {
                    cycles += cpu.Step();
                }
                catch (const std::exception &error)
                {
                    std::cerr << "CPU stopped: " << error.what() << '\n';
                    cpu_running = false;
                    break;
                }
            }

            window.Present(memory.GetFramebuffer());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    catch (const std::exception &error)
    {
        std::cerr << "mintboy: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
