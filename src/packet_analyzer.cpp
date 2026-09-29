// language: C++, file: packet_analyzer.cpp, target: Linux, GCC/Clang
#include "packet_analyzer.h"

#include <cstdio>
#include <cstring>

namespace bst {

BlePduHeader PacketAnalyzer::parse_ble_header(const uint8_t* pdu) {
    BlePduHeader h{};
    uint16_t raw = static_cast<uint16_t>(pdu[0]) |
                   (static_cast<uint16_t>(pdu[1]) << 8);
    h.pdu_type = raw & 0x0F;
    h.tx_add   = (raw >> 6) & 0x01;
    h.rx_add   = (raw >> 7) & 0x01;
    h.length   = (raw >> 8) & 0xFF;
    return h;
}

std::vector<BleAdElement> PacketAnalyzer::parse_ble_ad(const uint8_t* payload, size_t len) {
    std::vector<BleAdElement> ads;
    size_t off = 0;
    while (off + 1 < len) {
        uint8_t l = payload[off];
        if (l == 0) break; // end marker
        if (off + 1 + l > len) break; // truncated
        BleAdElement e;
        e.length = l;
        e.type   = payload[off + 1];
        e.data.assign(payload + off + 2, payload + off + 1 + l);
        ads.push_back(std::move(e));
        off += 1 + l;
    }
    return ads;
}

std::string PacketAnalyzer::extract_name(const std::vector<BleAdElement>& ads) {
    for (const auto& e : ads) {
        // 0x09 = complete local name, 0x08 = shortened local name
        if (e.type == 0x09 || e.type == 0x08) {
            return std::string(reinterpret_cast<const char*>(e.data.data()), e.data.size());
        }
    }
    return {};
}

std::string PacketAnalyzer::format_mac(const uint8_t* mac6) {
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac6[0], mac6[1], mac6[2], mac6[3], mac6[4], mac6[5]);
    return std::string(buf);
}

void PacketAnalyzer::parse_acl_header(const uint8_t* hdr, uint16_t& handle,
                                      uint8_t& flags, uint16_t& length) {
    uint16_t raw = static_cast<uint16_t>(hdr[0]) |
                   (static_cast<uint16_t>(hdr[1]) << 8);
    handle = raw & 0x0FFF;
    flags  = (raw >> 12) & 0x03;
    length = static_cast<uint16_t>(hdr[2]) |
             (static_cast<uint16_t>(hdr[3]) << 8);
}

} // namespace bst
