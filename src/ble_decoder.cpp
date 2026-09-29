// language: C++, file: ble_decoder.cpp, target: Linux, GCC/Clang
#include "ble_decoder.h"
#include "utils.h"

#include <cstring>
#include <cstdio>

namespace bst {

// ---------------------------------------------------------------------------
// CRC24 (BLE): poly 0x00065B, init 0x555555, LSB-first.
// ---------------------------------------------------------------------------
uint32_t BleDecoder::crc24(const uint8_t* data, size_t len) {
    uint32_t crc = kBleCrcInit;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x01)
                crc = (crc >> 1) ^ kBleCrcPoly;
            else
                crc >>= 1;
        }
    }
    return crc & 0xFFFFFF;
}

bool BleDecoder::crc24_ok(const uint8_t* data, size_t len, const uint8_t crc[3]) {
    uint32_t calc = crc24(data, len);
    uint32_t recv = static_cast<uint32_t>(crc[0]) |
                    (static_cast<uint32_t>(crc[1]) << 8) |
                    (static_cast<uint32_t>(crc[2]) << 16);
    return calc == recv;
}

// ---------------------------------------------------------------------------
// Dewhitening LFSR. BLE whitening: x^7 + x^4 + 1, seeded from channel index.
// The seed is the 7-bit channel index (adv: 37->0x25, 38->0x26, 39->0x27;
// data channels use the channel index directly, 0..36).
// ---------------------------------------------------------------------------
uint8_t BleDecoder::dewhiten_byte(uint8_t b, uint8_t channel) {
    // LFSR state: 7 bits, seeded with channel index.
    uint8_t lfsr = channel & 0x7F;
    uint8_t out = 0;
    for (int i = 0; i < 8; i++) {
        // Whitening bit = bit 6 (MSB of the 7-bit LFSR).
        uint8_t wbit = (lfsr >> 6) & 0x01;
        // Feedback: taps at positions 6 and 3 (x^7 + x^4 + 1).
        uint8_t fb = ((lfsr >> 6) ^ (lfsr >> 3)) & 0x01;
        lfsr = ((lfsr << 1) & 0x7F) | fb;
        // XOR the received bit with the whitening bit.
        uint8_t rbit = (b >> i) & 0x01;
        out |= (rbit ^ wbit) << i;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Access address correlation. The AA is 32 bits; we search the bitstream for
// the 32-bit pattern. Returns the byte offset of the first matching byte.
// ---------------------------------------------------------------------------
int BleDecoder::find_access_address(const std::vector<uint8_t>& bits) {
    // Pack bits into a 32-bit sliding window.
    if (bits.size() < 32) return -1;
    uint32_t window = 0;
    for (int i = 0; i < 32; i++) {
        window = (window << 1) | (bits[i] & 0x01);
    }
    // The AA is transmitted LSB-first; compare against the bit-reversed value.
    uint32_t target = 0;
    for (int i = 0; i < 32; i++) {
        target = (target << 1) | ((kBleAdvAccessAddress >> i) & 0x01);
    }
    if (window == target) return 0;
    for (size_t i = 32; i < bits.size(); i++) {
        window = (window << 1) | (bits[i] & 0x01);
        if (window == target) return static_cast<int>(i - 31);
    }
    return -1;
}

// ---------------------------------------------------------------------------
// AD structure parsing.
// ---------------------------------------------------------------------------
void BleDecoder::parse_ad(const uint8_t* payload, size_t len, BleAdv& out) {
    size_t off = 0;
    while (off + 1 < len) {
        uint8_t l = payload[off];
        if (l == 0) break; // end marker
        if (off + 1 + l > len) break; // truncated
        uint8_t type = payload[off + 1];
        const uint8_t* data = payload + off + 2;
        size_t dlen = l - 1;

        switch (type) {
            case kAdCompleteLocalName:
            case kAdShortLocalName:
                out.name.assign(reinterpret_cast<const char*>(data), dlen);
                break;
            case kAdTxPower:
                if (dlen >= 1) {
                    out.tx_power = static_cast<int8_t>(data[0]);
                    out.has_tx_power = true;
                }
                break;
            case kAdCompleteUuid16:
            case kAdIncompleteUuid16:
                for (size_t i = 0; i + 1 < dlen; i += 2) {
                    uint16_t uuid = static_cast<uint16_t>(data[i]) |
                                    (static_cast<uint16_t>(data[i + 1]) << 8);
                    out.services.push_back(uuid);
                }
                break;
            case kAdMfgSpecific:
                out.mfg_data.assign(data, data + dlen);
                break;
            default:
                break;
        }
        off += 1 + l;
    }
}

// ---------------------------------------------------------------------------
// Full decode: find AA, dewhiten, parse header + payload, verify CRC.
// ---------------------------------------------------------------------------
bool BleDecoder::decode(const std::vector<uint8_t>& bits, uint8_t channel,
                        int8_t rssi, BleAdv& out) {
    int aa_off = find_access_address(bits);
    if (aa_off < 0) return false;

    // After the 32-bit AA comes the PDU header (2 bytes) + payload + CRC (3 bytes).
    // The PDU header length field tells us the payload length.
    size_t pdu_start = static_cast<size_t>(aa_off) + 32;
    if (pdu_start + 16 > bits.size()) return false; // need at least header

    // Pack the PDU header bytes (LSB-first bitstream -> bytes).
    uint8_t hdr[2];
    for (int b = 0; b < 2; b++) {
        uint8_t byte = 0;
        for (int i = 0; i < 8; i++) {
            size_t idx = pdu_start + b * 8 + i;
            if (idx >= bits.size()) return false;
            byte |= (bits[idx] & 0x01) << i;
        }
        hdr[b] = dewhiten_byte(byte, channel);
    }

    uint16_t hdr_raw = static_cast<uint16_t>(hdr[0]) |
                       (static_cast<uint16_t>(hdr[1]) << 8);
    out.adv_type = hdr_raw & 0x0F;
    uint8_t tx_add = (hdr_raw >> 6) & 0x01;
    uint8_t rx_add = (hdr_raw >> 7) & 0x01;
    uint8_t length = (hdr_raw >> 8) & 0x3F;

    // Payload + CRC.
    size_t total = 2 + length + 3; // header + payload + crc
    if (pdu_start + total * 8 > bits.size()) return false;

    std::vector<uint8_t> pdu(total);
    pdu[0] = hdr[0];
    pdu[1] = hdr[1];
    for (size_t i = 0; i < length + 3; i++) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; b++) {
            size_t idx = pdu_start + (2 + i) * 8 + b;
            byte |= (bits[idx] & 0x01) << b;
        }
        pdu[2 + i] = dewhiten_byte(byte, channel);
    }

    // Verify CRC24 over header + payload.
    if (!crc24_ok(pdu.data(), 2 + length, pdu.data() + 2 + length)) {
        return false;
    }

    // Extract advA (6 bytes) if present (ADV_IND, ADV_DIRECT_IND, ADV_NONCONN_IND,
    // SCAN_REQ, SCAN_RSP, CONNECT_IND all carry a 6-byte address).
    size_t off = 2;
    if (out.adv_type <= 5) {
        if (off + 6 <= 2 + length) {
            uint8_t mac[6];
            std::memcpy(mac, pdu.data() + off, 6);
            out.mac = format_mac(mac);
            off += 6;
        }
    }

    // Remaining payload is AD structures.
    if (off < 2 + length) {
        parse_ad(pdu.data() + off, 2 + length - off, out);
    }

    out.rssi_dbm = rssi;
    out.channel = channel;
    out.timestamp_ms = static_cast<uint64_t>(now_seconds() * 1000.0);
    return true;
}

} // namespace bst
