# audio-server MVP - Workplan

**GitHub Milestone:** [Week of Feb 17-21](https://github.com/audiocontrol-org/audio-server/milestone/1)
**GitHub Issues:**

- [Parent: MVP Audio Streaming Server (#1)](https://github.com/audiocontrol-org/audio-server/issues/1)
- [Set up CMake build system with JUCE (#2)](https://github.com/audiocontrol-org/audio-server/issues/2)
- [Create project skeleton with Config and JsonBuilder (#3)](https://github.com/audiocontrol-org/audio-server/issues/3)
- [Implement AudioEngine with JUCE device management (#4)](https://github.com/audiocontrol-org/audio-server/issues/4)
- [Implement TransportBackend interface and TcpPcmProtocol (#5)](https://github.com/audiocontrol-org/audio-server/issues/5)
- [Implement TcpPcmBackend for sender and receiver (#6)](https://github.com/audiocontrol-org/audio-server/issues/6)
- [Integrate AudioEngine with TcpPcmBackend (#7)](https://github.com/audiocontrol-org/audio-server/issues/7)
- [Implement HTTP API with cpp-httplib (#8)](https://github.com/audiocontrol-org/audio-server/issues/8)
- [Write README and add LICENSE (#9)](https://github.com/audiocontrol-org/audio-server/issues/9)

---

## Phase 1: Project Setup

### Tasks

1. **Set up CMake build system**
   - CMakeLists.txt with JUCE FetchContent
   - cpp-httplib vendored in deps/
   - Platform-specific linking (CoreAudio, ALSA, WinMM)

2. **Create project skeleton**
   - src/Main.cpp entry point
   - src/Config.h configuration struct
   - src/JsonBuilder.h JSON utility
   - .gitignore for C++/CMake

### Acceptance Criteria

- [ ] Project builds on macOS
- [ ] Empty binary runs and exits cleanly

---

## Phase 2: Audio Device Layer

### Tasks

1. **Implement AudioEngine**
   - JUCE AudioDeviceManager integration
   - Device enumeration (inputs in sender mode, outputs in receiver mode)
   - AudioIODeviceCallback for capture/playback

2. **Add --list-devices CLI option**
   - Print available devices and exit

### Acceptance Criteria

- [ ] `--list-devices` shows available audio devices
- [ ] AudioEngine can open a device and receive callbacks

---

## Phase 3: Transport Layer

### Tasks

1. **Define TransportBackend interface**
   - Abstract interface for transport implementations
   - StreamConfig and TransportStatus structs

2. **Implement TcpPcmProtocol**
   - Wire protocol constants (magic, header format)
   - Header serialization/deserialization
   - Chunk header handling

3. **Implement TcpPcmBackend**
   - Sender: connect to receiver, send header, stream chunks
   - Receiver: listen on port, parse header, invoke callback
   - Keepalive handling (2s interval)
   - Disconnect detection (5s timeout)

### Acceptance Criteria

- [ ] Sender can connect to receiver
- [ ] Header is sent and parsed correctly
- [ ] Audio chunks stream continuously

---

## Phase 4: End-to-End Streaming

### Tasks

1. **Integrate AudioEngine with TcpPcmBackend**
   - Sender: audio callback → sendAudio()
   - Receiver: transport callback → playback buffer
   - Ring buffer for jitter handling

2. **Implement CLI argument parsing**
   - --mode, --device, --target, --port
   - --sample-rate, --channels, --buffer-size
   - --api-port, --transport, --verbose

### Acceptance Criteria

- [ ] Audio streams between two terminals on localhost
- [ ] Audio streams between two machines on LAN
- [ ] Latency is under 20ms on wired LAN

---

## Phase 5: HTTP API

### Tasks

1. **Implement ApiServer**
   - cpp-httplib server on --api-port
   - CORS headers for browser access

2. **Implement endpoints**
   - GET /status - current state
   - GET /devices - list audio devices
   - POST /stream/start - start streaming
   - POST /stream/stop - stop streaming
   - GET /transports - list backends
   - PUT /transport - switch backend

3. **Runtime reconfiguration**
   - Change device without restart
   - Start/stop stream via API

### Acceptance Criteria

- [ ] curl can query /status and /devices
- [ ] curl can start and stop streaming
- [ ] Web editor can control audio-server

---

## Phase 6: Documentation and Polish

### Tasks

1. **Write README.md**
   - Overview and architecture
   - Build instructions
   - CLI reference
   - API reference

2. **Add LICENSE file**
   - MIT license

3. **Verification testing**
   - Test on macOS
   - Document Linux/Windows build requirements

### Acceptance Criteria

- [ ] README documents all features
- [ ] Build instructions are accurate
- [ ] Project is ready for use

---

## Dependencies

```
Phase 1 → Phase 2 → Phase 4
              ↓
          Phase 3 → Phase 4 → Phase 5 → Phase 6
```

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| JUCE audio device issues | Follow midi-server patterns exactly |
| TCP latency too high | Buffer tuning, consider UDP for future |
| Cross-platform build issues | Test on CI early |
