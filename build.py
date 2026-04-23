#!/usr/bin/env python3
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import multiprocessing
import platform
import sys


def run_command(command, cwd=None):
    try:
        subprocess.run(command, shell=True, check=True, cwd=cwd)
    except subprocess.CalledProcessError as e:
        print(f"Error: Command failed with exit code {e.returncode}")
        sys.exit(e.returncode)


def clean(build_dir):
    if os.path.exists(build_dir):
        print(f"Cleaning {build_dir}...")
        shutil.rmtree(build_dir)
    else:
        print("Nothing to clean.")


def build(args, project_dir, build_dir):
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    toolchain_files_dir = f"{project_dir}/cmake/toolchains"
    toolchain_path = f"{toolchain_files_dir}/{args.target_platform}-linux-gnu.cmake"

    cmake_cmd = (
        f"cmake {project_dir} "
        f"-DCMAKE_BUILD_TYPE={args.build_type.capitalize()} "
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_path} "
    )

    local_googletests_path = os.environ.get("LOCAL_GOOGLETESTS_PATH")
    if local_googletests_path is not None:
        assert os.path.isdir(local_googletests_path)
        cmake_cmd += f"-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST={local_googletests_path} "

    make_cmd = f"make -j{args.jobs}"

    print(f"--- Configuring ({args.build_type}) for {args.target_platform} ---")
    run_command(cmake_cmd, cwd=build_dir)

    print(f"--- Building with {args.jobs} jobs ---")
    run_command(make_cmd, cwd=build_dir)

    if args.run_tests:
        print("------------------------------------------------")
        print("Running tests via CTest...")
        print("------------------------------------------------")
        run_command(f"ctest --output-on-failure -j{args.jobs}", cwd=build_dir)


def main():
    current_arch = platform.machine()
    if current_arch in ["x86_64", "amd64"]:
        default_platform = "x86_64"
    elif current_arch in ["aarch64", "arm64"]:
        default_platform = "aarch64"
    else:
        raise ValueError("Error: unknown host platform")

    parser = argparse.ArgumentParser(description="build / clean")
    subparsers = parser.add_subparsers(dest="command", required=True)

    build_parser =     subparsers.add_parser("build", help="build the project")
    build_parser.add_argument("--target-platform",
                              choices=["x86_64", "aarch64"],
                              default=default_platform,
                              help=f"Target platform (default: {default_platform})")
    build_parser.add_argument("--use-lld",
                              action="store_true",
                              default=False,
                              help="Use ld.lld linker (default: False - use system linker)")
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
    build_dir = project_dir + "/output"

    if args.command == "clean":
        clean(build_dir)
    elif args.command == "build":
        print(f"Project directory: {project_dir}")
        print(f"Build directory:   {build_dir}")
        print(f"Target platform:   {args.target_platform}")

        build(args, project_dir, build_dir)


if __name__ == "__main__":
    main()
