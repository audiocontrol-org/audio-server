#include "ApiServer.h"
#include "JsonBuilder.h"
#include <iostream>

namespace audioserver {

ApiServer::ApiServer(AudioEngine& audioEngine, TransportBackend& transport, Config& config)
    : audioEngine_(audioEngine)
    , transport_(transport)
    , config_(config)
    , server_(std::make_unique<httplib::Server>()) {
    setupRoutes();
}

ApiServer::~ApiServer() {
    stop();
}

bool ApiServer::start(uint16_t port) {
    if (running_) {
        return false;
    }

    running_ = true;
    serverThread_ = std::thread([this, port]() {
        if (!server_->listen("0.0.0.0", port)) {
            std::cerr << "Failed to start API server on port " << port << std::endl;
            running_ = false;
        }
    });

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return running_;
}

void ApiServer::stop() {
    if (running_) {
        running_ = false;
        server_->stop();
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }
}

void ApiServer::setupRoutes() {
    // Handle preflight requests
    server_->Options("/(.*)", [this](const httplib::Request&, httplib::Response& res) {
        addCorsHeaders(res);
        res.status = 204;
    });

    server_->Get("/status", [this](const httplib::Request& req, httplib::Response& res) {
        handleStatus(req, res);
    });

    server_->Get("/devices", [this](const httplib::Request& req, httplib::Response& res) {
        handleDevices(req, res);
    });

    server_->Post("/stream/start", [this](const httplib::Request& req, httplib::Response& res) {
        handleStreamStart(req, res);
    });

    server_->Post("/stream/stop", [this](const httplib::Request& req, httplib::Response& res) {
        handleStreamStop(req, res);
    });

    server_->Get("/transports", [this](const httplib::Request& req, httplib::Response& res) {
        handleTransports(req, res);
    });

    server_->Put("/transport", [this](const httplib::Request& req, httplib::Response& res) {
        handleTransportSwitch(req, res);
    });
}

void ApiServer::addCorsHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

void ApiServer::handleStatus(const httplib::Request&, httplib::Response& res) {
    auto transportStatus = transport_.getStatus();
    auto streamConfig = audioEngine_.getStreamConfig();

    std::string stateStr;
    switch (transportStatus.state) {
        case TransportState::Disconnected: stateStr = "disconnected"; break;
        case TransportState::Connecting: stateStr = "connecting"; break;
        case TransportState::Connected: stateStr = "connected"; break;
        case TransportState::Streaming: stateStr = "streaming"; break;
        case TransportState::Error: stateStr = "error"; break;
    }

    std::string modeStr = (config_.mode == Mode::Sender) ? "sender" : "receiver";

    JsonBuilder json;
    json.beginObject()
        .keyValue("mode", modeStr)
        .keyValue("state", stateStr)
        .keyValue("device", audioEngine_.getCurrentDeviceName())
        .key("stream").beginObject()
            .keyValue("sampleRate", streamConfig.sampleRate)
            .keyValue("channels", streamConfig.channels)
            .keyValue("bufferSize", streamConfig.bufferSize)
        .endObject()
        .key("transport").beginObject()
            .keyValue("name", transport_.getName())
            .keyValue("peerAddress", transportStatus.peerAddress)
            .keyValue("peerPort", transportStatus.peerPort)
            .keyValue("bytesSent", static_cast<uint32_t>(transportStatus.bytesSent))
            .keyValue("bytesReceived", static_cast<uint32_t>(transportStatus.bytesReceived))
            .keyValue("packetsLost", transportStatus.packetsLost)
        .endObject();

    if (!transportStatus.errorMessage.empty()) {
        json.keyValue("error", transportStatus.errorMessage);
    }

    json.endObject();

    addCorsHeaders(res);
    res.set_content(json.build(), "application/json");
}

void ApiServer::handleDevices(const httplib::Request&, httplib::Response& res) {
    auto inputs = audioEngine_.getInputDevices();
    auto outputs = audioEngine_.getOutputDevices();

    JsonBuilder json;
    json.beginObject()
        .key("inputs").beginArray();

    for (const auto& device : inputs) {
        json.beginObject()
            .keyValue("name", device.name)
            .keyValue("type", device.type)
            .keyValue("channels", device.numInputChannels)
        .endObject();
    }

    json.endArray()
        .key("outputs").beginArray();

    for (const auto& device : outputs) {
        json.beginObject()
            .keyValue("name", device.name)
            .keyValue("type", device.type)
            .keyValue("channels", device.numOutputChannels)
        .endObject();
    }

    json.endArray().endObject();

    addCorsHeaders(res);
    res.set_content(json.build(), "application/json");
}

void ApiServer::handleStreamStart(const httplib::Request&, httplib::Response& res) {
    auto status = transport_.getStatus();

    if (status.state == TransportState::Streaming) {
        JsonBuilder json;
        json.beginObject()
            .keyValue("success", false)
            .keyValue("error", "Stream already active")
        .endObject();

        addCorsHeaders(res);
        res.status = 400;
        res.set_content(json.build(), "application/json");
        return;
    }

    // For sender mode, we need a target
    if (config_.mode == Mode::Sender && config_.target.empty()) {
        JsonBuilder json;
        json.beginObject()
            .keyValue("success", false)
            .keyValue("error", "No target specified for sender mode")
        .endObject();

        addCorsHeaders(res);
        res.status = 400;
        res.set_content(json.build(), "application/json");
        return;
    }

    StreamConfig streamConfig;
    streamConfig.sampleRate = config_.sampleRate;
    streamConfig.channels = config_.channels;
    streamConfig.bufferSize = config_.bufferSize;

    bool success = false;
    if (config_.mode == Mode::Sender) {
        success = transport_.startSender(config_.target, config_.port, streamConfig);
    } else {
        success = transport_.startReceiver(config_.port, streamConfig);
    }

    JsonBuilder json;
    json.beginObject()
        .keyValue("success", success);

    if (!success) {
        json.keyValue("error", transport_.getStatus().errorMessage);
    }

    json.endObject();

    addCorsHeaders(res);
    res.status = success ? 200 : 500;
    res.set_content(json.build(), "application/json");
}

void ApiServer::handleStreamStop(const httplib::Request&, httplib::Response& res) {
    transport_.stop();

    JsonBuilder json;
    json.beginObject()
        .keyValue("success", true)
    .endObject();

    addCorsHeaders(res);
    res.set_content(json.build(), "application/json");
}

void ApiServer::handleTransports(const httplib::Request&, httplib::Response& res) {
    JsonBuilder json;
    json.beginObject()
        .key("transports").beginArray()
            .beginObject()
                .keyValue("name", "tcp-pcm")
                .keyValue("description", "TCP with raw PCM audio")
                .keyValue("active", true)
            .endObject()
        .endArray()
    .endObject();

    addCorsHeaders(res);
    res.set_content(json.build(), "application/json");
}

void ApiServer::handleTransportSwitch(const httplib::Request&, httplib::Response& res) {
    // Only tcp-pcm is currently supported
    JsonBuilder json;
    json.beginObject()
        .keyValue("success", false)
        .keyValue("error", "Only tcp-pcm transport is currently supported")
    .endObject();

    addCorsHeaders(res);
    res.status = 400;
    res.set_content(json.build(), "application/json");
}

} // namespace audioserver
