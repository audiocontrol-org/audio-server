#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace audioserver {

class ToneGenerator {
public:
    ToneGenerator(uint32_t sampleRate, uint32_t frequency, uint16_t channels)
        : sampleRate_(sampleRate)
        , frequency_(frequency)
        , channels_(channels)
        , phase_(0.0)
        , phaseIncrement_(2.0 * M_PI * frequency / sampleRate) {
    }

    void generate(float* const* channelData, int numChannels, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            float sample = static_cast<float>(std::sin(phase_) * 0.5);  // 0.5 amplitude
            phase_ += phaseIncrement_;
            if (phase_ >= 2.0 * M_PI) {
                phase_ -= 2.0 * M_PI;
            }

            for (int ch = 0; ch < numChannels && ch < channels_; ++ch) {
                channelData[ch][i] = sample;
            }
        }
    }

    void generateInterleaved(float* data, int numChannels, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            float sample = static_cast<float>(std::sin(phase_) * 0.5);
            phase_ += phaseIncrement_;
            if (phase_ >= 2.0 * M_PI) {
                phase_ -= 2.0 * M_PI;
            }

            for (int ch = 0; ch < numChannels; ++ch) {
                data[i * numChannels + ch] = sample;
            }
        }
    }

private:
    uint32_t sampleRate_;
    uint32_t frequency_;
    uint16_t channels_;
    double phase_;
    double phaseIncrement_;
};

} // namespace audioserver
