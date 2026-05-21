#include "mintboy/memory.hpp"

namespace mintboy
{
    Memory::Memory(Cartridge &cartridge)
        : cartridge_(cartridge)
    {
    }

    Byte Memory::ReadByte(Word address) const
    {
        if (address <= 0x7FFF)
        {
            return cartridge_.Read(address);
        }

        if (address >= 0xC000 && address <= 0xDFFF)
        {
            return work_ram_[address - 0xC000];
        }

        if (address >= 0xE000 && address <= 0xFDFF)
        {
            return work_ram_[address - 0xE000];
        }

        if (address >= 0xFF80 && address <= 0xFFFE)
        {
            return high_ram_[address - 0xFF80];
        }

        if (address == 0xFFFF)
        {
            return interrupt_enable_;
        }

        return 0xFF;
    }

    void Memory::WriteByte(Word address, Byte value)
    {
        if (address <= 0x7FFF)
        {
            cartridge_.Write(address, value);
            return;
        }

        if (address >= 0xC000 && address <= 0xDFFF)
        {
            work_ram_[address - 0xC000] = value;
            return;
        }

        if (address >= 0xE000 && address <= 0xFDFF)
        {
            work_ram_[address - 0xE000] = value;
            return;
        }

        if (address >= 0xFF80 && address <= 0xFFFE)
        {
            high_ram_[address - 0xFF80] = value;
            return;
        }

        if (address == 0xFFFF)
        {
            interrupt_enable_ = value;
        }
    }
}
