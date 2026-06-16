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
```

3) Library is located in `output/<target-os>_<target-arch>/libcbcengine.so`.
For iOS, the output is `output/<target-os>_aarch64/libcangjie-interpreter.dylib`.

## How to run tests

Build project for the native target with additional `--run-tests` option:

```bash
python3 build.py build --run-tests
```

Cross-target builds with `--run-tests` are not allowed.
