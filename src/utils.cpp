// language: C++, file: utils.cpp, target: Linux, GCC/Clang
#include "utils.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>

namespace bst {

static LogLevel g_level = LogLevel::Info;

void log_set_level(LogLevel lvl) { g_level = lvl; }

void log_msg(LogLevel lvl, const std::string& tag, const std::string& msg) {
    if (static_cast<int>(lvl) < static_cast<int>(g_level)) return;
    const char* lvl_str = "DBG";
    switch (lvl) {
        case LogLevel::Debug: lvl_str = "DBG"; break;
        case LogLevel::Info:  lvl_str = "INF"; break;
        case LogLevel::Warn:  lvl_str = "WRN"; break;
        case LogLevel::Error: lvl_str = "ERR"; break;
    }
    fprintf(stderr, "[%s] [%s] %s\n", lvl_str, tag.c_str(), msg.c_str());
}

void sleep_ms(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

bool parse_mac(const std::string& s, uint8_t out[6]) {
    unsigned b[6];
    if (std::sscanf(s.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x",
                    &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6)
        return false;
    for (int i = 0; i < 6; i++) out[i] = static_cast<uint8_t>(b[i]);
    return true;
}

std::string format_mac(const uint8_t mac[6]) {
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buf);
}

bool read_file(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream iss(s);
    while (std::getline(iss, cur, delim)) {
        if (!cur.empty()) out.push_back(cur);
    }
    return out;
}

} // namespace bst
