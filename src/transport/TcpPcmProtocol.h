#pragma once

#include "../Config.h"
#include <cstdint>
#include <cstring>
#include <array>
#include <vector>

namespace audioserver {

// Wire protocol constants
constexpr std::array<char, 4> PROTOCOL_MAGIC = {'A', 'C', 'A', 'U'};
constexpr uint16_t PROTOCOL_VERSION = 1;
constexpr size_t STREAM_HEADER_SIZE = 20;
constexpr size_t CHUNK_HEADER_SIZE = 8;
constexpr uint16_t KEEPALIVE_INTERVAL_MS = 2000;
constexpr uint16_t DISCONNECT_TIMEOUT_MS = 5000;

// Stream header format (20 bytes):
// - Magic: 4 bytes "ACAU"
// - Version: 2 bytes
// - Sample rate: 4 bytes
// - Channels: 2 bytes
// - Bits per sample: 2 bytes
// - Buffer size: 4 bytes
// - Reserved: 2 bytes

struct StreamHeader {
    std::array<char, 4> magic = PROTOCOL_MAGIC;
    uint16_t version = PROTOCOL_VERSION;
    uint32_t sampleRate = 48000;
    uint16_t channels = 2;
    uint16_t bitsPerSample = 32;
    uint32_t bufferSize = 512;
    uint16_t reserved = 0;

    static StreamHeader fromConfig(const StreamConfig& config) {
        StreamHeader header;
        header.sampleRate = config.sampleRate;
        header.channels = config.channels;
        header.bitsPerSample = config.bitsPerSample;
        header.bufferSize = config.bufferSize;
        return header;
    }

    StreamConfig toConfig() const {
        StreamConfig config;
        config.sampleRate = sampleRate;
        config.channels = channels;
        config.bitsPerSample = bitsPerSample;
        config.bufferSize = bufferSize;
        return config;
    }

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> data(STREAM_HEADER_SIZE);
        size_t offset = 0;

        std::memcpy(data.data() + offset, magic.data(), 4);
        offset += 4;

        std::memcpy(data.data() + offset, &version, 2);
        offset += 2;

        std::memcpy(data.data() + offset, &sampleRate, 4);
        offset += 4;

        std::memcpy(data.data() + offset, &channels, 2);
        offset += 2;

        std::memcpy(data.data() + offset, &bitsPerSample, 2);
        offset += 2;

        std::memcpy(data.data() + offset, &bufferSize, 4);
        offset += 4;

        std::memcpy(data.data() + offset, &reserved, 2);

        return data;
    }

    static bool deserialize(const uint8_t* data, size_t size, StreamHeader& header) {
        if (size < STREAM_HEADER_SIZE) {
            return false;
        }

        size_t offset = 0;

        std::memcpy(header.magic.data(), data + offset, 4);
        offset += 4;

        if (header.magic != PROTOCOL_MAGIC) {
            return false;
        }

        std::memcpy(&header.version, data + offset, 2);
        offset += 2;

        std::memcpy(&header.sampleRate, data + offset, 4);
        offset += 4;

        std::memcpy(&header.channels, data + offset, 2);
        offset += 2;

        std::memcpy(&header.bitsPerSample, data + offset, 2);
        offset += 2;

        std::memcpy(&header.bufferSize, data + offset, 4);
        offset += 4;

        std::memcpy(&header.reserved, data + offset, 2);

        return true;
    }
};

// Chunk header format (8 bytes):
// - Size: 4 bytes (number of bytes of audio data)
// - Sequence: 4 bytes (monotonically increasing)

struct ChunkHeader {
    uint32_t size = 0;
    uint32_t sequence = 0;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> data(CHUNK_HEADER_SIZE);
        std::memcpy(data.data(), &size, 4);
        std::memcpy(data.data() + 4, &sequence, 4);
        return data;
    }

    static bool deserialize(const uint8_t* data, size_t dataSize, ChunkHeader& header) {
        if (dataSize < CHUNK_HEADER_SIZE) {
            return false;
        }
        std::memcpy(&header.size, data, 4);
        std::memcpy(&header.sequence, data + 4, 4);
        return true;
    }
};

} // namespace audioserver
