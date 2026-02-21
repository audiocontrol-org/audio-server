# audio-server MVP - Product Requirements Document

**Created:** 2026-02-20
**Status:** Approved
**Owner:** audiocontrol-org

## Problem Statement

Users editing vintage synthesizers and samplers via the audiocontrol web editors need to hear the audio output of their devices. Currently, this requires running physical audio cables from the device to their workstation, which is inconvenient when the device is across the room or in a different location.

The audio-server provides network audio streaming, allowing users to monitor device audio on their local workstation without physical cable runs.

## User Stories

- As a user editing a Roland S-330 via the web editor, I want to hear the sampler's audio output on my workstation speakers so that I can preview samples and patches in real-time.
- As a user with a studio setup, I want a headless audio streaming daemon so that I don't need to run a GUI application or maintain a display connection.
- As a developer, I want an HTTP API for stream control so that the web editor can integrate audio monitoring alongside MIDI editing.

## Success Criteria

- [ ] Two instances of audio-server can stream audio between machines on a LAN
- [ ] Latency is under 20ms on wired LAN
- [ ] Web editor can discover devices and control streaming via HTTP API
- [ ] No external dependencies beyond JUCE and cpp-httplib

## Scope

### In Scope

- Sender mode: capture audio from local device, stream to receiver
- Receiver mode: receive stream, play through local output device
- TCP/PCM transport for wired LAN
- HTTP API for status, device listing, and stream control
- CLI for all configuration options
- Cross-platform support (macOS, Linux, Windows)

### Out of Scope

- Browser-based audio playback (no WebRTC, no WebSocket audio)
- Multi-room synchronized playback
- Audio recording
- Compression or encoding (Opus, FLAC)
- Remote DSP processing
- WiFi-optimized transports (future phase)

## Dependencies

- JUCE 8.x for audio device access
- cpp-httplib for HTTP API
- CMake 3.22+ for build system

## Technical Constraints

- C++17 standard
- Single binary deployment (no runtime dependencies)
- Must follow midi-server patterns for consistency
- Wire protocol must be extensible for future transport backends

## Open Questions

None - requirements are complete.

## Appendix

### Wire Protocol Summary

**Stream Header (20 bytes):**
- Magic: "ACAU"
- Protocol version: 1
- Sample rate, channels, bits per sample, buffer size

**Audio Chunks:**
- 8-byte header (size + sequence number)
- Interleaved float32 samples

### API Endpoints

- `GET /status` - Server state
- `GET /devices` - Available audio devices
- `POST /stream/start` - Start streaming
- `POST /stream/stop` - Stop streaming
- `GET /transports` - Available transport backends
- `PUT /transport` - Switch transport backend
