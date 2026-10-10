#!/usr/bin/env python3
"""Exercise HeaderPro's production template with small Buffer/parameter fixtures."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

SOURCE = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cfloat>
#include <climits>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

#define buffer_h
#define ofxOceanodeNodeModel_h
struct ofEventArgs {};
struct ofEventListener {
    shared_ptr<bool> active;
    ofEventListener() = default;
    explicit ofEventListener(shared_ptr<bool> state) : active(state) {}
    ofEventListener(ofEventListener&&) = default;
    ofEventListener& operator=(ofEventListener&& other) {
        unsubscribe(); active = std::move(other.active); return *this;
    }
    void unsubscribe() { if(active) *active = false; active.reset(); }
};
struct FrameEvent {
    vector<pair<shared_ptr<bool>, function<void()>>> callbacks;
    template<typename F> ofEventListener newListener(F callback) {
        auto active = make_shared<bool>(true);
        callbacks.push_back({active, callback});
        return ofEventListener(active);
    }
    void notify() {
        for(auto &item : callbacks) if(*item.first) item.second();
    }
};
struct Timestamp {
    double ms = 0;
    uint64_t epochMicroseconds() const { return static_cast<uint64_t>(ms * 1000); }
    void substractMs(float value) { ms -= value; }
    bool operator<=(const Timestamp &other) const { return ms <= other.ms; }
};
template<typename T1, typename T2 = T1> struct buffer {
    struct Frame {
        T1 value;
        Timestamp timestamp;
        T1 getObjectRef() { return value; }
    };
    vector<Frame> frames;
    FrameEvent frameAdded;
    FrameEvent &onFrameAdded() { return frameAdded; }
    void addFrame(Frame frame) { frames.push_back(frame); frameAdded.notify(); }
    size_t getSize() { return frames.size(); }
    Frame &getFrame(int index, bool fromTail) {
        assert(fromTail && !frames.empty());
        const size_t position = static_cast<size_t>(index);
        return frames[position < frames.size() ? frames.size() - 1 - position : 0];
    }
    Frame &getClosestFrameDelayMs(float delay, bool relative, bool *overflow = nullptr) {
        assert(relative && !frames.empty());
        if(overflow) *overflow = delay > 0 &&
            delay * 1000 > frames.back().timestamp.epochMicroseconds() - frames.front().timestamp.epochMicroseconds();
        if(delay == 0) return frames.back();
        if(overflow && *overflow) return frames.front();
        const double target = frames.back().timestamp.ms - delay;
        auto *closest = &frames.back();
        for(auto it = frames.rbegin(); it != frames.rend(); ++it) {
            auto &frame = *it;
            if(abs(frame.timestamp.ms - target) < abs(closest->timestamp.ms - target))
                closest = &frame;
        }
        return *closest;
    }
    Timestamp getFirstFrameTimestamp() { return frames.back().timestamp; }
    Timestamp getLastFrameTimestamp() { return frames.front().timestamp; }
};
template<typename T> struct ofParameter {
    string name;
    T value{};
    vector<function<void(T&)>> callbacks;
    ofParameter &set(string label, T initial) {
        name = label; value = initial; return *this;
    }
    ofParameter &set(string label, T initial, T, T) { return set(label, initial); }
    const T &get() const { return value; }
    const T *operator->() const { return &value; }
    operator const T &() const { return value; }
    ofParameter &operator=(const T &next) {
        value = next;
        for(auto &callback : callbacks) callback(value);
        return *this;
    }
    template<typename F> int newListener(F callback) {
        callbacks.emplace_back(callback); return 0;
    }
};
struct ofEventListeners { void push(int) {} };
constexpr int ofxOceanodeParameterFlags_DisplayMinimized = 1;
struct ofxOceanodeNodeModel {
    string nameIdentifier, description;
    unordered_map<string, void*> parameters, inspectorParameters;
    ofxOceanodeNodeModel(string name) : nameIdentifier(name) {}
    virtual ~ofxOceanodeNodeModel() = default;
    virtual void setup() {}
    virtual void update(ofEventArgs &) {}
    template<typename T> void addParameter(ofParameter<T> &p, int = 0) {
        parameters[p.name] = &p;
    }
    template<typename T> void addInspectorParameter(ofParameter<T> &p) {
        inspectorParameters[p.name] = &p;
    }
    void addParameterDropdown(ofParameter<int> &p, string name, int initial,
                              vector<string>, int = 0) {
        addParameter(p.set(name, initial));
    }
};

#include "bufferHeaderPro.h"
#include "bufferHeader.h"

template<typename T, typename Node> ofParameter<T> &parameter(Node &node, string name) {
    return *static_cast<ofParameter<T>*>(node.parameters.at(name));
}
void expect(const vector<float> &actual, const vector<float> &wanted) {
    if(actual != wanted) {
        cerr << "expected:";
        for(float x : wanted) cerr << " " << x;
        cerr << " | actual:";
        for(float x : actual) cerr << " " << x;
        cerr << "\n";
        abort();
    }
}
int main() {
    buffer<float> history;
    history.frames = {{10, {0}}, {20, {10}}, {30, {30}}, {40, {60}}};
    bufferHeaderPro<float> node("f", 0, true);
    assert(node.nameIdentifier == "HeaderPro f");
    assert(node.inspectorParameters.count("Offset as Position") == 0);
    node.setup();
    auto &source = parameter<buffer<float>*>(node, "Buffer Input");
    auto &copies = parameter<int>(node, "Copies");
    auto &mode = parameter<int>(node, "Offset Mode");
    auto &offset = parameter<vector<float>>(node, "Offset");
    auto &output = parameter<vector<float>>(node, "Output");
    auto &overflow = parameter<bool>(node, "Overflow");
    auto &onUpdate = *static_cast<ofParameter<bool>*>(node.inspectorParameters.at("Calculate On Update"));
    ofEventArgs args;
    source = &history;
    node.update(args);
    expect(output.get(), {40});

    copies = 3; offset = {1}; node.update(args);
    expect(output.get(), {40, 30, 20});
    offset = {0, 1.9f, 3}; node.update(args);
    expect(output.get(), {40, 30, 10});
    assert(!overflow.get());
    copies = 2; offset = {4}; node.update(args);
    expect(output.get(), {40, 10});
    assert(overflow.get());

    mode = 2; copies = 4; offset = {0, 0.5f, 1, 1.2f}; node.update(args);
    expect(output.get(), {40, 30, 10, 10});
    assert(overflow.get());
    copies = 1; offset = {1}; node.update(args);
    expect(output.get(), {10});
    assert(!overflow.get());

    mode = 1; copies = 1; offset = {1}; node.update(args);
    expect(output.get(), {40});
    assert(!overflow.get());
    copies = 4; offset = {0, 9, 25, 60}; node.update(args);
    expect(output.get(), {40, 40, 30, 10});
    assert(!overflow.get());
    offset = {0, 9, 25, 61}; node.update(args);
    expect(output.get(), {40, 40, 30, 10});
    assert(overflow.get());
    copies = 1; offset = {1e30f}; node.update(args);
    expect(output.get(), {10});
    assert(overflow.get());

    buffer<float> sampled;
    for(int i = 0; i < 100; ++i)
        sampled.frames.push_back({static_cast<float>(i), {i * 16.667}});
    source = &sampled; offset = {16}; node.update(args);
    expect(output.get(), {98});
    assert(!overflow.get());
    offset = {160}; node.update(args);
    expect(output.get(), {89});
    assert(!overflow.get());
    source = &history; node.update(args);

    // A mode change recalculates even with per-frame updates disabled.
    onUpdate = false;
    copies = 1; offset = {1}; mode = 0;
    expect(output.get(), {30});
    history.addFrame({50, {70}});
    node.update(args);
    expect(output.get(), {30});
    onUpdate = true;
    history.addFrame({60, {80}});
    expect(output.get(), {50});

    // Old Header: zero milliseconds must select the latest value even when
    // all recorded timestamps are identical, and must not indicate overflow.
    buffer<float> repeated;
    repeated.frames = {{10, {0}}, {20, {0}}, {30, {0}}};
    bufferHeader<float> legacy("f", 0, true);
    legacy.setup();
    parameter<buffer<float>*>(legacy, "Buffer Input") = &history;
    parameter<vector<float>>(legacy, "Offset") = vector<float>{1};
    legacy.update(args);
    expect(parameter<vector<float>>(legacy, "Output").get(), {60});
    assert(!parameter<bool>(legacy, "Overflow").get());
    parameter<buffer<float>*>(legacy, "Buffer Input") = &repeated;
    parameter<vector<float>>(legacy, "Offset") = vector<float>{0};
    legacy.update(args);
    expect(parameter<vector<float>>(legacy, "Output").get(), {30});
    assert(!parameter<bool>(legacy, "Overflow").get());
    repeated.addFrame({40, {0}});
    expect(parameter<vector<float>>(legacy, "Output").get(), {40});
    assert(!parameter<bool>(legacy, "Overflow").get());
    auto &asPosition = *static_cast<ofParameter<bool>*>(
        legacy.inspectorParameters.at("Offset as Position"));
    asPosition = true;
    parameter<vector<float>>(legacy, "Offset") = vector<float>{FLT_MAX};
    legacy.update(args);
    expect(parameter<vector<float>>(legacy, "Output").get(), {10});
    assert(parameter<bool>(legacy, "Overflow").get());
    asPosition = false;
    parameter<vector<float>>(legacy, "Offset") = vector<float>{0};
    legacy.update(args);
    buffer<float> replacement;
    replacement.frames = {{70, {100}}};
    parameter<buffer<float>*>(legacy, "Buffer Input") = &replacement;
    legacy.update(args);
    expect(parameter<vector<float>>(legacy, "Output").get(), {70});
    repeated.addFrame({50, {0}});
    expect(parameter<vector<float>>(legacy, "Output").get(), {70});
    replacement.addFrame({80, {100}});
    expect(parameter<vector<float>>(legacy, "Output").get(), {80});
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="header-pro-") as directory:
        folder = Path(directory)
        source = folder / "header_pro.cpp"
        binary = folder / "header_pro"
        source.write_text(SOURCE)
        subprocess.run([
            "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I" + str(ROOT / "src/Nodes/Default_Nodes/Buffer"),
            "-I" + str(ROOT / "src/Nodes"),
            str(source), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)
    print("Header regression passed: legacy 0/1 ms, HeaderPro modes, vectors, overflow, and sample refresh.")


if __name__ == "__main__":
    main()
