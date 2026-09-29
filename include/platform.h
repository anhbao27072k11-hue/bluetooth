// language: C++, file: platform.h, target: Windows/Linux, MSVC/MinGW/GCC
// *Small platform abstraction — Ctrl+C handling, timing, secure random seed*
//
// The original Linux build used POSIX signal handling (std::signal(SIGINT))
// and relied on the OS for timing. On Windows, SIGINT via std::signal is
// unreliable for console apps; the correct mechanism is SetConsoleCtrlHandler.
// This header exposes a tiny, portable surface and hides the #ifdef _WIN32
// behind platform_win.cpp / platform_posix.cpp.
#pragma once

#include <cstdint>

namespace bst {
namespace platform {

// ---------------------------------------------------------------------------
// Ctrl+C / console shutdown handling.
//
// Registers a handler that sets the shared stop flag when the user presses
// Ctrl+C (or the console sends a close/break event). On Windows this uses
// SetConsoleCtrlHandler; on POSIX it installs a SIGINT/SIGTERM handler.
//
// Returns true on success.
// ---------------------------------------------------------------------------
bool install_ctrl_c_handler();

// Returns true once Ctrl+C (or SIGINT/SIGTERM) has been received.
bool stop_requested();

// Reset the stop flag (mainly for tests).
void clear_stop_flag();

// ---------------------------------------------------------------------------
// High-resolution monotonic time in seconds (steady clock).
// ---------------------------------------------------------------------------
double now_seconds();

// ---------------------------------------------------------------------------
// Cryptographically-seeded random 64-bit value.
// On Windows uses BCryptGenRandom; on POSIX reads /dev/urandom; falls back to
// std::random_device. Used to seed the jammer's noise generator.
// ---------------------------------------------------------------------------
uint64_t secure_random_u64();

} // namespace platform
} // namespace bst
