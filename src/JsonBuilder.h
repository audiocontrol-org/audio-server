#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <utility>

namespace audioserver {

class JsonBuilder {
public:
    JsonBuilder& beginObject() {
        append("{");
        depth_++;
        needsComma_.push_back(false);
        return *this;
    }

    JsonBuilder& endObject() {
        depth_--;
        needsComma_.pop_back();
        append("}");
        markNeedsComma();
        return *this;
    }

    JsonBuilder& beginArray() {
        append("[");
        depth_++;
        needsComma_.push_back(false);
        return *this;
    }

    JsonBuilder& endArray() {
        depth_--;
        needsComma_.pop_back();
        append("]");
        markNeedsComma();
        return *this;
    }

    JsonBuilder& key(const std::string& k) {
        maybeComma();
        append("\"" + escape(k) + "\":");
        return *this;
    }

    JsonBuilder& value(const std::string& v) {
        maybeComma();
        append("\"" + escape(v) + "\"");
        markNeedsComma();
        return *this;
    }

    JsonBuilder& value(const char* v) {
        return value(std::string(v));
    }

    JsonBuilder& value(int v) {
        maybeComma();
        append(std::to_string(v));
        markNeedsComma();
        return *this;
    }

    JsonBuilder& value(uint32_t v) {
        maybeComma();
        append(std::to_string(v));
        markNeedsComma();
        return *this;
    }

    JsonBuilder& value(uint16_t v) {
        maybeComma();
        append(std::to_string(v));
        markNeedsComma();
        return *this;
    }

    JsonBuilder& value(bool v) {
        maybeComma();
        append(v ? "true" : "false");
        markNeedsComma();
        return *this;
    }

    JsonBuilder& value(double v) {
        maybeComma();
        append(std::to_string(v));
        markNeedsComma();
        return *this;
    }

    JsonBuilder& keyValue(const std::string& k, const std::string& v) {
        key(k);
        append("\"" + escape(v) + "\"");
        markNeedsComma();
        return *this;
    }

    JsonBuilder& keyValue(const std::string& k, const char* v) {
        return keyValue(k, std::string(v));
    }

    JsonBuilder& keyValue(const std::string& k, int v) {
        key(k);
        append(std::to_string(v));
        markNeedsComma();
        return *this;
    }

    JsonBuilder& keyValue(const std::string& k, uint32_t v) {
        key(k);
        append(std::to_string(v));
        markNeedsComma();
        return *this;
    }

    JsonBuilder& keyValue(const std::string& k, uint16_t v) {
        key(k);
        append(std::to_string(v));
        markNeedsComma();
        return *this;
    }

    JsonBuilder& keyValue(const std::string& k, bool v) {
        key(k);
        append(v ? "true" : "false");
        markNeedsComma();
        return *this;
    }

    JsonBuilder& keyValue(const std::string& k, double v) {
        key(k);
        append(std::to_string(v));
        markNeedsComma();
        return *this;
    }

    std::string build() const {
        return ss_.str();
    }

private:
    void append(const std::string& s) {
        ss_ << s;
    }

    void maybeComma() {
        if (!needsComma_.empty() && needsComma_.back()) {
            append(",");
            needsComma_.back() = false;
        }
    }

    void markNeedsComma() {
        if (!needsComma_.empty()) {
            needsComma_.back() = true;
        }
    }

    std::string escape(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }

    std::stringstream ss_;
    int depth_ = 0;
    std::vector<bool> needsComma_;
};

} // namespace audioserver
