#pragma once

#include "Config.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace audioserver {

struct AudioDeviceInfo {
    std::string name;
    std::string type;
    int numInputChannels;
    int numOutputChannels;
    std::vector<double> sampleRates;
    std::vector<int> bufferSizes;
};

class AudioEngine : public juce::AudioIODeviceCallback {
public:
    using AudioCallback = std::function<void(const float* const*, int, int)>;
    using PlaybackCallback = std::function<bool(float* const*, int, int)>;

    AudioEngine();
    ~AudioEngine() override;

    bool initialize(const Config& config);
    void shutdown();

    std::vector<AudioDeviceInfo> getInputDevices() const;
    std::vector<AudioDeviceInfo> getOutputDevices() const;

    bool openDevice(const std::string& deviceName, Mode mode);
    void closeDevice();
    bool isDeviceOpen() const;

    std::string getCurrentDeviceName() const;
    StreamConfig getStreamConfig() const;

    void setAudioCallback(AudioCallback callback);
    void setPlaybackCallback(PlaybackCallback callback);

    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    std::unique_ptr<juce::AudioDeviceManager> deviceManager_;
    AudioCallback audioCallback_;
    PlaybackCallback playbackCallback_;
    Mode mode_ = Mode::Receiver;
    StreamConfig streamConfig_;
    bool deviceOpen_ = false;
};

} // namespace audioserver
