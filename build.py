#!/usr/bin/env python3
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import multiprocessing
import platform
import sys
import time
from dataclasses import dataclass
from typing import Any, Callable


TARGET_OSES = ["linux", "android", "ios", "ios-sim", "macos"]
TARGET_ARCHES = ["x86_64", "aarch64"]
TARGET_ARCH_ALIASES = {
    "amd64": "x86_64",
    "arm64": "aarch64",
}
SUPPORTED_TARGETS = {
    ("linux", "x86_64"),
    ("linux", "aarch64"),
    ("android", "aarch64"),
    ("ios", "aarch64"),
    ("ios-sim", "aarch64"),
    ("ios-sim", "x86_64"),
    ("macos", "aarch64"),
    ("macos", "x86_64"),
}

ANDROID_PLATFORM = "android-26"
ANDROID_ABI = "arm64-v8a"
HELPER_TARGETS = {
    ("linux", "x86_64"): ("x86_64-linux-gnu", "libcbcengine-helper.so"),
    ("linux", "aarch64"): ("aarch64-linux-gnu", "libcbcengine-helper.so"),
    ("ios", "aarch64"): ("aarch64-apple-ios", "libcbcengine-helper.dylib"),
    ("ios-sim", "aarch64"): ("aarch64-apple-ios-simulator", "libcbcengine-helper.dylib"),
    ("ios-sim", "x86_64"): ("x86_64-apple-ios-simulator", "libcbcengine-helper.dylib"),
    ("macos", "aarch64"): ("aarch64-apple-darwin", "libcbcengine-helper.dylib"),
    ("macos", "x86_64"): ("x86_64-apple-darwin", "libcbcengine-helper.dylib"),
}
APPLE_HELPER_SDKS = {
    ("ios", "aarch64"): "iphoneos",
    ("ios-sim", "aarch64"): "iphonesimulator",
    ("ios-sim", "x86_64"): "iphonesimulator",
    ("macos", "aarch64"): "macosx",
    ("macos", "x86_64"): "macosx",
}


def run_command(command: str, cwd=None):
    try:
        subprocess.run(command, shell=True, check=True, cwd=cwd)
    except subprocess.CalledProcessError as e:
        print(f"Error: Command failed with exit code {e.returncode}")
        sys.exit(e.returncode)


def run_command_args(command, cwd=None, env=None):
    try:
        subprocess.run(command, check=True, cwd=cwd, env=env)
    except subprocess.CalledProcessError as e:
        print(f"Error: Command failed with exit code {e.returncode}")
        sys.exit(e.returncode)


def fail(message):
    raise RuntimeError(message)


def clean(build_dir: Path):
    if os.path.exists(build_dir):
        print(f"Cleaning {build_dir}...")
        shutil.rmtree(build_dir)
    else:
        print("Nothing to clean.")


def target_name(target_os, target_arch):
    return f"{target_os}_{target_arch}"


def normalize_target_arch(target_arch):
    return TARGET_ARCH_ALIASES.get(target_arch, target_arch)


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
    if current_os == "darwin":
        return "macos"
    fail(f"Unsupported host OS: {current_os}.")


@dataclass
class CmakeOptions:
    args: Any
    project_dir: Path

    def toolchain_options(self) -> list[str]:
        args = self.args
        if args.target_os == "android":
            android_ndk_home = os.environ.get("ANDROID_NDK_HOME")
            if android_ndk_home is None:
                return fail(f"ANDROID_NDK_HOME must be set for {target_name(args.target_os, args.target_arch)} builds")
            toolchain_path = Path(android_ndk_home) / "build/cmake/android.toolchain.cmake"
            if not toolchain_path.is_file():
                fail(f"Android NDK toolchain file does not exist: {toolchain_path}")
            return [
                f"-DCMAKE_TOOLCHAIN_FILE={toolchain_path}",
                f"-DANDROID_PLATFORM={ANDROID_PLATFORM}",
                f"-DANDROID_ABI={ANDROID_ABI}",
            ]
        elif args.target_os in ["ios", "ios-sim", "macos"]:
            if detect_host_os() != "macos":
                fail(f"{args.target_os} builds require macOS and the Xcode command-line tools")
            toolchain_path = self.project_dir / f"cmake/toolchains/{args.target_arch}-{args.target_os}-clang.cmake"
            if not toolchain_path.is_file():
                fail(f"Toolchain file does not exist: {toolchain_path}")
            return [f"-DCMAKE_TOOLCHAIN_FILE={toolchain_path}"]
        else:
            toolchain_path = self.project_dir / "cmake/toolchains" / f"{args.target_arch}-{args.target_os}-gnu-clang.cmake"
            if not toolchain_path.is_file():
                fail(f"Toolchain file does not exist: {toolchain_path}")
            return [f"-DCMAKE_TOOLCHAIN_FILE={toolchain_path}"]

    def build_type(self) -> list[str]:
        return [f"-DCMAKE_BUILD_TYPE={self.args.build_type.capitalize()}"]

    def cbc_engine_opts(self) -> list[str]:
        build_testing = "ON" if self.args.run_tests else "OFF"
        build_int_syms = "ON" if self.args.enable_int_syms else "OFF"
        build_sanitizers = "ON" if getattr(self.args, "sanitizers", False) else "OFF"
        build_compiler_warnings = "ON" if self.args.compiler_warning else "OFF"
        return [
            f"-DBUILD_TESTING={build_testing}",
            f"-DINT_SYMS={build_int_syms}",
            f"-DENABLE_SANITIZERS={build_sanitizers}",
            f"-DCOMPILER_WARNINGS={build_compiler_warnings}",
        ]


def get_xcode_sdkroot(sdk) -> str:
    try:
        sdkroot = subprocess.check_output(
            ["xcrun", "--sdk", sdk, "--show-sdk-path"],
            text=True,
        ).strip()

        if not sdkroot:
            fail(f"xcrun returned an empty SDK path for {sdk}")
        return sdkroot
    except FileNotFoundError:
        fail("xcrun was not found. Install the Xcode command-line tools.")
    except subprocess.CalledProcessError as e:
        print(f"Error: xcrun failed with exit code {e.returncode}")
        sys.exit(e.returncode)
    assert False, "unreachable"


def prepare_helper_environment(target_os, target_arch):
    env = os.environ.copy()

    if target_os in ["ios", "ios-sim", "macos"]:
        if detect_host_os() != "macos":
            fail(f"{target_os} helper library builds require macOS and the Xcode command-line tools")

        sdk = APPLE_HELPER_SDKS[(target_os, target_arch)]
        sdkroot = get_xcode_sdkroot(sdk)
        env["SDKROOT"] = sdkroot

    return env


def build(args, project_dir: Path, build_dir: Path):
    validate_target(args.target_os, args.target_arch)
    host_os = detect_host_os()
    host_arch = detect_host_arch()

    if args.run_tests and is_cross_target(args.target_os, args.target_arch, host_os, host_arch):
        fail(
            "--run-tests requires a native target. "
            f"Host is {target_name(host_os, host_arch)}, "
            f"target is {target_name(args.target_os, args.target_arch)}."
        )

    cmake_options = CmakeOptions(args, project_dir)
    cmake_options = " ".join(
            cmake_options.toolchain_options() +
            cmake_options.build_type() +
            cmake_options.cbc_engine_opts()
    )

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


def cbcengine_lib_name(target_os):
    suffix = ".dylib" if target_os in ["ios", "ios-sim", "macos"] else ".so"
    return f"libcbcengine{suffix}"


def resolve_cbcengine_lib(path_str, target_os):
    path = Path(path_str).expanduser()
    if path.is_dir():
        path = path / cbcengine_lib_name(target_os)
    if not path.is_file():
        fail(f"cbcengine library not found: {path} "
             "(pass the library file or the build directory containing it)")
    return path.resolve()


def build_launcher(args, project_dir, build_dir):
    validate_target(args.target_os, args.target_arch)

    cangjie_home = os.environ.get("CANGJIE_HOME")
    if cangjie_home is None:
        return fail("CANGJIE_HOME must be set for launcher builds. Source <CANGJIE_SDK>/envsetup.sh first.")

    if args.cbcengine_lib is not None:
        cbcengine_lib = resolve_cbcengine_lib(args.cbcengine_lib, args.target_os)
    else:
        cbcengine_lib = Path(cangjie_home) / "tools/lib" / cbcengine_lib_name(args.target_os)

    build_dir = Path(build_dir) / "launcher"
    build_dir.mkdir(parents=True, exist_ok=True)

    launcher_dir = project_dir / "tools/launcher"

    cmake_options = CmakeOptions(args, project_dir)
    cmake_options = " ".join(
            cmake_options.toolchain_options() +
            cmake_options.build_type() +
            [f"-DCANGJIE_HOME={cangjie_home}",
             f"-DCBCENGINE_LIB={cbcengine_lib}"]
    )

    cmake_cmd = (
        f"cmake {launcher_dir} "
        f"{cmake_options} "
        f"-B {build_dir} "
    )

    print(f"--- Building launcher for {target_name(args.target_os, args.target_arch)} ---")
    run_command(cmake_cmd, cwd=build_dir)
    run_command(f"cmake --build {build_dir} -j{args.jobs}", cwd=build_dir)

def build_helper_lib(args, project_dir, build_dir):
    validate_target(args.target_os, args.target_arch)
    helper_target = (args.target_os, args.target_arch)
    if helper_target not in HELPER_TARGETS:
        fail(
            "Unsupported helper target combination: "
            f"--target-os={args.target_os}, --target-arch={args.target_arch}"
        )

    cangjie_home = os.environ.get("CANGJIE_HOME")
    if cangjie_home is None:
        return fail("CANGJIE_HOME must be set for helper library builds. Source <CANGJIE_SDK>/envsetup.sh first.")

    cjc_path = Path(cangjie_home) / "bin/cjc"
    if not cjc_path.is_file():
        fail(f"Cangjie compiler does not exist: {cjc_path}")

    helper_sources_dir = Path(project_dir) / "tools/launcher"
    helper_cj_source = helper_sources_dir / "cbcengine-helper.cj"
    if not helper_cj_source.is_file():
        fail(f"Helper source does not exist: {helper_cj_source}")

    target, helper_lib_name = HELPER_TARGETS[helper_target]
    env = prepare_helper_environment(args.target_os, args.target_arch)
    build_path = Path(build_dir)
    build_path.mkdir(parents=True, exist_ok=True)
    output_path = build_path / helper_lib_name

    print(f"--- Building {helper_lib_name} for {target_name(args.target_os, args.target_arch)} ---")

    link_command = [
        str(cjc_path),
        "--fobf-layout",
        "--fno-obf-export-symbols",
        "--fobf-source-path",
        "--obf-sym-output-mapping",
        "libcbcengine-helper.obf.map",
        "--target",
        target,
        str(helper_cj_source),
        "--output-type=dylib",
        "-o",
        str(output_path),
    ]
    run_command_args(link_command, cwd=build_dir, env=env)
    print(f"Output: {output_path}")


def precommit(args, project_dir, build_dir: Path):
    for build_type in ["release", "debug"]:
        print(f"  Build {build_type} and run tests")
        start_time = time.monotonic()

        build_args = argparse.Namespace(**vars(args),
                                        build_type=build_type,
                                        run_tests=True,
                                        enable_int_syms=False,
                                        compiler_warning=True,
                                        sanitizers=True)

        try:
            clean(build_dir)
            build(build_args, project_dir, build_dir)
        except (SystemExit, RuntimeError):
            elapsed = time.monotonic() - start_time
            print(f"  {build_type} build and test failed after {elapsed:.0f} seconds")
            raise

        elapsed = time.monotonic() - start_time
        print(f"  {build_type} build and test took {elapsed:.0f} seconds")

def main():
    project_dir = Path(__file__).parent.resolve()
    build_root_dir = project_dir / "output"

    def do_clean(_):
        clean(build_root_dir)

    def perform_build(args, build_task: Callable[[Any, Path, Path], Any]):
        build_dir = build_root_dir / f"{target_name(args.target_os, args.target_arch)}"
        print(f"Project directory: {project_dir}")
        print(f"Build directory:   {build_dir}")
        print(f"Target OS:         {args.target_os}")
        print(f"Target arch:       {args.target_arch}")
        build_task(args, project_dir, build_dir)

    host_os = detect_host_os()
    host_arch = detect_host_arch()
    default_target_os = host_os if host_os in TARGET_OSES else None

    parser = argparse.ArgumentParser(description="build / clean")
    subparsers = parser.add_subparsers(dest="command", required=True)

    common_parser = argparse.ArgumentParser(add_help=False)
    target_os_help = "Target operating system"
    if default_target_os is not None:
        target_os_help += f" (default: {default_target_os})"

    common_parser.add_argument("--target-os",
                               choices=TARGET_OSES,
                               default=default_target_os,
                               required=default_target_os is None,
                               help=target_os_help)
    common_parser.add_argument("--target-arch",
                               type=normalize_target_arch,
                               choices=TARGET_ARCHES,
                               default=host_arch,
                               help=f"Target architecture (default: {host_arch})")
    common_parser.add_argument("-j", "--jobs",
                               type=int,
                               default=multiprocessing.cpu_count(),
                               help=f"Number of parallel jobs (default: {multiprocessing.cpu_count()})")

    build_parser = subparsers.add_parser("build", parents=[common_parser], help="build the project")
    build_parser.set_defaults(dispatch=lambda args: perform_build(args, build))
    build_parser.add_argument("-t", "--build-type",
                              choices=["debug", "release"],
                              default="debug",
                              help="Build configuration (default: debug)")
    build_parser.add_argument("--run-tests",
                              action="store_true",
                              help="Run CTest after successful build")
    build_parser.add_argument("--compiler-warning",
                              action="store_true",
                              help="Enable additional compiler warnings")
    build_parser.add_argument("--int-syms",
                              dest="enable_int_syms",
                              action="store_true",
                              help="Enable interpreter labels symbols(slight performance penalty)")
    build_parser.add_argument("--sanitizers",
                              action="store_true",
                              help="Enable asan+ubsan sanitizers (for unit test runs)")

    helper_parser = subparsers.add_parser("build-helper-lib", parents=[common_parser], help="build libcbcengine-helper shared library")
    helper_parser.set_defaults(dispatch=lambda args: perform_build(args, build_helper_lib))

    launcher_parser = subparsers.add_parser("build-launcher", parents=[common_parser], help="build launcher")
    launcher_parser.set_defaults(dispatch=lambda args: perform_build(args, build_launcher))
    launcher_parser.add_argument("-t", "--build-type",
                                  choices=["debug", "release"],
                                  default="release",
                                  help="Build configuration (default: release)")
    launcher_parser.add_argument("--cbcengine-lib",
                                  metavar="PATH",
                                  help="libcbcengine shared library or a directory containing it "
                                       "(e.g. output/linux_x86_64); overrides the library from CANGJIE_HOME")

    clean_parser = subparsers.add_parser("clean", help="clean build artifacts")
    clean_parser.set_defaults(dispatch=do_clean)

    precommit_parser = subparsers.add_parser("precommit",
                          parents=[common_parser],
                          help="clean + build & test (release, then debug)")
    precommit_parser.set_defaults(dispatch=lambda args: perform_build(args, precommit))

    args = parser.parse_args()
    args.dispatch(args)


if __name__ == "__main__":
    main()
