#include "test.hpp"

#include "mintboy/apu.hpp"

#include <algorithm>
#include <vector>

namespace
{
    bool HasNonZeroSample(const std::vector<float> &samples)
    {
        return std::any_of(samples.begin(), samples.end(), [](float sample)
                           { return sample != 0.0F; });
    }
}

MINTBOY_TEST(apu_starts_disabled)
{
    mintboy::Apu apu;

    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0x70);
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

    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0xF0);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF12) == 0xF3);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF24) == 0x77);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF30) == 0xA5);
}

MINTBOY_TEST(apu_clears_registers_when_disabled)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF12, 0xF3);
    apu.WriteByte(0xFF30, 0xA5);
    apu.WriteByte(0xFF26, 0x00);

    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0x70);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF12) == 0x00);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF30) == 0xA5);
}

MINTBOY_TEST(apu_preserves_length_counters_when_disabled)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF26, 0x00);
    apu.WriteByte(0xFF11, 0x3F);
    apu.Tick(8192 * 4);
    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF12, 0xF0);
    apu.WriteByte(0xFF14, 0xC0);

    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0xF1);
    apu.Tick(8192);

    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0xF0);
}

MINTBOY_TEST(apu_allows_wave_ram_writes_while_disabled)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF30, 0x5A);

    MINTBOY_REQUIRE(apu.ReadByte(0xFF30) == 0x5A);
}

MINTBOY_TEST(apu_reads_control_channel_status_bits)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0xF0);

    apu.WriteByte(0xFF11, 0x80);
    apu.WriteByte(0xFF12, 0xF0);
    apu.WriteByte(0xFF14, 0x80);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0xF1);

    apu.WriteByte(0xFF12, 0x00);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0xF0);
}

MINTBOY_TEST(apu_masks_register_reads)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF10, 0x12);
    apu.WriteByte(0xFF11, 0x80);
    apu.WriteByte(0xFF13, 0x34);
    apu.WriteByte(0xFF15, 0x00);
    apu.WriteByte(0xFF14, 0x01);
    apu.WriteByte(0xFF1A, 0x80);
    apu.WriteByte(0xFF1F, 0x00);
    apu.WriteByte(0xFF1B, 0x56);
    apu.WriteByte(0xFF1C, 0x20);
    apu.WriteByte(0xFF27, 0x00);

    MINTBOY_REQUIRE(apu.ReadByte(0xFF10) == 0x92);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF11) == 0xBF);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF13) == 0xFF);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF15) == 0xFF);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF14) == 0xBF);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF1A) == 0xFF);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF1F) == 0xFF);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF1B) == 0xFF);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF1C) == 0xBF);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF27) == 0xFF);
}

MINTBOY_TEST(apu_generates_square_channel_samples)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x11);
    apu.WriteByte(0xFF11, 0x80);
    apu.WriteByte(0xFF12, 0xF0);
    apu.WriteByte(0xFF13, 0x00);
    apu.WriteByte(0xFF14, 0x87);
    apu.Tick(4194304 / 60);

    const std::vector<float> samples = apu.DrainSamples();
    MINTBOY_REQUIRE(!samples.empty());
    MINTBOY_REQUIRE(HasNonZeroSample(samples));
}

MINTBOY_TEST(apu_does_not_trigger_square_channel_when_dac_is_disabled)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x11);
    apu.WriteByte(0xFF11, 0x80);
    apu.WriteByte(0xFF12, 0x00);
    apu.WriteByte(0xFF14, 0x80);
    apu.Tick(4194304 / 60);

    MINTBOY_REQUIRE(!HasNonZeroSample(apu.DrainSamples()));
}

MINTBOY_TEST(apu_disables_square_channel_when_dac_is_turned_off)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x11);
    apu.WriteByte(0xFF11, 0x80);
    apu.WriteByte(0xFF12, 0xF0);
    apu.WriteByte(0xFF14, 0x80);
    apu.Tick(1024);
    MINTBOY_REQUIRE(HasNonZeroSample(apu.DrainSamples()));

    apu.WriteByte(0xFF12, 0x00);
    apu.Tick(4194304 / 60);

    MINTBOY_REQUIRE(!HasNonZeroSample(apu.DrainSamples()));
}

MINTBOY_TEST(apu_drain_samples_clears_pending_samples)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x11);
    apu.WriteByte(0xFF11, 0x80);
    apu.WriteByte(0xFF12, 0xF0);
    apu.WriteByte(0xFF14, 0x80);
    apu.Tick(4194304 / 60);

    MINTBOY_REQUIRE(!apu.DrainSamples().empty());
    MINTBOY_REQUIRE(apu.DrainSamples().empty());
}

MINTBOY_TEST(apu_stops_square_channel_when_length_expires)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x11);
    apu.WriteByte(0xFF11, 0xBF);
    apu.WriteByte(0xFF12, 0xF0);
    apu.WriteByte(0xFF14, 0xC0);
    apu.Tick(1024);
    MINTBOY_REQUIRE(HasNonZeroSample(apu.DrainSamples()));

    apu.Tick(8192);
    MINTBOY_REQUIRE(!HasNonZeroSample(apu.DrainSamples()));
}

MINTBOY_TEST(apu_clocks_square_length_when_length_enable_is_set_between_length_steps)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x11);
    apu.WriteByte(0xFF11, 0xBF);
    apu.WriteByte(0xFF12, 0xF0);
    apu.WriteByte(0xFF14, 0x80);
    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0xF1);

    apu.Tick(8192);
    apu.WriteByte(0xFF14, 0x40);

    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0xF0);
}

MINTBOY_TEST(apu_keeps_square_running_when_length_enable_is_set_on_length_step)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x11);
    apu.WriteByte(0xFF11, 0xBF);
    apu.WriteByte(0xFF12, 0xF0);
    apu.WriteByte(0xFF14, 0x80);
    apu.WriteByte(0xFF14, 0x40);

    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0xF1);
}

MINTBOY_TEST(apu_updates_square_channel_volume_envelope)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x11);
    apu.WriteByte(0xFF11, 0x80);
    apu.WriteByte(0xFF12, 0x19);
    apu.WriteByte(0xFF14, 0x80);
    apu.Tick(8192 * 8 * 5);

    const std::vector<float> samples = apu.DrainSamples();
    MINTBOY_REQUIRE(!samples.empty());
    MINTBOY_REQUIRE(std::any_of(samples.begin(), samples.end(), [](float sample)
                                { return sample > 0.05F || sample < -0.05F; }));
}

MINTBOY_TEST(apu_disables_channel1_when_frequency_sweep_overflows)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x11);
    apu.WriteByte(0xFF10, 0x11);
    apu.WriteByte(0xFF11, 0x80);
    apu.WriteByte(0xFF12, 0xF0);
    apu.WriteByte(0xFF13, 0x00);
    apu.WriteByte(0xFF14, 0x87);

    MINTBOY_REQUIRE(apu.ReadByte(0xFF26) == 0xF0);
}

MINTBOY_TEST(apu_generates_noise_channel_samples)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x88);
    apu.WriteByte(0xFF20, 0x00);
    apu.WriteByte(0xFF21, 0xF0);
    apu.WriteByte(0xFF22, 0x00);
    apu.WriteByte(0xFF23, 0x80);
    apu.Tick(4194304 / 60);

    const std::vector<float> samples = apu.DrainSamples();
    MINTBOY_REQUIRE(!samples.empty());
    MINTBOY_REQUIRE(HasNonZeroSample(samples));
}

MINTBOY_TEST(apu_does_not_trigger_noise_channel_when_dac_is_disabled)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x88);
    apu.WriteByte(0xFF20, 0x00);
    apu.WriteByte(0xFF21, 0x00);
    apu.WriteByte(0xFF22, 0x00);
    apu.WriteByte(0xFF23, 0x80);
    apu.Tick(4194304 / 60);

    MINTBOY_REQUIRE(!HasNonZeroSample(apu.DrainSamples()));
}

MINTBOY_TEST(apu_stops_noise_channel_when_length_expires)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x88);
    apu.WriteByte(0xFF20, 0x3F);
    apu.WriteByte(0xFF21, 0xF0);
    apu.WriteByte(0xFF22, 0x00);
    apu.WriteByte(0xFF23, 0xC0);
    apu.Tick(1024);
    MINTBOY_REQUIRE(HasNonZeroSample(apu.DrainSamples()));

    apu.Tick(8192);
    MINTBOY_REQUIRE(!HasNonZeroSample(apu.DrainSamples()));
}

MINTBOY_TEST(apu_generates_wave_channel_samples)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x44);
    for (int index = 0; index < 16; ++index)
    {
        apu.WriteByte(static_cast<mintboy::Word>(0xFF30 + index), static_cast<mintboy::Byte>(0xF0));
    }
    apu.WriteByte(0xFF1A, 0x80);
    apu.WriteByte(0xFF1B, 0x00);
    apu.WriteByte(0xFF1C, 0x20);
    apu.WriteByte(0xFF1D, 0x00);
    apu.WriteByte(0xFF1E, 0x87);
    apu.Tick(4194304 / 60);

    const std::vector<float> samples = apu.DrainSamples();
    MINTBOY_REQUIRE(!samples.empty());
    MINTBOY_REQUIRE(HasNonZeroSample(samples));
}

MINTBOY_TEST(apu_does_not_trigger_wave_channel_when_dac_is_disabled)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x44);
    for (int index = 0; index < 16; ++index)
    {
        apu.WriteByte(static_cast<mintboy::Word>(0xFF30 + index), static_cast<mintboy::Byte>(0xF0));
    }
    apu.WriteByte(0xFF1A, 0x00);
    apu.WriteByte(0xFF1C, 0x20);
    apu.WriteByte(0xFF1E, 0x80);
    apu.Tick(4194304 / 60);

    MINTBOY_REQUIRE(!HasNonZeroSample(apu.DrainSamples()));
}

MINTBOY_TEST(apu_stops_wave_channel_when_length_expires)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x44);
    for (int index = 0; index < 16; ++index)
    {
        apu.WriteByte(static_cast<mintboy::Word>(0xFF30 + index), static_cast<mintboy::Byte>(0xF0));
    }
    apu.WriteByte(0xFF1A, 0x80);
    apu.WriteByte(0xFF1B, 0xFF);
    apu.WriteByte(0xFF1C, 0x20);
    apu.WriteByte(0xFF1E, 0xC0);
    apu.Tick(1024);
    MINTBOY_REQUIRE(HasNonZeroSample(apu.DrainSamples()));

    apu.Tick(8192);
    MINTBOY_REQUIRE(!HasNonZeroSample(apu.DrainSamples()));
}

MINTBOY_TEST(apu_applies_wave_channel_volume_shift)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x44);
    for (int index = 0; index < 16; ++index)
    {
        apu.WriteByte(static_cast<mintboy::Word>(0xFF30 + index), static_cast<mintboy::Byte>(0xFF));
    }
    apu.WriteByte(0xFF1A, 0x80);
    apu.WriteByte(0xFF1C, 0x60);
    apu.WriteByte(0xFF1E, 0x80);
    apu.Tick(4194304 / 60);

    const std::vector<float> samples = apu.DrainSamples();
    MINTBOY_REQUIRE(!samples.empty());
    MINTBOY_REQUIRE(std::all_of(samples.begin(), samples.end(), [](float sample)
                                { return sample <= 0.26F && sample >= -0.26F; }));
}

MINTBOY_TEST(apu_respects_channel_output_select)
{
    mintboy::Apu apu;

    apu.WriteByte(0xFF26, 0x80);
    apu.WriteByte(0xFF24, 0x77);
    apu.WriteByte(0xFF25, 0x00);
    apu.WriteByte(0xFF11, 0x80);
    apu.WriteByte(0xFF12, 0xF0);
    apu.WriteByte(0xFF14, 0x80);
    apu.Tick(4194304 / 60);

    MINTBOY_REQUIRE(!HasNonZeroSample(apu.DrainSamples()));

    apu.WriteByte(0xFF25, 0x11);
    apu.Tick(4194304 / 60);
    MINTBOY_REQUIRE(HasNonZeroSample(apu.DrainSamples()));
}
