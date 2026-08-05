# Hotfix interpreter

TODO There will be some description of the project

## How to build interpreter lib

There is ready dev container (<project_dir>/.devcontainer/debian) for easy environment setup and project building.

1) Clean directory with artifacts if it exists:

```bash
python3 build.py clean
```

2) Build `libcbcengine.so` for target platform:

```bash
python3 build.py build --target-os=linux --target-arch=<x86_64 or aarch64>
```

For Android builds, set `ANDROID_NDK_HOME` first:

```bash
export ANDROID_NDK_HOME=/path/to/android-ndk
python3 build.py build --target-os=android --target-arch=aarch64
```

For iOS device and simulator builds, run the build directly on macOS with Xcode installed:

```bash
python3 build.py build --target-os=ios --target-arch=aarch64
python3 build.py build --target-os=ios-sim --target-arch=aarch64
python3 build.py build --target-os=ios-sim --target-arch=x86_64
```

By default, Apple builds load `libcbcengine-helper.dylib`. To resolve helper
symbols from a static library linked into the application instead, add
`--static-helper`:

```bash
python3 build.py build --target-os=ios-sim --target-arch=x86_64 --static-helper
```

To build the helper library:
1. \[for iOS\] Install XCode.
2. Setup the Cangjie SDK environment (`source <CANGJIE_SDK>/envsetup.sh`) and run:

```bash
python3 build.py build-helper-lib --target-os=ios --target-arch=aarch64
python3 build.py build-helper-lib --target-os=ios-sim --target-arch=aarch64
python3 build.py build-helper-lib --target-os=ios-sim --target-arch=x86_64
```

3) Library is located in `output/<target-os>_<target-arch>/libcbcengine.<so/dylib>`.
Both helper variants are produced for iOS targets:

- `output/<target-os>_<target-arch>/libcbcengine-helper.dylib`
- `output/<target-os>_<target-arch>/libcbcengine-helper.a`

Applications using a `cbcengine` built with `--static-helper` must link the matching static helper archive and expose its symbols through the application handle passed in `DYN_CJNativeInterface::appLibHandle`.

## How to run tests

Build project for the native target with additional `--run-tests` option:

```bash
python3 build.py build --run-tests
```

Cross-target builds with `--run-tests` are not allowed.
