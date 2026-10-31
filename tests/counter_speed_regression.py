#!/usr/bin/env python3
"""Run production Counter/Mod Time with real OF parameters and ASan/UBSan.

The node-model adapter supplies parameter groups and a controlled transport;
no graphical host, audio device or OF update loop is launched.
"""
import os
from pathlib import Path
import subprocess
import tempfile

from of_environment import ROOT, of_root

SOURCE = r'''
#include "ofMain.h"
#include "ofxOceanodeTransport.h"
#include <cassert>
#include <cfloat>
using namespace std;

// Only the host services are adapted; parameters, events, clocks and both
// node implementations below are production code.
#define ofxOceanodeNodeModel_h
struct ofxOceanodeNodeModel {
    string description;
    ofColor color;
    ofParameterGroup parameters, inspector;
    ofxOceanodeTransport transport;
    ofxOceanodeNodeModel(string) {}
    virtual ~ofxOceanodeNodeModel() = default;
    virtual void setup() {}
    virtual void update(ofEventArgs&) {}
    virtual void resetPhase() {}
    virtual void setBpm(float) {}
    virtual void loadBeforeConnections(ofJson&) {}
    virtual bool isSink() const { return false; }
    template<class T> void addParameter(ofParameter<T>& p) { parameters.add(p); }
    template<class T> void addOutputParameter(ofParameter<T>& p) { parameters.add(p); }
    template<class T> void addInspectorParameter(ofParameter<T>& p) { inspector.add(p); }
    void addTimeSourceParameter(ofParameter<bool>&) {}
    void setNodeSnapshotsDisabled(bool) {}
    ofxOceanodeFrameTransportState getFrameTransportState() const { return transport.getFrameState(); }
    struct GlobalFrame { struct State { double time = 0; } current; };
    GlobalFrame getFrameGlobalTimeState() const { return {}; }
};
#include "phasor.h"
#include "modTime.h"

template<class T, class Node> ofParameter<T>& param(Node& node, const string& name) {
    return node.parameters.get(name).template cast<T>();
}
ofParameter<bool>& sync(counter& node) { return node.inspector.getBool("Sync To Transport"); }
const vector<float>& out(counter& node) { return param<vector<float>>(node, "Out").get(); }
void expect(const vector<float>& actual, const vector<float>& expected) {
    assert(actual.size() == expected.size());
    for(size_t i = 0; i < actual.size(); ++i) {
        if(abs(actual[i] - expected[i]) > 0.0001f) {
            cerr << "lane " << i << ": " << actual[i] << " != " << expected[i] << endl;
            abort();
        }
    }
}
void tick(counter& node, double time) {
    node.setTime(time);
    ofEventArgs args;
    node.update(args);
}
void transportTick(counter& node, double delta) {
    node.transport.advanceFrameStep(delta);
    node.transport.latchFrameState();
    tick(node, node.getTime());
}

int main() {
    counter node;
    node.setup();
    expect(out(node), {0});
    expect(param<vector<float>>(node, "Speed"), {1});
    tick(node, 10);
    expect(out(node), {10});
    node.resetPhase(); // RstWPhs defaults to false.
    tick(node, 11);
    expect(out(node), {11});
    param<void>(node, "Reset").trigger();
    tick(node, 11);
    expect(out(node), {0});
    tick(node, 12);
    expect(out(node), {1});

    // Compare against the actual legacy conjunction, with one speed edit per
    // input event (the original Mod Time drops earlier edits between events).
    counter integrated;
    integrated.setup();
    modTime legacy;
    legacy.setup();
    const vector<float> rates{1, 2, 0, 0.25f, 4, 0.5f};
    for(int frame = 0; frame < 1000; ++frame) {
        if(frame % 37 == 0) {
            const vector<float> speed{rates[(frame / 37) % rates.size()]};
            param<vector<float>>(integrated, "Speed") = speed;
            param<vector<float>>(legacy, "Speed") = speed;
        }
        const float time = frame * 0.125f;
        tick(integrated, time);
        param<vector<float>>(legacy, "Input") = vector<float>{time};
        expect(out(integrated), param<vector<float>>(legacy, "Output"));
    }

    counter lanes;
    lanes.setup();
    param<vector<float>>(lanes, "Speed") = vector<float>{1, 2, 0.5f, 0};
    tick(lanes, 0);
    tick(lanes, 2);
    expect(out(lanes), {2, 4, 1, 0});
    int notifications = 0;
    auto listener = param<vector<float>>(lanes, "Out").newListener(
        [&](vector<float>&) { ++notifications; });
    param<vector<float>>(lanes, "Speed") = vector<float>{0, 1, 2, 4};
    assert(notifications == 0); // Edits alone never emit a new time sample.
    tick(lanes, 3);
    expect(out(lanes), {3, 6, 1.5f, 0});
    tick(lanes, 4);
    expect(out(lanes), {3, 7, 3.5f, 4});
    param<vector<float>>(lanes, "Speed") = vector<float>{20, 30, 40, 50};
    param<vector<float>>(lanes, "Speed") = vector<float>{1, 2, 3, 4};
    tick(lanes, 4); // Several edits have the same net effect as the final edit.
    expect(out(lanes), {3, 7, 3.5f, 4});
    tick(lanes, 5);
    expect(out(lanes), {4, 9, 6.5f, 8});
    param<void>(lanes, "Reset").trigger();
    tick(lanes, 5);
    expect(out(lanes), {0, 0, 0, 0});
    tick(lanes, 6);
    expect(out(lanes), {1, 2, 3, 4});

    // Resize during motion: new lanes broadcast the first lane's phase.
    param<vector<float>>(lanes, "Speed") = vector<float>{2};
    tick(lanes, 6);
    expect(out(lanes), {1});
    tick(lanes, 7);
    expect(out(lanes), {3});
    param<vector<float>>(lanes, "Speed") = vector<float>{2, 4};
    tick(lanes, 7);
    expect(out(lanes), {3, 3});
    tick(lanes, 8);
    expect(out(lanes), {5, 7});
    lanes.resetPhase(); // Clear modulation offset while keeping source time.
    tick(lanes, 8);
    expect(out(lanes), {6, 12});
    param<bool>(lanes, "RstWPhs") = true;
    lanes.resetPhase();
    tick(lanes, 8);
    expect(out(lanes), {0, 0});
    param<vector<float>>(lanes, "Speed") = vector<float>{};
    tick(lanes, 8);
    tick(lanes, 9);
    expect(out(lanes), {1}); // Empty vectors safely fall back to normal speed.

    counter synced;
    synced.setup();
    synced.transport.setDriverMode(TransportDriverMode::FrameStep);
    synced.transport.latchFrameState();
    sync(synced) = true;
    synced.transport.play();
    transportTick(synced, 1);
    expect(out(synced), {1});
    synced.transport.pause();
    transportTick(synced, 2);
    expect(out(synced), {1});
    param<void>(synced, "Reset").trigger();
    transportTick(synced, 0);
    expect(out(synced), {1}); // Reset remains ignored in sync mode.
    param<vector<float>>(synced, "Speed") = vector<float>{2, 0.5f};
    transportTick(synced, 0);
    expect(out(synced), {1, 1});
    synced.transport.play();
    transportTick(synced, 1);
    expect(out(synced), {3, 1.5f});
    synced.transport.seekToBeat(8); // 4 seconds at 120 BPM.
    transportTick(synced, 0);
    expect(out(synced), {7, 2.5f});
    synced.transport.setLoop(true, 2, 6);
    synced.transport.seekToBeat(5);
    transportTick(synced, 1); // Wrap from 3.5 seconds to 1.5 seconds.
    expect(out(synced), {2, 1.25f});
    synced.setTime(100);
    sync(synced) = false;
    tick(synced, 100);
    expect(out(synced), {2, 1.25f});
    tick(synced, 101);
    expect(out(synced), {4, 1.75f});

    // Long-running source clocks retain sub-frame precision after reset.
    counter precise;
    precise.setup();
    tick(precise, 1000000000.0);
    param<void>(precise, "Reset").trigger();
    param<vector<float>>(precise, "Speed") = vector<float>{2};
    tick(precise, 1000000000.0);
    tick(precise, 1000000000.125);
    expect(out(precise), {0.25f});
    cout << "Counter Speed regression passed: legacy equivalence, vectors, continuity, hold, reset, resize, transport and precision.\n";
}
'''


def main():
    of = of_root()
    includes = [ROOT / 'src/Nodes', ROOT / 'src/Nodes/Default_Nodes/Generators',
                ROOT / 'src/Nodes/Default_Nodes/Base', ROOT / 'src/Managers',
                ROOT.parent / 'ofxOceanodeCollection/src']
    includes += sorted({p.parent for p in (of / 'libs/openFrameworks').rglob('*.h')})
    includes += [p for p in (of / 'libs').glob('*/include') if p.is_dir()]
    includes += [of / 'libs/freetype/include/freetype', of / 'libs/cairo/include/cairo']
    libdir = of / 'libs/openFrameworksCompiled/lib/macos'
    frameworks = ['Accelerate', 'AppKit', 'ApplicationServices', 'AudioToolbox',
                  'AVFoundation', 'Cocoa', 'CoreAudio', 'CoreFoundation', 'CoreMedia',
                  'CoreServices', 'CoreVideo', 'IOKit', 'OpenGL', 'QuartzCore',
                  'Security', 'SystemConfiguration']
    with tempfile.TemporaryDirectory(prefix='oceanode-counter-speed-') as folder:
        folder = Path(folder)
        source = folder / 'regression.cpp'
        source.write_text(SOURCE)
        command = [os.environ.get('CXX', 'clang++'), '-std=c++17', '-O1', '-g', '-w',
                   '-DOF_NO_FMOD', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                   *['-I' + str(p) for p in includes], str(source),
                   str(ROOT / 'src/Managers/ofxOceanodeTransport.cpp'),
                   str(of / 'libs/openFrameworks/types/ofParameter.cpp'),
                   str(of / 'libs/openFrameworks/types/ofParameterGroup.cpp'),
                   *[str(p) for p in sorted(libdir.glob('*.a'))], '-Wl,-dead_strip']
        for framework in frameworks:
            command += ['-framework', framework]
        subprocess.run(command + ['-o', str(folder / 'regression')], check=True)
        subprocess.run([str(folder / 'regression')], check=True,
                       env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})


if __name__ == '__main__':
    main()
