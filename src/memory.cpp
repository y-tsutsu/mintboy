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
        constexpr Word LcdControlAddress = 0xFF40;
        constexpr Word LcdStatusAddress = 0xFF41;
        constexpr Word ScrollYAddress = 0xFF42;
        constexpr Word ScrollXAddress = 0xFF43;
        constexpr Word LyAddress = 0xFF44;
        constexpr Word LycAddress = 0xFF45;
        constexpr Word BgPaletteAddress = 0xFF47;
        constexpr Byte VBlankInterruptBit = 0x01;
        constexpr Byte TimerInterruptBit = 0x04;
        constexpr Byte LcdEnabledBit = 0x80;
        constexpr Byte LyCompareFlag = 0x04;
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

            if (address == LcdStatusAddress)
            {
                return static_cast<Byte>(io_registers_[address - 0xFF00] | 0x80);
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

    const Memory::Framebuffer &Memory::GetFramebuffer() const
    {
        return framebuffer_;
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

            if (address == LcdControlAddress)
            {
                const bool was_enabled = (io_registers_[LcdControlAddress - 0xFF00] & LcdEnabledBit) != 0;
                const bool is_enabled = (value & LcdEnabledBit) != 0;
                io_registers_[LcdControlAddress - 0xFF00] = value;

                if (!is_enabled)
                {
                    ppu_cycles_ = 0;
                    io_registers_[LyAddress - 0xFF00] = 0;
                    SetPpuMode(0);
                }
                else if (!was_enabled)
                {
                    ppu_cycles_ = 0;
                    io_registers_[LyAddress - 0xFF00] = 0;
                    SetPpuMode(2);
                }
                UpdateLyCompareFlag();
                return;
            }

            if (address == LcdStatusAddress)
            {
                io_registers_[LcdStatusAddress - 0xFF00] = static_cast<Byte>((value & 0x78) | (io_registers_[LcdStatusAddress - 0xFF00] & 0x07));
                return;
            }

            if (address == LyAddress)
            {
                io_registers_[LyAddress - 0xFF00] = 0;
                ppu_cycles_ = 0;
                UpdateLyCompareFlag();
                return;
            }

            if (address == LycAddress)
            {
                io_registers_[LycAddress - 0xFF00] = value;
                UpdateLyCompareFlag();
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
        TickTimer(cycles);
        TickPpu(cycles);
    }

    void Memory::TickTimer(int cycles)
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
                RequestInterrupt(TimerInterruptBit);
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

    void Memory::TickPpu(int cycles)
    {
        if ((io_registers_[LcdControlAddress - 0xFF00] & LcdEnabledBit) == 0)
        {
            return;
        }

        ppu_cycles_ += cycles;
        while (ppu_cycles_ >= 456)
        {
            ppu_cycles_ -= 456;
            Byte &ly = io_registers_[LyAddress - 0xFF00];
            ++ly;

            if (ly == 144)
            {
                SetPpuMode(1);
                RequestInterrupt(VBlankInterruptBit);
            }
            else if (ly > 153)
            {
                ly = 0;
                SetPpuMode(2);
            }
            else if (ly < 144)
            {
                RenderScanline(ly);
            }

            UpdateLyCompareFlag();
        }

        const Byte ly = io_registers_[LyAddress - 0xFF00];
        if (ly >= 144)
        {
            SetPpuMode(1);
            return;
        }

        if (ppu_cycles_ < 80)
        {
            SetPpuMode(2);
        }
        else if (ppu_cycles_ < 252)
        {
            SetPpuMode(3);
        }
        else
        {
            SetPpuMode(0);
        }
    }

    void Memory::SetPpuMode(Byte mode)
    {
        Byte &status = io_registers_[LcdStatusAddress - 0xFF00];
        status = static_cast<Byte>((status & 0xFC) | (mode & 0x03));
    }

    void Memory::RenderScanline(Byte y)
    {
        constexpr std::array<std::uint32_t, 4> palette = {
            0xFF9BBC0F,
            0xFF8BAC0F,
            0xFF306230,
            0xFF0F380F,
        };

        const Byte lcd_control = io_registers_[LcdControlAddress - 0xFF00];
        const bool bg_enabled = (lcd_control & 0x01) != 0;
        if (!bg_enabled)
        {
            for (int x = 0; x < ScreenWidth; ++x)
            {
                framebuffer_[static_cast<std::size_t>(y) * ScreenWidth + x] = palette[0];
            }
            return;
        }

        const Word tile_map_base = (lcd_control & 0x08) != 0 ? 0x1C00 : 0x1800;
        const bool unsigned_tile_index = (lcd_control & 0x10) != 0;
        const Byte scroll_y = io_registers_[ScrollYAddress - 0xFF00];
        const Byte scroll_x = io_registers_[ScrollXAddress - 0xFF00];
        const Byte bg_palette = io_registers_[BgPaletteAddress - 0xFF00];

        for (int x = 0; x < ScreenWidth; ++x)
        {
            const Byte bg_x = static_cast<Byte>(x + scroll_x);
            const Byte bg_y = static_cast<Byte>(y + scroll_y);
            const std::size_t tile_column = bg_x / 8;
            const std::size_t tile_row = bg_y / 8;
            const Byte tile_index = video_ram_[tile_map_base + tile_row * 32 + tile_column];
            const std::size_t tile_data_offset = unsigned_tile_index
                                                     ? static_cast<std::size_t>(tile_index) * 16
                                                     : static_cast<std::size_t>(0x1000 + static_cast<std::int8_t>(tile_index) * 16);
            const std::size_t tile_line = bg_y % 8;
            const Byte low = video_ram_[tile_data_offset + tile_line * 2];
            const Byte high = video_ram_[tile_data_offset + tile_line * 2 + 1];
            const int bit = 7 - (bg_x % 8);
            const Byte color_index = static_cast<Byte>(((high >> bit) & 0x01) << 1 | ((low >> bit) & 0x01));
            const Byte palette_index = static_cast<Byte>((bg_palette >> (color_index * 2)) & 0x03);
            framebuffer_[static_cast<std::size_t>(y) * ScreenWidth + x] = palette[palette_index];
        }
    }

    void Memory::RequestInterrupt(Byte bit)
    {
        io_registers_[InterruptFlagAddress - 0xFF00] |= bit;
    }

    void Memory::UpdateLyCompareFlag()
    {
        Byte &status = io_registers_[LcdStatusAddress - 0xFF00];
        const bool match = io_registers_[LyAddress - 0xFF00] == io_registers_[LycAddress - 0xFF00];
        if (match)
        {
            status |= LyCompareFlag;
        }
        else
        {
            status &= static_cast<Byte>(~LyCompareFlag);
        }
    }
}
