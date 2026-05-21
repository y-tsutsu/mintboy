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
    memory.WriteByte(0xFF00, 0x9A);
    memory.WriteByte(0xFF7F, 0xBC);

    MINTBOY_REQUIRE(memory.ReadByte(0xFE00) == 0x56);
    MINTBOY_REQUIRE(memory.ReadByte(0xFE9F) == 0x78);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF00) == 0x9A);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF7F) == 0xBC);
}
