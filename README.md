# lite-streaming-client

A lightweight streaming media library written in C++17 **for learning purposes**.

The goal is to understand and implement streaming protocols (RTSP, RTMP, WebRTC, etc.) **from scratch without using FFmpeg**, making it easier to learn the underlying mechanisms of streaming media.

Inspired by [ZLMediaKit](https://github.com/ZLMediaKit/ZLMediaKit).

## Why This Project?

- **Learning-oriented**: Clean, readable code to understand how streaming protocols work
- **No FFmpeg dependency**: Implement protocols from scratch for deeper understanding
- **Minimal design**: Focus on core concepts without complex abstractions

## Features

### Currently Supported
- [x] RTSP/1.0 client (pull stream)
- [x] Digest authentication
- [x] RTP over TCP (interleaved mode)
- [x] RingBuffer with GOP caching

### Planned
- [ ] RTMP client
- [ ] RTMP server
- [ ] WebRTC support
- [ ] RTP over UDP
- [ ] H.264/H.265 depacketization
- [ ] Protocol conversion (RTSP to RTMP, etc.)

## Architecture

```
+---------------------------------------------------+
|              lite-streaming-client                |
+---------------------------------------------------+
|                                                   |
|  +-------------+  +-------------+  +------------+ |
|  | RtspClient  |  | RtmpClient  |  | WebRTC     | |
|  +------+------+  +------+------+  +-----+------+ |
|         |                |               |        |
|         v                v               v        |
|  +---------------------------------------------+  |
|  |              RingBuffer (GOP Cache)         |  |
|  +---------------------------------------------+  |
|                                                   |
+---------------------------------------------------+
```

## RTSP Flow

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
        std::cout << "Play: " << (ok ? "Success" : "Failed") << std::endl;
    });
    
    client->getRing()->setOnData([](const RtpPacket::Ptr& pkt) {
        std::cout << "RTP seq=" << pkt->seq << std::endl;
    });
    
    client->play("rtsp://admin:123456@192.168.1.64:554/stream");
    
    std::this_thread::sleep_for(std::chrono::seconds(30));
    client->shutdown();
    
    return 0;
}
```

## Project Structure

```
lite-streaming-client/
├── CMakeLists.txt
├── README.md
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
- OpenSSL (for MD5)
- POSIX sockets (Linux/macOS)

## References

- [ZLMediaKit](https://github.com/ZLMediaKit/ZLMediaKit)
- [RFC 2326 - RTSP](https://tools.ietf.org/html/rfc2326)
- [RFC 3550 - RTP](https://tools.ietf.org/html/rfc3550)
- [RFC 2617 - HTTP Authentication](https://tools.ietf.org/html/rfc2617)

## License

MIT License
