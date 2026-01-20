#pragma once
#include <string>
#include <functional>

class RtspSplitter {
public:
    void input(const char* data, size_t len) {
        _buffer.append(data, len);

        while (!_buffer.empty()) {
            if (_rtp_mode && _buffer[0] == '$') {
                // RTP interleaved: $channel(1) + len(2) + data
                if (_buffer.size() < 4) break;
                uint16_t pkt_len = (uint8_t)_buffer[1] << 8 | (uint8_t)_buffer[2];
                // 注意：ZLM格式是 $channel + len(2B bigendian)
                // 实际RTSP是 $channel(1) + len(2B bigendian)
                pkt_len = (uint8_t)_buffer[2] << 8 | (uint8_t)_buffer[3];
                if (_buffer.size() < 4 + pkt_len) break;

                uint8_t channel = _buffer[1];
                if (_on_rtp && channel % 2 == 0) {  // RTP（偶数通道）
                    _on_rtp(_buffer.data() + 4, pkt_len, channel / 2);
                }
                _buffer.erase(0, 4 + pkt_len);
            } else {
                // RTSP响应
                size_t pos = _buffer.find("\r\n\r\n");
                if (pos == std::string::npos) break;

                std::string response = _buffer.substr(0, pos + 4);
                _buffer.erase(0, pos + 4);

                // 处理Content-Length
                size_t cl_pos = response.find("Content-Length:");
                if (cl_pos != std::string::npos) {
                    int content_len = atoi(response.c_str() + cl_pos + 15);
                    if (_buffer.size() < (size_t)content_len) {
                        _buffer = response + _buffer;  // 放回去
                        break;
                    }
                    response += _buffer.substr(0, content_len);
                    _buffer.erase(0, content_len);
                }

                if (_on_response) _on_response(response);
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
    std::function<void(const char*, size_t, int)> _on_rtp;  // data, len, track_idx
};