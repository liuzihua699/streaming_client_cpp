#pragma once
#include <string>
#include <functional>
#include <cstdint>

class RtspSplitter {
public:
    void input(const char* data, size_t len) {
        _buffer.append(data, len);

        while (!_buffer.empty()) {
            if (_rtp_mode && _buffer.size() >= 4 && _buffer[0] == '$') {
                // RTP interleaved packet: $<channel><len_h><len_l><data>
                int channel = (uint8_t)_buffer[1];
                size_t pkt_len = ((uint8_t)_buffer[2] << 8) | (uint8_t)_buffer[3];

                if (_buffer.size() < 4 + pkt_len) break;

                if (_on_rtp) {
                    _on_rtp(_buffer.data() + 4, pkt_len, channel);
                }
                _buffer.erase(0, 4 + pkt_len);
            } else {
                // RTSP response
                size_t header_end = _buffer.find("\r\n\r\n");
                if (header_end == std::string::npos) break;

                size_t content_len = 0;
                size_t pos = _buffer.find("Content-Length:");
                if (pos != std::string::npos && pos < header_end) {
                    content_len = std::stoul(_buffer.substr(pos + 15));
                }

                size_t total_len = header_end + 4 + content_len;
                if (_buffer.size() < total_len) break;

                if (_on_response) {
                    _on_response(_buffer.substr(0, total_len));
                }
                _buffer.erase(0, total_len);
            }
        }
    }

    void enableRtp(bool enable) { _rtp_mode = enable; }
    void setOnResponse(std::function<void(const std::string&)> cb) { _on_response = cb; }
    void setOnRtp(std::function<void(const char*, size_t, int)> cb) { _on_rtp = cb; }

private:
    std::string _buffer;
    bool _rtp_mode = false;
    std::function<void(const std::string&)> _on_response;
    std::function<void(const char*, size_t, int)> _on_rtp;
};
