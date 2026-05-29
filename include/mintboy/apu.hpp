#pragma once

#include "mintboy/types.hpp"

#include <array>
#include <vector>

namespace mintboy
{
    class Apu
    {
    public:
        [[nodiscard]] Byte ReadByte(Word address) const;
        void WriteByte(Word address, Byte value);
        void Tick(int cycles);
        [[nodiscard]] std::vector<float> DrainSamples();

    private:
        static constexpr Word RegisterStart = 0xFF10;
        static constexpr Word RegisterEnd = 0xFF3F;
        static constexpr Word ControlAddress = 0xFF26;
        static constexpr int CpuFrequency = 4'194'304;
        static constexpr int SampleRate = 48'000;

        struct SquareChannel
        {
            bool enabled = false;
            double phase = 0.0;
            int length_counter = 0;
            int volume = 0;
            int envelope_timer = 0;
            int sweep_shadow_frequency = 0;
            int sweep_timer = 0;
            bool sweep_enabled = false;
            bool sweep_negate_calculated = false;
        };

        struct NoiseChannel
        {
            bool enabled = false;
            std::uint16_t lfsr = 0x7FFF;
            double timer = 0.0;
            int length_counter = 0;
            int volume = 0;
            int envelope_timer = 0;
        };

        struct WaveChannel
        {
            bool enabled = false;
            double position = 0.0;
            int length_counter = 0;
        };

        [[nodiscard]] static bool IsRegisterAddress(Word address);
        [[nodiscard]] static std::size_t RegisterIndex(Word address);
        [[nodiscard]] Byte ReadControl() const;
        [[nodiscard]] Byte ReadRegister(Word address) const;
        [[nodiscard]] bool IsEnabled() const;
        [[nodiscard]] float MixChannels(float channel1, float channel2, float channel3, float channel4) const;
        [[nodiscard]] float RenderSquareSample(SquareChannel &channel, Word duty_address, Word frequency_low_address, Word frequency_high_address);
        [[nodiscard]] float RenderWaveSample();
        [[nodiscard]] float RenderNoiseSample();
        [[nodiscard]] bool IsSquareDacEnabled(Word volume_address) const;
        [[nodiscard]] bool IsWaveDacEnabled() const;
        [[nodiscard]] bool IsNoiseDacEnabled() const;
        [[nodiscard]] bool ShouldClockLengthImmediately() const;
        void ClockLengthOnEnable(Word address, Byte old_value, Byte new_value);
        void ReloadSquareLength(SquareChannel &channel, Word duty_address);
        void TriggerSquare(SquareChannel &channel, Word volume_address);
        [[nodiscard]] int SquareFrequency(Word frequency_low_address, Word frequency_high_address) const;
        void WriteSquareFrequency(Word frequency_low_address, Word frequency_high_address, int frequency);
        [[nodiscard]] int CalculateSweepFrequency() const;
        void TickSweep();
        void TriggerWave();
        void TriggerNoise();
        void TickFrameSequencer(int cycles);
        void TickLength(SquareChannel &channel, Word frequency_high_address);
        void TickWaveLength();
        void TickNoiseLength();
        void TickEnvelope(SquareChannel &channel, Word volume_address);
        void TickNoiseEnvelope();

        std::array<Byte, RegisterEnd - RegisterStart + 1> registers_{};
        SquareChannel square1_{};
        SquareChannel square2_{};
        WaveChannel wave_{};
        NoiseChannel noise_{};
        int frame_sequencer_cycles_ = 0;
        int frame_sequencer_step_ = 0;
        double sample_cycles_ = 0.0;
        std::vector<float> pending_samples_;
    };
}
