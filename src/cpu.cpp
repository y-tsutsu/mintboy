#include "mintboy/cpu.hpp"

#include <stdexcept>

namespace mintboy
{
    Cpu::Cpu(Memory &memory)
        : memory_(memory)
    {
    }

    const Registers &Cpu::GetRegisters() const
    {
        return registers_;
    }

    bool Cpu::IsHalted() const
    {
        return halted_;
    }

    int Cpu::Step()
    {
        if (halted_)
        {
            return 4;
        }

        const Byte opcode = FetchByte();
        switch (opcode)
        {
        case 0x00: // NOP
            return 4;
        case 0x06: // LD B,d8
            registers_.b = FetchByte();
            return 8;
        case 0x0E: // LD C,d8
            registers_.c = FetchByte();
            return 8;
        case 0x16: // LD D,d8
            registers_.d = FetchByte();
            return 8;
        case 0x1E: // LD E,d8
            registers_.e = FetchByte();
            return 8;
        case 0x26: // LD H,d8
            registers_.h = FetchByte();
            return 8;
        case 0x2E: // LD L,d8
            registers_.l = FetchByte();
            return 8;
        case 0x3E: // LD A,d8
            registers_.a = FetchByte();
            return 8;
        case 0x76: // HALT
            halted_ = true;
            return 4;
        case 0xC3: // JP a16
            registers_.pc = FetchWord();
            return 16;
        default:
            throw std::runtime_error("unimplemented CPU opcode");
        }
    }

    Byte Cpu::FetchByte()
    {
        const Byte value = memory_.ReadByte(registers_.pc);
        ++registers_.pc;
        return value;
    }

    Word Cpu::FetchWord()
    {
        const Byte low = FetchByte();
        const Byte high = FetchByte();
        return static_cast<Word>(low | (high << 8));
    }
}
