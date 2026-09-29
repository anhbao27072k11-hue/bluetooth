// language: C++, file: platform_win.cpp, target: Windows, MSVC/MinGW
// *Windows implementation of the platform layer*
//
// Ctrl+C handling on Windows console apps is done with SetConsoleCtrlHandler,
// not std::signal. This file implements that, plus a BCryptGenRandom-based
// secure random source and a steady-clock timer.
#include "platform.h"

#include <atomic>
#include <chrono>
#include <random>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace bst {
namespace platform {

namespace {

std::atomic<int> g_stop{0};

#ifdef _WIN32
// Console control handler: called on a dedicated thread when the user presses
// Ctrl+C, closes the console, or the system sends a break/close event.
BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            g_stop.store(1, std::memory_order_relaxed);
            return TRUE; // handled; do not pass to the next handler
        default:
            return FALSE;
    }
}
#endif

} // namespace

bool install_ctrl_c_handler() {
#ifdef _WIN32
    // AddHandler returns FALSE if the handler is already installed; treat that
    // as success too (idempotent).
    if (!SetConsoleCtrlHandler(console_ctrl_handler, TRUE)) {
        return GetLastError() == ERROR_INVALID_PARAMETER;
    }
    return true;
#else
    // POSIX fallback (kept for completeness; the Windows build uses the above).
    return false;
#endif
}

bool stop_requested() {
    return g_stop.load(std::memory_order_relaxed) != 0;
}

void clear_stop_flag() {
    g_stop.store(0, std::memory_order_relaxed);
}

double now_seconds() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

uint64_t secure_random_u64() {
#ifdef _WIN32
    uint64_t out = 0;
    if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&out), sizeof(out),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0 /* STATUS_SUCCESS */) {
        return out;
    }
    // Fallback: std::random_device (may be deterministic on some MSVC builds,
    // but only used if BCrypt fails, which is essentially never).
    std::random_device rd;
    return (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd());
#else
    std::random_device rd;
    return (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd());
#endif
}

} // namespace platform
} // namespace bst
