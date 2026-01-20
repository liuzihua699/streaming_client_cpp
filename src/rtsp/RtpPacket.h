#pragma once
#include <memory>
#include <string>
#include <cstring>

struct RtpPacket {
    using Ptr = std::shared_ptr<RtpPacket>;

    uint8_t  version = 2;
    uint8_t  padding = 0;
    uint8_t  extension = 0;
    uint8_t  csrc_count = 0;
    uint8_t  marker = 0;
    uint8_t  pt = 0;           // payload type
    uint16_t seq = 0;          // 序列号
    uint32_t timestamp = 0;    // 时间戳
    uint32_t ssrc = 0;
    std::string payload;       // RTP payload

    // 从原始数据解析
    static Ptr parse(const char* data, size_t len) {
        if (len < 12) return nullptr;

        auto pkt = std::make_shared<RtpPacket>();
        pkt->version    = (data[0] >> 6) & 0x03;
        pkt->padding    = (data[0] >> 5) & 0x01;
        pkt->extension  = (data[0] >> 4) & 0x01;
        pkt->csrc_count = data[0] & 0x0F;
        pkt->marker     = (data[1] >> 7) & 0x01;
        pkt->pt         = data[1] & 0x7F;
        pkt->seq        = (uint8_t)data[2] << 8 | (uint8_t)data[3];
        pkt->timestamp  = (uint8_t)data[4] << 24 | (uint8_t)data[5] << 16 |
                          (uint8_t)data[6] << 8  | (uint8_t)data[7];
        pkt->ssrc       = (uint8_t)data[8] << 24 | (uint8_t)data[9] << 16 |
                          (uint8_t)data[10] << 8 | (uint8_t)data[11];

        size_t header_len = 12 + pkt->csrc_count * 4;
        if (len > header_len) {
            pkt->payload.assign(data + header_len, len - header_len);
        }
        return pkt;
    }

    // 判断是否视频关键帧（H264 IDR）
    bool isKeyFrame() const {
        if (payload.size() < 2) return false;

        uint8_t nalu_type = (uint8_t)payload[0] & 0x1F;

        if (nalu_type == 5) {
            return true;  // 单个IDR帧
        } else if (nalu_type == 28) {
            // FU-A分片：只有起始分片且是IDR才是关键帧
            uint8_t fu_header = (uint8_t)payload[1];
            bool start_bit = (fu_header & 0x80) != 0;
            uint8_t fu_type = fu_header & 0x1F;
            return start_bit && fu_type == 5;
        }
        return false;
    }
};