#!/usr/bin/env python3
"""Verify Global Pitch's scale table: quantization and Shift (degrees, within the octave).

Compiles the real ofxOceanodeGlobalPitch.h against a stub node model, so no
openFrameworks app is needed. Requires Python 3 and clang++.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

# What ofxOceanodeGlobalPitch.h takes from the node model and OF.
STUB_MODEL = r'''
#pragma once
#include <sstream>
#include <string>
#include <vector>
class ofxOceanodeNodeModel {
public:
    std::string getParents() const { return "Canvas"; }
    std::string nodeName() const { return "Stub"; }
    int getNumIdentifier() const { return 1; }
};
inline std::string ofToString(int v) { std::ostringstream s; s << v; return s.str(); }
inline std::vector<std::string> ofSplitString(const std::string& text, const std::string& sep, bool, bool) {
    std::vector<std::string> out;
    size_t from = 0;
    while(from <= text.size()) {
        const size_t at = text.find(sep, from);
        const std::string part = text.substr(from, at == std::string::npos ? std::string::npos : at - from);
        if(!part.empty()) out.push_back(part);
        if(at == std::string::npos) break;
        from = at + sep.size();
    }
    return out;
}
'''

CASES = r'''
#include "ofxOceanodeGlobalPitch.h"
#include <cassert>
#include <cmath>
#include <iostream>
using GP = ofxOceanodeGlobalPitch;
static void nearAt(float a, float b, int line) {
    if(std::abs(a - b) >= 1e-5f) { std::cerr << "line " << line << ": expected " << b << ", got " << a << "\n"; assert(false); }
}
#define near(a, b) nearAt((a), (b), __LINE__)
int main() {
    const std::vector<float> pentatonic = {0, 3, 5, 7, 10}; // C minor pentatonic
    // No shift: the nearest degree (the lower one on a tie).
    auto plain = GP::offsetsFor(pentatonic);
    near(plain[0], 0); near(plain[2], 1); near(plain[4], -1); near(plain[11], -1); near(plain[8], -1);
    // Shift +1: every degree to the next one, staying in the octave: the top
    // degree wraps to the bottom.
    auto up = GP::offsetsFor(pentatonic, 1);
    near(up[0], 3);   // C  -> Eb
    near(up[3], 2);   // Eb -> F
    near(up[5], 2);   // F  -> G
    near(up[7], 3);   // G  -> Bb
    near(up[10], -10); // Bb -> C of the same octave
    near(up[2], 3);   // D quantizes to Eb, then -> F
    // Shift -1 is the scale's size - 1; shifting by the size is no shift.
    near(GP::offsetsFor(pentatonic, -1)[0], 10); // C -> Bb, same octave
    near(GP::offsetsFor(pentatonic, -1)[3], -3); // Eb -> C
    for(int pc = 0; pc < 12; pc++) {
        near(GP::offsetsFor(pentatonic, 5)[pc], plain[pc]);
        near(GP::offsetsFor(pentatonic, 4)[pc], GP::offsetsFor(pentatonic, -1)[pc]);
        near(GP::offsetsFor(pentatonic, 7)[pc], GP::offsetsFor(pentatonic, 2)[pc]);
    }
    assert(GP::distinctDegrees({4, 0, 2, 12, 5, 7, 9, 11}).size() == 7);
    // Degrees given unsorted, repeated or above 11 are one scale.
    auto major = GP::offsetsFor({4, 0, 2, 12, 5, 7, 9, 11}, 2);
    near(major[0], 4); near(major[11], -9); // C -> E, B -> D of the same octave
    // A one-degree scale: every note is that degree (G: the C above), shifted or not.
    near(GP::offsetsFor({0}, 1)[7], 5);

    // Through apply(): scale, shift, then transpose, for a following node.
    ofxOceanodeNodeModel node;
    GP::follow(&node, true, {});
    GP::setScale(pentatonic, true);
    GP::setShift(1);
    near(GP::apply(&node, 60), 63);  // C4 -> Eb4
    near(GP::apply(&node, 65), 67);  // F4 -> G4
    near(GP::apply(&node, 70), 60);  // Bb4 -> C4 (same octave)
    // A fractional note keeps its fraction: quantized by its nearest semitone.
    near(GP::apply(&node, 61.37f), 63.37f); // C#4 (nearest C) -> Eb, +0.37
    near(GP::apply(&node, 70.5f), 59.5f); // rounds to B (tie: the lower Bb) -> C4, keeping +0.5
    GP::setTranspose(2);
    near(GP::apply(&node, 60), 65);  // shifted to Eb, then +2
    GP::setTranspose(0);
    // The low scale has its own shift, along its own degrees, below the split.
    GP::setLowScale({0, 7}, 60, true);
    near(GP::apply(&node, 48), 48);  // the main shift leaves the low scale alone
    GP::setLowShift(1);
    near(GP::apply(&node, 48), 55);  // C3 -> G3 (low scale: C, G)
    near(GP::apply(&node, 55), 48);  // G3 -> C3 (same octave)
    near(GP::apply(&node, 60), 63);  // at the split: the main scale, its own shift
    GP::setLowShift(0);
    // A bypassed scale shifts nothing.
    GP::setScale(pentatonic, false);
    GP::setLowScale({}, 0, false);
    near(GP::apply(&node, 61), 61);
    GP::setShift(0);
    GP::unfollow(&node);
    std::cout << "Global Pitch passed: nearest degree, Shift wrapping in the octave, low scale, bypass, transpose order.\n";
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="oceanode-global-pitch-") as folder:
        folder = Path(folder)
        (folder / "ofxOceanodeNodeModel.h").write_text(STUB_MODEL)
        # A copy of the header next to the stub, so its quoted include finds the stub first.
        (folder / "ofxOceanodeGlobalPitch.h").write_text((ROOT / "src/Nodes/ofxOceanodeGlobalPitch.h").read_text())
        cpp = folder / "regression.cpp"
        cpp.write_text(CASES)
        binary = folder / "regression"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O1", "-g",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-I" + str(folder), str(cpp),
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
