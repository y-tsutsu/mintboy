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

    private:
        Cartridge &cartridge_;
        std::array<Byte, 0x2000> work_ram_{};
        std::array<Byte, 0x7F> high_ram_{};
        Byte interrupt_enable_ = 0;
    };
}
