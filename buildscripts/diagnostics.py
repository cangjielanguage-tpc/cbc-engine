from pathlib import Path


def print_cmake_diagnostics(build_dir):
    print("--- CMake configuration diagnostics ---", flush=True)
    cmake_files = Path(build_dir) / "CMakeFiles"
    found = False
    for name in ("CMakeError.log", "CMakeConfigureLog.yaml", "CMakeOutput.log"):
        path = cmake_files / name
        if not path.is_file():
            continue
        found = True
        print(f"--- {path} ---", flush=True)
        try:
            with path.open(encoding="utf-8", errors="replace") as log:
                for line in log:
                    print(line, end="", flush=True)
            print(flush=True)
        except OSError as error:
            print(f"Could not read {path}: {error}", flush=True)
    if not found:
        print("No CMake diagnostic logs were produced.", flush=True)
    print("--- End CMake configuration diagnostics ---", flush=True)
