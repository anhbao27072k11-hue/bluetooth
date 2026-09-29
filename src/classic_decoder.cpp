// language: C++, file: classic_decoder.cpp, target: Linux, GCC/Clang
#include "classic_decoder.h"
#include "utils.h"

#include <cstring>

namespace bst {

uint8_t ClassicDev::major_class(uint32_t cod) {
    return static_cast<uint8_t>((cod >> 8) & 0x1F);
}

uint8_t ClassicDev::minor_class(uint32_t cod) {
    return static_cast<uint8_t>((cod >> 2) & 0x3F);
}

const char* ClassicDev::major_class_name(uint8_t major) {
    switch (major) {
        case 0x00: return "Miscellaneous";
        case 0x01: return "Computer";
        case 0x02: return "Phone";
        case 0x03: return "LAN/Network Access Point";
        case 0x04: return "Audio/Video";
        case 0x05: return "Peripheral";
        case 0x06: return "Imaging";
        case 0x07: return "Wearable";
        case 0x08: return "Toy";
        case 0x09: return "Health";
        case 0x1F: return "Uncategorized";
        default:   return "Unknown";
    }
}

// FHS packet layout (after the 18-bit access code + 54-bit header):
//   bits 0..23   : LAP (lower 24 bits of BD_ADDR)
//   bits 24..31  : reserved
//   bits 32..55  : UAP (8 bits) + NAP (16 bits) -> upper 24 bits of BD_ADDR
//   bits 56..79  : Class of Device (24 bits)
//   bits 80..95  : clock offset (16 bits)
// The payload passed here is the FHS body starting at the LAP field.
bool ClassicDecoder::parse_fhs(const uint8_t* payload, size_t len, int8_t rssi,
                               ClassicDev& out) {
    if (len < 12) return false; // need 96 bits = 12 bytes

    // BD_ADDR: LAP (bytes 0..2), UAP (byte 3), NAP (bytes 4..5).
    // On air, LAP is transmitted LSB-first; we read the raw bytes.
    uint8_t bd[6];
    bd[0] = payload[0];
    bd[1] = payload[1];
    bd[2] = payload[2];
    bd[3] = payload[3]; // UAP
    bd[4] = payload[4]; // NAP low
    bd[5] = payload[5]; // NAP high
    out.bd_addr = format_mac(bd);

    // Class of Device: bytes 7..9 (24 bits).
    out.class_of_device = static_cast<uint32_t>(payload[7]) |
                          (static_cast<uint32_t>(payload[8]) << 8) |
                          (static_cast<uint32_t>(payload[9]) << 16);

    // Clock offset: bytes 10..11 (16 bits).
    out.clock_offset = static_cast<uint16_t>(payload[10]) |
                       (static_cast<uint16_t>(payload[11]) << 8);

    out.rssi_dbm = rssi;
    out.timestamp_ms = static_cast<uint64_t>(now_seconds() * 1000.0);
    return true;
}

} // namespace bst
