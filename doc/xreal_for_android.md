# XREAL Air (including Air 2 Ultra) on Android with Monado

<!--
Copyright 2026, Collabora, Ltd. and the Monado contributors
SPDX-License-Identifier: BSL-1.0
-->

This guide walks from a fresh Ubuntu/Linux machine with only the Monado source tree cloned, to:

1. a working Android build environment,
2. a built Monado Android APK,
3. deployment to a phone,
4. practical debugging for XREAL USB sensor access (IMU/HID focus, no display required).

> Scope note: this is focused on getting Monado + XREAL sensor access running on Android.  
> In this branch, HID/IMU access is the primary path that has been adapted for Android-style usage.

---

## 1) Host prerequisites (Ubuntu)

From a terminal on your Linux machine:

```bash
sudo apt update
sudo apt install -y \
  openjdk-17-jdk \
  unzip wget curl git \
  cmake ninja-build pkg-config \
  python3 python3-venv \
  usbutils
```

Check Java:

```bash
java -version
```

You should see Java 17.

---

## 2) Install Android SDK + NDK (command-line)

Pick an SDK location (example: `$HOME/Android/Sdk`):

```bash
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
mkdir -p "$ANDROID_SDK_ROOT"
```

Download Android command-line tools:

```bash
cd /tmp
wget https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
unzip -q commandlinetools-linux-11076708_latest.zip -d cmdline-tools-extract
mkdir -p "$ANDROID_SDK_ROOT/cmdline-tools"
mv cmdline-tools-extract/cmdline-tools "$ANDROID_SDK_ROOT/cmdline-tools/latest"
```

Put tools on PATH:

```bash
export PATH="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin:$ANDROID_SDK_ROOT/platform-tools:$PATH"
```

Install required Android packages:

```bash
yes | sdkmanager --licenses
sdkmanager \
  "platform-tools" \
  "platforms;android-35" \
  "build-tools;34.0.0" \
  "ndk;26.3.11579264" \
  "cmake;3.22.1"
```

Optional but recommended: put these exports in your shell rc (`~/.bashrc`/`~/.zshrc`):

```bash
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
export PATH="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin:$ANDROID_SDK_ROOT/platform-tools:$PATH"
```

---

## 3) Create `local.properties` in the repo

Create this file:

`/home/runner/work/monado/monado/local.properties`

Example:

```properties
sdk.dir=/home/<your-user>/Android/Sdk
ndk.dir=/home/<your-user>/Android/Sdk/ndk/26.3.11579264
```

Optional extras:

```properties
# If Eigen is not in /usr/include/eigen3
# eigenIncludeDir=/path/to/eigen

# If Gradle/CMake cannot find python3
# pythonBinary=/usr/bin/python3
```

---

## 4) Build Monado Android APK (out-of-process variant)

From repo root:

```bash
cd /home/runner/work/monado/monado
./gradlew --max-workers 4 assembleOutOfProcessDebug
```

APK output directory:

`/home/runner/work/monado/monado/src/xrt/targets/openxr_android/build/outputs/apk/outOfProcess/debug/`

If needed:

```bash
find /home/runner/work/monado/monado/src/xrt/targets/openxr_android/build/outputs/apk -name "*.apk"
```

---

## 5) Connect phone and install

Enable Developer Options + USB debugging on the phone, then:

```bash
adb devices
```

Install directly via Gradle:

```bash
cd /home/runner/work/monado/monado
./gradlew installOutOfProcessDebug
```

Or install APK manually:

```bash
adb install -r /home/runner/work/monado/monado/src/xrt/targets/openxr_android/build/outputs/apk/outOfProcess/debug/<your-apk-name>.apk
```

Expected package (out-of-process):

`org.freedesktop.monado.openxr_runtime.out_of_process`

Check installed:

```bash
adb shell pm list packages | grep org.freedesktop.monado.openxr_runtime
```

---

## 6) Make Monado the active OpenXR runtime on phone

Monado exposes OpenXR runtime metadata via Android services. Confirm it is visible:

```bash
adb shell cmd package query-intent-services -a org.khronos.openxr.OpenXRRuntimeService
```

If your device uses an **OpenXR Runtime Broker** app, open it and select Monado out-of-process runtime.

Launch Monado app UI once (recommended):

```bash
adb shell monkey -p org.freedesktop.monado.openxr_runtime.out_of_process 1
```

---

## 7) Connect XREAL glasses and verify USB visibility

Connect the XREAL Air / Air 2 Ultra through USB-C (host-capable phone/port required).

Useful checks:

```bash
adb shell dumpsys usb
adb shell dumpsys usb | grep -Ei "xreal|0x3318|3318|0426|0428|0432|0424"
```

XREAL IDs used by the driver:

- VID `0x3318`
- PIDs:
  - Air: `0x0424`
  - Air 2: `0x0428`
  - Air 2 Pro: `0x0432`
  - Air 2 Ultra: `0x0426`

---

## 8) Run-time debugging: IMU/HID and service status

### 8.1 Basic service/process checks

```bash
adb shell dumpsys activity services | grep -i monado
adb shell ps -A | grep -i monado
```

### 8.2 Logcat filters

Monado native logs are routed to logcat with `monado.<function>` style tags.

```bash
adb logcat -c
adb logcat -v time | grep -Ei "monado|MonadoService|MonadoImpl|monado-ipc-client|xreal|prober|hid|usb"
```

### 8.3 What to look for

- Driver creation success/failure messages for XREAL.
- HID open errors (especially permission/access failures).
- Prober messages indicating interface discovery/open results.
- Runtime service lifecycle: `MonadoService onStartCommand/onBind/...`

---

## 9) Common failure modes and fixes

### `adb devices` shows no device

- Reconnect cable, re-accept USB debugging prompt.
- Try:
  ```bash
  adb kill-server
  adb start-server
  adb devices
  ```

### Runtime not selectable in broker

- Ensure out-of-process APK is installed.
- Re-run:
  ```bash
  adb shell cmd package query-intent-services -a org.khronos.openxr.OpenXRRuntimeService
  ```
- Launch app once with `monkey` command above.

### USB device is connected but no XREAL data

- Confirm USB dump includes VID/PID.
- Check logcat for HID/libusb open failures.
- If logs show access/permission failures, your phone/build may require additional USB permission plumbing or elevated access depending on vendor ROM/security policy.

### Build fails on missing SDK/NDK/CMake

- Re-check `local.properties`.
- Re-run `sdkmanager` package install commands.
- Ensure `ndk;26.3.11579264` and `cmake;3.22.1` are installed.

---

## 10) About recording EuRoC-style datasets

Monado’s EuRoC driver in-tree is primarily for dataset playback/import paths, not a turnkey Android recorder app.

This setup gets you to a running Android Monado runtime with XREAL sensor-path debugging.  
For full EuRoC dataset export on-phone, you typically add an app/tooling layer that subscribes to Monado tracking data and writes EuRoC-format files.

---

## 11) Quick command recap

```bash
# Build
cd /home/runner/work/monado/monado
./gradlew assembleOutOfProcessDebug

# Install
./gradlew installOutOfProcessDebug

# Verify package/runtime
adb shell pm list packages | grep org.freedesktop.monado.openxr_runtime
adb shell cmd package query-intent-services -a org.khronos.openxr.OpenXRRuntimeService

# Launch UI once
adb shell monkey -p org.freedesktop.monado.openxr_runtime.out_of_process 1

# USB + logs
adb shell dumpsys usb | grep -Ei "xreal|3318|0426|0428|0432|0424"
adb logcat -v time | grep -Ei "monado|xreal|usb|hid"
```

