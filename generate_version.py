from datetime import datetime

from os import makedirs
from os.path import exists
from os.path import join as pjoin
from SCons.Script import (  # type: ignore[import,no-redef]  # noqa F811
    Import,
    Return,
)
from subprocess import check_output

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


def generate_version_header(*args, **kwargs):
    version = get_git_commit()
    firmware = get_git_tag_or_hash()
    now = datetime.now().strftime("%Y-%m-%d@%H:%M:%S")

    src_dir = env.get(  # type: ignore[attr-defined]
        "PROJECT_SRC_DIR", "include"
    )
    if not exists(src_dir):
        makedirs(src_dir)

    version_file = pjoin(src_dir, "version.h")

    new_content = f"""// AUTO GENERATED FILE, DO NOT EDIT
#ifndef NT_PROJECT_VERSION_H
#define NT_PROJECT_VERSION_H

#define NT_GIT_VERSION "{version}"
#define NT_FIRMWARE_VERSION "{firmware}"
#define NT_DATETIME_VERSION "{now}"

#endif // NT_PROJECT_VERSION_H
"""

    # Only write the file if it doesn't exist or the content has changed
    # This prevents unnecessary rebuilds of files that include version.h
    write_file = True
    if exists(version_file):
        with open(version_file, "r") as f:
            existing_content = f.read()
            if existing_content == new_content:
                write_file = False

    if write_file:
        print(f"Updating {version_file} with version: {version}")
        with open(version_file, "w") as f:
            f.write(new_content)
    else:
        print(f"Version {version} unchanged. Skipping {version_file} update.")


# Execute the function before building
generate_version_header()
