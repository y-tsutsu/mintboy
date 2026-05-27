#pragma once

#include "mintboy/apu.hpp"
#include "mintboy/cartridge.hpp"
#include "mintboy/types.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mintboy
{
    class Memory
    {
    public:
        enum class JoypadButton : Byte
        {
            Right = 0,
            Left = 1,
            Up = 2,
            Down = 3,
            A = 4,
            B = 5,
            Select = 6,
            Start = 7,
        };

        static constexpr int ScreenWidth = 160;
        static constexpr int ScreenHeight = 144;
        using Framebuffer = std::array<std::uint32_t, ScreenWidth * ScreenHeight>;

        explicit Memory(Cartridge &cartridge);

        [[nodiscard]] Byte ReadByte(Word address) const;
        [[nodiscard]] const Framebuffer &GetFramebuffer() const;
        void SetJoypadButton(JoypadButton button, bool pressed);
        void WriteByte(Word address, Byte value);
        void Tick(int cycles);
        [[nodiscard]] std::vector<float> DrainAudioSamples();
        [[nodiscard]] std::string DrainSerialOutput();

    private:
        [[nodiscard]] Byte ReadJoypad() const;
        void TraceJoypad(const char *event, Byte value) const;
        [[nodiscard]] int TimerPeriodCycles() const;
        void TickTimer(int cycles);
        void TickPpu(int cycles);
        void RenderScanline(Byte y);
        void RenderSprites(Byte y, std::array<Byte, ScreenWidth> &background_color_indices);
        [[nodiscard]] std::uint32_t MapPaletteColor(Byte palette_value, Byte color_index) const;
        void StartDmaTransfer(Byte source_high);
        void SetPpuMode(Byte mode);
        void RequestStatInterrupt(Byte source_bit);
        void RequestInterrupt(Byte bit);
        void UpdateLyCompareFlag();

        Cartridge &cartridge_;
        Apu apu_{};
        std::string serial_output_;
        std::array<Byte, 0x2000> video_ram_{};
        std::array<Byte, 0x2000> work_ram_{};
        std::array<Byte, 0xA0> oam_{};
        std::array<Byte, 0x80> io_registers_{};
        std::array<Byte, 0x7F> high_ram_{};
        Framebuffer framebuffer_{};
        Byte interrupt_enable_ = 0;
        Byte joypad_buttons_ = 0;
        mutable Byte last_traced_joypad_value_ = 0xFF;
        mutable Byte last_traced_joypad_buttons_ = 0xFF;
        int divider_cycles_ = 0;
        int timer_cycles_ = 0;
        int ppu_cycles_ = 0;
    };
}
