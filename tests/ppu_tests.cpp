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

MINTBOY_TEST(ppu_lcd_enable_initializes_mode_and_ly)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF44, 0x80);
    memory.WriteByte(0xFF40, 0x80);

    MINTBOY_REQUIRE(memory.ReadByte(0xFF44) == 0);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 2);
}

MINTBOY_TEST(ppu_updates_mode_during_visible_scanline)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x80);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 2);

    memory.Tick(80);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 3);

    memory.Tick(172);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 0);

    memory.Tick(204);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF44) == 1);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 2);
}

MINTBOY_TEST(ppu_enters_vblank_and_requests_interrupt)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x80);
    memory.Tick(456 * 144);

    MINTBOY_REQUIRE(memory.ReadByte(0xFF44) == 144);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 1);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF0F) & 0x01) != 0);
}

MINTBOY_TEST(ppu_wraps_ly_after_vblank)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x80);
    memory.Tick(456 * 154);

    MINTBOY_REQUIRE(memory.ReadByte(0xFF44) == 0);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 2);
}

MINTBOY_TEST(ppu_sets_lyc_compare_flag)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF45, 1);
    memory.WriteByte(0xFF40, 0x80);

    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x04) == 0);

    memory.Tick(456);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF44) == 1);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x04) != 0);
}
