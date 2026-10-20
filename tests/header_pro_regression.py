#!/usr/bin/env python3
"""Exercise production HeaderPro/Buffer templates with small parameter/event fixtures."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

SOURCE = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
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

#define frame_h
#define ofxOceanodeNodeModel_h
struct ofEventArgs {};
struct ofEventListener {
    shared_ptr<bool> active;
    ofEventListener() = default;
    explicit ofEventListener(shared_ptr<bool> state) : active(state) {}
    ofEventListener(ofEventListener&&) = default;
    ~ofEventListener() { unsubscribe(); }
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
    inline static double nowMs = 0;
    double ms = nowMs;
    int64_t epochMicrosecondsSigned() const { return static_cast<int64_t>(std::round(ms * 1000)); }
    void update() { ms = nowMs; }
};
template<typename T1, typename T2 = T1> struct bufferFrame {
    T1 value;
    Timestamp timestamp;
    using Assign = function<void(T1&, T2&)>;
    using Return = function<T1(T2&)>;
    bufferFrame(T1 item, Timestamp time, Assign, Return) : value(item), timestamp(time) {}
    void update(T1 item, Timestamp time) { value = item; timestamp = time; }
    T1 getObjectRef() { return value; }
    Timestamp getTimestamp() { return timestamp; }
};
template<typename T> using ofEvent = FrameEvent;
void ofNotifyEvent(ofEvent<void> &event) { event.notify(); }
#include "buffer.h"
template<typename T> struct TestBuffer : buffer<T> {
    struct Sample { T value; Timestamp timestamp; };
    TestBuffer() : buffer<T>([](T &value, T &stored) { stored = value; },
                             [](T &stored) { return stored; }) { this->setMaxSize(1000); }
    void addFrame(Sample sample) { buffer<T>::addFrame(sample.value, sample.timestamp); }
    void addFrames(initializer_list<Sample> samples) {
        for(auto sample : samples) addFrame(sample);
    }
};
template<typename T> struct ofParameter {
    string name;
    T value{};
    bool serializable = true;
    void setSerializable(bool value) { serializable = value; }
    vector<pair<shared_ptr<bool>, function<void(T&)>>> callbacks;
    ofParameter &set(string label, T initial) {
        name = label; value = initial; return *this;
    }
    ofParameter &set(string label, T initial, T, T) { return set(label, initial); }
    const T &get() const { return value; }
    const T *operator->() const { return &value; }
    operator const T &() const { return value; }
    ofParameter &operator=(const T &next) {
        value = next;
        for(auto &callback : callbacks) if(*callback.first) callback.second(value);
        return *this;
    }
    template<typename F> ofEventListener newListener(F callback) {
        auto active = make_shared<bool>(true);
        callbacks.push_back({active, callback});
        return ofEventListener(active);
    }
};
template<> struct ofParameter<void> {
    string name;
    FrameEvent event;
    ofParameter &set(string label) { name = label; return *this; }
    template<typename F> ofEventListener newListener(F callback) { return event.newListener(callback); }
    void trigger() { event.notify(); }
};
struct ofEventListeners {
    vector<ofEventListener> listeners;
    void push(ofEventListener listener) { listeners.push_back(std::move(listener)); }
};
constexpr int ofxOceanodeParameterFlags_DisplayMinimized = 1;
struct ofxOceanodeNodeModel {
    string nameIdentifier, description;
    bool timestampConnected = false;
    unordered_map<string, void*> parameters, inspectorParameters;
    ofxOceanodeNodeModel(string name) : nameIdentifier(name) {}
    virtual ~ofxOceanodeNodeModel() = default;
    virtual void setup() {}
    virtual void update(ofEventArgs &) {}
    struct ConnectionInfo {
        bool connected;
        bool hasInConnection() const { return connected; }
    };
    template<typename T> ConnectionInfo getOceanodeParameter(ofParameter<T>&) {
        return {timestampConnected};
    }
    template<typename T> void addParameter(ofParameter<T> &p, int = 0) {
        parameters[p.name] = &p;
    }
    template<typename T> void addOutputParameter(ofParameter<T> &p, int flags = 0) {
        addParameter(p, flags);
    }
    template<typename T> void addInspectorParameter(ofParameter<T> &p) {
        inspectorParameters[p.name] = &p;
    }
    void addParameterDropdown(ofParameter<int> &p, string name, int initial,
                              vector<string>, int = 0) {
        addParameter(p.set(name, initial));
    }
};

#define phasor_h
struct timeGenerator : ofxOceanodeNodeModel { using ofxOceanodeNodeModel::ofxOceanodeNodeModel; };
#define ofxOceanodeShared_h
struct ofxOceanodeShared {
    inline static bool loading = false;
    static bool isPresetLoading() { return loading; }
};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter" // Legacy Buffer constructor arguments.
#include "bufferNode.h"
#pragma clang diagnostic pop
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
template<typename T>
void recorderStartupTest(const T &zero, const T &first, const T &second, bool connectedTimestamp) {
    ofxOceanodeShared::loading = true;
    bufferNode<T> recorder("test", 0.0f, true);
    recorder.setup();
    recorder.timestampConnected = connectedTimestamp;
    auto *history = parameter<buffer<T>*>(recorder, "Output").get();
    parameter<int>(recorder, "Max Size") = 1000;
    auto feed = [&](const T &value, double ms){
        Timestamp::nowMs = ms;
        parameter<Timestamp>(recorder, "Timestamp") = Timestamp{ms};
        parameter<T>(recorder, "Input") = value;
    };
    // Parameter recall and connection activation can send defaults and saved
    // values in any order. None belongs to the new live recording.
    feed(first, 0);
    feed(zero, 1);
    feed(second, 2);
    feed(zero, 3);
    assert(history->getSize() == 0);

    bufferHeaderPro<T> reader("test", zero, false);
    reader.setup();
    parameter<buffer<T>*>(reader, "Buffer Input") = history;
    parameter<int>(reader, "Copies") = 3;
    parameter<int>(reader, "Offset Mode") = 1;
    parameter<vector<float>>(reader, "Offset") = vector<float>{0, 5000, 10000};
    *static_cast<ofParameter<bool>*>(reader.inspectorParameters.at("Fit Available History")) = true;
    auto &output = parameter<vector<T>>(reader, "Output");
    ofEventArgs args;
    reader.update(args);
    assert(output.get().empty());
    ofxOceanodeShared::loading = false;
    feed(first, 100);
    assert(history->getSize() == 1);
    assert(output.get() == vector<T>(3, first));
    feed(second, 200);
    assert(history->getSize() == 2);
    assert(output.get().front() == second);
    assert(output.get().back() == first); // The final read is real history, not startup zero.

    // A subsequent preset load must not add temporary samples to a retained
    // recorder either, or silently erase its existing history.
    ofxOceanodeShared::loading = true;
    feed(zero, 300);
    feed(first, 301);
    assert(history->getSize() == 2);
    ofxOceanodeShared::loading = false;
    feed(zero, 400); // A real runtime zero remains valid.
    assert(history->getSize() == 3);
    assert(history->getFrame(0, true).getObjectRef() == zero);
    assert(output.get().front() == zero);
    parameter<bool>(recorder, "Rec") = false;
    feed(first, 500);
    assert(history->getSize() == 3);
    parameter<bool>(recorder, "Rec") = true;
    feed(second, 600);
    assert(history->getSize() == 4);

    // An actual zero as the first runtime sample must not be discarded.
    history->clear();
    feed(zero, 700);
    feed(first, 800);
    assert(history->getSize() == 2);
    assert(output.get().back() == zero);
}
void startupTests() {
    TestBuffer<float> growing;
    bufferHeaderPro<float> node("f", 0, true);
    node.setup();
    auto &source = parameter<buffer<float>*>(node, "Buffer Input");
    auto &copies = parameter<int>(node, "Copies");
    auto &mode = parameter<int>(node, "Offset Mode");
    auto &offset = parameter<vector<float>>(node, "Offset");
    auto &output = parameter<vector<float>>(node, "Output");
    auto &overflow = parameter<bool>(node, "Overflow");
    auto &fit = *static_cast<ofParameter<bool>*>(node.inspectorParameters.at("Fit Available History"));
    auto &onUpdate = *static_cast<ofParameter<bool>*>(node.inspectorParameters.at("Calculate On Update"));
    ofEventArgs args;
    assert(!fit.get()); // Existing presets retain their original read behaviour.
    mode = 1; copies = 16;
    vector<float> requested;
    for(int i = 0; i < 16; ++i) requested.push_back(i * (10000.0f / 15));
    offset = requested;
    source = &growing;
    fit = true; node.update(args);
    expect(output.get(), {});
    assert(overflow.get());

    growing.addFrame({0, {0}});
    expect(output.get(), vector<float>(16, 0)); // One real zero sample is still valid.
    growing.addFrame({1, {100}});
    assert(output.get().front() == 1 && output.get().back() == 0);
    for(int i = 2; i < 16; ++i) growing.addFrame({static_cast<float>(i), {i * 100.0}});
    expect(output.get(), {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0});
    assert(overflow.get()); // Nominal ten-second history is not available yet.
    fit = false; node.update(args);
    expect(output.get(), {15, 8, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    fit = true;

    // Preserve unequal delays and caller order, using only requested copies.
    copies = 3; offset = {10000, 0, 2000, 1000000}; node.update(args);
    expect(output.get(), {0, 15, 12});
    for(int i = 16; i <= 100; ++i) growing.addFrame({static_cast<float>(i), {i * 100.0}});
    expect(output.get(), {0, 100, 80});
    assert(!overflow.get());
    for(int i = 101; i <= 160; ++i) growing.addFrame({static_cast<float>(i), {i * 100.0}});
    expect(output.get(), {60, 160, 140}); // Stop expanding at the requested ten seconds.
    assert(!overflow.get());

    // A single spacing value expands before determining the maximum delay.
    offset = {10000}; node.update(args);
    expect(output.get(), {160, 80, 0}); // Requests 0/10000/20000 -> 0/8000/16000.
    assert(overflow.get());
    offset = vector<float>{0}; node.update(args);
    expect(output.get(), {160, 160, 160});
    assert(!overflow.get());
    copies = 1; offset = {20000}; node.update(args);
    expect(output.get(), {0});
    assert(overflow.get());

    // The inspector switch also refreshes when automatic calculation is off.
    onUpdate = false;
    copies = 3; offset = {10000};
    expect(output.get(), {160, 80, 0});
    fit = false;
    expect(output.get(), {160, 60, 0});
    fit = true;
    expect(output.get(), {160, 80, 0});

    // Other offset modes keep their semantics even with fitting enabled.
    mode = 0; offset = {1};
    expect(output.get(), {160, 159, 158});
    mode = 2; offset = {0, 0.5f, 1};
    expect(output.get(), {160, 80, 0});
    mode = 1;
    growing.clear();
    offset = {10000}; // Explicit refresh with automatic calculation disabled.
    expect(output.get(), {});
    assert(overflow.get());
    source = nullptr;
    expect(output.get(), {});

    // Irregular timestamp spacing must fit elapsed time, not sample indices.
    // Store paired coordinates to exercise vector-valued HeaderPro as well.
    TestBuffer<vector<float>> coordinates;
    coordinates.addFrames({{{0, 0}, {0}}, {{1, 2}, {100}},
                           {{5, 10}, {500}}, {{10, 20}, {1000}}});
    bufferHeaderPro<vector<float>> pairs("v_f", {0}, false);
    pairs.setup();
    parameter<buffer<vector<float>>*>(pairs, "Buffer Input") = &coordinates;
    parameter<int>(pairs, "Offset Mode") = 1;
    parameter<int>(pairs, "Copies") = 3;
    parameter<vector<float>>(pairs, "Offset") = vector<float>{0, 5000, 10000};
    *static_cast<ofParameter<bool>*>(pairs.inspectorParameters.at("Fit Available History")) = true;
    pairs.update(args);
    const vector<vector<float>> expected = {{10, 20}, {5, 10}, {0, 0}};
    assert(parameter<vector<vector<float>>>(pairs, "Output").get() == expected);
    coordinates.clear(); coordinates.addFrame({{7, 14}, {0}});
    assert(parameter<vector<vector<float>>>(pairs, "Output").get() ==
           vector<vector<float>>(3, {7, 14}));
}
int main() {
    for(bool connectedTimestamp : {false, true}){
        recorderStartupTest<float>(0, 0.6f, 0.7f, connectedTimestamp);
        recorderStartupTest<vector<float>>({0, 0}, {0.6f, 0.4f}, {0.7f, 0.3f}, connectedTimestamp);
    }
    startupTests();
    TestBuffer<float> history;
    history.addFrames({{10, {0}}, {20, {10}}, {30, {30}}, {40, {60}}});
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

    TestBuffer<float> sampled;
    for(int i = 0; i < 100; ++i)
        sampled.addFrame({static_cast<float>(i), {i * 16.667}});
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
    TestBuffer<float> repeated;
    repeated.addFrames({{10, {0}}, {20, {0}}, {30, {0}}});
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
    TestBuffer<float> replacement;
    replacement.addFrames({{70, {100}}});
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
            "-I" + str(ROOT / "src"),
            "-I" + str(ROOT / "src/Nodes/Default_Nodes/Generators"),
            str(source), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)
    print("Header regression passed: preset-load recording, runtime zeros, production Buffer lookup, fitted startup/growth, vectors, modes, overflow, and sample refresh.")


if __name__ == "__main__":
    main()
