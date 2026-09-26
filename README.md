# FrameSmith

<p align="center"><img src="docs/architecture.svg" width="96%" /></p>

<p align="center">
  <img src="https://img.shields.io/badge/Android-NDK-37D9A5?style=for-the-badge&logo=android" />
  <img src="https://img.shields.io/badge/Vulkan-1.1-E86FA4?style=for-the-badge&logo=vulkan" />
  <img src="https://img.shields.io/badge/Kotlin-JNI-6E4BE4?style=for-the-badge&logo=kotlin" />
  <img src="https://img.shields.io/badge/GPU-timestamps-F0A94B?style=for-the-badge" />
</p>

**Android NDK + Vulkan renderer with explicit frame pacing and GPU timing.**

FrameSmith is a native Android graphics project that exposes the graphics stack instead of hiding it behind a game engine:

```text
Kotlin Activity
    │
SurfaceView / SurfaceHolder
    │
    ▼
JNI
    │
ANativeWindow + AssetManager
    │
    ▼
Vulkan
  instance
    ↓
physical device
    ↓
logical device + queues
    ↓
Android surface
    ↓
swapchain + image views
    ↓
render pass + graphics pipeline
    ↓
command buffers
    ↓
acquire → submit → present
    ↓
GPU timestamp query + CPU frame pacer
```

## What the renderer actually does

The native engine implements:

- Vulkan instance and physical-device discovery
- graphics + present queue-family selection
- Android `VkSurfaceKHR`
- logical device and `VK_KHR_swapchain`
- FIFO swapchain creation
- swapchain image views
- render pass and framebuffers
- graphics pipeline
- GLSL → SPIR-V shader packaging
- dynamic viewport/scissor
- command-pool and command-buffer recording
- `vkAcquireNextImageKHR`
- semaphore/fence synchronization
- queue submission
- `vkQueuePresentKHR`
- swapchain recreation
- two frames in flight
- Vulkan timestamp-query measurement
- CPU average + p95 frame-time tracking
- live Android overlay for CPU/GPU frame timing

The rendered scene is intentionally simple: a three-vertex, three-color triangle. The project is about the **driver-facing rendering path and measurement infrastructure**, not scene complexity.

## Build

```bash
gradle :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

The Android Gradle shader pipeline compiles:

```text
app/src/main/shaders/scene.vert
app/src/main/shaders/scene.frag
```

into packaged SPIR-V assets consumed by the native renderer.

## Frame measurement

The overlay reports both:

```text
CPU frame avg
CPU frame p95
GPU timestamp duration
presented frame count
jank state
```

The CPU tracker keeps a rolling 120-frame window. GPU duration comes from Vulkan timestamp queries around the graphics work when supported by the device.

## CI

The project has two independent validation paths.

**Host core**
- compiles `FramePacer.cpp` as ordinary C++20
- validates rolling average, p95, and jank detection

**Android build**
- provisions Android SDK 35, NDK, and CMake
- compiles GLSL shaders
- compiles the native Vulkan library
- builds the debug APK
- verifies the SPIR-V shader assets are actually packaged
- uploads the APK as a workflow artifact

The current Android build is green in GitHub Actions.

## Repository map

```text
app/src/main/java/            Android lifecycle + live overlay
app/src/main/cpp/             Vulkan renderer, JNI, frame pacer
app/src/main/shaders/         GLSL shader sources
host_test/                    portable frame-pacing tests
scripts/device_report.sh      adb GPU/device inspection helper
docs/architecture.svg         rendering architecture
```

## Hardware boundary

CI proves the APK and Vulkan native code compile and package correctly. Final on-device GPU timings are hardware-dependent and should be measured on an actual Vulkan-capable Android phone rather than fabricated in documentation.
