#!/usr/bin/env bash
set -euo pipefail
adb shell getprop ro.product.model
adb shell getprop ro.build.version.release
adb shell cmd gpu vkjson 2>/dev/null || true
adb shell dumpsys SurfaceFlinger --latency-clear 2>/dev/null || true
