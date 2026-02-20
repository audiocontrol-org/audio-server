#pragma once

#include <vector>
#include <atomic>
#include <cstring>

namespace audioserver {

template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : buffer_(capacity)
        , capacity_(capacity)
        , readPos_(0)
        , writePos_(0) {
    }

    size_t write(const T* data, size_t count) {
        size_t available = capacity_ - size();
        size_t toWrite = std::min(count, available);

        if (toWrite == 0) {
            return 0;
        }

        size_t writePos = writePos_.load(std::memory_order_relaxed);

        for (size_t i = 0; i < toWrite; ++i) {
            buffer_[(writePos + i) % capacity_] = data[i];
        }

        writePos_.store((writePos + toWrite) % capacity_, std::memory_order_release);
        return toWrite;
    }

    size_t read(T* data, size_t count) {
        size_t available = size();
        size_t toRead = std::min(count, available);

        if (toRead == 0) {
            return 0;
        }

        size_t readPos = readPos_.load(std::memory_order_relaxed);

        for (size_t i = 0; i < toRead; ++i) {
            data[i] = buffer_[(readPos + i) % capacity_];
        }

        readPos_.store((readPos + toRead) % capacity_, std::memory_order_release);
        return toRead;
    }

    size_t size() const {
        size_t write = writePos_.load(std::memory_order_acquire);
        size_t read = readPos_.load(std::memory_order_acquire);

        if (write >= read) {
            return write - read;
        }
        return capacity_ - read + write;
    }

    size_t available() const {
        return capacity_ - size() - 1;
    }

    void clear() {
        readPos_.store(0, std::memory_order_relaxed);
        writePos_.store(0, std::memory_order_relaxed);
    }

    size_t capacity() const {
        return capacity_;
    }

private:
    std::vector<T> buffer_;
    size_t capacity_;
    std::atomic<size_t> readPos_;
    std::atomic<size_t> writePos_;
};

} // namespace audioserver
