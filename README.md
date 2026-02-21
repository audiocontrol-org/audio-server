# audio-server

Headless network audio streaming server for the audiocontrol ecosystem. Enables monitoring of vintage synthesizer and sampler audio over a LAN.

## Features

- **Sender mode**: Capture audio from local input device, stream to receiver
- **Receiver mode**: Receive stream, play through local output device
- **TCP/PCM transport**: Low-latency raw PCM streaming over TCP
- **HTTP API**: RESTful control for integration with web editors
- **Cross-platform**: macOS, Linux, Windows

## Building

### Requirements

- CMake 3.22+
- C++17 compiler
- Platform audio SDK (CoreAudio on macOS, ALSA on Linux, WinMM on Windows)

### Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j4
```

The binary will be at `build/audio-server`.

## Usage

### List Available Devices

```bash
audio-server --list-devices
```

### Start as Receiver

```bash
# Use default output device
audio-server --mode receiver

# Specify device
audio-server --mode receiver --device "MacBook Pro Speakers"
```

### Start as Sender

```bash
# Stream to receiver at 192.168.1.100
audio-server --mode sender --target 192.168.1.100

# Specify input device
audio-server --mode sender --target 192.168.1.100 --device "USB Audio Interface"
```

### CLI Options

| Option | Description | Default |
|--------|-------------|---------|
| `--mode <MODE>` | Operating mode: `sender` or `receiver` | `receiver` |
| `--device <NAME>` | Audio device name | System default |
| `--target <HOST>` | Receiver address (sender mode) | - |
| `--port <PORT>` | Streaming port | `9876` |
| `--api-port <PORT>` | HTTP API port | `8080` |
| `--sample-rate <RATE>` | Sample rate in Hz | `48000` |
| `--channels <N>` | Number of channels | `2` |
| `--buffer-size <SIZE>` | Buffer size in samples | `512` |
| `--transport <TYPE>` | Transport backend | `tcp-pcm` |
| `--list-devices` | List audio devices and exit | - |
| `--verbose, -v` | Enable verbose logging | - |
| `--help, -h` | Show help | - |

## HTTP API

The server exposes an HTTP API for control and monitoring.

### GET /status

Returns current server state.

```json
{
  "mode": "receiver",
  "state": "streaming",
  "device": "MacBook Pro Speakers",
  "stream": {
    "sampleRate": 48000,
    "channels": 2,
    "bufferSize": 512
  },
  "transport": {
    "name": "tcp-pcm",
    "peerAddress": "192.168.1.50",
    "peerPort": 54321,
    "bytesSent": 0,
    "bytesReceived": 1048576,
    "packetsLost": 0
  }
}
```

### GET /devices

Lists available audio devices.

```json
{
  "inputs": [
    {"name": "USB Audio Interface", "type": "CoreAudio", "channels": 2}
  ],
  "outputs": [
    {"name": "MacBook Pro Speakers", "type": "CoreAudio", "channels": 2}
  ]
}
```

### POST /stream/start

Start streaming.

```json
{"success": true}
```

### POST /stream/stop

Stop streaming.

```json
{"success": true}
```

### GET /transports

List available transport backends.

```json
{
  "transports": [
    {"name": "tcp-pcm", "description": "TCP with raw PCM audio", "active": true}
  ]
}
```

## Wire Protocol

### Stream Header (20 bytes)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Magic: "ACAU" |
| 4 | 2 | Protocol version (1) |
| 6 | 4 | Sample rate |
| 10 | 2 | Channels |
| 12 | 2 | Bits per sample (32) |
| 14 | 4 | Buffer size |
| 18 | 2 | Reserved |

### Audio Chunk Header (8 bytes)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Chunk size in bytes |
| 4 | 4 | Sequence number |

Audio data follows as interleaved float32 samples.

### Keepalive

Zero-size chunks (size=0) are sent every 2 seconds as keepalives.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        audio-server                         │
├─────────────────────────────────────────────────────────────┤
│  Main.cpp                                                   │
│  - CLI argument parsing                                     │
│  - Component wiring                                         │
│  - Signal handling                                          │
├─────────────────────────────────────────────────────────────┤
│  AudioEngine              │  ApiServer                      │
│  - JUCE device management │  - cpp-httplib server           │
│  - Capture/playback       │  - REST endpoints               │
│  - Device enumeration     │  - CORS support                 │
├───────────────────────────┴─────────────────────────────────┤
│  TransportBackend (interface)                               │
│  └── TcpPcmBackend                                          │
│      - TCP socket management                                │
│      - Protocol serialization                               │
│      - Keepalive handling                                   │
├─────────────────────────────────────────────────────────────┤
│  RingBuffer               │  JsonBuilder                    │
│  - Lock-free jitter buffer│  - JSON serialization           │
└─────────────────────────────────────────────────────────────┘
```

## License

MIT License - see [LICENSE](LICENSE) for details.
