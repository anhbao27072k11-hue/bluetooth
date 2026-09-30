// language: C++, file: test_ble_decoder.cpp, target: Linux, GCC/Clang
// *Unit test — feed a captured BLE adv PDU bitstream, assert parser extracts MAC + name*
#include "ble_decoder.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace bst;

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_fail++; } } while (0)

// Build a BLE adv bitstream (LSB-first per byte) from a raw PDU + CRC.
static std::vector<uint8_t> build_adv_bits(const uint8_t* pdu, size_t len,
                                           uint8_t channel) {
    uint32_t crc = BleDecoder::crc24(pdu, len);
    uint8_t crc_bytes[3] = { static_cast<uint8_t>(crc & 0xFF),
                             static_cast<uint8_t>((crc >> 8) & 0xFF),
                             static_cast<uint8_t>((crc >> 16) & 0xFF) };

    std::vector<uint8_t> bytes;
    for (int i = 0; i < 4; i++) {
        bytes.push_back(static_cast<uint8_t>((kBleAdvAccessAddress >> (8 * i)) & 0xFF));
    }
    for (size_t i = 0; i < len; i++) bytes.push_back(BleDecoder::dewhiten_byte(pdu[i], channel));
    for (int i = 0; i < 3; i++) bytes.push_back(BleDecoder::dewhiten_byte(crc_bytes[i], channel));

    std::vector<uint8_t> bits;
    for (uint8_t b : bytes) {
        for (int i = 0; i < 8; i++) bits.push_back((b >> i) & 0x01);
    }
    return bits;
}

int main() {
    // ADV_IND (type 0), txAdd=0, rxAdd=0, length=6+8
    // advA = 0xAA:BB:CC:DD:EE:FF, AD = complete local name "Beacon"
    // AD structure: len=0x07 (1 type + 6 name bytes), type=0x09, "Beacon"
    uint8_t pdu[16] = { 0x00, 0x0E,
                        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
                        0x07, 0x09, 'B', 'e', 'a', 'c', 'o', 'n' };

    auto bits = build_adv_bits(pdu, sizeof(pdu), 37);
    BleAdv adv;
    BleDecoder dec;
    CHECK(dec.decode(bits, 37, -45, adv));
    CHECK(adv.mac == "AA:BB:CC:DD:EE:FF");
    CHECK(adv.name == "Beacon");
    CHECK(adv.adv_type == 0);
    CHECK(adv.channel == 37);
    CHECK(adv.rssi_dbm == -45);

    // Corrupted CRC: flip a bit in the last CRC byte of the bitstream.
    // The bitstream layout is AA(32) + PDU(16*8) + CRC(3*8). The last byte is
    // the MSB of the CRC. Flip its LSB and the decoder must reject the packet.
    std::vector<uint8_t> bad = bits;
    size_t crc_msb_bit = (4 + sizeof(pdu)) * 8 + 16; // last byte, bit 0
    bad[crc_msb_bit] ^= 0x01;
    BleAdv rejected;
    CHECK(!dec.decode(bad, 37, -45, rejected));

    if (g_fail == 0) { std::printf("test_ble_decoder: ALL PASS\n"); return 0; }
    std::printf("test_ble_decoder: %d FAILURES\n", g_fail);
    return 1;
}
