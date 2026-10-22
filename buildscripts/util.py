import subprocess
import sys


def run_command(command, cwd=None, capture=False):
    try:
        result = subprocess.run(command, shell=True, check=True, cwd=cwd,
                                capture_output=capture, text=capture)
    except subprocess.CalledProcessError as e:
        if capture:
            if e.stdout:
                print(e.stdout, end="")
            if e.stderr:
                print(e.stderr, end="")
        print(f"Error: Command failed with exit code {e.returncode}")
        sys.exit(e.returncode)
    return result.stdout if capture else None


def fail(message):
    raise RuntimeError(message)
