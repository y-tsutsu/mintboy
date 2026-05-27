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
        [[nodiscard]] bool IsStopped() const;
        [[nodiscard]] bool IsInterruptMasterEnabled() const;

        int Step();

    private:
        int ExecuteInstruction();
        [[nodiscard]] Byte PendingInterrupts() const;
        [[nodiscard]] int ServiceInterrupt();

        Byte ReadByte(Word address);
        void WriteByte(Word address, Byte value);
        void IdleCycle();
        Byte FetchByte();
        Word FetchWord();
        std::int8_t FetchSignedByte();
        void PushWord(Word value);
        Word PopWord();

        Byte ReadRegisterByIndex(Byte index);
        void WriteRegisterByIndex(Byte index, Byte value);
        void IncrementRegisterByIndex(Byte index);
        void DecrementRegisterByIndex(Byte index);
        void ExecuteAlu(Byte operation, Byte value);
        int ExecuteCb(Byte opcode);

        [[nodiscard]] bool GetFlag(Byte flag) const;
        void SetFlag(Byte flag, bool enabled);

        Memory &memory_;
        Registers registers_{};
        bool halted_ = false;
        bool stopped_ = false;
        bool interrupt_master_enabled_ = false;
        bool enable_interrupts_after_next_instruction_ = false;
        int elapsed_cycles_ = 0;
    };
}
