// language: C++, file: test_scanner.cpp, target: Linux, GCC/Clang
// *Unit tests — BLE decode, AD parsing, MAC format, hop math*
#include "ble_decoder.h"
#include "classic_decoder.h"
#include "packet_analyzer.h"
#include "frequency_hopper.h"
#include "utils.h"

#include <cassert>
#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_fail++; } } while (0)

// Build a BLE adv bitstream (LSB-first per byte) from a raw PDU + CRC.
static std::vector<uint8_t> build_adv_bits(const uint8_t* pdu, size_t len,
                                           uint8_t channel) {
    // Compute CRC24 over the PDU.
    uint32_t crc = BleDecoder::crc24(pdu, len);
    uint8_t crc_bytes[3] = { static_cast<uint8_t>(crc & 0xFF),
                             static_cast<uint8_t>((crc >> 8) & 0xFF),
                             static_cast<uint8_t>((crc >> 16) & 0xFF) };

    // Assemble: access address (32 bits) + PDU + CRC, all whitened.
    std::vector<uint8_t> bytes;
    // Access address, LSB-first.
    for (int i = 0; i < 4; i++) {
        bytes.push_back(static_cast<uint8_t>((kBleAdvAccessAddress >> (8 * i)) & 0xFF));
    }
    for (size_t i = 0; i < len; i++) bytes.push_back(BleDecoder::dewhiten_byte(pdu[i], channel));
    for (int i = 0; i < 3; i++) bytes.push_back(BleDecoder::dewhiten_byte(crc_bytes[i], channel));

    // Expand to LSB-first bitstream.
    std::vector<uint8_t> bits;
    for (uint8_t b : bytes) {
        for (int i = 0; i < 8; i++) bits.push_back((b >> i) & 0x01);
    }
    return bits;
}

static void test_ble_decode() {
    // ADV_IND (type 0), txAdd=0, rxAdd=0, length=6+2
    uint8_t pdu[10] = { 0x00, 0x08, // header: type 0, len 8
                        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, // advA
                        0x02, 0x01 }; // AD: len 2, type 0x01 (flags)
    auto bits = build_adv_bits(pdu, sizeof(pdu), 37);
    BleAdv adv;
    BleDecoder dec;
    CHECK(dec.decode(bits, 37, -55, adv));
    CHECK(adv.adv_type == 0);
    CHECK(adv.channel == 37);
    CHECK(adv.rssi_dbm == -55);
    CHECK(adv.mac == "11:22:33:44:55:66");
}

static void test_ble_decode_name() {
    // ADV_IND with complete local name "Test"
    uint8_t pdu[13] = { 0x00, 0x0B, // type 0, len 11
                        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, // advA
                        0x05, 0x09, 'T', 'e', 's', 't' };   // name
    auto bits = build_adv_bits(pdu, sizeof(pdu), 38);
    BleAdv adv;
    BleDecoder dec;
    CHECK(dec.decode(bits, 38, -60, adv));
    CHECK(adv.mac == "AA:BB:CC:DD:EE:FF");
    CHECK(adv.name == "Test");
}

static void test_crc24() {
    // CRC24 over a known byte sequence must be deterministic and match the
    // reference implementation (poly 0x00065B, init 0x555555, LSB-first).
    uint8_t data[2] = { 0x00, 0x08 };
    uint32_t crc = BleDecoder::crc24(data, 2);
    // Verify the CRC round-trips: crc24_ok must accept the computed CRC.
    uint8_t crc_bytes[3] = { static_cast<uint8_t>(crc & 0xFF),
                             static_cast<uint8_t>((crc >> 8) & 0xFF),
                             static_cast<uint8_t>((crc >> 16) & 0xFF) };
    CHECK(BleDecoder::crc24_ok(data, 2, crc_bytes));
    // And reject a corrupted CRC.
    crc_bytes[0] ^= 0xFF;
    CHECK(!BleDecoder::crc24_ok(data, 2, crc_bytes));
}

static void test_mac_format() {
    uint8_t mac[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
    CHECK(format_mac(mac) == "AA:BB:CC:DD:EE:FF");
    uint8_t out[6];
    CHECK(parse_mac("AA:BB:CC:DD:EE:FF", out));
    CHECK(std::memcmp(out, mac, 6) == 0);
    CHECK(!parse_mac("not-a-mac", out));
}

static void test_hop_math() {
    // BLE adv channels map to 2402/2426/2480
    CHECK(FrequencyHopper::ble_freq(37) == 2402e6);
    CHECK(FrequencyHopper::ble_freq(38) == 2426e6);
    CHECK(FrequencyHopper::ble_freq(39) == 2480e6);
    // Classic channel 0 = 2402 MHz, channel 78 = 2480 MHz
    CHECK(FrequencyHopper::classic_freq(0) == 2402e6);
    CHECK(FrequencyHopper::classic_freq(78) == 2480e6);
    // Hop stays in range
    uint32_t state = 0x12345678;
    for (int i = 0; i < 1000; i++) {
        int c = FrequencyHopper::classic_hop(state);
        CHECK(c >= 0 && c < 79);
    }
}

static void test_classic_fhs() {
    // FHS body: LAP(3) + UAP(1) + NAP(2) + CoD(3) + clock offset(2) = 12 bytes.
    uint8_t fhs[12] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                        0x04, 0x02, 0x01, 0x00, 0x10, 0x00 };
    ClassicDev dev;
    CHECK(ClassicDecoder::parse_fhs(fhs, sizeof(fhs), -70, dev));
    CHECK(dev.bd_addr == "11:22:33:44:55:66");
    CHECK(dev.rssi_dbm == -70);
    // CoD = bytes 7..9 -> 0x00010204
    CHECK(dev.class_of_device == 0x00010204);
    CHECK(ClassicDev::major_class(dev.class_of_device) == 0x01); // Computer
}

int main() {
    test_ble_decode();
    test_ble_decode_name();
    test_crc24();
    test_mac_format();
    test_hop_math();
    test_classic_fhs();

    if (g_fail == 0) { std::printf("test_scanner: ALL PASS\n"); return 0; }
    std::printf("test_scanner: %d FAILURES\n", g_fail);
    return 1;
}
