#!/usr/bin/env python3
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import multiprocessing
import platform
import sys


TARGET_OSES = ["linux", "android"]
TARGET_ARCHES = ["x86_64", "aarch64"]
SUPPORTED_TARGETS = {
    ("linux", "x86_64"),
    ("linux", "aarch64"),
    ("android", "aarch64"),
}

ANDROID_PLATFORM = "android-26"
ANDROID_ABI = "arm64-v8a"


def run_command(command, cwd=None):
    try:
        subprocess.run(command, shell=True, check=True, cwd=cwd)
    except subprocess.CalledProcessError as e:
        print(f"Error: Command failed with exit code {e.returncode}")
        sys.exit(e.returncode)


def fail(message):
    raise RuntimeError(message)


def clean(build_dir):
    if os.path.exists(build_dir):
        print(f"Cleaning {build_dir}...")
        shutil.rmtree(build_dir)
    else:
        print("Nothing to clean.")


def target_name(target_os, target_arch):
    return f"{target_os}_{target_arch}"


def is_android_target(target_os, target_arch):
    return (target_os, target_arch) == ("android", "aarch64")


def validate_target(target_os, target_arch):
    if (target_os, target_arch) not in SUPPORTED_TARGETS:
        fail(
            "Unsupported target combination: "
            f"--target-os={target_os}, --target-arch={target_arch}"
        )


def is_cross_target(target_os, target_arch, host_os, host_arch):
    return target_os != host_os or target_arch != host_arch


def detect_host_arch():
    current_arch = platform.machine().lower()
    if current_arch in ["x86_64", "amd64", "x64"]:
        return "x86_64"
    if current_arch in ["aarch64", "arm64", "arm64-v8a"]:
        return "aarch64"
    raise ValueError("Error: unknown host architecture")


def detect_host_os():
    current_os = platform.system().lower()
    if current_os == "linux":
        return current_os
    fail(f"Unsupported host OS: {current_os}. Only Linux hosts are supported for now.")


def prepare_cmake_options(args, project_dir):
    build_type = f"-DCMAKE_BUILD_TYPE={args.build_type.capitalize()} "
    build_testing = "ON" if args.run_tests else "OFF"

    if is_android_target(args.target_os, args.target_arch):
        android_ndk_home = os.environ.get("ANDROID_NDK_HOME")
        if android_ndk_home is None:
            fail(
                "ANDROID_NDK_HOME must be set for "
                f"{target_name(args.target_os, args.target_arch)} builds"
            )

        toolchain_path = Path(android_ndk_home) / "build/cmake/android.toolchain.cmake"
        if not toolchain_path.is_file():
            fail(f"Android NDK toolchain file does not exist: {toolchain_path}")

        return (
            f"{build_type}"
            f"-DBUILD_TESTING={build_testing} "
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain_path} "
            f"-DANDROID_PLATFORM={ANDROID_PLATFORM} "
            f"-DANDROID_ABI={ANDROID_ABI} "
        )

    toolchain_files_dir = f"{project_dir}/cmake/toolchains"
    toolchain_path = f"{toolchain_files_dir}/{args.target_arch}-{args.target_os}-gnu-clang.cmake"
    if not Path(toolchain_path).is_file():
        fail(f"Toolchain file does not exist: {toolchain_path}")
    return (
        f"{build_type}"
        f"-DBUILD_TESTING={build_testing} "
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_path} "
    )


def build(args, project_dir, build_dir):
    validate_target(args.target_os, args.target_arch)
    host_os = detect_host_os()
    host_arch = detect_host_arch()

    if args.run_tests and is_cross_target(args.target_os, args.target_arch, host_os, host_arch):
        fail(
            "--run-tests requires a native target. "
            f"Host is {target_name(host_os, host_arch)}, "
            f"target is {target_name(args.target_os, args.target_arch)}."
        )

    cmake_options = prepare_cmake_options(args, project_dir)

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    cmake_cmd = (
        f"cmake {project_dir} "
        f"{cmake_options}"
    )

    local_googletests_path = os.environ.get("LOCAL_GOOGLETESTS_PATH")
    if local_googletests_path is not None:
        assert os.path.isdir(local_googletests_path)
        cmake_cmd += f"-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST={local_googletests_path} "

    make_cmd = f"make -j{args.jobs}"

    print(
        "--- Configuring "
        f"({args.build_type}) for {target_name(args.target_os, args.target_arch)} ---"
    )
    run_command(cmake_cmd, cwd=build_dir)

    print(f"--- Building with {args.jobs} jobs ---")
    run_command(make_cmd, cwd=build_dir)

    if args.run_tests:
        print("------------------------------------------------")
        print("Running tests via CTest...")
        print("------------------------------------------------")
        run_command(f"ctest --output-on-failure -j{args.jobs}", cwd=build_dir)


def main():
    host_os = detect_host_os()
    host_arch = detect_host_arch()

    parser = argparse.ArgumentParser(description="build / clean")
    subparsers = parser.add_subparsers(dest="command", required=True)

    build_parser = subparsers.add_parser("build", help="build the project")
    build_parser.add_argument("--target-os",
                              choices=TARGET_OSES,
                              default=host_os,
                              help=f"Target operating system (default: {host_os})")
    build_parser.add_argument("--target-arch",
                              choices=TARGET_ARCHES,
                              default=host_arch,
                              help=f"Target architecture (default: {host_arch})")
    build_parser.add_argument("-t", "--build-type",
                              choices=["debug", "release"],
                              default="debug",
                              help="Build configuration (default: debug)")
    build_parser.add_argument("--run-tests",
                              action="store_true",
                              help="Run CTest after successful build")
    build_parser.add_argument("-j", "--jobs",
                              type=int,
                              default=multiprocessing.cpu_count(),
                              help=f"Number of parallel jobs (default: {multiprocessing.cpu_count()})")

    subparsers.add_parser("clean", help="clean build artifacts")

    args = parser.parse_args()

    project_dir = str(Path(__file__).parent.resolve())
    build_root_dir = project_dir + "/output"

    if args.command == "clean":
        clean(build_root_dir)
    elif args.command == "build":
        build_dir = build_root_dir + f"/{target_name(args.target_os, args.target_arch)}"
        print(f"Project directory: {project_dir}")
        print(f"Build directory:   {build_dir}")
        print(f"Target OS:         {args.target_os}")
        print(f"Target arch:       {args.target_arch}")

        build(args, project_dir, build_dir)


if __name__ == "__main__":
    main()
