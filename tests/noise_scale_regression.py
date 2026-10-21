#!/usr/bin/env python3
"""Exercise production Noise Scale behavior using the real OF noise function."""
from pathlib import Path
import subprocess
import tempfile

from of_environment import ROOT, of_root


SOURCE = r'''
#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

#define ofxOceanodeNodeModel_h
struct ofEventArgs {};
struct ofColor { ofColor() = default; ofColor(int, int, int) {} };
struct TimeState { double time = 0; };
struct FrameTimeState { TimeState previous, current; };
template<typename T> struct ofParameter {
    string name;
    T value{};
    ofParameter &set(string label, T initial, T, T) {
        name = label; value = initial; return *this;
    }
    const T &get() const { return value; }
    const T *operator->() const { return &value; }
    operator const T &() const { return value; }
    ofParameter &operator=(const T &next) { value = next; return *this; }
};
struct ofxOceanodeNodeModel {
    ofColor color;
    string description;
    unordered_map<string, void*> parameters, inspectorParameters;
    FrameTimeState frameTime;
    explicit ofxOceanodeNodeModel(string) {}
    template<typename T> void addParameter(ofParameter<T> &p) { parameters[p.name] = &p; }
    template<typename T> void addOutputParameter(ofParameter<T> &p) { addParameter(p); }
    template<typename T> void addInspectorParameter(ofParameter<T> &p) { inspectorParameters[p.name] = &p; }
    FrameTimeState getFrameGlobalTimeState() const { return frameTime; }
};

#include "ofNoise.h"
struct Sample { float x, y; };
vector<Sample> samples;
float ofNoise(float x, float y) {
    samples.push_back({x, y});
    return _slang_library_noise2(x, y) * 0.5f + 0.5f;
}
#include "noise.h"

template<typename T> ofParameter<T> &parameter(noise &node, string name) {
    return *static_cast<ofParameter<T>*>(node.parameters.at(name));
}
ofParameter<float> &inspector(noise &node, string name) {
    return *static_cast<ofParameter<float>*>(node.inspectorParameters.at(name));
}
void checkNear(double a, double b, int line) {
    if(abs(a - b) >= 1e-5) {
        cerr << "Line " << line << ": expected " << b << ", got " << a << "\n";
        abort();
    }
}
#define near(a, b) checkNear(a, b, __LINE__)
void tick(noise &node, double dt = 1.0 / 60) {
    node.frameTime.previous = node.frameTime.current;
    node.frameTime.current.time += dt;
    samples.clear();
    ofEventArgs args;
    node.update(args);
    for(float value : parameter<vector<float>>(node, "Output").get())
        assert(isfinite(value) && value >= 0 && value <= 1);
}

int main() {
    noise node;
    node.setup();
    near(inspector(node, "Scale Smooth Ms").get(), 0);
    parameter<vector<float>>(node, "Input") = {0.25f};
    parameter<vector<float>>(node, "Speed") = {2};
    parameter<vector<float>>(node, "Index") = {0, 0.5f, 1};
    parameter<vector<float>>(node, "Seed") = {1000};
    parameter<float>(node, "Scale") = 4;
    tick(node);
    // Loaded Scale is applied on the first frame, with a stable Seed axis.
    near(samples[0].x, 0.5); near(samples[1].x, 2.5); near(samples[2].x, 4.5);
    for(auto sample : samples) near(sample.y, 1000);
    const float anchor = parameter<vector<float>>(node, "Output").get()[0];

    // Scale changes are immediate by default; smoothing is opt-in.
    parameter<float>(node, "Scale") = 6;
    tick(node);
    near(samples[1].x, 3.5); near(samples[2].x, 6.5);
    parameter<float>(node, "Scale") = 4;
    tick(node);
    near(samples[2].x, 4.5);
    inspector(node, "Scale Smooth Ms") = 100;
    parameter<float>(node, "Scale") = 8;
    tick(node);
    const float intermediateScale = samples[2].x - 0.5f;
    assert(intermediateScale > 4 && intermediateScale < 5);
    near(parameter<vector<float>>(node, "Output").get()[0], anchor);
    near(samples[1].x - 0.5, intermediateScale * 0.5);
    for(auto sample : samples) near(sample.y, 1000);

    // A changing Input/Speed is still applied immediately during smoothing.
    parameter<vector<float>>(node, "Input") = {1};
    parameter<vector<float>>(node, "Speed") = {3};
    tick(node);
    near(samples[0].x, 3);
    assert(samples[2].x - 3 > intermediateScale && samples[2].x - 3 < 8);
    for(int i = 0; i < 120; ++i) tick(node);
    near(samples[2].x, 11);

    // Zero elapsed time (or a clock rewind) holds the current Scale.
    parameter<float>(node, "Scale") = 0;
    tick(node, 0); near(samples[2].x, 11);
    tick(node, -1); near(samples[2].x, 11);
    tick(node);
    assert(samples[2].x > 3 && samples[2].x < 11);

    // Smoothing can be disabled, including while a transition is underway.
    inspector(node, "Scale Smooth Ms") = 0;
    tick(node, 0);
    for(auto sample : samples) { near(sample.x, 3); near(sample.y, 1000); }
    parameter<float>(node, "Scale") = 2;
    tick(node);
    near(samples[1].x, 4); near(samples[2].x, 5);

    // The loop crossfade uses the same scale and leaves Seed unscaled on both paths.
    parameter<vector<float>>(node, "Seed") = {2, 4, 6};
    inspector(node, "Crossfade Pct") = 0.67f;
    tick(node);
    assert(samples.size() == 5);
    near(samples[1].x, 4); near(samples[1].y, 4);
    near(samples[2].x, 2); near(samples[2].y, 0);
    auto loop = parameter<vector<float>>(node, "Output").get();
    near(loop.front(), loop.back());
    inspector(node, "Scale Smooth Ms") = 100;
    parameter<float>(node, "Scale") = 4;
    tick(node);
    assert(samples[3].x > 5 && samples[3].x < 7);
    near(samples[1].x + samples[2].x, 6);
    near(samples[1].y, 4); near(samples[2].y, 0);
    loop = parameter<vector<float>>(node, "Output").get();
    near(loop.front(), loop.back());

    // Equal elapsed time produces equal responses at different frame rates.
    noise slow, fast;
    for(noise *n : {&slow, &fast}) {
        n->setup();
        inspector(*n, "Scale Smooth Ms") = 100;
        parameter<vector<float>>(*n, "Index") = {1};
        parameter<float>(*n, "Scale") = 0;
        tick(*n);
        parameter<float>(*n, "Scale") = 4;
    }
    for(int i = 0; i < 30; ++i) tick(slow, 1.0 / 30);
    const float slowX = samples[0].x;
    const float slowOutput = parameter<vector<float>>(slow, "Output").get()[0];
    for(int i = 0; i < 120; ++i) tick(fast, 1.0 / 120);
    near(samples[0].x, slowX);
    near(parameter<vector<float>>(fast, "Output").get()[0], slowOutput);

    // Retargeting and resizing mid-transition keep one continuous Scale for all slots.
    parameter<float>(fast, "Scale") = 2;
    parameter<vector<float>>(fast, "Index") = {0, 0.5f, 1, 1};
    tick(fast);
    assert(samples.size() == 4);
    assert(samples[2].x > 2 && samples[2].x < slowX);
    near(samples[1].x, samples[2].x * 0.5); near(samples[3].x, samples[2].x);
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="noise-scale-") as directory:
        folder = Path(directory)
        source = folder / "noise_scale.cpp"
        binary = folder / "noise_scale"
        source.write_text(SOURCE)
        subprocess.run([
            "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-parameter", "-Wno-sign-compare", "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer",
            "-I" + str(ROOT / "src/Nodes"),
            "-I" + str(ROOT / "src/Nodes/Default_Nodes/Generators"),
            "-I" + str(of_root() / "libs/openFrameworks/utils"),
            str(source), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)
    print("Noise Scale regression passed: stable Seed, startup, smoothing, frame rates, retargeting, and loop crossfade.")


if __name__ == "__main__":
    main()
