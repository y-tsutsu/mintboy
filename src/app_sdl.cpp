#include "mintboy/cartridge.hpp"
#include "mintboy/cpu.hpp"
#include "mintboy/memory.hpp"

#include <SDL.h>

#include <array>
#include <chrono>
#include <cstdint>
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

    class Sdl
    {
    public:
        Sdl()
        {
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0)
            {
                throw std::runtime_error(SDL_GetError());
            }
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

    void UpdateJoypad(mintboy::Memory &memory, const SDL_KeyboardEvent &key, bool pressed)
    {
        switch (key.keysym.scancode)
        {
        case SDL_SCANCODE_RIGHT:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::Right, pressed);
            return;
        case SDL_SCANCODE_LEFT:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::Left, pressed);
            return;
        case SDL_SCANCODE_UP:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::Up, pressed);
            return;
        case SDL_SCANCODE_DOWN:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::Down, pressed);
            return;
        case SDL_SCANCODE_Z:
        case SDL_SCANCODE_A:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::A, pressed);
            return;
        case SDL_SCANCODE_X:
        case SDL_SCANCODE_S:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::B, pressed);
            return;
        case SDL_SCANCODE_BACKSPACE:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::Select, pressed);
            return;
        case SDL_SCANCODE_RETURN:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::Start, pressed);
            return;
        default:
            break;
        }

        switch (key.keysym.sym)
        {
        case SDLK_RIGHT:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::Right, pressed);
            break;
        case SDLK_LEFT:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::Left, pressed);
            break;
        case SDLK_UP:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::Up, pressed);
            break;
        case SDLK_DOWN:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::Down, pressed);
            break;
        case SDLK_z:
        case SDLK_a:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::A, pressed);
            break;
        case SDLK_x:
        case SDLK_s:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::B, pressed);
            break;
        case SDLK_BACKSPACE:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::Select, pressed);
            break;
        case SDLK_RETURN:
            memory.SetJoypadButton(mintboy::Memory::JoypadButton::Start, pressed);
            break;
        default:
            break;
        }
    }

    bool PollEvents(mintboy::Memory &memory)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT)
            {
                return true;
            }

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            {
                return true;
            }

            if (event.type == SDL_KEYDOWN && event.key.repeat == 0)
            {
                UpdateJoypad(memory, event.key, true);
            }

            if (event.type == SDL_KEYUP)
            {
                UpdateJoypad(memory, event.key, false);
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

        bool running = true;
        bool cpu_running = true;
        while (running)
        {
            running = !PollEvents(memory);

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
