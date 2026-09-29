cmake_minimum_required(VERSION 3.16)
project(bluetooth_signal_tool CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(BUILD_TESTS "Build unit tests" ON)
option(USE_HACKRF "Enable HackRF backend" ON)
option(USE_UHD "Enable USRP backend" OFF)
option(USE_BLADERF "Enable BladeRF backend" OFF)

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# DSP: self-contained (dsp_compat). No liquid-dsp dependency on any platform.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# libhackrf
# ---------------------------------------------------------------------------
if(USE_HACKRF)
  if(WIN32)
    # Windows: link against hackrf.lib (MSVC) or libhackrf.a (MinGW).
    # Provide a hint path so users can point at their built libhackrf.
    find_library(HACKRF_LIB
      NAMES hackrf hackrf_static libhackrf
      HINTS
        ${HACKRF_ROOT}/lib
        ${HACKRF_ROOT}/bin
        ${HACKRF_ROOT}/build/src
        ${HACKRF_ROOT}/build/src/Debug
        ${HACKRF_ROOT}/build/src/Release
        ${HACKRF_ROOT}/src
        ${HACKRF_ROOT}/src/Debug
        ${HACKRF_ROOT}/src/Release
        ${HACKRF_ROOT}/libhackrf/src
        ${HACKRF_ROOT}/libhackrf/src/Debug
        ${HACKRF_ROOT}/libhackrf/src/Release
        ${HACKRF_ROOT}/../build/libhackrf/src
        ${HACKRF_ROOT}/../build/libhackrf/src/Release
        "C:/hackrf/lib"
    )
    find_path(HACKRF_INCLUDE
      NAMES libhackrf/hackrf.h hackrf.h
      HINTS
        ${HACKRF_ROOT}/include
        ${HACKRF_ROOT}/libhackrf/src
        ${HACKRF_ROOT}/host/libhackrf/src
        "C:/hackrf/include"
    )
  else()
    find_library(HACKRF_LIB hackrf)
    find_path(HACKRF_INCLUDE libhackrf/hackrf.h)
  endif()
  if(NOT HACKRF_LIB OR NOT HACKRF_INCLUDE)
    message(FATAL_ERROR "libhackrf not found. Build it for Windows (see README 'Windows Build') or set HACKRF_ROOT.")
  endif()
endif()

# ---- libuhd ----
if(USE_UHD)
  find_library(UHD_LIB uhd)
  find_path(UHD_INCLUDE uhd/usrp/multi_usrp.hpp)
  if(NOT UHD_LIB OR NOT UHD_INCLUDE)
    message(FATAL_ERROR "libuhd not found.")
  endif()
endif()

# ---- libbladerf ----
if(USE_BLADERF)
  find_library(BLADERF_LIB bladerf)
  find_path(BLADERF_INCLUDE libbladerf.h)
  if(NOT BLADERF_LIB OR NOT BLADERF_INCLUDE)
    message(FATAL_ERROR "libbladerf not found.")
  endif()
endif()

set(SOURCES
  src/main.cpp
  src/sdr_device.cpp
  src/ble_scanner.cpp
  src/classic_scanner.cpp
  src/jammer.cpp
  src/frequency_hopper.cpp
  src/packet_analyzer.cpp
  src/gfsk_demod.cpp
  src/ble_decoder.cpp
  src/classic_decoder.cpp
  src/dsp_compat.cpp
  src/utils.cpp
)

# Platform layer: platform_win.cpp is portable (has a POSIX fallback branch).
list(APPEND SOURCES src/platform_win.cpp)

add_executable(bluetooth_signal_tool ${SOURCES})

target_include_directories(bluetooth_signal_tool PRIVATE
  include
)

target_link_libraries(bluetooth_signal_tool PRIVATE
  Threads::Threads
)

if(WIN32)
  # Windows console app: link the Win32 subsystem libs used by platform_win.cpp.
  target_link_libraries(bluetooth_signal_tool PRIVATE bcrypt)
endif()

if(USE_HACKRF)
  target_include_directories(bluetooth_signal_tool PRIVATE ${HACKRF_INCLUDE})
  target_link_libraries(bluetooth_signal_tool PRIVATE ${HACKRF_LIB})
  target_compile_definitions(bluetooth_signal_tool PRIVATE USE_HACKRF=1)
endif()

if(USE_UHD)
  target_include_directories(bluetooth_signal_tool PRIVATE ${UHD_INCLUDE})
  target_link_libraries(bluetooth_signal_tool PRIVATE ${UHD_LIB})
  target_compile_definitions(bluetooth_signal_tool PRIVATE USE_UHD=1)
endif()

if(USE_BLADERF)
  target_include_directories(bluetooth_signal_tool PRIVATE ${BLADERF_INCLUDE})
  target_link_libraries(bluetooth_signal_tool PRIVATE ${BLADERF_LIB})
  target_compile_definitions(bluetooth_signal_tool PRIVATE USE_BLADERF=1)
endif()

if(BUILD_TESTS)
  enable_testing()

  add_executable(test_scanner tests/test_scanner.cpp src/ble_decoder.cpp src/classic_decoder.cpp src/packet_analyzer.cpp src/frequency_hopper.cpp src/utils.cpp)
  target_include_directories(test_scanner PRIVATE include)
  target_link_libraries(test_scanner PRIVATE Threads::Threads)
  add_test(NAME test_scanner COMMAND test_scanner)

  add_executable(test_jammer tests/test_jammer.cpp src/jammer.cpp src/frequency_hopper.cpp src/utils.cpp src/platform_win.cpp)
  target_include_directories(test_jammer PRIVATE include)
  target_link_libraries(test_jammer PRIVATE Threads::Threads)
  if(WIN32)
    target_link_libraries(test_jammer PRIVATE bcrypt)
  endif()
  add_test(NAME test_jammer COMMAND test_jammer)

  add_executable(test_gfsk_demod tests/test_gfsk_demod.cpp src/gfsk_demod.cpp src/dsp_compat.cpp)
  target_include_directories(test_gfsk_demod PRIVATE include)
  target_link_libraries(test_gfsk_demod PRIVATE Threads::Threads)
  add_test(NAME test_gfsk_demod COMMAND test_gfsk_demod)

  add_executable(test_dsp_compat tests/test_dsp_compat.cpp src/dsp_compat.cpp)
  target_include_directories(test_dsp_compat PRIVATE include)
  target_link_libraries(test_dsp_compat PRIVATE Threads::Threads)
  add_test(NAME test_dsp_compat COMMAND test_dsp_compat)

  add_executable(test_ble_decoder tests/test_ble_decoder.cpp src/ble_decoder.cpp)
  target_include_directories(test_ble_decoder PRIVATE include)
  target_link_libraries(test_ble_decoder PRIVATE Threads::Threads)
  add_test(NAME test_ble_decoder COMMAND test_ble_decoder)

  add_executable(test_classic_decoder tests/test_classic_decoder.cpp src/classic_decoder.cpp src/utils.cpp)
  target_include_directories(test_classic_decoder PRIVATE include)
  target_link_libraries(test_classic_decoder PRIVATE Threads::Threads)
  add_test(NAME test_classic_decoder COMMAND test_classic_decoder)
endif()
