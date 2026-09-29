# Bluetooth Signal Tool

SDR-based Bluetooth reconnaissance and focused interference tool. Scans and decodes
BLE advertisements and Bluetooth Classic inquiry traffic, then optionally injects
focused noise on the active channels to disrupt the link.

**Backends:** HackRF (libhackrf), USRP (libuhd), BladeRF (libbladerf).
**DSP:** self-contained (`src/dsp_compat.cpp`) — no external DSP library.

## Capabilities

| Mode | What it does |
|------|--------------|
| `scan ble`   | Listen on BLE adv channels 37/38/39 (2402/2426/2480 MHz), decode advertisement PDUs |
| `scan classic` | Run Bluetooth Classic inquiry, collect device addresses + class-of-device |
| `jam ble`    | Inject noise on the BLE data channels a target is using |
| `jam classic`| Hop the 79 Classic channels and inject on active ones |
| `analyze`    | Parse captured BLE PDU / Classic ACL packets offline |

## Build (Linux)

```bash
# dependencies
git submodule update --init --recursive
# build libhackrf
cd third_party/libhackrf && mkdir build && cd build && cmake .. && make -j$(nproc) && sudo make install

# this project (no liquid-dsp needed — DSP is self-contained)
mkdir build && cd build
cmake .. -DUSE_HACKRF=ON
make -j$(nproc)
```

## Windows Build

The tool builds natively on Windows 11 x64 as a standalone `bluetooth_signal_tool.exe`.
No WSL, no MSYS2 shell, no Linux dependency. The DSP is self-contained
(`src/dsp_compat.cpp`), so there is **no liquid-dsp** to build.

### 1. Install the toolchain

- **Visual Studio 2022** with the **"Desktop development with C++"** workload
  (this provides MSVC and the Windows SDK).
- **CMake** (>= 3.16). Install via the Visual Studio Installer (individual
  component "C++ CMake tools for Windows") or from https://cmake.org/download/.
- **Git** (optional, for cloning libhackrf).

### 2. Install the WinUSB driver for HackRF (Zadig)

HackRF on Windows needs the **WinUSB** driver instead of the default libusb-win32
driver. Install it with [Zadig](https://zadig.akeo.ie/):

1. Plug in the HackRF.
2. Open Zadig as Administrator.
3. From the **Options** menu, enable **"List All Devices"**.
4. In the device dropdown, select **"HackRF One"** (or "HackRF Jawbreaker").
5. Set the target driver to **WinUSB** (the dropdown on the right).
6. Click **"Replace Driver"** / **"Install Driver"** and wait for it to finish.
7. Unplug and re-plug the HackRF. It should now enumerate under WinUSB.

### 3. Build libhackrf for Windows

libhackrf has a native CMake build. From a **"x64 Native Tools Command Prompt
for VS 2022"** (or any shell with MSVC + CMake on PATH):

```bat
git clone https://github.com/greatscottgadgets/hackrf.git
cd hackrf
cmake -B build -S host -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

This produces `build/src/Release/hackrf.lib` (MSVC) and the headers under
`host/libhackrf/src/`. Copy `hackrf.dll` next to your `.exe` at runtime.

> **Prebuilt alternative:** some distributions ship prebuilt Windows libhackrf
> binaries. If you use one, point CMake at it with `-DHACKRF_ROOT=<dir>` where
> `<dir>` contains `lib/` and `include/`.

### 4. Configure and build this project

From the project root, in a **"x64 Native Tools Command Prompt for VS 2022"**:

```bat
cmake -B build -DUSE_HACKRF=ON -DHACKRF_ROOT=C:\path\to\hackrf -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

If libhackrf is installed somewhere standard, you can omit `-DHACKRF_ROOT`.

### 5. Run

The output is `build/Release/bluetooth_signal_tool.exe`. Copy `hackrf.dll`
next to it, then run from PowerShell or cmd.exe:

```powershell
.\bluetooth_signal_tool.exe scan ble --backend hackrf --gain 40
.\bluetooth_signal_tool.exe scan classic --backend hackrf
.\bluetooth_signal_tool.exe jam ble --target 11:22:33:44:55:66 --backend hackrf
.\bluetooth_signal_tool.exe analyze --file capture.iq
```

Ctrl+C performs a clean shutdown (handled via `SetConsoleCtrlHandler`).

### Windows notes

- **Threading:** all threads use `std::thread` / `std::mutex` / `std::condition_variable`
  (C++17 standard library). No pthreads.
- **Signal handling:** Ctrl+C is handled with `SetConsoleCtrlHandler` in
  `src/platform_win.cpp` (wrapped behind `include/platform.h`).
- **Random seed:** the jammer seeds its noise generator from `BCryptGenRandom`
  (Windows CSPRNG), not `/dev/urandom`.
- **USRP / BladeRF:** optional and **off by default** on Windows
  (`-DUSE_UHD=OFF`, `-DUSE_BLADERF=OFF`). Only HackRF is enabled by default.

## Continuous Integration

A GitHub Actions workflow (`.github/workflows/windows-build.yml`) builds and
tests the project natively on `windows-2022` on every push/PR to `main`/`master`
(and can be triggered manually via **Actions → windows-build → Run workflow**).

It performs the same steps as the manual Windows build above:

1. Checks out the repo **with submodules** (so `third_party/libhackrf` is present).
2. Sets up the MSVC toolchain (`ilammy/msvc-dev-cmd`).
3. Installs **libusb** via the preinstalled **vcpkg** (libhackrf's Windows dependency).
4. Builds **libhackrf** from source and **installs** it into a prefix
   (`hackrf-install/`), so the header lands at `include/libhackrf/hackrf.h` and
   the library at `bin/hackrf.lib`.
5. Configures this project with `-DHACKRF_ROOT` pointing at that prefix,
   `-DBUILD_TESTS=ON`, MSVC x64.
6. Builds the **Release** `.exe`.
7. Runs the unit tests via **ctest** and verifies the `.exe` exists.

No WSL, no MSYS2, no liquid-dsp — the same constraints as the local Windows build.

## Usage

```bash
# scan BLE advertisements
./bluetooth_signal_tool scan ble --backend hackrf --gain 40

# scan classic devices
./bluetooth_signal_tool scan classic --backend hackrf

# jam a specific BLE target (by MAC or RSSI threshold)
./bluetooth_signal_tool jam ble --target 11:22:33:44:55:66 --backend hackrf

# jam classic on all 79 channels
./bluetooth_signal_tool jam classic --backend hackrf --dwell-ms 2
```

## Channel map

- **BLE:** 40 channels, 2 MHz spacing, 2402–2480 MHz. Advertising on 37/38/39.
- **Classic:** 79 channels, 1 MHz spacing, 2402–2480 MHz. Adaptive frequency hopping.

See `config/channels.json` for the full map and modulation parameters.

## DSP replacements (liquid-dsp → self-contained)

The original Linux build used liquid-dsp for the GFSK demodulator's lowpass FIR.
On Windows liquid-dsp is not available, so every call was replaced with a
self-contained implementation in `src/dsp_compat.cpp` / `include/dsp_compat.h`.
There is **no external DSP dependency**.

| liquid-dsp call | Replacement | Math / verification |
|-----------------|-------------|---------------------|
| `firfilt_crcf_create_kaiser(num_taps, fc, beta)` | `dsp::FirFilter::design_kaiser(num_taps, fc, beta)` | Kaiser-windowed lowpass: ideal `2·fc·sinc(2·fc·n)` × Kaiser window `I0(β·√(1−(n/M)²))/I0(β)`, normalized to unity DC gain. `I0` is the modified Bessel function of the first kind, order 0, computed by series. Verified in `test_dsp_compat` (DC gain ≈ 1, stopband attenuation, `I0` reference values). |
| `firfilt_crcf_set_scale(fir, scale)` | `dsp::FirFilter::set_scale(scale)` | Multiplies all taps by a constant. Trivial; exercised by the DC-gain test. |
| `firfilt_crcf_execute(fir, x, &y)` | `dsp::FirFilter::execute(x)` | Circular-buffer convolution `y[n] = Σₖ taps[k]·x[n−k]`, O(num_taps) per sample. Verified in `test_dsp_compat` against a naive direct convolution (bit-exact match). |
| `firfilt_crcf_reset(fir)` | `dsp::FirFilter::reset()` | Zeroes the delay line. Verified in `test_dsp_compat` (post-reset zero input → zero output). |
| `firfilt_crcf_destroy(fir)` | `dsp::FirFilter` destructor | RAII; no manual free. |

The full GFSK demod path (DC block → IQ correction → FIR → discriminator →
Gardner timing → slicer) is exercised end-to-end by `test_gfsk_demod`, which
modulates synthetic bits, injects AWGN at 10 dB SNR, and asserts the demod
recovers >80% of bits.

## Legal

Radio interference is regulated in most jurisdictions. This tool is for
authorized testing of equipment you own or have permission to test. Operating
a jammer against third-party devices is illegal in many countries.
