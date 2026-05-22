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

MINTBOY_TEST(memory_stores_oam_and_io_registers)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFE00, 0x56);
    memory.WriteByte(0xFE9F, 0x78);
    memory.WriteByte(0xFF7F, 0xBC);

    MINTBOY_REQUIRE(memory.ReadByte(0xFE00) == 0x56);
    MINTBOY_REQUIRE(memory.ReadByte(0xFE9F) == 0x78);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF7F) == 0xBC);
}

MINTBOY_TEST(memory_reads_joypad_direction_buttons)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

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
