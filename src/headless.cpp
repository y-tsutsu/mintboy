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

    struct Options
    {
        int frames = DefaultFrameCount;
        std::string frame_dump_path;
        bool render_video = false;
    };

    int ParseFrameCount(const char *text)
    {
        const int frames = std::stoi(text);
        if (frames <= 0)
        {
            throw std::invalid_argument("frame count must be positive");
        }
        return frames;
    }

    Options ParseOptions(int argc, char **argv)
    {
        Options options;
        if (argc >= 3)
        {
            options.frames = ParseFrameCount(argv[2]);
        }

        for (int arg = 3; arg < argc; ++arg)
        {
            const std::string option = argv[arg];
            if (option == "--render")
            {
                options.render_video = true;
            }
            else if (option == "--dump-frame")
            {
                if (arg + 1 >= argc)
                {
                    throw std::invalid_argument("--dump-frame requires a path");
                }
                options.frame_dump_path = argv[++arg];
                options.render_video = true;
            }
            else
            {
                throw std::invalid_argument("usage: mintboy_headless <rom.gb> [frames] [--render] [--dump-frame frame.ppm]");
            }
        }

        return options;
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

    bool IsBlarggTestDone(mintboy::Memory &memory)
    {
        return memory.ReadByte(0xA001) == 0xDE &&
               memory.ReadByte(0xA002) == 0xB0 &&
               memory.ReadByte(0xA003) == 0x61 &&
               memory.ReadByte(0xA000) != 0x80;
    }

    bool IsSerialTestDone(const std::string &serial_output)
    {
        return serial_output.find("Passed all tests") != std::string::npos ||
               serial_output.find("\nPassed\n") != std::string::npos ||
               serial_output.find("\nFailed") != std::string::npos;
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
    if (argc < 2 || argc > 6)
    {
        std::cerr << "usage: mintboy_headless <rom.gb> [frames] [--render] [--dump-frame frame.ppm]\n";
        return 2;
    }

    try
    {
        const Options options = ParseOptions(argc, argv);

        mintboy::Cartridge cartridge = mintboy::Cartridge::LoadFromFile(argv[1]);
        mintboy::Memory memory(cartridge);
        memory.SetAudioSampleGenerationEnabled(false);
        memory.SetVideoRenderingEnabled(options.render_video);
        mintboy::Cpu cpu(memory);
        std::string serial_output;

        int executed_frames = 0;
        for (; executed_frames < options.frames; ++executed_frames)
        {
            int cycles = 0;
            while (cycles < CyclesPerFrame)
            {
                cycles += cpu.Step();
            }
            [[maybe_unused]] const auto audio_samples = memory.DrainAudioSamples();
            serial_output += memory.DrainSerialOutput();
            if (IsBlarggTestDone(memory) || IsSerialTestDone(serial_output))
            {
                ++executed_frames;
                break;
            }
        }

        std::cout << "Title: " << cartridge.Title() << '\n';
        std::cout << "Frames: " << executed_frames << '\n';
        std::cout << std::format("Framebuffer hash: 0x{:016X}\n", FramebufferHash(memory.GetFramebuffer()));
        std::cout << "CPU PC: " << std::format("0x{:04X}", cpu.GetRegisters().pc) << '\n';
        if (!options.frame_dump_path.empty())
        {
            DumpFramebuffer(memory.GetFramebuffer(), options.frame_dump_path);
            std::cout << "Frame dump: " << options.frame_dump_path << '\n';
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
