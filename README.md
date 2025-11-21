# Scopes++

[![CMake](https://img.shields.io/badge/build-cmake-blue.svg)](https://cmake.org) [![C++23](https://img.shields.io/badge/C%2B%2B-23-brightgreen.svg)](https://isocpp.org) [![License](https://img.shields.io/badge/license-GPLv3-blue.svg)](#license) ![Stars](https://img.shields.io/github/stars/MindStudioOfficial/scopes_plusplus?style=flat)

![Scopes++ Screenshot](docs/screenshots/ScopesPlusPlus_20251121.webp "Scopes++ Screenshot")

**Scopes++** is a work-in-progress native C++23 realtime **waveform**, **vectorscope**, video-analyzer implemented with ImGui, OpenGL and OpenCL. It aims to provide video professionals with a high-performance tool for checking color, exposure, and signal integrity in **live video** streams.

This software is an open-source rework of LiveScopes.tv, which I previously wrote in Dart and C++ with Flutter. The goal of this rework is to solve performance and graphics-API limitations, while also providing it as an open-source project for the community to use and contribute to. The use of ImGui as the GUI framework also allows for a very flexible and user-customizable interface with pop-out windows and docking, which was not possible in the previous Flutter-based implementation.

This project is organized as a standard CMake project and produces a single executable. Runtime assets and OpenCL kernels are copied next to the executable by post-build steps so the binary can be run directly from `build/`.

## Features (WIP)

- [x] Waveforms (Luma, RGB, RGB Parade, RGB Blacklevel, YUV Parade)
- [x] UV Vectorscope
- [x] CIE 1931 Chromaticity
- [x] Double Diamond Scope
- [x] False Color Viewer
- [x] Selectable color spaces (Linear RGB, sRGB, BT.709, BT.601_525, BT.601_625, BT.2020) and legal ranges
- [x] GPU-accelerated image pipeline (OpenGL + OpenCL interop)
- [x] 32-Bit float based processing pipeline
- [x] Customizable & dockable user-interface
- [x] NDI Input support

## Roadmap

- [ ] Add webcam capture sources
- [ ] Add Blackmagic DeckLink input support
- [ ] Add support for LUTs
- [ ] Add focus peaking
- [ ] Add exposure zebra


## Known issues

- Mismatch between OpenGL and OpenCL device selection when multiple GPUs are present. This causes errors/crashes.


## Overview

```
src/
└─ app/
   ├─ src/        # C++ sources
   ├─ assets/     # UI and static assets
   └─ kernels/    # OpenCL kernels
ext/              # External libraries (see below)
docs/screenshots/ # Documentation images
```

## Quickstart (Windows)

```powershell
git clone https://github.com/MindStudioOfficial/scopes_plusplus.git
cd scopes_plusplus
cmake -B build
cmake --build build --parallel --config Release
.\build\src\app\Release\scopes++.exe
```

### Notes

- The NDI SDK is expected under `ext/ndi`. A post-build step copies `Processing.NDI.Lib.Advanced.x64.dll` to the runtime folder. You need to download the NDI SDK separately from [NDI's website](https://ndi.tv/sdk/) due to licensing restrictions. The expected folder structure is:

```
📂 ext/ndi
├── 📂 include
│   ├── Processing.NDI.Lib.h
│   └── other headers
├── Processing.NDI.Lib.Advanced.x64.dll
├── Processing.NDI.Lib.Advanced.x64.lib
└── Processing.NDI.Lib.License.txt
```

- OpenCL kernels and `assets/` are copied to the build output via CMake post-build steps/targets.

## Development patterns & conventions

- New source files: place `.cpp` files under `src/app/src` and headers under `src/app/src` (CMake uses `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)`). Re-run `setup_cmake.bat` if CMake doesn't pick up new files.
- Precompiled headers (PCH) are used for MSVC: `pch.hpp` / `pch.cpp`. Follow the existing pattern if adding large headers.
- Keep binary dependencies in `ext/` where possible; CMake links to those targets or libraries for deterministic builds.

## Contributing

Contributions are welcome. Please open issues for discussion before submitting larger changes. For small fixes, open a pull request with a clear description of the change and how it was tested.

Suggested PR checklist:
- Build succeeds in Debug and Release on Windows.
- Any new runtime asset or OpenCL kernel is added to the correct `assets/` or `kernels/` folder and tested.
- Follow existing code style and minimal, focused commits.

## License

This project is licensed under the GNU General Public License v3.0. See the [LICENSE](LICENSE) file for details.
