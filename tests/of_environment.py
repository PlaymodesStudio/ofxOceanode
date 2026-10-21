from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def of_root():
    if os.environ.get("OCEANODE_TEST_OF_ROOT"):
        return Path(os.environ["OCEANODE_TEST_OF_ROOT"])
    candidate = ROOT.parent.parent
    if (candidate / "libs/openFrameworks").is_dir():
        return candidate
    common = Path(subprocess.check_output(
        ["git", "rev-parse", "--path-format=absolute", "--git-common-dir"],
        cwd=ROOT, text=True).strip())
    return common.parent.parent.parent
