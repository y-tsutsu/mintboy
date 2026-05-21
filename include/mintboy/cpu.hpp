#pragma once

#include "mintboy/memory.hpp"
#include "mintboy/types.hpp"

namespace mintboy
{
    struct Registers
    {
        Byte a = 0x01;
        Byte f = 0xB0;
        Byte b = 0x00;
        Byte c = 0x13;
        Byte d = 0x00;
        Byte e = 0xD8;
        Byte h = 0x01;
        Byte l = 0x4D;
        Word sp = 0xFFFE;
        Word pc = 0x0100;
    };

    class Cpu
    {
    public:
        explicit Cpu(Memory &memory);

        [[nodiscard]] const Registers &GetRegisters() const;
        [[nodiscard]] bool IsHalted() const;

        int Step();

    private:
        Byte FetchByte();
        Word FetchWord();

        Memory &memory_;
        Registers registers_{};
        bool halted_ = false;
    };
}
