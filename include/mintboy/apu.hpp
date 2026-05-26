#pragma once

#include "mintboy/types.hpp"

#include <array>

namespace mintboy
{
    class Apu
    {
    public:
        [[nodiscard]] Byte ReadByte(Word address) const;
        void WriteByte(Word address, Byte value);
        void Tick(int cycles);

    private:
        static constexpr Word RegisterStart = 0xFF10;
        static constexpr Word RegisterEnd = 0xFF3F;
        static constexpr Word ControlAddress = 0xFF26;

        [[nodiscard]] static bool IsRegisterAddress(Word address);
        [[nodiscard]] static std::size_t RegisterIndex(Word address);

        std::array<Byte, RegisterEnd - RegisterStart + 1> registers_{};
    };
}
