#include "test.hpp"

#include "mintboy/cartridge.hpp"
#include "mintboy/memory.hpp"

#include <vector>

namespace
{
    mintboy::Cartridge MakeCartridge()
    {
        return mintboy::Cartridge(std::vector<mintboy::Byte>(0x8000, 0));
    }

    mintboy::Cartridge MakeMbc1RamCartridge()
    {
        std::vector<mintboy::Byte> rom(0x8000, 0);
        rom[0x0147] = 0x02;
        rom[0x0148] = 0x00;
        rom[0x0149] = 0x02;
        return mintboy::Cartridge(std::move(rom));
    }
}

MINTBOY_TEST(memory_stores_video_ram)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0x8000, 0x12);
    memory.WriteByte(0x9FFF, 0x34);

    MINTBOY_REQUIRE(memory.ReadByte(0x8000) == 0x12);
    MINTBOY_REQUIRE(memory.ReadByte(0x9FFF) == 0x34);
}

MINTBOY_TEST(memory_maps_cartridge_external_ram)
{
    mintboy::Cartridge cartridge = MakeMbc1RamCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xA000, 0x12);
    MINTBOY_REQUIRE(memory.ReadByte(0xA000) == 0xFF);

    memory.WriteByte(0x0000, 0x0A);
    memory.WriteByte(0xA000, 0x34);
    MINTBOY_REQUIRE(memory.ReadByte(0xA000) == 0x34);
}

MINTBOY_TEST(memory_stores_oam_and_io_registers)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x00);
    memory.WriteByte(0xFE00, 0x56);
    memory.WriteByte(0xFE9F, 0x78);
    memory.WriteByte(0xFF7F, 0xBC);

    MINTBOY_REQUIRE(memory.ReadByte(0xFE00) == 0x56);
    MINTBOY_REQUIRE(memory.ReadByte(0xFE9F) == 0x78);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF7F) == 0xBC);
}

MINTBOY_TEST(memory_tracks_key1_speed_switch_state)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    MINTBOY_REQUIRE(memory.ReadByte(0xFF4D) == 0x7E);

    memory.WriteByte(0xFF4D, 0xFF);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF4D) == 0x7F);

    MINTBOY_REQUIRE(memory.ConsumeSpeedSwitchRequest());
    MINTBOY_REQUIRE(memory.ReadByte(0xFF4D) == 0xFE);
    MINTBOY_REQUIRE(!memory.ConsumeSpeedSwitchRequest());
}

MINTBOY_TEST(memory_maps_apu_registers)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF12, 0xF3);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF12) == 0x00);

    memory.WriteByte(0xFF26, 0x80);
    memory.WriteByte(0xFF12, 0xF3);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF26) == 0xF0);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF12) == 0xF3);
}

MINTBOY_TEST(memory_captures_serial_transfer_output)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF01, 'O');
    memory.WriteByte(0xFF02, 0x81);
    memory.WriteByte(0xFF01, 'K');
    memory.WriteByte(0xFF02, 0x81);

    MINTBOY_REQUIRE(memory.DrainSerialOutput() == "OK");
    MINTBOY_REQUIRE(memory.DrainSerialOutput().empty());
    MINTBOY_REQUIRE(memory.ReadByte(0xFF02) == 0x01);
}

MINTBOY_TEST(memory_masks_interrupt_flag_unused_bits)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF0F, 0x00);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF0F) == 0xE0);

    memory.WriteByte(0xFF0F, 0xFF);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF0F) == 0xFF);
}

MINTBOY_TEST(memory_reads_joypad_direction_buttons)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    MINTBOY_REQUIRE(memory.ReadByte(0xFF00) == 0xFF);

    memory.WriteByte(0xFF00, 0x20);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF00) & 0x0F) == 0x0F);

    memory.SetJoypadButton(mintboy::Memory::JoypadButton::Right, true);
    memory.SetJoypadButton(mintboy::Memory::JoypadButton::Down, true);

    MINTBOY_REQUIRE((memory.ReadByte(0xFF00) & 0x0F) == 0x06);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF0F) & 0x10) != 0);

    memory.SetJoypadButton(mintboy::Memory::JoypadButton::Right, false);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF00) & 0x0F) == 0x07);
}

MINTBOY_TEST(memory_reads_joypad_action_buttons)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF00, 0x10);
    memory.SetJoypadButton(mintboy::Memory::JoypadButton::A, true);
    memory.SetJoypadButton(mintboy::Memory::JoypadButton::Start, true);

    MINTBOY_REQUIRE((memory.ReadByte(0xFF00) & 0x0F) == 0x06);
}

MINTBOY_TEST(memory_requests_joypad_interrupt_only_for_selected_group)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF00, 0x20);
    memory.SetJoypadButton(mintboy::Memory::JoypadButton::A, true);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF0F) & 0x10) == 0);

    memory.SetJoypadButton(mintboy::Memory::JoypadButton::Right, true);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF0F) & 0x10) != 0);

    memory.WriteByte(0xFF0F, 0);
    memory.WriteByte(0xFF00, 0x10);
    memory.SetJoypadButton(mintboy::Memory::JoypadButton::B, true);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF0F) & 0x10) != 0);
}

MINTBOY_TEST(memory_keeps_joypad_button_groups_separate)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.SetJoypadButton(mintboy::Memory::JoypadButton::A, true);
    memory.SetJoypadButton(mintboy::Memory::JoypadButton::B, true);

    memory.WriteByte(0xFF00, 0x20);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF00) & 0x0F) == 0x0F);

    memory.WriteByte(0xFF00, 0x10);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF00) & 0x0F) == 0x0C);

    memory.SetJoypadButton(mintboy::Memory::JoypadButton::Right, true);
    memory.WriteByte(0xFF00, 0x20);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF00) & 0x0F) == 0x0E);

    memory.WriteByte(0xFF00, 0x10);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF00) & 0x0F) == 0x0C);
}
