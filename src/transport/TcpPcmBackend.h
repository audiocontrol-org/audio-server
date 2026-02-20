#pragma once

#include "TransportBackend.h"
#include "TcpPcmProtocol.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace audioserver {

class TcpPcmBackend : public TransportBackend {
public:
    TcpPcmBackend();
    ~TcpPcmBackend() override;

    std::string getName() const override { return "tcp-pcm"; }
    std::string getDescription() const override { return "TCP with raw PCM audio"; }

    bool startSender(const std::string& targetHost, uint16_t port, const StreamConfig& config) override;
    bool startReceiver(uint16_t port, const StreamConfig& config) override;
    void stop() override;

    bool sendAudio(const float* const* channelData, int numChannels, int numSamples) override;

    TransportStatus getStatus() const override;

    void setAudioReceivedCallback(AudioReceivedCallback callback) override;
    void setConnectionCallback(ConnectionCallback callback) override;

private:
    void senderThread();
    void receiverThread();
    void acceptThread();
    void keepaliveThread();
    bool sendAll(int socket, const void* data, size_t size);
    bool receiveAll(int socket, void* data, size_t size);

    std::atomic<bool> running_{false};
    std::atomic<TransportState> state_{TransportState::Disconnected};

    std::thread workerThread_;
    std::thread acceptThread_;
    std::thread keepaliveThread_;

    int socket_ = -1;
    int clientSocket_ = -1;
    int serverSocket_ = -1;

    std::string targetHost_;
    uint16_t port_ = 0;
    StreamConfig streamConfig_;

    AudioReceivedCallback audioCallback_;
    ConnectionCallback connectionCallback_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;

    std::atomic<uint64_t> bytesSent_{0};
    std::atomic<uint64_t> bytesReceived_{0};
    std::atomic<uint32_t> packetsLost_{0};
    std::atomic<uint32_t> sequence_{0};

    std::vector<uint8_t> sendBuffer_;
    std::vector<float> interleavedBuffer_;

    std::string errorMessage_;
    std::string peerAddress_;
    uint16_t peerPort_ = 0;
};

} // namespace audioserver
