#!/usr/bin/env python3
"""Syntax-check an addon's or app's sources against this ofxOceanode's headers.

For repos without a regression harness (sibling addons, apps). Like
integration_compile.py this does not link or launch anything.

    python3 tests/addon_syntax_check.py ../ofxOceanodeMidi --addons ofxMidi --define OFXOCEANODE_USE_MIDI
    python3 tests/addon_syntax_check.py ../ofxOceanodeTextures/src/someNode.cpp

A directory argument checks every .cpp under its src/ (or the directory
itself when it has no src/). --addons adds other addons' src/ and libs/
include folders; each checked repo's own folders are always included.
"""
from pathlib import Path
import argparse
import concurrent.futures
import os
import subprocess

from of_environment import ROOT, of_root, addon_include_dirs


def header_dirs(base):
    return sorted({p.parent for p in Path(base).rglob("*.h")}) if Path(base).is_dir() else []


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("targets", nargs="+", type=Path, help="addon/app folders or .cpp files")
    parser.add_argument("--addons", default="", help="comma-separated extra addons to include")
    parser.add_argument("--define", action="append", default=[], help="preprocessor define (repeatable)")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    of = of_root()

    sources, repos = [], []
    for target in args.targets:
        target = target.resolve()
        if target.is_file():
            sources.append(target)
            repos.append(next((p for p in target.parents if (p / "src").is_dir()), target.parent))
        else:
            src = target / "src" if (target / "src").is_dir() else target
            sources += sorted(p for p in src.rglob("*.cpp") if "/bin/" not in str(p))
            repos.append(target)

    includes = [ROOT / "src", *header_dirs(ROOT / "src")]
    includes += header_dirs(of / "libs/openFrameworks")
    includes += [p for p in (of / "libs").glob("*/include") if p.is_dir()]
    includes += [of / "libs/freetype/include/freetype", of / "libs/cairo/include/cairo"]
    for name in ["ofxImGui", *[a for a in args.addons.split(",") if a]]:
        addon = of / "addons" / name
        includes += [*addon_include_dirs(addon), *header_dirs(addon / "libs")]
    for repo in repos:
        src = repo / "src" if (repo / "src").is_dir() else repo
        includes += [src, *header_dirs(src), *header_dirs(repo / "libs")]

    flags = [os.environ.get("CXX", "clang++"), "-std=c++17", "-fsyntax-only", "-w",
             "-DOF_NO_FMOD", "-DGL_SILENCE_DEPRECATION", *["-D" + d for d in args.define]]
    flags += ["-I" + str(p) for p in dict.fromkeys(includes)]

    def check(path):
        result = subprocess.run(flags + [str(path)], text=True, capture_output=True)
        print(("PASS " if result.returncode == 0 else "FAIL ") + str(path)
              + ("" if result.returncode == 0 else "\n" + result.stderr), flush=True)
        return result.returncode

    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        failures = sum(bool(code) for code in pool.map(check, sources))
    print(f"Syntax checked {len(sources)} translation units; {failures} failed", flush=True)
    raise SystemExit(bool(failures))


if __name__ == "__main__":
    main()
