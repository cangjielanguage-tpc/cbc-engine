from pathlib import Path
import glob
import os
import shutil

import yaml

from buildscripts.util import fail, run_command


COVERAGE_TEST_BINARY = "cbc_test"

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_ROOT_DIR = os.path.join(PROJECT_DIR, "output")
CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "coverage.config.yml")


class CoverageConfig:
    def __init__(self, path, build_root_dir):
        if not os.path.isfile(path):
            fail(f"Coverage config file does not exist: {path}")
        with open(path) as f:
            config = yaml.safe_load(f)
        self.coverage_dir = os.path.join(build_root_dir, config["coverage_dir"])
        self.profiles_dir = os.path.join(self.coverage_dir, "profiles")
        self.covered_dirs = config["covered_dirs"]


coverage = CoverageConfig(CONFIG_PATH, BUILD_ROOT_DIR)


def reset_coverage_profiles():
    if os.path.isdir(coverage.profiles_dir):
        shutil.rmtree(coverage.profiles_dir)
    os.makedirs(coverage.profiles_dir, exist_ok=True)


def build_coverage_report(project_dir, build_dir):
    # llvm-cov substring-matches the positional sources, so absolute
    # project dirs keep test sources and vendored code out of the report.
    sources = " ".join(f"'{os.path.join(project_dir, d)}'" for d in coverage.covered_dirs)

    profraws = sorted(glob.glob(os.path.join(coverage.profiles_dir, "*.profraw")))
    if not profraws:
        fail(f"No coverage profiles collected in {coverage.profiles_dir}")

    binary = os.path.join(build_dir, "test", COVERAGE_TEST_BINARY)
    if not os.path.isfile(binary):
        fail(f"Coverage report requires the test binary: {binary}")

    profdata_path = os.path.join(coverage.coverage_dir, "cbc.profdata")

    print("--- Merging coverage profiles ---")
    run_command(
        f"llvm-profdata merge -sparse {coverage.profiles_dir}/*.profraw -o '{profdata_path}'"
    )

    print("--- Generating coverage report ---")
    report_output = run_command(
        f"llvm-cov report '{binary}' "
        f"--instr-profile='{profdata_path}' "
        f"{sources}",
        capture=True,
    )
    Path(os.path.join(coverage.coverage_dir, "summary.txt")).write_text(report_output)

    html_dir = os.path.join(coverage.coverage_dir, "html")
    run_command(
        f"llvm-cov show '{binary}' "
        f"--instr-profile='{profdata_path}' "
        f"{sources} "
        f"--format=html --show-branches=count --output-dir='{html_dir}'"
    )

    total = next((l for l in report_output.splitlines() if l.startswith("TOTAL")), "")
    parts = total.split()
    covered = ", ".join(coverage.covered_dirs)
    print("================================================")
    if len(parts) >= 13:
        print(f"  Coverage ({covered}): Lines {parts[-4]}, Branches {parts[-1]}")
    else:
        print(f"  {total}")
    print(f"  Full report:  {os.path.join(coverage.coverage_dir, 'summary.txt')}")
    print(f"  HTML report:  {html_dir}/index.html")
    print("================================================")
