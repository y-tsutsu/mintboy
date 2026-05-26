#include "test.hpp"

#include "mintboy/apu.hpp"

#include <vector>

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

MINTBOY_TEST(apu_generates_square_channel_samples)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF11, 0x80);
    apu.WriteByte(0xFF12, 0xF0);
    apu.WriteByte(0xFF13, 0x00);
    apu.WriteByte(0xFF14, 0x87);
    apu.Tick(4194304 / 60);

    const std::vector<float> samples = apu.DrainSamples();
    MINTBOY_REQUIRE(!samples.empty());
}

MINTBOY_TEST(apu_drain_samples_clears_pending_samples)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF11, 0x80);
    apu.WriteByte(0xFF12, 0xF0);
    apu.WriteByte(0xFF14, 0x80);
    apu.Tick(4194304 / 60);

    MINTBOY_REQUIRE(!apu.DrainSamples().empty());
    MINTBOY_REQUIRE(apu.DrainSamples().empty());
}
