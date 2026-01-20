#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <cstring>

struct RtpPacket {
    using Ptr = std::shared_ptr<RtpPacket>;

    uint8_t  version = 2;
    uint8_t  padding = 0;
    uint8_t  extension = 0;
    uint8_t  csrc_count = 0;
    uint8_t  marker = 0;
    uint8_t  pt = 0;         // Payload Type
    uint16_t seq = 0;        // Sequence Number
    uint32_t timestamp = 0;  // Timestamp
    uint32_t ssrc = 0;       // SSRC
    std::string payload;     // RTP Payload

    static Ptr parse(const char* data, size_t len) {
        if (len < 12) return nullptr;

        auto pkt = std::make_shared<RtpPacket>();
        const uint8_t* p = (const uint8_t*)data;

        pkt->version    = (p[0] >> 6) & 0x03;
        pkt->padding    = (p[0] >> 5) & 0x01;
        pkt->extension  = (p[0] >> 4) & 0x01;
        pkt->csrc_count = p[0] & 0x0F;
        pkt->marker     = (p[1] >> 7) & 0x01;
        pkt->pt         = p[1] & 0x7F;

        pkt->seq = (p[2] << 8) | p[3];
        pkt->timestamp = (p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];
        pkt->ssrc = (p[8] << 24) | (p[9] << 16) | (p[10] << 8) | p[11];

        size_t header_len = 12 + pkt->csrc_count * 4;

        // 处理扩展头
        if (pkt->extension && len > header_len + 4) {
            uint16_t ext_len = (p[header_len + 2] << 8) | p[header_len + 3];
            header_len += 4 + ext_len * 4;
        }

        if (len > header_len) {
            pkt->payload.assign(data + header_len, len - header_len);
        }

        return pkt;
    }

    // 检测是否为H264 IDR帧
    bool isKeyFrame() const {
        if (payload.size() < 2) return false;
        uint8_t nalu_type = (uint8_t)payload[0] & 0x1F;

        // 单个NALU - IDR帧 (type 5)
        if (nalu_type == 5) return true;

        // FU-A分片
        if (nalu_type == 28) {
            uint8_t fu_header = (uint8_t)payload[1];
            bool start_bit = (fu_header & 0x80) != 0;
            uint8_t fu_type = fu_header & 0x1F;
            return start_bit && fu_type == 5;
        }

        return false;
    }
};
