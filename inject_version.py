from datetime import datetime

from inspect import getframeinfo, currentframe
from os.path import dirname, realpath, join

from SCons.Script import (  # type: ignore[import,no-redef]  # noqa F811
    Import,
    DefaultEnvironment,
)
from subprocess import check_output

TAG = "NT"
env = None  # type: ignore[var-annotated]

Import("env")  # type: ignore[arg-type]


def get_git_commit():
    try:
        # Run the git command to get the short hash
        commit = (
            check_output(["git", "rev-parse", "--short", "HEAD"])
            .decode("utf-8")
            .strip()
        )
        return commit
    except Exception:
        # Fallback if git fails or isn't initialized
        return "unknown"


def get_git_tag_or_hash():
    try:
        # --tags: Look at all tags
        # --always: Fall back to the unique commit hash if no tags exist
        # --dirty: Append "-dirty" if there are uncommitted local changes
        cmd = ["git", "describe", "--tags", "--always", "--dirty"]
        version = check_output(cmd).decode("utf-8").strip()
        return version
    except Exception:
        # Fallback if Git isn't installed or initialized in the directory
        return "v0.0.0-unknown"


def write_version_header(version, firmware, now):
    print(version, firmware, now)
    cf = currentframe()
    if cf is None:
        raise RuntimeError("Failed to get current frame")
    script_path = getframeinfo(cf).filename
    lib_dir = dirname(realpath(script_path))
    header_path = join(lib_dir, "src", "version.h")
    with open(header_path, "w") as f:
        f.write("#pragma once\n\n")
        f.write(f'#define {TAG}_GIT_VERSION "{version}"\n')
        f.write(f'#define {TAG}_FIRMWARE_VERSION "{firmware}"\n')
        f.write(f'#define {TAG}_DATETIME_VERSION "{now}"\n')


def generate_version_header(version, firmware, now):
    defines = [
        (f"{TAG}_GIT_VERSION", f'\\"{version}\\"'),
        (f"{TAG}_FIRMWARE_VERSION", f'\\"{firmware}\\"'),
        (f"{TAG}_DATETIME_VERSION", f'\\"{now}\\"'),
    ]
    print(defines)
    env.Append(CPPDEFINES=defines)  # type: ignore[arg-type]
    global_env = DefaultEnvironment()  # type: ignore[no-untyped-call]
    global_env.Append(CPPDEFINES=defines)  # type: ignore[arg-type]


# Execute the function before building
version = get_git_commit()
firmware = get_git_tag_or_hash()
now = datetime.now().strftime("%Y-%m-%d@%H:%M:%S")
# generate_version_header(version, firmware, now)
write_version_header(version, firmware, now)
