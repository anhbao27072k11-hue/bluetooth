// language: C++, file: classic_decoder.h, target: Linux, GCC/Clang
// *Bluetooth Classic FHS packet decoder — CoD, BD_ADDR, clock offset*
#pragma once

#include <cstdint>
#include <string>

namespace bst {

// Decoded Classic device from an FHS packet.
struct ClassicDev {
    std::string bd_addr;       // "AA:BB:CC:DD:EE:FF"
    uint32_t    class_of_device = 0; // 24-bit CoD
    uint16_t    clock_offset = 0;
    int8_t      rssi_dbm = 0;
    uint64_t    timestamp_ms = 0;

    // Decode the major device class from CoD bits 8..12.
    static uint8_t major_class(uint32_t cod);
    // Decode the minor device class from CoD bits 2..7.
    static uint8_t minor_class(uint32_t cod);
    // Human-readable major class name.
    static const char* major_class_name(uint8_t major);
};

class ClassicDecoder {
public:
    // Parse an FHS packet payload (after the 68-bit FHS header) into a ClassicDev.
    // Returns true on success.
    static bool parse_fhs(const uint8_t* payload, size_t len, int8_t rssi,
                          ClassicDev& out);
};

} // namespace bst
