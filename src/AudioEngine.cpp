#include "AudioEngine.h"
#include <iostream>

namespace audioserver {

AudioEngine::AudioEngine()
    : deviceManager_(std::make_unique<juce::AudioDeviceManager>()) {
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize(const Config& config) {
    mode_ = config.mode;
    streamConfig_.sampleRate = config.sampleRate;
    streamConfig_.channels = config.channels;
    streamConfig_.bufferSize = config.bufferSize;
    return true;
}

void AudioEngine::shutdown() {
    closeDevice();
}

std::vector<AudioDeviceInfo> AudioEngine::getInputDevices() const {
    std::vector<AudioDeviceInfo> devices;
    auto& types = deviceManager_->getAvailableDeviceTypes();

    for (auto* type : types) {
        type->scanForDevices();
        auto deviceNames = type->getDeviceNames(true);  // true = input devices

        for (const auto& name : deviceNames) {
            AudioDeviceInfo info;
            info.name = name.toStdString();
            info.type = type->getTypeName().toStdString();
            info.numInputChannels = 2;  // Default assumption
            info.numOutputChannels = 0;
            devices.push_back(info);
        }
    }

    return devices;
}

std::vector<AudioDeviceInfo> AudioEngine::getOutputDevices() const {
    std::vector<AudioDeviceInfo> devices;
    auto& types = deviceManager_->getAvailableDeviceTypes();

    for (auto* type : types) {
        type->scanForDevices();
        auto deviceNames = type->getDeviceNames(false);  // false = output devices

        for (const auto& name : deviceNames) {
            AudioDeviceInfo info;
            info.name = name.toStdString();
            info.type = type->getTypeName().toStdString();
            info.numInputChannels = 0;
            info.numOutputChannels = 2;  // Default assumption
            devices.push_back(info);
        }
    }

    return devices;
}

bool AudioEngine::openDevice(const std::string& deviceName, Mode mode) {
    mode_ = mode;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.sampleRate = streamConfig_.sampleRate;
    setup.bufferSize = static_cast<int>(streamConfig_.bufferSize);

    if (mode == Mode::Sender) {
        setup.inputDeviceName = juce::String(deviceName);
        setup.outputDeviceName = "";
        setup.useDefaultInputChannels = true;
        setup.inputChannels.setRange(0, streamConfig_.channels, true);
    } else {
        setup.inputDeviceName = "";
        setup.outputDeviceName = juce::String(deviceName);
        setup.useDefaultOutputChannels = true;
        setup.outputChannels.setRange(0, streamConfig_.channels, true);
    }

    juce::String error;
    if (deviceName.empty()) {
        // Use default device
        int numInputChannels = (mode == Mode::Sender) ? streamConfig_.channels : 0;
        int numOutputChannels = (mode == Mode::Receiver) ? streamConfig_.channels : 0;
        error = deviceManager_->initialise(numInputChannels, numOutputChannels, nullptr, true);
    } else {
        error = deviceManager_->setAudioDeviceSetup(setup, true);
    }

    if (error.isNotEmpty()) {
        std::cerr << "Failed to open audio device: " << error.toStdString() << std::endl;
        return false;
    }

    deviceManager_->addAudioCallback(this);
    deviceOpen_ = true;

    auto* device = deviceManager_->getCurrentAudioDevice();
    if (device) {
        streamConfig_.sampleRate = static_cast<uint32_t>(device->getCurrentSampleRate());
        streamConfig_.bufferSize = static_cast<uint32_t>(device->getCurrentBufferSizeSamples());
    }

    return true;
}

void AudioEngine::closeDevice() {
    if (deviceOpen_) {
        deviceManager_->removeAudioCallback(this);
        deviceManager_->closeAudioDevice();
        deviceOpen_ = false;
    }
}

bool AudioEngine::isDeviceOpen() const {
    return deviceOpen_;
}

std::string AudioEngine::getCurrentDeviceName() const {
    auto* device = deviceManager_->getCurrentAudioDevice();
    if (device) {
        return device->getName().toStdString();
    }
    return "";
}

StreamConfig AudioEngine::getStreamConfig() const {
    return streamConfig_;
}

void AudioEngine::setAudioCallback(AudioCallback callback) {
    audioCallback_ = std::move(callback);
}

void AudioEngine::setPlaybackCallback(PlaybackCallback callback) {
    playbackCallback_ = std::move(callback);
}

void AudioEngine::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& /*context*/) {

    if (mode_ == Mode::Sender && audioCallback_ && numInputChannels > 0) {
        audioCallback_(inputChannelData, numInputChannels, numSamples);
    }

    if (mode_ == Mode::Receiver && playbackCallback_ && numOutputChannels > 0) {
        if (!playbackCallback_(outputChannelData, numOutputChannels, numSamples)) {
            // No audio available, output silence
            for (int ch = 0; ch < numOutputChannels; ++ch) {
                std::fill(outputChannelData[ch], outputChannelData[ch] + numSamples, 0.0f);
            }
        }
    }
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    if (device) {
        streamConfig_.sampleRate = static_cast<uint32_t>(device->getCurrentSampleRate());
        streamConfig_.bufferSize = static_cast<uint32_t>(device->getCurrentBufferSizeSamples());
    }
}

void AudioEngine::audioDeviceStopped() {
}

} // namespace audioserver
