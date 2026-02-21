#include "Config.h"
#include "AudioEngine.h"
#include "ApiServer.h"
#include "RingBuffer.h"
#include "ToneGenerator.h"
#include "transport/TcpPcmBackend.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

namespace {
    std::atomic<bool> g_running{true};
}

void signalHandler(int) {
    g_running = false;
}

void listDevices(audioserver::AudioEngine& engine) {
    std::cout << "Input Devices:\n";
    for (const auto& device : engine.getInputDevices()) {
        std::cout << "  - " << device.name << " (" << device.type << ")\n";
    }

    std::cout << "\nOutput Devices:\n";
    for (const auto& device : engine.getOutputDevices()) {
        std::cout << "  - " << device.name << " (" << device.type << ")\n";
    }
}

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI juceInit;

    audioserver::Config config;
    try {
        config = audioserver::Config::fromArgs(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        audioserver::Config::printUsage();
        return 1;
    }

    if (config.showHelp) {
        audioserver::Config::printUsage();
        return 0;
    }

    audioserver::AudioEngine audioEngine;
    if (!audioEngine.initialize(config)) {
        std::cerr << "Failed to initialize audio engine\n";
        return 1;
    }

    if (config.listDevices) {
        listDevices(audioEngine);
        return 0;
    }

    // Validate sender mode requirements
    if (config.mode == audioserver::Mode::Sender && config.target.empty()) {
        std::cerr << "Error: Sender mode requires --target <host>\n";
        return 1;
    }

    // Set up transport
    audioserver::TcpPcmBackend transport;

    // Ring buffer for receiver jitter handling (1 second buffer)
    size_t ringBufferSize = config.sampleRate * config.channels * 1;
    audioserver::RingBuffer<float> ringBuffer(ringBufferSize);

    // Connect audio engine and transport
    bool useTestTone = config.testTone && config.mode == audioserver::Mode::Sender;

    if (config.mode == audioserver::Mode::Sender && !useTestTone) {
        audioEngine.setAudioCallback([&transport](const float* const* data, int channels, int samples) {
            transport.sendAudio(data, channels, samples);
        });
    } else if (config.mode == audioserver::Mode::Receiver) {
        transport.setAudioReceivedCallback([&ringBuffer](const float* data, int channels, int samples) {
            size_t totalSamples = static_cast<size_t>(channels * samples);
            ringBuffer.write(data, totalSamples);
        });

        audioEngine.setPlaybackCallback([&ringBuffer](float* const* data, int channels, int samples) {
            size_t totalSamples = static_cast<size_t>(channels * samples);
            std::vector<float> interleavedBuffer(totalSamples);

            size_t read = ringBuffer.read(interleavedBuffer.data(), totalSamples);
            if (read < totalSamples) {
                // Underrun - fill remainder with silence
                std::fill(interleavedBuffer.begin() + static_cast<long>(read), interleavedBuffer.end(), 0.0f);
            }

            // De-interleave
            for (int i = 0; i < samples; ++i) {
                for (int ch = 0; ch < channels; ++ch) {
                    data[ch][i] = interleavedBuffer[static_cast<size_t>(i * channels + ch)];
                }
            }

            return read > 0;
        });
    }

    audioserver::StreamConfig streamConfig;
    streamConfig.sampleRate = config.sampleRate;
    streamConfig.channels = config.channels;
    streamConfig.bufferSize = config.bufferSize;

    // Open audio device (not needed for test-tone sender)
    if (!useTestTone) {
        if (!audioEngine.openDevice(config.device, config.mode)) {
            std::cerr << "Failed to open audio device\n";
            return 1;
        }
        streamConfig = audioEngine.getStreamConfig();
    }

    // Start transport
    bool transportStarted = false;
    if (config.mode == audioserver::Mode::Sender) {
        transportStarted = transport.startSender(config.target, config.port, streamConfig);
    } else {
        transportStarted = transport.startReceiver(config.port, streamConfig);
    }

    if (!transportStarted) {
        auto status = transport.getStatus();
        std::cerr << "Failed to start transport: " << status.errorMessage << "\n";
        return 1;
    }

    // Start API server
    audioserver::ApiServer apiServer(audioEngine, transport, config);
    if (!apiServer.start(config.apiPort)) {
        std::cerr << "Failed to start API server on port " << config.apiPort << "\n";
        return 1;
    }

    // Print startup info
    std::string modeStr = (config.mode == audioserver::Mode::Sender) ? "sender" : "receiver";
    std::cout << "audio-server started in " << modeStr << " mode\n";
    if (useTestTone) {
        std::cout << "  Source: Test tone (" << config.testToneFrequency << " Hz)\n";
    } else {
        std::cout << "  Device: " << audioEngine.getCurrentDeviceName() << "\n";
    }
    std::cout << "  Sample rate: " << streamConfig.sampleRate << " Hz\n";
    std::cout << "  Channels: " << streamConfig.channels << "\n";
    std::cout << "  Buffer size: " << streamConfig.bufferSize << " samples\n";
    std::cout << "  Streaming port: " << config.port << "\n";
    std::cout << "  API port: " << config.apiPort << "\n";

    if (config.mode == audioserver::Mode::Sender) {
        std::cout << "  Target: " << config.target << "\n";
    }

    std::cout << "\nPress Ctrl+C to exit\n";

    // Set up signal handler
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Test tone generator thread for sender mode
    std::thread toneThread;
    if (useTestTone) {
        toneThread = std::thread([&]() {
            audioserver::ToneGenerator toneGen(streamConfig.sampleRate,
                                                config.testToneFrequency,
                                                streamConfig.channels);

            const int bufferSize = static_cast<int>(streamConfig.bufferSize);
            const int channels = static_cast<int>(streamConfig.channels);

            std::vector<float*> channelPtrs(static_cast<size_t>(channels));
            std::vector<std::vector<float>> channelBuffers(static_cast<size_t>(channels));

            for (int ch = 0; ch < channels; ++ch) {
                channelBuffers[static_cast<size_t>(ch)].resize(static_cast<size_t>(bufferSize));
                channelPtrs[static_cast<size_t>(ch)] = channelBuffers[static_cast<size_t>(ch)].data();
            }

            // Use steady clock for accurate timing
            auto bufferDuration = std::chrono::microseconds(
                static_cast<long>(1000000.0 * bufferSize / streamConfig.sampleRate));
            auto nextTime = std::chrono::steady_clock::now();

            while (g_running) {
                auto status = transport.getStatus();
                if (status.state == audioserver::TransportState::Streaming) {
                    toneGen.generate(channelPtrs.data(), channels, bufferSize);
                    transport.sendAudio(const_cast<const float* const*>(channelPtrs.data()),
                                       channels, bufferSize);

                    // Schedule next buffer at precise interval
                    nextTime += bufferDuration;
                    auto now = std::chrono::steady_clock::now();
                    if (nextTime > now) {
                        std::this_thread::sleep_until(nextTime);
                    } else {
                        // We're behind, reset timing
                        nextTime = now;
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    nextTime = std::chrono::steady_clock::now();
                }
            }
        });
    }

    // Main loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (config.verbose) {
            auto status = transport.getStatus();
            std::string stateStr;
            switch (status.state) {
                case audioserver::TransportState::Disconnected: stateStr = "disconnected"; break;
                case audioserver::TransportState::Connecting: stateStr = "connecting"; break;
                case audioserver::TransportState::Connected: stateStr = "connected"; break;
                case audioserver::TransportState::Streaming: stateStr = "streaming"; break;
                case audioserver::TransportState::Error: stateStr = "error"; break;
            }
            std::cout << "\rState: " << stateStr
                      << " | Sent: " << status.bytesSent / 1024 << " KB"
                      << " | Recv: " << status.bytesReceived / 1024 << " KB"
                      << " | Lost: " << status.packetsLost << "   " << std::flush;
        }
    }

    // Wait for tone thread to finish
    if (toneThread.joinable()) {
        toneThread.join();
    }

    std::cout << "\nShutting down...\n";

    // Clean shutdown
    apiServer.stop();
    transport.stop();
    audioEngine.shutdown();

    return 0;
}
