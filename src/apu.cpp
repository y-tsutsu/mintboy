#include "mintboy/apu.hpp"

#include <algorithm>
#include <array>

namespace mintboy
{
    Byte Apu::ReadByte(Word address) const
    {
        if (!IsRegisterAddress(address))
        {
            return 0xFF;
        }

        return registers_[RegisterIndex(address)];
    }

    void Apu::WriteByte(Word address, Byte value)
    {
        if (!IsRegisterAddress(address))
        {
            return;
        }

        if (address == ControlAddress)
        {
            registers_[RegisterIndex(address)] = static_cast<Byte>(value & 0x80);
            if ((value & 0x80) == 0)
            {
                registers_.fill(0);
                square1_ = {};
                square2_ = {};
                wave_ = {};
                noise_ = {};
                frame_sequencer_cycles_ = 0;
                frame_sequencer_step_ = 0;
                sample_cycles_ = 0.0;
                pending_samples_.clear();
            }
            return;
        }

        if ((registers_[RegisterIndex(ControlAddress)] & 0x80) == 0 && address != 0xFF26)
        {
            return;
        }

        registers_[RegisterIndex(address)] = value;
        if (address == 0xFF14 && (value & 0x80) != 0)
        {
            TriggerSquare(square1_, 0xFF11, 0xFF12);
        }
        else if (address == 0xFF19 && (value & 0x80) != 0)
        {
            TriggerSquare(square2_, 0xFF16, 0xFF17);
        }
        else if (address == 0xFF1E && (value & 0x80) != 0)
        {
            TriggerWave();
        }
        else if (address == 0xFF23 && (value & 0x80) != 0)
        {
            TriggerNoise();
        }
    }

    void Apu::Tick(int cycles)
    {
        if (!IsEnabled())
        {
            return;
        }

        TickFrameSequencer(cycles);

        sample_cycles_ += cycles;
        constexpr double cycles_per_sample = static_cast<double>(CpuFrequency) / SampleRate;
        while (sample_cycles_ >= cycles_per_sample)
        {
            sample_cycles_ -= cycles_per_sample;
            const float channel1 = RenderSquareSample(square1_, 0xFF11, 0xFF13, 0xFF14);
            const float channel2 = RenderSquareSample(square2_, 0xFF16, 0xFF18, 0xFF19);
            const float channel3 = RenderWaveSample();
            const float channel4 = RenderNoiseSample();
            pending_samples_.push_back(std::clamp(MixChannels(channel1, channel2, channel3, channel4), -1.0F, 1.0F));
        }
    }

    std::vector<float> Apu::DrainSamples()
    {
        std::vector<float> samples;
        samples.swap(pending_samples_);
        return samples;
    }

    bool Apu::IsRegisterAddress(Word address)
    {
        return address >= RegisterStart && address <= RegisterEnd;
    }

    std::size_t Apu::RegisterIndex(Word address)
    {
        return address - RegisterStart;
    }

    bool Apu::IsEnabled() const
    {
        return (registers_[RegisterIndex(ControlAddress)] & 0x80) != 0;
    }

    float Apu::MixChannels(float channel1, float channel2, float channel3, float channel4) const
    {
        const Byte output_select = registers_[RegisterIndex(0xFF25)];
        const Byte volume = registers_[RegisterIndex(0xFF24)];
        const std::array<float, 4> channels = {channel1, channel2, channel3, channel4};

        float left = 0.0F;
        float right = 0.0F;
        int left_count = 0;
        int right_count = 0;
        for (int channel = 0; channel < 4; ++channel)
        {
            if ((output_select & (1 << channel)) != 0)
            {
                right += channels[static_cast<std::size_t>(channel)];
                ++right_count;
            }
            if ((output_select & (1 << (channel + 4))) != 0)
            {
                left += channels[static_cast<std::size_t>(channel)];
                ++left_count;
            }
        }

        if (left_count > 0)
        {
            left /= static_cast<float>(left_count);
        }
        if (right_count > 0)
        {
            right /= static_cast<float>(right_count);
        }

        const float left_volume = static_cast<float>(((volume >> 4) & 0x07) + 1) / 8.0F;
        const float right_volume = static_cast<float>((volume & 0x07) + 1) / 8.0F;
        return ((left * left_volume) + (right * right_volume)) * 0.5F;
    }

    float Apu::RenderSquareSample(SquareChannel &channel, Word duty_address, Word frequency_low_address, Word frequency_high_address)
    {
        if (!channel.enabled)
        {
            return 0.0F;
        }

        if (channel.volume == 0)
        {
            return 0.0F;
        }

        const int frequency_value = registers_[RegisterIndex(frequency_low_address)] |
                                    ((registers_[RegisterIndex(frequency_high_address)] & 0x07) << 8);
        if (frequency_value >= 2048)
        {
            return 0.0F;
        }

        const double frequency = 131072.0 / (2048 - frequency_value);
        channel.phase += frequency / SampleRate;
        while (channel.phase >= 1.0)
        {
            channel.phase -= 1.0;
        }

        constexpr std::array<double, 4> duty_thresholds = {0.125, 0.25, 0.5, 0.75};
        const Byte duty = static_cast<Byte>((registers_[RegisterIndex(duty_address)] >> 6) & 0x03);
        const float amplitude = static_cast<float>(channel.volume) / 15.0F;
        return channel.phase < duty_thresholds[duty] ? amplitude : -amplitude;
    }

    float Apu::RenderWaveSample()
    {
        if (!wave_.enabled || (registers_[RegisterIndex(0xFF1A)] & 0x80) == 0)
        {
            return 0.0F;
        }

        const int frequency_value = registers_[RegisterIndex(0xFF1D)] |
                                    ((registers_[RegisterIndex(0xFF1E)] & 0x07) << 8);
        if (frequency_value >= 2048)
        {
            return 0.0F;
        }

        const double frequency = 65536.0 / (2048 - frequency_value);
        wave_.position += 32.0 * frequency / SampleRate;
        while (wave_.position >= 32.0)
        {
            wave_.position -= 32.0;
        }

        const int sample_index = static_cast<int>(wave_.position);
        const Byte packed_sample = registers_[RegisterIndex(static_cast<Word>(0xFF30 + (sample_index / 2)))];
        const int sample = (sample_index & 1) == 0 ? packed_sample >> 4 : packed_sample & 0x0F;
        const int volume_code = (registers_[RegisterIndex(0xFF1C)] >> 5) & 0x03;
        if (volume_code == 0)
        {
            return 0.0F;
        }

        const int shifted_sample = sample >> (volume_code - 1);
        return (static_cast<float>(shifted_sample) / 7.5F) - 1.0F;
    }

    float Apu::RenderNoiseSample()
    {
        if (!noise_.enabled || noise_.volume == 0)
        {
            return 0.0F;
        }

        constexpr std::array<int, 8> divisors = {8, 16, 32, 48, 64, 80, 96, 112};
        const Byte polynomial = registers_[RegisterIndex(0xFF22)];
        const int divisor = divisors[polynomial & 0x07];
        const int clock_shift = polynomial >> 4;
        const double frequency = static_cast<double>(CpuFrequency) / (divisor << clock_shift);
        noise_.timer += frequency / SampleRate;

        while (noise_.timer >= 1.0)
        {
            noise_.timer -= 1.0;
            const std::uint16_t bit = static_cast<std::uint16_t>((noise_.lfsr ^ (noise_.lfsr >> 1)) & 0x01);
            noise_.lfsr = static_cast<std::uint16_t>((noise_.lfsr >> 1) | (bit << 14));
            if ((polynomial & 0x08) != 0)
            {
                noise_.lfsr = static_cast<std::uint16_t>((noise_.lfsr & ~(1 << 6)) | (bit << 6));
            }
        }

        const float amplitude = static_cast<float>(noise_.volume) / 15.0F;
        return (noise_.lfsr & 0x01) == 0 ? amplitude : -amplitude;
    }

    void Apu::TriggerSquare(SquareChannel &channel, Word duty_address, Word volume_address)
    {
        const Byte duty_register = registers_[RegisterIndex(duty_address)];
        const int length_load = duty_register & 0x3F;
        channel.enabled = true;
        channel.phase = 0.0;
        channel.length_counter = length_load == 0 ? 64 : 64 - length_load;
        channel.volume = registers_[RegisterIndex(volume_address)] >> 4;
        const int envelope_period = registers_[RegisterIndex(volume_address)] & 0x07;
        channel.envelope_timer = envelope_period == 0 ? 8 : envelope_period;
    }

    void Apu::TriggerWave()
    {
        const int length_load = registers_[RegisterIndex(0xFF1B)];
        wave_.enabled = (registers_[RegisterIndex(0xFF1A)] & 0x80) != 0;
        wave_.position = 0.0;
        wave_.length_counter = length_load == 0 ? 256 : 256 - length_load;
    }

    void Apu::TriggerNoise()
    {
        const Byte length_register = registers_[RegisterIndex(0xFF20)];
        const int length_load = length_register & 0x3F;
        noise_.enabled = true;
        noise_.lfsr = 0x7FFF;
        noise_.timer = 0.0;
        noise_.length_counter = length_load == 0 ? 64 : 64 - length_load;
        noise_.volume = registers_[RegisterIndex(0xFF21)] >> 4;
        const int envelope_period = registers_[RegisterIndex(0xFF21)] & 0x07;
        noise_.envelope_timer = envelope_period == 0 ? 8 : envelope_period;
    }

    void Apu::TickFrameSequencer(int cycles)
    {
        frame_sequencer_cycles_ += cycles;
        constexpr int cycles_per_step = CpuFrequency / 512;
        while (frame_sequencer_cycles_ >= cycles_per_step)
        {
            frame_sequencer_cycles_ -= cycles_per_step;

            if (frame_sequencer_step_ == 0 || frame_sequencer_step_ == 2 || frame_sequencer_step_ == 4 || frame_sequencer_step_ == 6)
            {
                TickLength(square1_, 0xFF14);
                TickLength(square2_, 0xFF19);
                TickWaveLength();
                TickNoiseLength();
            }

            if (frame_sequencer_step_ == 7)
            {
                TickEnvelope(square1_, 0xFF12);
                TickEnvelope(square2_, 0xFF17);
                TickNoiseEnvelope();
            }

            frame_sequencer_step_ = (frame_sequencer_step_ + 1) & 0x07;
        }
    }

    void Apu::TickLength(SquareChannel &channel, Word frequency_high_address)
    {
        if (!channel.enabled || (registers_[RegisterIndex(frequency_high_address)] & 0x40) == 0 || channel.length_counter <= 0)
        {
            return;
        }

        --channel.length_counter;
        if (channel.length_counter == 0)
        {
            channel.enabled = false;
        }
    }

    void Apu::TickWaveLength()
    {
        if (!wave_.enabled || (registers_[RegisterIndex(0xFF1E)] & 0x40) == 0 || wave_.length_counter <= 0)
        {
            return;
        }

        --wave_.length_counter;
        if (wave_.length_counter == 0)
        {
            wave_.enabled = false;
        }
    }

    void Apu::TickNoiseLength()
    {
        if (!noise_.enabled || (registers_[RegisterIndex(0xFF23)] & 0x40) == 0 || noise_.length_counter <= 0)
        {
            return;
        }

        --noise_.length_counter;
        if (noise_.length_counter == 0)
        {
            noise_.enabled = false;
        }
    }

    void Apu::TickEnvelope(SquareChannel &channel, Word volume_address)
    {
        if (!channel.enabled)
        {
            return;
        }

        const Byte volume_register = registers_[RegisterIndex(volume_address)];
        const int envelope_period = volume_register & 0x07;
        if (envelope_period == 0)
        {
            return;
        }

        --channel.envelope_timer;
        if (channel.envelope_timer > 0)
        {
            return;
        }

        channel.envelope_timer = envelope_period;
        const bool increase = (volume_register & 0x08) != 0;
        if (increase && channel.volume < 15)
        {
            ++channel.volume;
        }
        else if (!increase && channel.volume > 0)
        {
            --channel.volume;
        }
    }

    void Apu::TickNoiseEnvelope()
    {
        if (!noise_.enabled)
        {
            return;
        }

        const Byte volume_register = registers_[RegisterIndex(0xFF21)];
        const int envelope_period = volume_register & 0x07;
        if (envelope_period == 0)
        {
            return;
        }

        --noise_.envelope_timer;
        if (noise_.envelope_timer > 0)
        {
            return;
        }

        noise_.envelope_timer = envelope_period;
        const bool increase = (volume_register & 0x08) != 0;
        if (increase && noise_.volume < 15)
        {
            ++noise_.volume;
        }
        else if (!increase && noise_.volume > 0)
        {
            --noise_.volume;
        }
    }
}
