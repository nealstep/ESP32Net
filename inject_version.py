from ast import Import
from subprocess import check_output
from SCons.Script import (  # type: ignore[import,no-redef]  # noqa F811
    Import,
    Return,
)

env = None  # type: ignore[var-annotated]

Import("env")  # type: ignore[arg-type]

if env.IsIntegrationDump():  # type: ignore[attr-defined]
    Return()


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


git_version = get_git_commit()
print(f"Injecting Git Commit: {git_version}")
env.Append(  # type: ignore[attr-defined]
    CPPDEFINES=[("GIT_VERSION", f'\\"{git_version}\\"')]
)

firmware_version = get_git_tag_or_hash()
print(f">>> Injecting Firmware Version Tag: {firmware_version} <<<")
env.Append(  # type: ignore[attr-defined]
    CPPDEFINES=[("FIRMWARE_VERSION", f'\\"{firmware_version}\\"')]
)
