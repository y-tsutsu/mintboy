#include "mintboy/cartridge.hpp"
#include "mintboy/cpu.hpp"
#include "mintboy/memory.hpp"

#include <cstdint>
#include <exception>
#include <format>
#include <fstream>
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

    std::string ParseFrameDumpPath(int argc, char **argv)
    {
        if (argc == 2 || argc == 3)
        {
            return {};
        }

        if (argc == 5 && std::string(argv[3]) == "--dump-frame")
        {
            return argv[4];
        }

        throw std::invalid_argument("usage: mintboy_headless <rom.gb> [frames] [--dump-frame frame.ppm]");
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

    void DumpFramebuffer(const mintboy::Memory::Framebuffer &framebuffer, const std::string &path)
    {
        std::ofstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("failed to open frame dump: " + path);
        }

        file << "P6\n"
             << mintboy::Memory::ScreenWidth << ' ' << mintboy::Memory::ScreenHeight << "\n255\n";
        for (const std::uint32_t pixel : framebuffer)
        {
            const char rgb[] = {
                static_cast<char>((pixel >> 16) & 0xFF),
                static_cast<char>((pixel >> 8) & 0xFF),
                static_cast<char>(pixel & 0xFF),
            };
            file.write(rgb, sizeof(rgb));
        }
    }

    std::string BlarggMemoryOutput(mintboy::Memory &memory)
    {
        if (memory.ReadByte(0xA001) != 0xDE || memory.ReadByte(0xA002) != 0xB0 || memory.ReadByte(0xA003) != 0x61)
        {
            return {};
        }

        std::string output = std::format("Blargg memory status: 0x{:02X}\n", memory.ReadByte(0xA000));
        output += "Blargg memory output:\n";
        for (mintboy::Word address = 0xA004; address <= 0xBFFF; ++address)
        {
            const mintboy::Byte value = memory.ReadByte(address);
            if (value == 0)
            {
                break;
            }
            output.push_back(static_cast<char>(value));
        }
        if (!output.empty() && output.back() != '\n')
        {
            output.push_back('\n');
        }
        return output;
    }
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 5)
    {
        std::cerr << "usage: mintboy_headless <rom.gb> [frames] [--dump-frame frame.ppm]\n";
        return 2;
    }

    try
    {
        const int frames = argc >= 3 ? ParseFrameCount(argv[2]) : DefaultFrameCount;
        const std::string frame_dump_path = ParseFrameDumpPath(argc, argv);

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
        if (!frame_dump_path.empty())
        {
            DumpFramebuffer(memory.GetFramebuffer(), frame_dump_path);
            std::cout << "Frame dump: " << frame_dump_path << '\n';
        }
        if (!serial_output.empty())
        {
            std::cout << "Serial output:\n"
                      << serial_output;
            if (serial_output.back() != '\n')
            {
                std::cout << '\n';
            }
        }

        const std::string blargg_memory_output = BlarggMemoryOutput(memory);
        if (!blargg_memory_output.empty())
        {
            std::cout << blargg_memory_output;
        }
    }
    catch (const std::exception &error)
    {
        std::cerr << "mintboy_headless: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
