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
            float sample = 0.0F;
            sample += RenderSquareSample(square1_, 0xFF11, 0xFF13, 0xFF14);
            sample += RenderSquareSample(square2_, 0xFF16, 0xFF18, 0xFF19);
            pending_samples_.push_back(std::clamp(sample * 0.25F, -1.0F, 1.0F));
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
            }

            if (frame_sequencer_step_ == 7)
            {
                TickEnvelope(square1_, 0xFF12);
                TickEnvelope(square2_, 0xFF17);
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
}
