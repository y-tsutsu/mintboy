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
        };

        [[nodiscard]] static bool IsRegisterAddress(Word address);
        [[nodiscard]] static std::size_t RegisterIndex(Word address);
        [[nodiscard]] bool IsEnabled() const;
        [[nodiscard]] float RenderSquareSample(SquareChannel &channel, Word duty_address, Word frequency_low_address, Word frequency_high_address);
        void TriggerSquare(SquareChannel &channel, Word duty_address, Word volume_address);
        void TickFrameSequencer(int cycles);
        void TickLength(SquareChannel &channel, Word frequency_high_address);
        void TickEnvelope(SquareChannel &channel, Word volume_address);

        std::array<Byte, RegisterEnd - RegisterStart + 1> registers_{};
        SquareChannel square1_{};
        SquareChannel square2_{};
        int frame_sequencer_cycles_ = 0;
        int frame_sequencer_step_ = 0;
        double sample_cycles_ = 0.0;
        std::vector<float> pending_samples_;
    };
}
