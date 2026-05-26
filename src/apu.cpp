#include "mintboy/apu.hpp"

namespace mintboy
{
    Byte Apu::ReadByte(Word address) const
    {
        if (!IsRegisterAddress(address))
        {
            return 0xFF;
        }

        return registers_[RegisterIndex(address)];
    }

    void Apu::WriteByte(Word address, Byte value)
    {
        if (!IsRegisterAddress(address))
        {
            return;
        }

        if (address == ControlAddress)
        {
            registers_[RegisterIndex(address)] = static_cast<Byte>(value & 0x80);
            if ((value & 0x80) == 0)
            {
                registers_.fill(0);
            }
            return;
        }

        if ((registers_[RegisterIndex(ControlAddress)] & 0x80) == 0 && address != 0xFF26)
        {
            return;
        }

        registers_[RegisterIndex(address)] = value;
    }

    void Apu::Tick(int)
    {
    }

    bool Apu::IsRegisterAddress(Word address)
    {
        return address >= RegisterStart && address <= RegisterEnd;
    }

    std::size_t Apu::RegisterIndex(Word address)
    {
        return address - RegisterStart;
    }
}
