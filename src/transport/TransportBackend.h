#pragma once

#include "../Config.h"
#include <functional>
#include <string>

namespace audioserver {

enum class TransportState {
    Disconnected,
    Connecting,
    Connected,
    Streaming,
    Error
};

struct TransportStatus {
    TransportState state = TransportState::Disconnected;
    std::string peerAddress;
    uint16_t peerPort = 0;
    uint64_t bytesSent = 0;
    uint64_t bytesReceived = 0;
    uint32_t packetsLost = 0;
    std::string errorMessage;
};

class TransportBackend {
public:
    using AudioReceivedCallback = std::function<void(const float*, int, int)>;
    using ConnectionCallback = std::function<void(bool connected)>;

    virtual ~TransportBackend() = default;

    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;

    virtual bool startSender(const std::string& targetHost, uint16_t port, const StreamConfig& config) = 0;
    virtual bool startReceiver(uint16_t port, const StreamConfig& config) = 0;
    virtual void stop() = 0;

    virtual bool sendAudio(const float* const* channelData, int numChannels, int numSamples) = 0;

    virtual TransportStatus getStatus() const = 0;

    virtual void setAudioReceivedCallback(AudioReceivedCallback callback) = 0;
    virtual void setConnectionCallback(ConnectionCallback callback) = 0;
};

} // namespace audioserver
