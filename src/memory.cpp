#include "mintboy/memory.hpp"

#include <cstdlib>
#include <format>
#include <iostream>

namespace mintboy
{
    namespace
    {
        constexpr Word JoypadAddress = 0xFF00;
        constexpr Word SerialDataAddress = 0xFF01;
        constexpr Word SerialControlAddress = 0xFF02;
        constexpr Word DividerAddress = 0xFF04;
        constexpr Word TimerCounterAddress = 0xFF05;
        constexpr Word TimerModuloAddress = 0xFF06;
        constexpr Word TimerControlAddress = 0xFF07;
        constexpr Word InterruptFlagAddress = 0xFF0F;
        constexpr Word ApuRegisterStartAddress = 0xFF10;
        constexpr Word ApuRegisterEndAddress = 0xFF3F;
        constexpr Word LcdControlAddress = 0xFF40;
        constexpr Word LcdStatusAddress = 0xFF41;
        constexpr Word ScrollYAddress = 0xFF42;
        constexpr Word ScrollXAddress = 0xFF43;
        constexpr Word LyAddress = 0xFF44;
        constexpr Word LycAddress = 0xFF45;
        constexpr Word DmaAddress = 0xFF46;
        constexpr Word BgPaletteAddress = 0xFF47;
        constexpr Word ObjectPalette0Address = 0xFF48;
        constexpr Word ObjectPalette1Address = 0xFF49;
        constexpr Word WindowYAddress = 0xFF4A;
        constexpr Word WindowXAddress = 0xFF4B;
        constexpr Word Key1Address = 0xFF4D;
        constexpr Byte VBlankInterruptBit = 0x01;
        constexpr Byte LcdStatInterruptBit = 0x02;
        constexpr Byte TimerInterruptBit = 0x04;
        constexpr Byte LcdEnabledBit = 0x80;
        constexpr Byte LyCompareFlag = 0x04;
        constexpr Byte HBlankStatInterruptEnable = 0x08;
        constexpr Byte VBlankStatInterruptEnable = 0x10;
        constexpr Byte OamStatInterruptEnable = 0x20;
        constexpr Byte LycStatInterruptEnable = 0x40;
    }

    Memory::Memory(Cartridge &cartridge)
        : cartridge_(cartridge)
    {
        io_registers_[JoypadAddress - 0xFF00] = 0x30;
        io_registers_[LcdControlAddress - 0xFF00] = 0x91;
        io_registers_[BgPaletteAddress - 0xFF00] = 0xFC;
        io_registers_[ObjectPalette0Address - 0xFF00] = 0xFF;
        io_registers_[ObjectPalette1Address - 0xFF00] = 0xFF;
        SetPpuMode(2);
        UpdateLyCompareFlag();
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

        if (address >= 0xA000 && address <= 0xBFFF)
        {
            return cartridge_.ReadRam(address);
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
            if (IsOamBugActive())
            {
                return 0xFF;
            }
            return oam_[address - 0xFE00];
        }

        if (address >= 0xFF00 && address <= 0xFF7F)
        {
            if (address >= ApuRegisterStartAddress && address <= ApuRegisterEndAddress)
            {
                return apu_.ReadByte(address);
            }

            if (address == JoypadAddress)
            {
                return ReadJoypad();
            }

            if (address == TimerControlAddress)
            {
                return static_cast<Byte>(io_registers_[address - 0xFF00] | 0xF8);
            }

            if (address == InterruptFlagAddress)
            {
                return static_cast<Byte>(io_registers_[InterruptFlagAddress - 0xFF00] | 0xE0);
            }

            if (address == LcdStatusAddress)
            {
                return static_cast<Byte>(io_registers_[address - 0xFF00] | 0x80);
            }

            if (address == Key1Address)
            {
                return static_cast<Byte>(0x7E | (double_speed_ ? 0x80 : 0x00) | (prepare_speed_switch_ ? 0x01 : 0x00));
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

        if (address >= 0xA000 && address <= 0xBFFF)
        {
            cartridge_.WriteRam(address, value);
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
            if (IsOamBugActive())
            {
                return;
            }
            oam_[address - 0xFE00] = value;
            return;
        }

        if (address >= 0xFF00 && address <= 0xFF7F)
        {
            if (address >= ApuRegisterStartAddress && address <= ApuRegisterEndAddress)
            {
                apu_.WriteByte(address, value);
                return;
            }

            if (address == JoypadAddress)
            {
                io_registers_[JoypadAddress - 0xFF00] = static_cast<Byte>(value & 0x30);
                TraceJoypad("write", io_registers_[JoypadAddress - 0xFF00]);
                return;
            }

            if (address == SerialControlAddress)
            {
                io_registers_[SerialControlAddress - 0xFF00] = value;
                if (value == 0x81)
                {
                    serial_output_.push_back(static_cast<char>(io_registers_[SerialDataAddress - 0xFF00]));
                    io_registers_[SerialControlAddress - 0xFF00] = 0x01;
                }
                return;
            }

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

            if (address == InterruptFlagAddress)
            {
                io_registers_[InterruptFlagAddress - 0xFF00] = static_cast<Byte>(value & 0x1F);
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
                    scanline_rendered_ = false;
                    SetPpuMode(0);
                }
                else if (!was_enabled)
                {
                    ppu_cycles_ = 4;
                    io_registers_[LyAddress - 0xFF00] = 0;
                    scanline_rendered_ = false;
                    SetPpuMode(2);
                }
                UpdateLyCompareFlag();
                return;
            }

            if (address == LcdStatusAddress)
            {
                io_registers_[LcdStatusAddress - 0xFF00] = static_cast<Byte>((value & 0x78) | (io_registers_[LcdStatusAddress - 0xFF00] & 0x07));
                UpdateLyCompareFlag();
                return;
            }

            if (address == Key1Address)
            {
                prepare_speed_switch_ = (value & 0x01) != 0;
                return;
            }

            if (address == LyAddress)
            {
                io_registers_[LyAddress - 0xFF00] = 0;
                ppu_cycles_ = 0;
                scanline_rendered_ = false;
                UpdateLyCompareFlag();
                return;
            }

            if (address == LycAddress)
            {
                io_registers_[LycAddress - 0xFF00] = value;
                UpdateLyCompareFlag();
                return;
            }

            if (address == DmaAddress)
            {
                io_registers_[DmaAddress - 0xFF00] = value;
                StartDmaTransfer(value);
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

    void Memory::SetJoypadButton(JoypadButton button, bool pressed)
    {
        const Byte previous_joypad = ReadJoypad();
        const Byte mask = static_cast<Byte>(1 << static_cast<Byte>(button));
        const bool was_pressed = (joypad_buttons_ & mask) != 0;
        if (pressed)
        {
            joypad_buttons_ |= mask;
        }
        else
        {
            joypad_buttons_ &= static_cast<Byte>(~mask);
        }

        const Byte current_joypad = ReadJoypad();
        const Byte newly_pressed_visible_bits = static_cast<Byte>((previous_joypad & ~current_joypad) & 0x0F);
        if (joypad_buttons_ != last_traced_joypad_buttons_)
        {
            last_traced_joypad_buttons_ = joypad_buttons_;
            TraceJoypad("buttons", joypad_buttons_);
        }
        if (pressed && !was_pressed && newly_pressed_visible_bits != 0)
        {
            RequestInterrupt(0x10);
        }
    }

    Byte Memory::ReadJoypad() const
    {
        const Byte select = static_cast<Byte>(io_registers_[JoypadAddress - 0xFF00] & 0x30);
        Byte low_nibble = 0x0F;

        if ((select & 0x10) == 0)
        {
            low_nibble &= static_cast<Byte>(~(joypad_buttons_ & 0x0F));
        }

        if ((select & 0x20) == 0)
        {
            low_nibble &= static_cast<Byte>(~((joypad_buttons_ >> 4) & 0x0F));
        }

        const Byte value = static_cast<Byte>(0xC0 | select | low_nibble);
        if (value != last_traced_joypad_value_)
        {
            last_traced_joypad_value_ = value;
            TraceJoypad("read", value);
        }
        return value;
    }

    void Memory::TraceJoypad(const char *event, Byte value) const
    {
        if (std::getenv("MINTBOY_TRACE_JOYPAD") == nullptr)
        {
            return;
        }

        std::cerr << std::format(
            "joypad {} value=0x{:02X} select=0x{:02X} buttons=0x{:02X}\n",
            event,
            value,
            io_registers_[JoypadAddress - 0xFF00] & 0x30,
            joypad_buttons_);
    }

    void Memory::Tick(int cycles)
    {
        TickTimer(cycles);
        TickPpu(cycles);
        apu_.Tick(cycles);
    }

    std::vector<float> Memory::DrainAudioSamples()
    {
        return apu_.DrainSamples();
    }

    void Memory::SetAudioSampleGenerationEnabled(bool enabled)
    {
        apu_.SetSampleGenerationEnabled(enabled);
    }

    void Memory::SetVideoRenderingEnabled(bool enabled)
    {
        video_rendering_enabled_ = enabled;
    }

    bool Memory::ConsumeSpeedSwitchRequest()
    {
        if (!prepare_speed_switch_)
        {
            return false;
        }

        prepare_speed_switch_ = false;
        double_speed_ = !double_speed_;
        return true;
    }

    std::string Memory::DrainSerialOutput()
    {
        std::string output;
        output.swap(serial_output_);
        return output;
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
        while (true)
        {
            Byte &ly = io_registers_[LyAddress - 0xFF00];
            if (ly < 144 && video_rendering_enabled_ && !scanline_rendered_ && ppu_cycles_ >= 252)
            {
                RenderScanline(ly);
                scanline_rendered_ = true;
            }

            if (ppu_cycles_ < 456)
            {
                break;
            }

            ppu_cycles_ -= 456;
            if (ly < 144 && video_rendering_enabled_ && !scanline_rendered_)
            {
                RenderScanline(ly);
            }

            ++ly;
            scanline_rendered_ = false;

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
        const Byte previous_mode = static_cast<Byte>(status & 0x03);
        status = static_cast<Byte>((status & 0xFC) | (mode & 0x03));

        if (mode == previous_mode)
        {
            return;
        }

        switch (mode)
        {
        case 0:
            RequestStatInterrupt(HBlankStatInterruptEnable);
            break;
        case 1:
            RequestStatInterrupt(VBlankStatInterruptEnable);
            break;
        case 2:
            RequestStatInterrupt(OamStatInterruptEnable);
            break;
        default:
            break;
        }
    }

    void Memory::RenderScanline(Byte y)
    {
        const Byte lcd_control = io_registers_[LcdControlAddress - 0xFF00];
        const bool bg_enabled = (lcd_control & 0x01) != 0;
        std::array<Byte, ScreenWidth> background_color_indices{};

        if (!bg_enabled)
        {
            for (int x = 0; x < ScreenWidth; ++x)
            {
                framebuffer_[static_cast<std::size_t>(y) * ScreenWidth + x] = MapPaletteColor(0xE4, 0);
            }
            RenderSprites(y, background_color_indices);
            return;
        }

        const Word tile_map_base = (lcd_control & 0x08) != 0 ? 0x1C00 : 0x1800;
        const bool unsigned_tile_index = (lcd_control & 0x10) != 0;
        const Byte scroll_y = io_registers_[ScrollYAddress - 0xFF00];
        const Byte scroll_x = io_registers_[ScrollXAddress - 0xFF00];
        const Byte bg_palette = io_registers_[BgPaletteAddress - 0xFF00];
        const bool window_enabled = (lcd_control & 0x20) != 0;
        const Byte window_y = io_registers_[WindowYAddress - 0xFF00];
        const int window_x = static_cast<int>(io_registers_[WindowXAddress - 0xFF00]) - 7;
        const Word window_tile_map_base = (lcd_control & 0x40) != 0 ? 0x1C00 : 0x1800;

        for (int x = 0; x < ScreenWidth; ++x)
        {
            const bool use_window = window_enabled && y >= window_y && x >= window_x;
            const Byte map_x = use_window ? static_cast<Byte>(x - window_x) : static_cast<Byte>(x + scroll_x);
            const Byte map_y = use_window ? static_cast<Byte>(y - window_y) : static_cast<Byte>(y + scroll_y);
            const std::size_t tile_column = map_x / 8;
            const std::size_t tile_row = map_y / 8;
            const Word selected_tile_map_base = use_window ? window_tile_map_base : tile_map_base;
            const Byte tile_index = video_ram_[selected_tile_map_base + tile_row * 32 + tile_column];
            const std::size_t tile_data_offset = unsigned_tile_index
                                                     ? static_cast<std::size_t>(tile_index) * 16
                                                     : static_cast<std::size_t>(0x1000 + static_cast<std::int8_t>(tile_index) * 16);
            const std::size_t tile_line = map_y % 8;
            const Byte low = video_ram_[tile_data_offset + tile_line * 2];
            const Byte high = video_ram_[tile_data_offset + tile_line * 2 + 1];
            const int bit = 7 - (map_x % 8);
            const Byte color_index = static_cast<Byte>(((high >> bit) & 0x01) << 1 | ((low >> bit) & 0x01));
            const Byte palette_index = static_cast<Byte>((bg_palette >> (color_index * 2)) & 0x03);
            background_color_indices[x] = color_index;
            framebuffer_[static_cast<std::size_t>(y) * ScreenWidth + x] = MapPaletteColor(0xE4, palette_index);
        }

        RenderSprites(y, background_color_indices);
    }

    void Memory::RenderSprites(Byte y, std::array<Byte, ScreenWidth> &background_color_indices)
    {
        const Byte lcd_control = io_registers_[LcdControlAddress - 0xFF00];
        if ((lcd_control & 0x02) == 0)
        {
            return;
        }

        const int sprite_height = (lcd_control & 0x04) != 0 ? 16 : 8;
        int rendered_sprites = 0;
        for (std::size_t sprite = 0; sprite < 40 && rendered_sprites < 10; ++sprite)
        {
            const std::size_t base = sprite * 4;
            const int sprite_y = static_cast<int>(oam_[base]) - 16;
            const int sprite_x = static_cast<int>(oam_[base + 1]) - 8;
            Byte tile_index = oam_[base + 2];
            const Byte attributes = oam_[base + 3];

            if (static_cast<int>(y) < sprite_y || static_cast<int>(y) >= sprite_y + sprite_height)
            {
                continue;
            }

            ++rendered_sprites;
            const bool behind_background = (attributes & 0x80) != 0;
            const bool y_flip = (attributes & 0x40) != 0;
            const bool x_flip = (attributes & 0x20) != 0;
            const Byte palette_value = (attributes & 0x10) != 0
                                           ? io_registers_[ObjectPalette1Address - 0xFF00]
                                           : io_registers_[ObjectPalette0Address - 0xFF00];

            int tile_line = static_cast<int>(y) - sprite_y;
            if (y_flip)
            {
                tile_line = sprite_height - 1 - tile_line;
            }

            if (sprite_height == 16)
            {
                tile_index = static_cast<Byte>(tile_index & 0xFE);
            }

            const std::size_t tile_data_offset = static_cast<std::size_t>(tile_index) * 16 + static_cast<std::size_t>(tile_line) * 2;
            const Byte low = video_ram_[tile_data_offset];
            const Byte high = video_ram_[tile_data_offset + 1];

            for (int pixel = 0; pixel < 8; ++pixel)
            {
                const int screen_x = sprite_x + pixel;
                if (screen_x < 0 || screen_x >= ScreenWidth)
                {
                    continue;
                }

                const int bit = x_flip ? pixel : 7 - pixel;
                const Byte color_index = static_cast<Byte>(((high >> bit) & 0x01) << 1 | ((low >> bit) & 0x01));
                if (color_index == 0)
                {
                    continue;
                }

                if (behind_background && background_color_indices[screen_x] != 0)
                {
                    continue;
                }

                framebuffer_[static_cast<std::size_t>(y) * ScreenWidth + screen_x] = MapPaletteColor(palette_value, color_index);
            }
        }
    }

    std::uint32_t Memory::MapPaletteColor(Byte palette_value, Byte color_index) const
    {
        constexpr std::array<std::uint32_t, 4> palette = {
            0xFF9BBC0F,
            0xFF8BAC0F,
            0xFF306230,
            0xFF0F380F,
        };
        const Byte palette_index = static_cast<Byte>((palette_value >> (color_index * 2)) & 0x03);
        return palette[palette_index];
    }

    void Memory::StartDmaTransfer(Byte source_high)
    {
        const Word source = static_cast<Word>(source_high << 8);
        for (Word offset = 0; offset < 0x00A0; ++offset)
        {
            oam_[offset] = ReadByte(static_cast<Word>(source + offset));
        }
    }

    void Memory::CorruptOamForRead(Word address)
    {
        if (!IsOamBugAddress(address) || !IsOamBugActive())
        {
            return;
        }

        ApplyOamReadCorruption();
    }

    void Memory::CorruptOamForWrite(Word address)
    {
        if (!IsOamBugAddress(address) || !IsOamBugActive())
        {
            return;
        }

        ApplyOamWriteCorruption();
    }

    bool Memory::IsOamBugAddress(Word address) const
    {
        return address >= 0xFE00 && address <= 0xFEFF;
    }

    bool Memory::IsOamBugActive() const
    {
        const Byte lcd_control = io_registers_[LcdControlAddress - 0xFF00];
        const Byte ly = io_registers_[LyAddress - 0xFF00];
        return (lcd_control & LcdEnabledBit) != 0 && ly < 144 && ppu_cycles_ < 80;
    }

    std::uint16_t Memory::ReadOamWord(std::size_t row, std::size_t word) const
    {
        const std::size_t offset = row * 8 + word * 2;
        return static_cast<std::uint16_t>(oam_[offset] | (oam_[offset + 1] << 8));
    }

    void Memory::WriteOamWord(std::size_t row, std::size_t word, std::uint16_t value)
    {
        const std::size_t offset = row * 8 + word * 2;
        oam_[offset] = static_cast<Byte>(value);
        oam_[offset + 1] = static_cast<Byte>(value >> 8);
    }

    void Memory::ApplyOamReadCorruption()
    {
        const auto row = static_cast<std::size_t>(ppu_cycles_ / 4);
        if (row == 0 || row >= 20)
        {
            return;
        }

        const auto row_offset = static_cast<std::size_t>(row * 8);
        if ((row_offset & 0x18) == 0x10)
        {
            if (row < 19)
            {
                const std::uint16_t a = ReadOamWord(row - 2, 0);
                const std::uint16_t b = ReadOamWord(row - 1, 0);
                const std::uint16_t c = ReadOamWord(row, 0);
                const std::uint16_t d = ReadOamWord(row - 1, 2);
                WriteOamWord(row - 1, 0, static_cast<std::uint16_t>((b & (a | c | d)) | (a & c & d)));
                for (std::size_t word = 0; word < 4; ++word)
                {
                    WriteOamWord(row - 2, word, ReadOamWord(row - 1, word));
                }
            }
        }
        else if ((row_offset & 0x18) == 0x00)
        {
            if (row < 19)
            {
                const std::uint16_t current_first = ReadOamWord(row, 0);
                const std::uint16_t previous_third = ReadOamWord(row - 1, 2);
                const std::uint16_t previous_first = ReadOamWord(row - 1, 0);
                const std::uint16_t two_rows_first = ReadOamWord(row - 2, 0);
                const std::uint16_t four_rows_first = ReadOamWord(row - 4, 0);
                std::uint16_t first_word = 0;

                if (row_offset == 0x20)
                {
                    first_word = static_cast<std::uint16_t>((previous_first & (current_first | previous_third | two_rows_first | four_rows_first)) |
                                                            (current_first & previous_third & two_rows_first & four_rows_first));
                }
                else if (row_offset == 0x40)
                {
                    const std::uint16_t first_oam_word = ReadOamWord(0, 0);
                    const std::uint16_t previous_second = ReadOamWord(row - 1, 1);
                    const std::uint16_t two_rows_second = ReadOamWord(row - 2, 1);
                    first_word = static_cast<std::uint16_t>((previous_first & (four_rows_first | two_rows_first | (~previous_second & two_rows_second) | previous_third | current_first)) |
                                                            (previous_third & two_rows_first & four_rows_first));
                    (void)first_oam_word;
                }
                else if (row_offset == 0x60)
                {
                    first_word = static_cast<std::uint16_t>((previous_first & (current_first | previous_third | two_rows_first | four_rows_first)) |
                                                            (previous_third & two_rows_first & four_rows_first));
                }
                else
                {
                    first_word = static_cast<std::uint16_t>(previous_first | (current_first & previous_third & two_rows_first & four_rows_first));
                }

                WriteOamWord(row - 1, 0, first_word);
                for (std::size_t word = 0; word < 4; ++word)
                {
                    const std::uint16_t value = ReadOamWord(row - 1, word);
                    WriteOamWord(row - 2, word, value);
                    WriteOamWord(row - 4, word, value);
                }
            }
        }
        else
        {
            const std::uint16_t a = ReadOamWord(row, 0);
            const std::uint16_t b = ReadOamWord(row - 1, 0);
            const std::uint16_t c = ReadOamWord(row - 1, 2);
            WriteOamWord(row - 1, 0, static_cast<std::uint16_t>(b | (a & c)));
            WriteOamWord(row, 0, ReadOamWord(row - 1, 0));
        }

        for (std::size_t word = 0; word < 4; ++word)
        {
            WriteOamWord(row, word, ReadOamWord(row - 1, word));
        }
    }

    void Memory::ApplyOamWriteCorruption()
    {
        const auto row = static_cast<std::size_t>((ppu_cycles_ / 4) + 1);
        if (row == 0 || row >= 20)
        {
            return;
        }

        const std::uint16_t a = ReadOamWord(row, 0);
        const std::uint16_t b = ReadOamWord(row - 1, 0);
        const std::uint16_t c = ReadOamWord(row - 1, 2);
        WriteOamWord(row, 0, static_cast<std::uint16_t>(((a ^ c) & (b ^ c)) ^ c));
        for (std::size_t word = 1; word < 4; ++word)
        {
            WriteOamWord(row, word, ReadOamWord(row - 1, word));
        }
    }

    void Memory::RequestInterrupt(Byte bit)
    {
        io_registers_[InterruptFlagAddress - 0xFF00] |= bit;
    }

    void Memory::RequestStatInterrupt(Byte source_bit)
    {
        if ((io_registers_[LcdStatusAddress - 0xFF00] & source_bit) != 0)
        {
            RequestInterrupt(LcdStatInterruptBit);
        }
    }

    void Memory::UpdateLyCompareFlag()
    {
        Byte &status = io_registers_[LcdStatusAddress - 0xFF00];
        const bool match = io_registers_[LyAddress - 0xFF00] == io_registers_[LycAddress - 0xFF00];
        const bool was_match = (status & LyCompareFlag) != 0;
        if (match)
        {
            status |= LyCompareFlag;
        }
        else
        {
            status &= static_cast<Byte>(~LyCompareFlag);
        }

        if (match && !was_match)
        {
            RequestStatInterrupt(LycStatInterruptEnable);
        }
    }
}
