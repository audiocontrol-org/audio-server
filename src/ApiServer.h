#pragma once

#include "Config.h"
#include "AudioEngine.h"
#include "transport/TransportBackend.h"
#include <httplib.h>
#include <memory>
#include <thread>
#include <atomic>

namespace audioserver {

class ApiServer {
public:
    ApiServer(AudioEngine& audioEngine, TransportBackend& transport, Config& config);
    ~ApiServer();

    bool start(uint16_t port);
    void stop();

    bool isRunning() const { return running_; }

private:
    void setupRoutes();
    void addCorsHeaders(httplib::Response& res);

    // Handlers
    void handleStatus(const httplib::Request& req, httplib::Response& res);
    void handleDevices(const httplib::Request& req, httplib::Response& res);
    void handleStreamStart(const httplib::Request& req, httplib::Response& res);
    void handleStreamStop(const httplib::Request& req, httplib::Response& res);
    void handleTransports(const httplib::Request& req, httplib::Response& res);
    void handleTransportSwitch(const httplib::Request& req, httplib::Response& res);

    AudioEngine& audioEngine_;
    TransportBackend& transport_;
    Config& config_;

    std::unique_ptr<httplib::Server> server_;
    std::thread serverThread_;
    std::atomic<bool> running_{false};
};

} // namespace audioserver
