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

To build the helper library, set up the Cangjie SDK environment
(`source <CANGJIE_SDK>/envsetup.sh`) and run the command for the target platform:

```bash
python3 build.py build-helper-lib --target-os=linux --target-arch=x86_64
python3 build.py build-helper-lib --target-os=linux --target-arch=aarch64
```

iOS helper builds require macOS and the Xcode command-line tools:

```bash
python3 build.py build-helper-lib --target-os=ios --target-arch=aarch64
python3 build.py build-helper-lib --target-os=ios-sim --target-arch=aarch64
python3 build.py build-helper-lib --target-os=ios-sim --target-arch=x86_64
```

The engine library is located in `output/<target-os>_<target-arch>/libcbcengine.<so/dylib>`.
Helper library is located in `output/<target-os>_<target-arch>/libcbcengine-helper.<so/dylib>`.

## How to run tests

Build project for the native target with additional `--run-tests` option:

```bash
python3 build.py build --run-tests
```

Cross-target builds with `--run-tests` are not allowed.
