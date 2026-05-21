#include "test.hpp"

#include "mintboy/cartridge.hpp"
#include "mintboy/cpu.hpp"
#include "mintboy/memory.hpp"

#include <vector>

namespace
{
    mintboy::Cartridge MakeCartridge()
    {
        return mintboy::Cartridge(std::vector<mintboy::Byte>(0x8000, 0));
    }
}

MINTBOY_TEST(timer_increments_divider_every_256_cycles)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.Tick(255);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF04) == 0);

    memory.Tick(1);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF04) == 1);

    memory.WriteByte(0xFF04, 0xFF);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF04) == 0);
}

MINTBOY_TEST(timer_increments_tima_when_enabled)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF07, 0x05);
    memory.Tick(15);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF05) == 0);

    memory.Tick(1);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF05) == 1);
}

MINTBOY_TEST(timer_overflow_reloads_modulo_and_requests_interrupt)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF05, 0xFF);
    memory.WriteByte(0xFF06, 0x42);
    memory.WriteByte(0xFF07, 0x05);

    memory.Tick(16);

    MINTBOY_REQUIRE(memory.ReadByte(0xFF05) == 0x42);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF0F) & 0x04) != 0);
}

MINTBOY_TEST(cpu_step_advances_timer)
{
    std::vector<mintboy::Byte> rom(0x8000, 0);
    rom[0x0100] = 0x00;
    rom[0x0101] = 0x00;
    rom[0x0102] = 0x00;
    rom[0x0103] = 0x00;

    mintboy::Cartridge cartridge(std::move(rom));
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    memory.WriteByte(0xFF07, 0x05);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF05) == 0);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF05) == 1);
}
