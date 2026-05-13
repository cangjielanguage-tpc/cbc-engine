# Hotfix interpreter

TODO There will be some description of the project

## How to build interpreter lib

<details>
<summary>Required tools</summary>

```bash
sudo apt install lld
```
</details>

1) Clean directory with artifacts if it exists:

```bash
python3 build.py clean
```

2) Build `libcbcengine.so` for target platform:

```bash
python3 build.py build --target-platform=<x86_64 or aarch64>
```

3) Library is located in `output/libcbcengine.so`

## How to run tests

Build project with additional `--run-tests` option:

```bash
python3 build.py build --run-tests
```