#include "test.hpp"

#include "mintboy/cartridge.hpp"
#include "mintboy/cpu.hpp"
#include "mintboy/memory.hpp"

#include <vector>

namespace
{
    mintboy::Cartridge MakeRom(std::initializer_list<mintboy::Byte> program)
    {
        std::vector<mintboy::Byte> rom(0x8000, 0);
        std::copy(program.begin(), program.end(), rom.begin() + 0x0100);
        return mintboy::Cartridge(std::move(rom));
    }
}

MINTBOY_TEST(cpu_executes_immediate_loads)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x3E,
        0x42,
        0x06,
        0x24,
        0x76,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x42);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0102);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().b == 0x24);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0104);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.IsHalted());
}

MINTBOY_TEST(cpu_executes_absolute_jump)
{
    mintboy::Cartridge cartridge = MakeRom({
        0xC3,
        0x34,
        0x12,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 16);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x1234);
}
