// language: C++, file: packet_analyzer.h, target: Linux, GCC/Clang
// *Packet analysis — parse BLE PDU and Classic ACL frames*
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bst {

// BLE advertising PDU header (16 bits)
struct BlePduHeader {
    uint8_t pdu_type;   // bits 0..3
    uint8_t tx_add;     // bit 6
    uint8_t rx_add;     // bit 7
    uint8_t length;     // payload length (bits 8..15)
};

// Parsed BLE advertising data element (AD structure)
struct BleAdElement {
    uint8_t  length;
    uint8_t  type;
    std::vector<uint8_t> data;
};

class PacketAnalyzer {
public:
    // Parse the 2-byte BLE PDU header.
    static BlePduHeader parse_ble_header(const uint8_t* pdu);

    // Split advertising payload into AD structures.
    static std::vector<BleAdElement> parse_ble_ad(const uint8_t* payload, size_t len);

    // Extract the local name from AD structures (type 0x09 / 0x08).
    static std::string extract_name(const std::vector<BleAdElement>& ads);

    // Format a MAC from 6 raw bytes (little-endian on air).
    static std::string format_mac(const uint8_t* mac6);

    // Classic ACL header (16 bits): handle, flags, length.
    static void parse_acl_header(const uint8_t* hdr, uint16_t& handle,
                                 uint8_t& flags, uint16_t& length);
};

} // namespace bst
