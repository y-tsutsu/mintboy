#include "mintboy/cartridge.hpp"
#include "mintboy/cpu.hpp"
#include "mintboy/memory.hpp"

#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    constexpr int CyclesPerFrame = 70224;
    constexpr int DefaultFrameCount = 60;

    int ParseFrameCount(const char *text)
    {
        const int frames = std::stoi(text);
        if (frames <= 0)
        {
            throw std::invalid_argument("frame count must be positive");
        }
        return frames;
    }

    std::uint64_t FramebufferHash(const mintboy::Memory::Framebuffer &framebuffer)
    {
        std::uint64_t hash = 14695981039346656037ULL;
        for (const std::uint32_t pixel : framebuffer)
        {
            hash ^= pixel;
            hash *= 1099511628211ULL;
        }
        return hash;
    }
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
    {
        std::cerr << "usage: mintboy_headless <rom.gb> [frames]\n";
        return 2;
    }

    try
    {
        const int frames = argc == 3 ? ParseFrameCount(argv[2]) : DefaultFrameCount;

        mintboy::Cartridge cartridge = mintboy::Cartridge::LoadFromFile(argv[1]);
        mintboy::Memory memory(cartridge);
        mintboy::Cpu cpu(memory);
        std::string serial_output;

        for (int frame = 0; frame < frames; ++frame)
        {
            int cycles = 0;
            while (cycles < CyclesPerFrame)
            {
                cycles += cpu.Step();
            }
            [[maybe_unused]] const auto audio_samples = memory.DrainAudioSamples();
            serial_output += memory.DrainSerialOutput();
        }

        std::cout << "Title: " << cartridge.Title() << '\n';
        std::cout << "Frames: " << frames << '\n';
        std::cout << std::format("Framebuffer hash: 0x{:016X}\n", FramebufferHash(memory.GetFramebuffer()));
        std::cout << "CPU PC: " << std::format("0x{:04X}", cpu.GetRegisters().pc) << '\n';
        if (!serial_output.empty())
        {
            std::cout << "Serial output:\n"
                      << serial_output;
            if (serial_output.back() != '\n')
            {
                std::cout << '\n';
            }
        }
    }
    catch (const std::exception &error)
    {
        std::cerr << "mintboy_headless: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
