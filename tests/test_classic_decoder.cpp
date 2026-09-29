// language: C++, file: test_classic_decoder.cpp, target: Linux, GCC/Clang
// *Unit test — feed a synthetic FHS packet, assert parser extracts BD_ADDR + CoD*
#include "classic_decoder.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_fail++; } } while (0)

int main() {
    // FHS body (96 bits = 12 bytes), on-air order:
    //   LAP(3) + UAP(1) + NAP(2) + CoD(3) + clock offset(2)
    // BD_ADDR = 11:22:33:44:55:66  (LAP=11:22:33, UAP=44, NAP=55:66)
    // CoD     = 0x00010204         (major=0x01 Computer, minor=0x01)
    // clock   = 0x0010
    uint8_t fhs[12] = {
        0x11, 0x22, 0x33,          // LAP
        0x44,                      // UAP
        0x55, 0x66,                // NAP
        0x04, 0x02, 0x01,          // CoD (LSB first: 0x04,0x02,0x01 -> 0x00010204)
        0x10, 0x00                 // clock offset (LSB first: 0x0010)
    };

    ClassicDev dev;
    CHECK(ClassicDecoder::parse_fhs(fhs, sizeof(fhs), -70, dev));
    CHECK(dev.bd_addr == "11:22:33:44:55:66");
    CHECK(dev.rssi_dbm == -70);
    CHECK(dev.class_of_device == 0x00010204);
    CHECK(dev.clock_offset == 0x0010);

    // Major/minor class decode.
    CHECK(ClassicDev::major_class(dev.class_of_device) == 0x01); // Computer
    CHECK(ClassicDev::minor_class(dev.class_of_device) == 0x01);
    CHECK(std::string(ClassicDev::major_class_name(0x01)) == "Computer");
    CHECK(std::string(ClassicDev::major_class_name(0x02)) == "Phone");

    // Truncated FHS must be rejected.
    ClassicDev bad;
    CHECK(!ClassicDecoder::parse_fhs(fhs, 11, -70, bad));

    if (g_fail == 0) { std::printf("test_classic_decoder: ALL PASS\n"); return 0; }
    std::printf("test_classic_decoder: %d FAILURES\n", g_fail);
    return 1;
}
