// language: C++, file: utils.h, target: Linux, GCC/Clang
// *Helpers — logging, timing, config loading*
#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <vector>

namespace bst {

// Simple leveled logger.
enum class LogLevel { Debug, Info, Warn, Error };

void log_set_level(LogLevel lvl);
void log_msg(LogLevel lvl, const std::string& tag, const std::string& msg);

#define LOG_D(tag, msg) ::bst::log_msg(::bst::LogLevel::Debug, tag, msg)
#define LOG_I(tag, msg) ::bst::log_msg(::bst::LogLevel::Info,  tag, msg)
#define LOG_W(tag, msg) ::bst::log_msg(::bst::LogLevel::Warn,  tag, msg)
#define LOG_E(tag, msg) ::bst::log_msg(::bst::LogLevel::Error, tag, msg)

// High-resolution monotonic clock helpers.
using Clock = std::chrono::steady_clock;
inline double now_seconds() {
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

// Sleep for ms (yields the thread).
void sleep_ms(uint32_t ms);

// Parse a MAC string "AA:BB:CC:DD:EE:FF" into 6 bytes. Returns false on bad input.
bool parse_mac(const std::string& s, uint8_t out[6]);

// Format a MAC from 6 bytes.
std::string format_mac(const uint8_t mac[6]);

// Load a JSON file into a string (no external JSON dep — callers parse as needed).
bool read_file(const std::string& path, std::string& out);

// Simple tokenizer for CLI args.
std::vector<std::string> split(const std::string& s, char delim);

} // namespace bst
