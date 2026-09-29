// language: C++, file: ble_decoder.h, target: Linux, GCC/Clang
// *BLE advertisement decoder — AA correlation, dewhitening, PDU/AD parse, CRC24*
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bst {

// BLE advertising access address (all adv PDUs use this).
constexpr uint32_t kBleAdvAccessAddress = 0x8E89BED6;

// CRC24 polynomial and init for BLE.
constexpr uint32_t kBleCrcPoly = 0x00065B;
constexpr uint32_t kBleCrcInit = 0x555555;

// AD type codes.
constexpr uint8_t kAdFlags            = 0x01;
constexpr uint8_t kAdIncompleteUuid16 = 0x02;
constexpr uint8_t kAdCompleteUuid16   = 0x03;
constexpr uint8_t kAdIncompleteUuid32 = 0x04;
constexpr uint8_t kAdCompleteUuid32   = 0x05;
constexpr uint8_t kAdIncompleteUuid128= 0x06;
constexpr uint8_t kAdCompleteUuid128  = 0x07;
constexpr uint8_t kAdShortLocalName   = 0x08;
constexpr uint8_t kAdCompleteLocalName= 0x09;
constexpr uint8_t kAdTxPower          = 0x0A;
constexpr uint8_t kAdMfgSpecific      = 0xFF;

// A decoded BLE advertisement.
struct BleAdv {
    std::string mac;              // "AA:BB:CC:DD:EE:FF"
    std::string name;             // local name (complete or short)
    int8_t      rssi_dbm = 0;
    uint8_t     adv_type = 0;     // ADV_IND=0, ADV_DIRECT_IND=1, ...
    int8_t      tx_power = 0;     // dBm, from AD 0x0A
    bool        has_tx_power = false;
    std::vector<uint16_t> services; // 16-bit service UUIDs
    std::vector<uint8_t>  mfg_data; // manufacturer specific payload
    uint64_t    timestamp_ms = 0;
    uint8_t     channel = 0;      // 37/38/39
};

class BleDecoder {
public:
    // Decode a raw bitstream (already demodulated, LSB-first per byte) into a
    // BleAdv. Returns true if a valid packet was found and parsed.
    // channel: 37/38/39 for adv, or data channel index for data PDUs.
    bool decode(const std::vector<uint8_t>& bits, uint8_t channel,
                int8_t rssi, BleAdv& out);

    // --- exposed for testing ---

    // Correlate the bitstream against the access address; returns byte offset
    // of the AA match, or -1.
    static int find_access_address(const std::vector<uint8_t>& bits);

    // Dewhiten a byte using the BLE LFSR seeded from the channel index.
    static uint8_t dewhiten_byte(uint8_t b, uint8_t channel);

    // Verify CRC24 over header+payload. Returns true on match.
    static bool crc24_ok(const uint8_t* data, size_t len, const uint8_t crc[3]);

    // Parse AD structures from a payload; fills the BleAdv fields.
    static void parse_ad(const uint8_t* payload, size_t len, BleAdv& out);

    // Compute CRC24 (poly 0x00065B, init 0x555555) over data.
    static uint32_t crc24(const uint8_t* data, size_t len);
};

} // namespace bst
