# AegisRTC

AegisRTC is a cross-platform C++20 real-time video transport prototype for lossy networks.

## Features

- Linux V4L2 camera capture with memory-mapped buffers.
- Windows Media Foundation camera capture.
- FFmpeg and deterministic simulated video encoders.
- Linux and Windows UDP transport.
- Video packetization and frame reassembly.
- NACK-based loss detection and retransmission cache.
- Deterministic network simulation for repeatable tests.
- CMake and CTest based builds.

## Directories

```text
include/aegis/   Public headers
engine/          Session, NACK, and adaptive control
media/           Capture and video encoders
network/         UDP, simulation, packetization, reassembly
tests/           Unit and end-to-end tests
```

## Linux

Configure with `cmake -S . -B build-linux`, build with `cmake --build build-linux --parallel`, and run tests with `ctest --test-dir build-linux --output-on-failure`.

Linux requires FFmpeg development libraries and V4L2 headers. The camera is commonly exposed as `/dev/video0`.

## Windows

Run from a Visual Studio Developer PowerShell. Configure with `cmake -S . -B build-msvc -G "Visual Studio 18 2026"`, build with `cmake --build build-msvc --config Debug --parallel`, and test with `ctest --test-dir build-msvc -C Debug --output-on-failure`.

The Windows build requires Visual Studio, FFmpeg development files, and Media Foundation. If FFmpeg DLLs are not on `PATH`, temporarily add `D:\vcpkg\installed\x64-windows\bin` to it.

## UDP endpoints used during development

Ubuntu uses local `192.168.233.129:9000` and remote `192.168.233.1:9001`.
Windows uses local `192.168.233.1:9001` and remote `192.168.233.129:9000`.

The deterministic `NetworkSimulator` remains available for tests. `NetworkBackend::kUdp` selects real UDP transport.

## Git workflow

Keep generated build directories out of commits. The usual workflow is: pull the current branch, edit source files, inspect with `git status`, add only intended source files, commit with a descriptive message, and push the current branch.

Platform-specific changes should be tested on their native operating system before merging into `main`.
