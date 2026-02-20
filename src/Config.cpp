#include "Config.h"
#include <iostream>
#include <cstring>
#include <stdexcept>

namespace audioserver {

Config Config::fromArgs(int argc, char* argv[]) {
    Config config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            config.showHelp = true;
        } else if (arg == "--list-devices") {
            config.listDevices = true;
        } else if (arg == "--verbose" || arg == "-v") {
            config.verbose = true;
        } else if (arg == "--mode" && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "sender") {
                config.mode = Mode::Sender;
            } else if (mode == "receiver") {
                config.mode = Mode::Receiver;
            } else {
                throw std::runtime_error("Invalid mode: " + mode);
            }
        } else if (arg == "--device" && i + 1 < argc) {
            config.device = argv[++i];
        } else if (arg == "--target" && i + 1 < argc) {
            config.target = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--api-port" && i + 1 < argc) {
            config.apiPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--sample-rate" && i + 1 < argc) {
            config.sampleRate = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (arg == "--channels" && i + 1 < argc) {
            config.channels = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--buffer-size" && i + 1 < argc) {
            config.bufferSize = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (arg == "--transport" && i + 1 < argc) {
            std::string transport = argv[++i];
            if (transport == "tcp-pcm") {
                config.transport = TransportType::TcpPcm;
            } else {
                throw std::runtime_error("Invalid transport: " + transport);
            }
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    return config;
}

void Config::printUsage() {
    std::cout << R"(audio-server - Network audio streaming server

USAGE:
    audio-server [OPTIONS]

OPTIONS:
    --mode <MODE>           Operating mode: sender or receiver (default: receiver)
    --device <NAME>         Audio device name (default: system default)
    --target <HOST>         Target receiver address (sender mode only)
    --port <PORT>           Streaming port (default: 9876)
    --api-port <PORT>       HTTP API port (default: 8080)
    --sample-rate <RATE>    Sample rate in Hz (default: 48000)
    --channels <N>          Number of channels (default: 2)
    --buffer-size <SIZE>    Buffer size in samples (default: 512)
    --transport <TYPE>      Transport backend: tcp-pcm (default: tcp-pcm)
    --list-devices          List available audio devices and exit
    --verbose, -v           Enable verbose logging
    --help, -h              Show this help message

EXAMPLES:
    # Start as receiver on default output device
    audio-server --mode receiver

    # Start as sender, stream to 192.168.1.100
    audio-server --mode sender --target 192.168.1.100

    # List available audio devices
    audio-server --list-devices
)";
}

} // namespace audioserver
