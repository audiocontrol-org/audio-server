#include "TcpPcmBackend.h"
#include <iostream>
#include <cstring>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR_CODE WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    using socket_t = int;
    #define INVALID_SOCKET -1
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_CODE errno
#endif

namespace audioserver {

TcpPcmBackend::TcpPcmBackend() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

TcpPcmBackend::~TcpPcmBackend() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool TcpPcmBackend::startSender(const std::string& targetHost, uint16_t port, const StreamConfig& config) {
    if (running_) {
        return false;
    }

    targetHost_ = targetHost;
    port_ = port;
    streamConfig_ = config;
    running_ = true;
    state_ = TransportState::Connecting;

    workerThread_ = std::thread(&TcpPcmBackend::senderThread, this);
    keepaliveThread_ = std::thread(&TcpPcmBackend::keepaliveThread, this);

    return true;
}

bool TcpPcmBackend::startReceiver(uint16_t port, const StreamConfig& config) {
    if (running_) {
        return false;
    }

    port_ = port;
    streamConfig_ = config;
    running_ = true;
    state_ = TransportState::Connecting;

    // Create server socket
    serverSocket_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (serverSocket_ == INVALID_SOCKET) {
        errorMessage_ = "Failed to create socket";
        state_ = TransportState::Error;
        return false;
    }

    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(serverSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        errorMessage_ = "Failed to bind to port " + std::to_string(port);
        CLOSE_SOCKET(serverSocket_);
        serverSocket_ = -1;
        state_ = TransportState::Error;
        return false;
    }

    if (listen(serverSocket_, 1) < 0) {
        errorMessage_ = "Failed to listen";
        CLOSE_SOCKET(serverSocket_);
        serverSocket_ = -1;
        state_ = TransportState::Error;
        return false;
    }

    acceptThread_ = std::thread(&TcpPcmBackend::acceptThread, this);
    keepaliveThread_ = std::thread(&TcpPcmBackend::keepaliveThread, this);

    return true;
}

void TcpPcmBackend::stop() {
    running_ = false;
    cv_.notify_all();

    if (socket_ != -1) {
        CLOSE_SOCKET(socket_);
        socket_ = -1;
    }
    if (clientSocket_ != -1) {
        CLOSE_SOCKET(clientSocket_);
        clientSocket_ = -1;
    }
    if (serverSocket_ != -1) {
        CLOSE_SOCKET(serverSocket_);
        serverSocket_ = -1;
    }

    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }
    if (keepaliveThread_.joinable()) {
        keepaliveThread_.join();
    }

    state_ = TransportState::Disconnected;
}

bool TcpPcmBackend::sendAudio(const float* const* channelData, int numChannels, int numSamples) {
    if (state_ != TransportState::Streaming || socket_ == -1) {
        return false;
    }

    // Interleave audio data
    size_t totalSamples = static_cast<size_t>(numChannels * numSamples);
    interleavedBuffer_.resize(totalSamples);

    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < numChannels; ++ch) {
            interleavedBuffer_[static_cast<size_t>(i * numChannels + ch)] = channelData[ch][i];
        }
    }

    // Create chunk header
    ChunkHeader chunkHeader;
    chunkHeader.size = static_cast<uint32_t>(totalSamples * sizeof(float));
    chunkHeader.sequence = sequence_++;

    auto headerData = chunkHeader.serialize();

    // Send header and data
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendAll(socket_, headerData.data(), headerData.size())) {
        return false;
    }
    if (!sendAll(socket_, interleavedBuffer_.data(), chunkHeader.size)) {
        return false;
    }

    bytesSent_ += headerData.size() + chunkHeader.size;
    return true;
}

TransportStatus TcpPcmBackend::getStatus() const {
    TransportStatus status;
    status.state = state_;
    status.peerAddress = peerAddress_;
    status.peerPort = peerPort_;
    status.bytesSent = bytesSent_;
    status.bytesReceived = bytesReceived_;
    status.packetsLost = packetsLost_;
    status.errorMessage = errorMessage_;
    return status;
}

void TcpPcmBackend::setAudioReceivedCallback(AudioReceivedCallback callback) {
    audioCallback_ = std::move(callback);
}

void TcpPcmBackend::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback_ = std::move(callback);
}

void TcpPcmBackend::senderThread() {
    // Connect to receiver
    socket_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (socket_ == INVALID_SOCKET) {
        errorMessage_ = "Failed to create socket";
        state_ = TransportState::Error;
        return;
    }

    // Disable Nagle's algorithm for lower latency
    int flag = 1;
    setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);

    if (inet_pton(AF_INET, targetHost_.c_str(), &addr.sin_addr) <= 0) {
        errorMessage_ = "Invalid address: " + targetHost_;
        state_ = TransportState::Error;
        CLOSE_SOCKET(socket_);
        socket_ = -1;
        return;
    }

    if (connect(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        errorMessage_ = "Failed to connect to " + targetHost_ + ":" + std::to_string(port_);
        state_ = TransportState::Error;
        CLOSE_SOCKET(socket_);
        socket_ = -1;
        return;
    }

    peerAddress_ = targetHost_;
    peerPort_ = port_;
    state_ = TransportState::Connected;

    if (connectionCallback_) {
        connectionCallback_(true);
    }

    // Send stream header
    StreamHeader header = StreamHeader::fromConfig(streamConfig_);
    auto headerData = header.serialize();

    if (!sendAll(socket_, headerData.data(), headerData.size())) {
        errorMessage_ = "Failed to send stream header";
        state_ = TransportState::Error;
        return;
    }

    state_ = TransportState::Streaming;

    // Sender is passive - audio is sent via sendAudio()
    // Just wait until stopped
    while (running_ && state_ == TransportState::Streaming) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(100));
    }
}

void TcpPcmBackend::acceptThread() {
    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        clientSocket_ = static_cast<int>(accept(serverSocket_,
            reinterpret_cast<sockaddr*>(&clientAddr), &clientLen));

        if (clientSocket_ == INVALID_SOCKET) {
            if (running_) {
                errorMessage_ = "Accept failed";
            }
            continue;
        }

        // Disable Nagle's algorithm
        int flag = 1;
        setsockopt(clientSocket_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));

        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, INET_ADDRSTRLEN);
        peerAddress_ = addrStr;
        peerPort_ = ntohs(clientAddr.sin_port);

        state_ = TransportState::Connected;

        if (connectionCallback_) {
            connectionCallback_(true);
        }

        // Start receiver thread for this client
        workerThread_ = std::thread(&TcpPcmBackend::receiverThread, this);
        workerThread_.join();

        if (connectionCallback_) {
            connectionCallback_(false);
        }

        CLOSE_SOCKET(clientSocket_);
        clientSocket_ = -1;
        state_ = TransportState::Connecting;
    }
}

void TcpPcmBackend::receiverThread() {
    // Receive stream header
    std::vector<uint8_t> headerBuffer(STREAM_HEADER_SIZE);
    if (!receiveAll(clientSocket_, headerBuffer.data(), STREAM_HEADER_SIZE)) {
        errorMessage_ = "Failed to receive stream header";
        state_ = TransportState::Error;
        return;
    }

    StreamHeader header;
    if (!StreamHeader::deserialize(headerBuffer.data(), headerBuffer.size(), header)) {
        errorMessage_ = "Invalid stream header";
        state_ = TransportState::Error;
        return;
    }

    streamConfig_ = header.toConfig();
    state_ = TransportState::Streaming;

    // Receive audio chunks
    std::vector<uint8_t> chunkHeaderBuffer(CHUNK_HEADER_SIZE);
    std::vector<float> audioBuffer;
    uint32_t expectedSequence = 0;

    while (running_ && state_ == TransportState::Streaming) {
        if (!receiveAll(clientSocket_, chunkHeaderBuffer.data(), CHUNK_HEADER_SIZE)) {
            if (running_) {
                errorMessage_ = "Connection lost";
                state_ = TransportState::Disconnected;
            }
            break;
        }

        ChunkHeader chunkHeader;
        if (!ChunkHeader::deserialize(chunkHeaderBuffer.data(), chunkHeaderBuffer.size(), chunkHeader)) {
            continue;
        }

        // Handle keepalive packets (size = 0)
        if (chunkHeader.size == 0) {
            continue;
        }

        // Check for packet loss
        if (chunkHeader.sequence != expectedSequence) {
            packetsLost_ += chunkHeader.sequence - expectedSequence;
        }
        expectedSequence = chunkHeader.sequence + 1;

        // Receive audio data
        audioBuffer.resize(chunkHeader.size / sizeof(float));
        if (!receiveAll(clientSocket_, audioBuffer.data(), chunkHeader.size)) {
            if (running_) {
                errorMessage_ = "Failed to receive audio data";
                state_ = TransportState::Error;
            }
            break;
        }

        bytesReceived_ += CHUNK_HEADER_SIZE + chunkHeader.size;

        // Invoke callback with received audio
        if (audioCallback_) {
            int numSamples = static_cast<int>(audioBuffer.size()) / streamConfig_.channels;
            audioCallback_(audioBuffer.data(), streamConfig_.channels, numSamples);
        }
    }
}

void TcpPcmBackend::keepaliveThread() {
    ChunkHeader keepalive;
    keepalive.size = 0;  // Zero-size chunk = keepalive

    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(KEEPALIVE_INTERVAL_MS));

        if (state_ == TransportState::Streaming && socket_ != -1) {
            auto data = keepalive.serialize();
            std::lock_guard<std::mutex> lock(mutex_);
            sendAll(socket_, data.data(), data.size());
        }
    }
}

bool TcpPcmBackend::sendAll(int sock, const void* data, size_t size) {
    const auto* ptr = static_cast<const char*>(data);
    size_t remaining = size;

    while (remaining > 0) {
        auto sent = send(sock, ptr, static_cast<int>(remaining), 0);
        if (sent <= 0) {
            return false;
        }
        ptr += sent;
        remaining -= static_cast<size_t>(sent);
    }

    return true;
}

bool TcpPcmBackend::receiveAll(int sock, void* data, size_t size) {
    auto* ptr = static_cast<char*>(data);
    size_t remaining = size;

    while (remaining > 0) {
        auto received = recv(sock, ptr, static_cast<int>(remaining), 0);
        if (received <= 0) {
            return false;
        }
        ptr += received;
        remaining -= static_cast<size_t>(received);
    }

    return true;
}

} // namespace audioserver
