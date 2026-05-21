#pragma once

#include "mintboy/memory.hpp"
#include "mintboy/types.hpp"

#include <cstdint>

namespace mintboy
{
    struct Registers
    {
        static constexpr Byte ZeroFlag = 0x80;
        static constexpr Byte SubtractFlag = 0x40;
        static constexpr Byte HalfCarryFlag = 0x20;
        static constexpr Byte CarryFlag = 0x10;

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

        [[nodiscard]] Word AF() const;
        [[nodiscard]] Word BC() const;
        [[nodiscard]] Word DE() const;
        [[nodiscard]] Word HL() const;

        void SetAF(Word value);
        void SetBC(Word value);
        void SetDE(Word value);
        void SetHL(Word value);
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
        std::int8_t FetchSignedByte();
        void PushWord(Word value);
        Word PopWord();

        [[nodiscard]] bool GetFlag(Byte flag) const;
        void SetFlag(Byte flag, bool enabled);

        Memory &memory_;
        Registers registers_{};
        bool halted_ = false;
    };
}
