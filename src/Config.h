#pragma once

#include <string>
#include <cstdint>

namespace audioserver {

enum class Mode {
    Sender,
    Receiver
};

enum class TransportType {
    TcpPcm
};

struct Config {
    Mode mode = Mode::Receiver;
    std::string device;
    std::string target;  // For sender: receiver address
    uint16_t port = 9876;
    uint16_t apiPort = 8080;
    uint32_t sampleRate = 48000;
    uint16_t channels = 2;
    uint32_t bufferSize = 512;
    TransportType transport = TransportType::TcpPcm;
    bool verbose = false;
    bool listDevices = false;
    bool showHelp = false;
    bool testTone = false;
    uint32_t testToneFrequency = 440;

    static Config fromArgs(int argc, char* argv[]);
    static void printUsage();
};

struct StreamConfig {
    uint32_t sampleRate = 48000;
    uint16_t channels = 2;
    uint16_t bitsPerSample = 32;  // float32
    uint32_t bufferSize = 512;
};

} // namespace audioserver
