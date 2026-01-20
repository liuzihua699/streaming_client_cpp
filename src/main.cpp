#include "rtsp/RtspClient.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " rtsp://url" << std::endl;
        return 1;
    }

    auto client = std::make_shared<RtspClient>();

    client->setOnPlayResult([](bool ok, const std::string& msg) {
        std::cout << "Play result: " << (ok ? "Success" : "Failed") << " - " << msg << std::endl;
    });

    client->getRing()->setOnData([](const RtpPacket::Ptr& pkt) {
        std::cout << "RTP: seq=" << pkt->seq
                  << " ts=" << pkt->timestamp
                  << " pt=" << (int)pkt->pt
                  << " size=" << pkt->payload.size()
                  << (pkt->isKeyFrame() ? " [KEY]" : "")
                  << std::endl;
    });

    client->play(argv[1]);

    // 运行30秒
    std::this_thread::sleep_for(std::chrono::seconds(30));
    client->shutdown();

    return 0;
}