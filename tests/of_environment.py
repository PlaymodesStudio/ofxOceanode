from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]

# ofxOceanodeContainer's implementation is split by concern. Tests that anchor on
# its text read this concatenation: core first and history last, so the slices
# "[containerClearDepth, ConnectionFailureInfo)", "[ConnectionFailureInfo,
# drawConnectionErrorDialogContents)" and "[~CanvasNodeSnapshot(), EOF)" keep
# their meaning.
CONTAINER_SOURCES = ("ofxOceanodeContainer.cpp", "ofxOceanodeContainerPresets.cpp",
                     "ofxOceanodeContainerPaths.cpp", "ofxOceanodeContainerClipboard.cpp",
                     "ofxOceanodeContainerEncapsulate.cpp", "ofxOceanodeContainerUnused.cpp",
                     "ofxOceanodeContainerMidiOsc.cpp", "ofxOceanodeContainerHistory.cpp")


def container_source():
    """Every ofxOceanodeContainer*.cpp as one string, in a fixed order."""
    return "\n".join((ROOT / "src/Managers" / name).read_text() for name in CONTAINER_SOURCES)

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


def addon_source(addon, relative):
    """A sibling addon's source by its old path, or by name when it moved."""
    path = Path(addon) / "src" / relative
    if path.exists():
        return path
    matches = sorted((Path(addon) / "src").rglob(Path(relative).name))
    if len(matches) != 1:
        raise FileNotFoundError(f"{relative} in {addon}: {len(matches)} matches")
    return matches[0]


def addon_include_dirs(addon):
    """Every folder with headers under a sibling addon's src/."""
    src = Path(addon) / "src"
    return [src, *sorted({p.parent for p in src.rglob("*.h")})]


def first_existing(*candidates):
    """The first candidate folder that exists (apps live in different trees)."""
    for candidate in candidates:
        if Path(candidate).is_dir():
            return Path(candidate)
    raise FileNotFoundError("none of: " + ", ".join(str(c) for c in candidates))
