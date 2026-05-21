#include "mintboy/memory.hpp"

namespace mintboy
{
    namespace
    {
        constexpr Word DividerAddress = 0xFF04;
        constexpr Word TimerCounterAddress = 0xFF05;
        constexpr Word TimerModuloAddress = 0xFF06;
        constexpr Word TimerControlAddress = 0xFF07;
        constexpr Word InterruptFlagAddress = 0xFF0F;
        constexpr Byte TimerInterruptBit = 0x04;
    }

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

        if (address >= 0x8000 && address <= 0x9FFF)
        {
            return video_ram_[address - 0x8000];
        }

        if (address >= 0xC000 && address <= 0xDFFF)
        {
            return work_ram_[address - 0xC000];
        }

        if (address >= 0xE000 && address <= 0xFDFF)
        {
            return work_ram_[address - 0xE000];
        }

        if (address >= 0xFE00 && address <= 0xFE9F)
        {
            return oam_[address - 0xFE00];
        }

        if (address >= 0xFF00 && address <= 0xFF7F)
        {
            if (address == TimerControlAddress)
            {
                return static_cast<Byte>(io_registers_[address - 0xFF00] | 0xF8);
            }

            return io_registers_[address - 0xFF00];
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

        if (address >= 0x8000 && address <= 0x9FFF)
        {
            video_ram_[address - 0x8000] = value;
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

        if (address >= 0xFE00 && address <= 0xFE9F)
        {
            oam_[address - 0xFE00] = value;
            return;
        }

        if (address >= 0xFF00 && address <= 0xFF7F)
        {
            if (address == DividerAddress)
            {
                io_registers_[DividerAddress - 0xFF00] = 0;
                divider_cycles_ = 0;
                return;
            }

            if (address == TimerControlAddress)
            {
                io_registers_[TimerControlAddress - 0xFF00] = static_cast<Byte>(value & 0x07);
                timer_cycles_ = 0;
                return;
            }

            io_registers_[address - 0xFF00] = value;
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

    void Memory::Tick(int cycles)
    {
        divider_cycles_ += cycles;
        while (divider_cycles_ >= 256)
        {
            divider_cycles_ -= 256;
            ++io_registers_[DividerAddress - 0xFF00];
        }

        const Byte timer_control = io_registers_[TimerControlAddress - 0xFF00];
        if ((timer_control & 0x04) == 0)
        {
            return;
        }

        timer_cycles_ += cycles;
        const int period = TimerPeriodCycles();
        while (timer_cycles_ >= period)
        {
            timer_cycles_ -= period;

            Byte &timer_counter = io_registers_[TimerCounterAddress - 0xFF00];
            if (timer_counter == 0xFF)
            {
                timer_counter = io_registers_[TimerModuloAddress - 0xFF00];
                io_registers_[InterruptFlagAddress - 0xFF00] |= TimerInterruptBit;
            }
            else
            {
                ++timer_counter;
            }
        }
    }

    int Memory::TimerPeriodCycles() const
    {
        switch (io_registers_[TimerControlAddress - 0xFF00] & 0x03)
        {
        case 0:
            return 1024;
        case 1:
            return 16;
        case 2:
            return 64;
        case 3:
            return 256;
        default:
            return 1024;
        }
    }
}
