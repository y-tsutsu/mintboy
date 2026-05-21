#pragma once

#include "mintboy/cartridge.hpp"
#include "mintboy/types.hpp"

#include <array>

namespace mintboy
{
    class Memory
    {
    public:
        explicit Memory(Cartridge &cartridge);

        [[nodiscard]] Byte ReadByte(Word address) const;
        void WriteByte(Word address, Byte value);
        void Tick(int cycles);

    private:
        [[nodiscard]] int TimerPeriodCycles() const;

        Cartridge &cartridge_;
        std::array<Byte, 0x2000> video_ram_{};
        std::array<Byte, 0x2000> work_ram_{};
        std::array<Byte, 0xA0> oam_{};
        std::array<Byte, 0x80> io_registers_{};
        std::array<Byte, 0x7F> high_ram_{};
        Byte interrupt_enable_ = 0;
        int divider_cycles_ = 0;
        int timer_cycles_ = 0;
    };
}
