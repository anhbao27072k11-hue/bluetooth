# third_party

Git submodules for the SDR and DSP dependencies. Initialize with:

```bash
git submodule update --init --recursive
```

| Directory     | Upstream                          | Purpose                    |
|---------------|-----------------------------------|----------------------------|
| `libhackrf`   | greatscottgadgets/hackrf          | HackRF One host library    |
| `libbladerf`  | Nuand/bladeRF                     | BladeRF host library       |

> **Note:** `liquid-dsp` is no longer a dependency. The DSP layer is
> self-contained in `src/dsp_compat.cpp` (see `../README.md` → "DSP replacements").

Build order: build the SDR library you need (e.g. `libhackrf`), then this project.
See `../README.md` for full build steps (Linux and Windows).
