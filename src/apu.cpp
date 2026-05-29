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

        if (address == ControlAddress)
        {
            return ReadControl();
        }

        return ReadRegister(address);
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
                const int square1_length = square1_.length_counter;
                const int square2_length = square2_.length_counter;
                const int wave_length = wave_.length_counter;
                const int noise_length = noise_.length_counter;
                std::fill(registers_.begin(), registers_.begin() + RegisterIndex(0xFF30), 0);
                square1_ = {};
                square2_ = {};
                wave_ = {};
                noise_ = {};
                square1_.length_counter = square1_length;
                square2_.length_counter = square2_length;
                wave_.length_counter = wave_length;
                noise_.length_counter = noise_length;
                frame_sequencer_cycles_ = 0;
                frame_sequencer_step_ = 0;
                apu_cycles_ = 0;
                sample_cycles_ = 0.0;
                pending_samples_.clear();
            }
            return;
        }

        if ((registers_[RegisterIndex(ControlAddress)] & 0x80) == 0 && address < 0xFF30)
        {
            if (address == 0xFF11)
            {
                square1_.length_counter = (value & 0x3F) == 0 ? 64 : 64 - (value & 0x3F);
            }
            else if (address == 0xFF16)
            {
                square2_.length_counter = (value & 0x3F) == 0 ? 64 : 64 - (value & 0x3F);
            }
            else if (address == 0xFF1B)
            {
                wave_.length_counter = value == 0 ? 256 : 256 - value;
            }
            else if (address == 0xFF20)
            {
                const int length_load = value & 0x3F;
                noise_.length_counter = length_load == 0 ? 64 : 64 - length_load;
            }
            return;
        }

        const Byte old_value = registers_[RegisterIndex(address)];
        registers_[RegisterIndex(address)] = value;
        if (address == 0xFF11)
        {
            ReloadSquareLength(square1_, 0xFF11);
        }
        else if (address == 0xFF16)
        {
            ReloadSquareLength(square2_, 0xFF16);
        }
        else if (address == 0xFF1B)
        {
            const int length_load = registers_[RegisterIndex(0xFF1B)];
            wave_.length_counter = length_load == 0 ? 256 : 256 - length_load;
        }
        else if (address == 0xFF20)
        {
            const int length_load = registers_[RegisterIndex(0xFF20)] & 0x3F;
            noise_.length_counter = length_load == 0 ? 64 : 64 - length_load;
        }

        if (address == 0xFF12 && !IsSquareDacEnabled(0xFF12))
        {
            square1_.enabled = false;
        }
        else if (address == 0xFF17 && !IsSquareDacEnabled(0xFF17))
        {
            square2_.enabled = false;
        }
        else if (address == 0xFF1A && !IsWaveDacEnabled())
        {
            wave_.enabled = false;
        }
        else if (address == 0xFF21 && !IsNoiseDacEnabled())
        {
            noise_.enabled = false;
        }

        if ((address == 0xFF1D || address == 0xFF1E) && wave_.enabled && (address != 0xFF1E || (value & 0x80) == 0))
        {
            wave_.pending_frequency_period = WaveFrequencyPeriod();
            wave_.frequency_period_pending = true;
        }

        if (address == 0xFF10 && (old_value & 0x08) != 0 && (value & 0x08) == 0 && square1_.sweep_negate_calculated)
        {
            square1_.enabled = false;
        }

        ClockLengthOnEnable(address, old_value, value);

        if (address == 0xFF14 && (value & 0x80) != 0)
        {
            const bool reloaded_length = square1_.length_counter == 0;
            TriggerSquare(square1_, 0xFF12);
            square1_.sweep_shadow_frequency = SquareFrequency(0xFF13, 0xFF14);
            const Byte sweep = registers_[RegisterIndex(0xFF10)];
            const int sweep_period = (sweep >> 4) & 0x07;
            const int sweep_shift = sweep & 0x07;
            square1_.sweep_timer = sweep_period;
            if (sweep_period == 0)
            {
                square1_.sweep_timer = 8;
            }
            square1_.sweep_negate_calculated = false;
            square1_.sweep_enabled = sweep_period != 0 || sweep_shift != 0;
            if (sweep_shift != 0 && CalculateSweepFrequency() > 2047)
            {
                if ((sweep & 0x08) != 0)
                {
                    square1_.sweep_negate_calculated = true;
                }
                square1_.enabled = false;
            }
            else if (sweep_shift != 0 && (sweep & 0x08) != 0)
            {
                square1_.sweep_negate_calculated = true;
            }
            if (reloaded_length && (value & 0x40) != 0 && ShouldClockLengthImmediately() && square1_.length_counter > 0)
            {
                TickLength(square1_, 0xFF14);
            }
        }
        else if (address == 0xFF19 && (value & 0x80) != 0)
        {
            const bool reloaded_length = square2_.length_counter == 0;
            TriggerSquare(square2_, 0xFF17);
            if (reloaded_length && (value & 0x40) != 0 && ShouldClockLengthImmediately() && square2_.length_counter > 0)
            {
                TickLength(square2_, 0xFF19);
            }
        }
        else if (address == 0xFF1E && (value & 0x80) != 0)
        {
            const bool reloaded_length = wave_.length_counter == 0;
            TriggerWave();
            if (reloaded_length && (value & 0x40) != 0 && ShouldClockLengthImmediately() && wave_.length_counter > 0)
            {
                TickWaveLength();
            }
        }
        else if (address == 0xFF23 && (value & 0x80) != 0)
        {
            const bool reloaded_length = noise_.length_counter == 0;
            TriggerNoise();
            if (reloaded_length && (value & 0x40) != 0 && ShouldClockLengthImmediately() && noise_.length_counter > 0)
            {
                TickNoiseLength();
            }
        }
    }

    void Apu::Tick(int cycles)
    {
        if (!IsEnabled())
        {
            return;
        }

        TickFrameSequencer(cycles);
        TickWaveTimer(cycles);
        apu_cycles_ += cycles;

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

    int Apu::WaveFrequencyPeriod() const
    {
        const int frequency_value = registers_[RegisterIndex(0xFF1D)] |
                                    ((registers_[RegisterIndex(0xFF1E)] & 0x07) << 8);
        return frequency_value >= 2048 ? 0 : (2048 - frequency_value) * 2;
    }

    Byte Apu::ReadControl() const
    {
        Byte value = static_cast<Byte>(0x70 | (registers_[RegisterIndex(ControlAddress)] & 0x80));
        if (square1_.enabled)
        {
            value |= 0x01;
        }
        if (square2_.enabled)
        {
            value |= 0x02;
        }
        if (wave_.enabled)
        {
            value |= 0x04;
        }
        if (noise_.enabled)
        {
            value |= 0x08;
        }
        return value;
    }

    Byte Apu::ReadRegister(Word address) const
    {
        const Byte value = registers_[RegisterIndex(address)];
        if (address >= 0xFF30 && address <= 0xFF3F && wave_.enabled)
        {
            // On DMG, CPU wave RAM reads only see CH3's bus value at the
            // narrow timing point where the channel is fetching wave RAM.
            if (wave_.last_access_cycle == apu_cycles_ - 2 && wave_.has_previous_byte)
            {
                return wave_.previous_byte;
            }
            return 0xFF;
        }

        switch (address)
        {
        case 0xFF10:
            return static_cast<Byte>(value | 0x80);
        case 0xFF11:
        case 0xFF16:
            return static_cast<Byte>(value | 0x3F);
        case 0xFF13:
        case 0xFF15:
        case 0xFF18:
        case 0xFF1F:
        case 0xFF1B:
        case 0xFF1D:
        case 0xFF20:
            return 0xFF;
        case 0xFF14:
        case 0xFF19:
        case 0xFF1E:
        case 0xFF23:
            return static_cast<Byte>(value | 0xBF);
        case 0xFF1A:
            return static_cast<Byte>(value | 0x7F);
        case 0xFF1C:
            return static_cast<Byte>(value | 0x9F);
        case 0xFF27:
        case 0xFF28:
        case 0xFF29:
        case 0xFF2A:
        case 0xFF2B:
        case 0xFF2C:
        case 0xFF2D:
        case 0xFF2E:
        case 0xFF2F:
            return 0xFF;
        default:
            return value;
        }
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

        const int frequency_value = SquareFrequency(frequency_low_address, frequency_high_address);
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

        const int sample_index = wave_.sample_index;
        const Byte packed_sample = registers_[RegisterIndex(static_cast<Word>(0xFF30 + (sample_index / 2)))];
        const int sample = (sample_index & 1) == 0 ? packed_sample >> 4 : packed_sample & 0x0F;
        const int volume_code = (registers_[RegisterIndex(0xFF1C)] >> 5) & 0x03;
        if (volume_code == 0)
        {
            return 0.0F;
        }

        constexpr std::array<float, 4> volume_scales = {0.0F, 1.0F, 0.5F, 0.25F};
        const float centered_sample = (static_cast<float>(sample) - 7.5F) / 7.5F;
        return centered_sample * volume_scales[static_cast<std::size_t>(volume_code)];
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

    Byte Apu::CurrentWaveRamByte() const
    {
        return registers_[RegisterIndex(static_cast<Word>(0xFF30 + (wave_.sample_index / 2)))];
    }

    bool Apu::IsSquareDacEnabled(Word volume_address) const
    {
        return (registers_[RegisterIndex(volume_address)] & 0xF8) != 0;
    }

    bool Apu::IsWaveDacEnabled() const
    {
        return (registers_[RegisterIndex(0xFF1A)] & 0x80) != 0;
    }

    bool Apu::IsNoiseDacEnabled() const
    {
        return (registers_[RegisterIndex(0xFF21)] & 0xF8) != 0;
    }

    bool Apu::ShouldClockLengthImmediately() const
    {
        return (frame_sequencer_step_ & 0x01) != 0;
    }

    void Apu::ClockLengthOnEnable(Word address, Byte old_value, Byte new_value)
    {
        if ((old_value & 0x40) != 0 || (new_value & 0x40) == 0 || !ShouldClockLengthImmediately())
        {
            return;
        }

        switch (address)
        {
        case 0xFF14:
            TickLength(square1_, 0xFF14);
            break;
        case 0xFF19:
            TickLength(square2_, 0xFF19);
            break;
        case 0xFF1E:
            TickWaveLength();
            break;
        case 0xFF23:
            TickNoiseLength();
            break;
        default:
            break;
        }
    }

    void Apu::ReloadSquareLength(SquareChannel &channel, Word duty_address)
    {
        const Byte duty_register = registers_[RegisterIndex(duty_address)];
        const int length_load = duty_register & 0x3F;
        channel.length_counter = length_load == 0 ? 64 : 64 - length_load;
    }

    void Apu::TriggerSquare(SquareChannel &channel, Word volume_address)
    {
        channel.enabled = IsSquareDacEnabled(volume_address);
        channel.phase = 0.0;
        if (channel.length_counter == 0)
        {
            channel.length_counter = 64;
        }
        channel.volume = registers_[RegisterIndex(volume_address)] >> 4;
        const int envelope_period = registers_[RegisterIndex(volume_address)] & 0x07;
        channel.envelope_timer = envelope_period == 0 ? 8 : envelope_period;
    }

    int Apu::SquareFrequency(Word frequency_low_address, Word frequency_high_address) const
    {
        return registers_[RegisterIndex(frequency_low_address)] |
               ((registers_[RegisterIndex(frequency_high_address)] & 0x07) << 8);
    }

    void Apu::WriteSquareFrequency(Word frequency_low_address, Word frequency_high_address, int frequency)
    {
        registers_[RegisterIndex(frequency_low_address)] = static_cast<Byte>(frequency & 0xFF);
        registers_[RegisterIndex(frequency_high_address)] = static_cast<Byte>((registers_[RegisterIndex(frequency_high_address)] & 0xF8) | ((frequency >> 8) & 0x07));
    }

    int Apu::CalculateSweepFrequency() const
    {
        const Byte sweep = registers_[RegisterIndex(0xFF10)];
        const int shift = sweep & 0x07;
        const int delta = square1_.sweep_shadow_frequency >> shift;
        return (sweep & 0x08) != 0
                   ? square1_.sweep_shadow_frequency - delta
                   : square1_.sweep_shadow_frequency + delta;
    }

    void Apu::TickSweep()
    {
        if (!square1_.sweep_enabled)
        {
            return;
        }

        --square1_.sweep_timer;
        if (square1_.sweep_timer > 0)
        {
            return;
        }

        const int period = (registers_[RegisterIndex(0xFF10)] >> 4) & 0x07;
        square1_.sweep_timer = period == 0 ? 8 : period;
        if (period == 0)
        {
            return;
        }

        const int shift = registers_[RegisterIndex(0xFF10)] & 0x07;
        const int frequency = CalculateSweepFrequency();
        if ((registers_[RegisterIndex(0xFF10)] & 0x08) != 0)
        {
            square1_.sweep_negate_calculated = true;
        }
        if (frequency > 2047)
        {
            square1_.enabled = false;
            return;
        }

        if (shift == 0)
        {
            return;
        }

        square1_.sweep_shadow_frequency = frequency;
        WriteSquareFrequency(0xFF13, 0xFF14, frequency);
        if (CalculateSweepFrequency() > 2047)
        {
            square1_.enabled = false;
        }
    }

    void Apu::TriggerWave()
    {
        wave_.enabled = IsWaveDacEnabled();
        wave_.sample_index = 0;
        wave_.last_access_cycle = -1;
        wave_.has_previous_byte = false;
        wave_.frequency_period = WaveFrequencyPeriod();
        wave_.pending_frequency_period = wave_.frequency_period;
        wave_.frequency_period_pending = false;
        wave_.frequency_timer = wave_.frequency_period;
        if (wave_.length_counter == 0)
        {
            wave_.length_counter = 256;
        }
    }

    void Apu::TriggerNoise()
    {
        noise_.enabled = IsNoiseDacEnabled();
        noise_.lfsr = 0x7FFF;
        noise_.timer = 0.0;
        if (noise_.length_counter == 0)
        {
            noise_.length_counter = 64;
        }
        noise_.volume = registers_[RegisterIndex(0xFF21)] >> 4;
        const int envelope_period = registers_[RegisterIndex(0xFF21)] & 0x07;
        noise_.envelope_timer = envelope_period == 0 ? 8 : envelope_period;
    }

    void Apu::TickWaveTimer(int cycles)
    {
        if (!wave_.enabled || !IsWaveDacEnabled())
        {
            return;
        }

        if (wave_.frequency_period <= 0)
        {
            return;
        }

        wave_.frequency_timer -= cycles;
        while (wave_.frequency_timer <= 0)
        {
            const int access_cycle = apu_cycles_ + cycles + wave_.frequency_timer;
            wave_.sample_index = (wave_.sample_index + 1) & 0x1F;
            // CH3 output is fed by a byte latch; CPU reads during playback
            // expose the previously latched byte, not arbitrary wave RAM.
            wave_.previous_byte = wave_.current_byte;
            wave_.has_previous_byte = wave_.last_access_cycle >= 0;
            wave_.current_byte = CurrentWaveRamByte();
            wave_.last_access_cycle = access_cycle;
            if (wave_.frequency_period_pending)
            {
                wave_.frequency_period = wave_.pending_frequency_period;
                wave_.frequency_period_pending = false;
            }
            wave_.frequency_timer += wave_.frequency_period;
        }
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

            if (frame_sequencer_step_ == 2 || frame_sequencer_step_ == 6)
            {
                TickSweep();
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
        if ((registers_[RegisterIndex(frequency_high_address)] & 0x40) == 0 || channel.length_counter <= 0)
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
        if ((registers_[RegisterIndex(0xFF1E)] & 0x40) == 0 || wave_.length_counter <= 0)
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
        if ((registers_[RegisterIndex(0xFF23)] & 0x40) == 0 || noise_.length_counter <= 0)
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
