#include "mintboy/cpu.hpp"

#include <cstdint>
#include <format>
#include <stdexcept>

namespace mintboy
{
    namespace
    {
        constexpr bool IsHalfCarryAdd(Byte lhs, Byte rhs)
        {
            return ((lhs & 0x0F) + (rhs & 0x0F)) > 0x0F;
        }

        constexpr bool IsHalfCarrySub(Byte lhs, Byte rhs)
        {
            return (lhs & 0x0F) < (rhs & 0x0F);
        }
    }

    Word Registers::AF() const
    {
        return static_cast<Word>((a << 8) | f);
    }

    Word Registers::BC() const
    {
        return static_cast<Word>((b << 8) | c);
    }

    Word Registers::DE() const
    {
        return static_cast<Word>((d << 8) | e);
    }

    Word Registers::HL() const
    {
        return static_cast<Word>((h << 8) | l);
    }

    void Registers::SetAF(Word value)
    {
        a = static_cast<Byte>(value >> 8);
        f = static_cast<Byte>(value & 0xF0);
    }

    void Registers::SetBC(Word value)
    {
        b = static_cast<Byte>(value >> 8);
        c = static_cast<Byte>(value);
    }

    void Registers::SetDE(Word value)
    {
        d = static_cast<Byte>(value >> 8);
        e = static_cast<Byte>(value);
    }

    void Registers::SetHL(Word value)
    {
        h = static_cast<Byte>(value >> 8);
        l = static_cast<Byte>(value);
    }

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

        if (opcode >= 0x40 && opcode <= 0x7F)
        {
            if (opcode == 0x76) // HALT
            {
                halted_ = true;
                return 4;
            }

            const Byte destination = static_cast<Byte>((opcode >> 3) & 0x07);
            const Byte source = static_cast<Byte>(opcode & 0x07);
            WriteRegisterByIndex(destination, ReadRegisterByIndex(source));
            return destination == 6 || source == 6 ? 8 : 4;
        }

        if (opcode >= 0x80 && opcode <= 0xBF)
        {
            const Byte operation = static_cast<Byte>((opcode >> 3) & 0x07);
            const Byte source = static_cast<Byte>(opcode & 0x07);
            ExecuteAlu(operation, ReadRegisterByIndex(source));
            return source == 6 ? 8 : 4;
        }

        switch (opcode)
        {
        case 0x00: // NOP
            return 4;
        case 0x01: // LD BC,d16
            registers_.SetBC(FetchWord());
            return 12;
        case 0x02: // LD (BC),A
            memory_.WriteByte(registers_.BC(), registers_.a);
            return 8;
        case 0x03: // INC BC
            registers_.SetBC(static_cast<Word>(registers_.BC() + 1));
            return 8;
        case 0x04: // INC B
            registers_.b = static_cast<Byte>(registers_.b + 1);
            SetFlag(Registers::ZeroFlag, registers_.b == 0);
            SetFlag(Registers::SubtractFlag, false);
            SetFlag(Registers::HalfCarryFlag, (registers_.b & 0x0F) == 0);
            return 4;
        case 0x05: // DEC B
            registers_.b = static_cast<Byte>(registers_.b - 1);
            SetFlag(Registers::ZeroFlag, registers_.b == 0);
            SetFlag(Registers::SubtractFlag, true);
            SetFlag(Registers::HalfCarryFlag, (registers_.b & 0x0F) == 0x0F);
            return 4;
        case 0x06: // LD B,d8
            registers_.b = FetchByte();
            return 8;
        case 0x09: // ADD HL,BC
        {
            const Word hl = registers_.HL();
            const Word bc = registers_.BC();
            const auto result = static_cast<std::uint32_t>(hl) + bc;
            registers_.SetHL(static_cast<Word>(result));
            SetFlag(Registers::SubtractFlag, false);
            SetFlag(Registers::HalfCarryFlag, ((hl & 0x0FFF) + (bc & 0x0FFF)) > 0x0FFF);
            SetFlag(Registers::CarryFlag, result > 0xFFFF);
            return 8;
        }
        case 0x0A: // LD A,(BC)
            registers_.a = memory_.ReadByte(registers_.BC());
            return 8;
        case 0x0C: // INC C
            registers_.c = static_cast<Byte>(registers_.c + 1);
            SetFlag(Registers::ZeroFlag, registers_.c == 0);
            SetFlag(Registers::SubtractFlag, false);
            SetFlag(Registers::HalfCarryFlag, (registers_.c & 0x0F) == 0);
            return 4;
        case 0x0D: // DEC C
            registers_.c = static_cast<Byte>(registers_.c - 1);
            SetFlag(Registers::ZeroFlag, registers_.c == 0);
            SetFlag(Registers::SubtractFlag, true);
            SetFlag(Registers::HalfCarryFlag, (registers_.c & 0x0F) == 0x0F);
            return 4;
        case 0x0E: // LD C,d8
            registers_.c = FetchByte();
            return 8;
        case 0x0F: // RRCA
        {
            const bool carry = (registers_.a & 0x01) != 0;
            registers_.a = static_cast<Byte>((registers_.a >> 1) | (carry ? 0x80 : 0x00));
            SetFlag(Registers::ZeroFlag, false);
            SetFlag(Registers::SubtractFlag, false);
            SetFlag(Registers::HalfCarryFlag, false);
            SetFlag(Registers::CarryFlag, carry);
            return 4;
        }
        case 0x11: // LD DE,d16
            registers_.SetDE(FetchWord());
            return 12;
        case 0x12: // LD (DE),A
            memory_.WriteByte(registers_.DE(), registers_.a);
            return 8;
        case 0x13: // INC DE
            registers_.SetDE(static_cast<Word>(registers_.DE() + 1));
            return 8;
        case 0x14: // INC D
            IncrementRegisterByIndex(2);
            return 4;
        case 0x15: // DEC D
            DecrementRegisterByIndex(2);
            return 4;
        case 0x16: // LD D,d8
            registers_.d = FetchByte();
            return 8;
        case 0x18: // JR r8
        {
            const auto offset = FetchSignedByte();
            registers_.pc = static_cast<Word>(registers_.pc + offset);
            return 12;
        }
        case 0x1A: // LD A,(DE)
            registers_.a = memory_.ReadByte(registers_.DE());
            return 8;
        case 0x1E: // LD E,d8
            registers_.e = FetchByte();
            return 8;
        case 0x1F: // RRA
        {
            const bool old_carry = GetFlag(Registers::CarryFlag);
            const bool new_carry = (registers_.a & 0x01) != 0;
            registers_.a = static_cast<Byte>((registers_.a >> 1) | (old_carry ? 0x80 : 0x00));
            SetFlag(Registers::ZeroFlag, false);
            SetFlag(Registers::SubtractFlag, false);
            SetFlag(Registers::HalfCarryFlag, false);
            SetFlag(Registers::CarryFlag, new_carry);
            return 4;
        }
        case 0x20: // JR NZ,r8
        {
            const auto offset = FetchSignedByte();
            if (!GetFlag(Registers::ZeroFlag))
            {
                registers_.pc = static_cast<Word>(registers_.pc + offset);
                return 12;
            }
            return 8;
        }
        case 0x21: // LD HL,d16
            registers_.SetHL(FetchWord());
            return 12;
        case 0x22: // LD (HL+),A
            memory_.WriteByte(registers_.HL(), registers_.a);
            registers_.SetHL(static_cast<Word>(registers_.HL() + 1));
            return 8;
        case 0x23: // INC HL
            registers_.SetHL(static_cast<Word>(registers_.HL() + 1));
            return 8;
        case 0x24: // INC H
            IncrementRegisterByIndex(4);
            return 4;
        case 0x25: // DEC H
            DecrementRegisterByIndex(4);
            return 4;
        case 0x26: // LD H,d8
            registers_.h = FetchByte();
            return 8;
        case 0x28: // JR Z,r8
        {
            const auto offset = FetchSignedByte();
            if (GetFlag(Registers::ZeroFlag))
            {
                registers_.pc = static_cast<Word>(registers_.pc + offset);
                return 12;
            }
            return 8;
        }
        case 0x2A: // LD A,(HL+)
            registers_.a = memory_.ReadByte(registers_.HL());
            registers_.SetHL(static_cast<Word>(registers_.HL() + 1));
            return 8;
        case 0x2E: // LD L,d8
            registers_.l = FetchByte();
            return 8;
        case 0x2F: // CPL
            registers_.a = static_cast<Byte>(~registers_.a);
            SetFlag(Registers::SubtractFlag, true);
            SetFlag(Registers::HalfCarryFlag, true);
            return 4;
        case 0x31: // LD SP,d16
            registers_.sp = FetchWord();
            return 12;
        case 0x32: // LD (HL-),A
            memory_.WriteByte(registers_.HL(), registers_.a);
            registers_.SetHL(static_cast<Word>(registers_.HL() - 1));
            return 8;
        case 0x36: // LD (HL),d8
            memory_.WriteByte(registers_.HL(), FetchByte());
            return 12;
        case 0x37: // SCF
            SetFlag(Registers::SubtractFlag, false);
            SetFlag(Registers::HalfCarryFlag, false);
            SetFlag(Registers::CarryFlag, true);
            return 4;
        case 0x3A: // LD A,(HL-)
            registers_.a = memory_.ReadByte(registers_.HL());
            registers_.SetHL(static_cast<Word>(registers_.HL() - 1));
            return 8;
        case 0x3C: // INC A
            IncrementRegisterByIndex(7);
            return 4;
        case 0x3D: // DEC A
            DecrementRegisterByIndex(7);
            return 4;
        case 0x3E: // LD A,d8
            registers_.a = FetchByte();
            return 8;
        case 0x3F: // CCF
            SetFlag(Registers::SubtractFlag, false);
            SetFlag(Registers::HalfCarryFlag, false);
            SetFlag(Registers::CarryFlag, !GetFlag(Registers::CarryFlag));
            return 4;
        case 0xC1: // POP BC
            registers_.SetBC(PopWord());
            return 12;
        case 0xC2: // JP NZ,a16
        {
            const Word address = FetchWord();
            if (!GetFlag(Registers::ZeroFlag))
            {
                registers_.pc = address;
                return 16;
            }
            return 12;
        }
        case 0xC3: // JP a16
            registers_.pc = FetchWord();
            return 16;
        case 0xC5: // PUSH BC
            PushWord(registers_.BC());
            return 16;
        case 0xC6: // ADD A,d8
            ExecuteAlu(0, FetchByte());
            return 8;
        case 0xC8: // RET Z
            if (GetFlag(Registers::ZeroFlag))
            {
                registers_.pc = PopWord();
                return 20;
            }
            return 8;
        case 0xC9: // RET
            registers_.pc = PopWord();
            return 16;
        case 0xCA: // JP Z,a16
        {
            const Word address = FetchWord();
            if (GetFlag(Registers::ZeroFlag))
            {
                registers_.pc = address;
                return 16;
            }
            return 12;
        }
        case 0xCC: // CALL Z,a16
        {
            const Word address = FetchWord();
            if (GetFlag(Registers::ZeroFlag))
            {
                PushWord(registers_.pc);
                registers_.pc = address;
                return 24;
            }
            return 12;
        }
        case 0xCD: // CALL a16
        {
            const Word address = FetchWord();
            PushWord(registers_.pc);
            registers_.pc = address;
            return 24;
        }
        case 0xD1: // POP DE
            registers_.SetDE(PopWord());
            return 12;
        case 0xD5: // PUSH DE
            PushWord(registers_.DE());
            return 16;
        case 0xD6: // SUB d8
            ExecuteAlu(2, FetchByte());
            return 8;
        case 0xE1: // POP HL
            registers_.SetHL(PopWord());
            return 12;
        case 0xE0: // LDH (a8),A
            memory_.WriteByte(static_cast<Word>(0xFF00 + FetchByte()), registers_.a);
            return 12;
        case 0xE2: // LD (C),A
            memory_.WriteByte(static_cast<Word>(0xFF00 + registers_.c), registers_.a);
            return 8;
        case 0xE5: // PUSH HL
            PushWord(registers_.HL());
            return 16;
        case 0xE6: // AND d8
            ExecuteAlu(4, FetchByte());
            return 8;
        case 0xEA: // LD (a16),A
            memory_.WriteByte(FetchWord(), registers_.a);
            return 16;
        case 0xF1: // POP AF
            registers_.SetAF(PopWord());
            return 12;
        case 0xF0: // LDH A,(a8)
            registers_.a = memory_.ReadByte(static_cast<Word>(0xFF00 + FetchByte()));
            return 12;
        case 0xF3: // DI
            return 4;
        case 0xF5: // PUSH AF
            PushWord(registers_.AF());
            return 16;
        case 0xF6: // OR d8
            ExecuteAlu(6, FetchByte());
            return 8;
        case 0xFA: // LD A,(a16)
            registers_.a = memory_.ReadByte(FetchWord());
            return 16;
        case 0xFB: // EI
            return 4;
        case 0xEE: // XOR d8
            ExecuteAlu(5, FetchByte());
            return 8;
        case 0xFE: // CP d8
            ExecuteAlu(7, FetchByte());
            return 8;
        default:
            throw std::runtime_error(std::format("unimplemented CPU opcode: 0x{:02X}", opcode));
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

    std::int8_t Cpu::FetchSignedByte()
    {
        return static_cast<std::int8_t>(FetchByte());
    }

    void Cpu::PushWord(Word value)
    {
        --registers_.sp;
        memory_.WriteByte(registers_.sp, static_cast<Byte>(value >> 8));
        --registers_.sp;
        memory_.WriteByte(registers_.sp, static_cast<Byte>(value));
    }

    Word Cpu::PopWord()
    {
        const Byte low = memory_.ReadByte(registers_.sp);
        ++registers_.sp;
        const Byte high = memory_.ReadByte(registers_.sp);
        ++registers_.sp;
        return static_cast<Word>(low | (high << 8));
    }

    Byte Cpu::ReadRegisterByIndex(Byte index) const
    {
        switch (index)
        {
        case 0:
            return registers_.b;
        case 1:
            return registers_.c;
        case 2:
            return registers_.d;
        case 3:
            return registers_.e;
        case 4:
            return registers_.h;
        case 5:
            return registers_.l;
        case 6:
            return memory_.ReadByte(registers_.HL());
        case 7:
            return registers_.a;
        default:
            throw std::logic_error("invalid register index");
        }
    }

    void Cpu::WriteRegisterByIndex(Byte index, Byte value)
    {
        switch (index)
        {
        case 0:
            registers_.b = value;
            return;
        case 1:
            registers_.c = value;
            return;
        case 2:
            registers_.d = value;
            return;
        case 3:
            registers_.e = value;
            return;
        case 4:
            registers_.h = value;
            return;
        case 5:
            registers_.l = value;
            return;
        case 6:
            memory_.WriteByte(registers_.HL(), value);
            return;
        case 7:
            registers_.a = value;
            return;
        default:
            throw std::logic_error("invalid register index");
        }
    }

    void Cpu::IncrementRegisterByIndex(Byte index)
    {
        const Byte original = ReadRegisterByIndex(index);
        const Byte result = static_cast<Byte>(original + 1);
        WriteRegisterByIndex(index, result);
        SetFlag(Registers::ZeroFlag, result == 0);
        SetFlag(Registers::SubtractFlag, false);
        SetFlag(Registers::HalfCarryFlag, IsHalfCarryAdd(original, 1));
    }

    void Cpu::DecrementRegisterByIndex(Byte index)
    {
        const Byte original = ReadRegisterByIndex(index);
        const Byte result = static_cast<Byte>(original - 1);
        WriteRegisterByIndex(index, result);
        SetFlag(Registers::ZeroFlag, result == 0);
        SetFlag(Registers::SubtractFlag, true);
        SetFlag(Registers::HalfCarryFlag, IsHalfCarrySub(original, 1));
    }

    void Cpu::ExecuteAlu(Byte operation, Byte value)
    {
        switch (operation)
        {
        case 0: // ADD A,r
        {
            const auto result = static_cast<std::uint16_t>(registers_.a) + value;
            SetFlag(Registers::ZeroFlag, static_cast<Byte>(result) == 0);
            SetFlag(Registers::SubtractFlag, false);
            SetFlag(Registers::HalfCarryFlag, IsHalfCarryAdd(registers_.a, value));
            SetFlag(Registers::CarryFlag, result > 0xFF);
            registers_.a = static_cast<Byte>(result);
            return;
        }
        case 1: // ADC A,r
        {
            const Byte carry = GetFlag(Registers::CarryFlag) ? 1 : 0;
            const auto result = static_cast<std::uint16_t>(registers_.a) + value + carry;
            SetFlag(Registers::ZeroFlag, static_cast<Byte>(result) == 0);
            SetFlag(Registers::SubtractFlag, false);
            SetFlag(Registers::HalfCarryFlag, ((registers_.a & 0x0F) + (value & 0x0F) + carry) > 0x0F);
            SetFlag(Registers::CarryFlag, result > 0xFF);
            registers_.a = static_cast<Byte>(result);
            return;
        }
        case 2: // SUB A,r
        {
            const Byte result = static_cast<Byte>(registers_.a - value);
            SetFlag(Registers::ZeroFlag, result == 0);
            SetFlag(Registers::SubtractFlag, true);
            SetFlag(Registers::HalfCarryFlag, IsHalfCarrySub(registers_.a, value));
            SetFlag(Registers::CarryFlag, registers_.a < value);
            registers_.a = result;
            return;
        }
        case 3: // SBC A,r
        {
            const Byte carry = GetFlag(Registers::CarryFlag) ? 1 : 0;
            const Byte result = static_cast<Byte>(registers_.a - value - carry);
            SetFlag(Registers::ZeroFlag, result == 0);
            SetFlag(Registers::SubtractFlag, true);
            SetFlag(Registers::HalfCarryFlag, (registers_.a & 0x0F) < ((value & 0x0F) + carry));
            SetFlag(Registers::CarryFlag, static_cast<std::uint16_t>(registers_.a) < static_cast<std::uint16_t>(value + carry));
            registers_.a = result;
            return;
        }
        case 4: // AND A,r
            registers_.a &= value;
            SetFlag(Registers::ZeroFlag, registers_.a == 0);
            SetFlag(Registers::SubtractFlag, false);
            SetFlag(Registers::HalfCarryFlag, true);
            SetFlag(Registers::CarryFlag, false);
            return;
        case 5: // XOR A,r
            registers_.a ^= value;
            SetFlag(Registers::ZeroFlag, registers_.a == 0);
            SetFlag(Registers::SubtractFlag, false);
            SetFlag(Registers::HalfCarryFlag, false);
            SetFlag(Registers::CarryFlag, false);
            return;
        case 6: // OR A,r
            registers_.a |= value;
            SetFlag(Registers::ZeroFlag, registers_.a == 0);
            SetFlag(Registers::SubtractFlag, false);
            SetFlag(Registers::HalfCarryFlag, false);
            SetFlag(Registers::CarryFlag, false);
            return;
        case 7: // CP A,r
            SetFlag(Registers::ZeroFlag, registers_.a == value);
            SetFlag(Registers::SubtractFlag, true);
            SetFlag(Registers::HalfCarryFlag, IsHalfCarrySub(registers_.a, value));
            SetFlag(Registers::CarryFlag, registers_.a < value);
            return;
        default:
            throw std::logic_error("invalid ALU operation");
        }
    }

    bool Cpu::GetFlag(Byte flag) const
    {
        return (registers_.f & flag) != 0;
    }

    void Cpu::SetFlag(Byte flag, bool enabled)
    {
        if (enabled)
        {
            registers_.f |= flag;
        }
        else
        {
            registers_.f &= static_cast<Byte>(~flag);
        }

        registers_.f &= 0xF0;
    }
}
