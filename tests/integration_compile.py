#!/usr/bin/env python3
"""Syntax-check production C++ against sibling OF/addon headers on macOS.

Checks all source files because the integration changes widely used headers.
This does not link or launch a host application. See timeline_regression.py for
OCEANODE_TEST_OF_ROOT; optional builds also need ofxOceanodeMidi/ofxOceanodeOSC.
"""
from pathlib import Path
import argparse
import concurrent.futures
import os
import subprocess
import tempfile

from timeline_regression import ROOT, of_root


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--midi-osc", action="store_true")
    parser.add_argument("--jobs", type=int, default=3)
    args = parser.parse_args()
    of = of_root()
    includes = [ROOT / "src", *sorted({p.parent for p in (ROOT / "src").rglob("*.h")})]
    includes += sorted({p.parent for p in (of / "libs/openFrameworks").rglob("*.h")})
    includes += [p for p in (of / "libs").glob("*/include") if p.is_dir()]
    includes += [of / "libs/freetype/include/freetype", of / "libs/cairo/include/cairo"]
    for addon in ("ofxImGui", "ofxMidi", "ofxOsc", "ofxOceanodeMidi", "ofxOceanodeOSC"):
        base = of / "addons" / addon
        includes += [base / "src", *sorted({p.parent for p in (base / "src").rglob("*.h")})]
        includes += sorted({p.parent for p in (base / "libs").rglob("*.h")})
    flags = [os.environ.get("CXX", "clang++"), "-std=c++17", "-fsyntax-only", "-w",
             "-DOF_NO_FMOD", "-DGL_SILENCE_DEPRECATION"]
    if args.midi_osc:
        flags += ["-DOFXOCEANODE_USE_MIDI", "-DOFXOCEANODE_USE_OSC"]
    flags += ["-I" + str(p) for p in dict.fromkeys(includes)]
    sources = sorted((ROOT / "src").rglob("*.cpp"))
    with tempfile.TemporaryDirectory(prefix="oceanode-compile-") as logs:
        def check(path):
            result = subprocess.run(flags + [str(path)], text=True, capture_output=True)
            if result.returncode:
                print("FAIL " + str(path.relative_to(ROOT)) + "\n" + result.stderr, flush=True)
            else:
                print("PASS " + str(path.relative_to(ROOT)), flush=True)
            (Path(logs) / path.name).write_text(result.stdout + result.stderr)
            return result.returncode
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
            results = list(pool.map(check, sources))
    failures = sum(bool(result) for result in results)
    print(f"Syntax checked {len(sources)} translation units; {failures} failed", flush=True)
    raise SystemExit(bool(failures))


if __name__ == "__main__":
    main()
