# lite-streaming-client

A lightweight, minimal RTSP client library written in C++17. Designed for learning and embedding into larger projects.

Inspired by [ZLMediaKit](https://github.com/ZLMediaKit/ZLMediaKit).

## Features

- RTSP/1.0 protocol implementation
- Digest authentication support
- RTP over TCP (interleaved mode)
- RingBuffer with GOP caching
- Single-header style, easy to integrate
- No external dependencies (except OpenSSL for MD5)

## Architecture

```
+---------------------------------------------------+
|                   RtspClient                      |
+---------------------------------------------------+
|  TcpClient (async TCP connection)                 |
|       |                                           |
|       v                                           |
|  RtspSplitter (separate RTSP response / RTP)      |
|       |                                           |
|       v                                           |
|  RtpPacket (parse RTP header)                     |
|       |                                           |
|       v                                           |
|  RingBuffer (GOP-based frame caching)             |
+---------------------------------------------------+
```

## RTSP Handshake Flow

```
Client                          Server
   |                               |
   |--- OPTIONS ------------------>|
   |<-- 401 Unauthorized ----------|
   |--- OPTIONS + Authorization -->|
   |<-- 200 OK --------------------|
   |                               |
   |--- DESCRIBE + Auth ---------->|
   |<-- 200 OK + SDP --------------|
   |                               |
   |--- SETUP + Auth ------------->|
   |<-- 200 OK + Session ----------|
   |                               |
   |--- PLAY + Session ----------->|
   |<-- 200 OK --------------------|
   |                               |
   |<== RTP Packets ===============|
   |              ...              |
```

## Quick Start

### Build

```bash
mkdir build && cd build
cmake ..
make
```

### Run

```bash
./simple_player rtsp://user:password@192.168.1.100:554/stream
```

### Example Code

```cpp
#include "rtsp/RtspClient.h"

int main() {
    auto client = std::make_shared<RtspClient>();
    
    client->setOnPlayResult([](bool ok, const std::string& msg) {
        std::cout << "Play: " << (ok ? "Success" : "Failed") << " - " << msg << std::endl;
    });
    
    client->getRing()->setOnData([](const RtpPacket::Ptr& pkt) {
        std::cout << "RTP seq=" << pkt->seq << " size=" << pkt->payload.size() << std::endl;
    });
    
    client->play("rtsp://admin:123456@192.168.1.64:554/stream");
    
    std::this_thread::sleep_for(std::chrono::seconds(30));
    client->shutdown();
    
    return 0;
}
```

## Project Structure

```
lite-rtsp-client/
├── CMakeLists.txt
├── README.md
├── LICENSE
└── src/
    ├── main.cpp
    ├── network/
    │   └── TcpClient.h
    ├── rtsp/
    │   ├── RtspClient.h
    │   ├── RtspSplitter.h
    │   └── RtpPacket.h
    └── util/
        └── RingBuffer.h
```

## Dependencies

- C++17
- OpenSSL (for MD5 in Digest authentication)
- POSIX sockets (Linux/macOS)

## Roadmap

- [ ] RTMP client support
- [ ] RTP over UDP
- [ ] RTCP handling
- [ ] H.264 depacketization (RTP to NAL units)
- [ ] Cross-platform (Windows support)

## References

- [ZLMediaKit](https://github.com/ZLMediaKit/ZLMediaKit) - Inspiration source
- [RFC 2326](https://tools.ietf.org/html/rfc2326) - RTSP Protocol
- [RFC 3550](https://tools.ietf.org/html/rfc3550) - RTP Protocol
- [RFC 2617](https://tools.ietf.org/html/rfc2617) - HTTP Digest Authentication

## License

MIT License
