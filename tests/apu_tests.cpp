#include "test.hpp"

#include "mintboy/apu.hpp"

MINTBOY_TEST(apu_starts_disabled)
{
    mintboy::Apu apu;

    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0x00);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF12) == 0x00);
}

MINTBOY_TEST(apu_ignores_channel_writes_while_disabled)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF12, 0xF3);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF12) == 0x00);
}

MINTBOY_TEST(apu_stores_registers_while_enabled)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF12, 0xF3);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF30, 0xA5);

    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0x80);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF12) == 0xF3);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF24) == 0x77);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF30) == 0xA5);
}

MINTBOY_TEST(apu_clears_registers_when_disabled)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF12, 0xF3);
    apu.WriteByte(0xFF26, 0x00);

    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0x00);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF12) == 0x00);
}
