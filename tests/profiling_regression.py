#!/usr/bin/env python3
"""CPU accounting, real OF listener hooks and real ImGui capture controls.

Uses a deterministic clock and ASan/UBSan. Controller drawing/accounting are
production code; canvas navigation is excluded. Does not launch a windowed host.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
from timeline_regression import ROOT, of_root
from new_preset_regression import block

PREFIX = r'''
#include "ofMain.h"
#include "ofxOceanodeParameter.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <iostream>
#include <thread>
// Expose snapshots to assert their preservation after graph changes.
#define private public
#include "Controls/ofxOceanodeProfilingController.h"
#undef private
using namespace oceanodeProfiling;
class ofxOceanodeContainer {
public:
    std::shared_ptr<Session> session;
    auto getProfilingSession() const { return session; }
};
class ofxOceanodeCanvas {};
bool ofxOceanodeProfilingController::locate(const std::shared_ptr<Record>&) { return false; }
// This fixture creates no connections or scopes.
void ofxOceanodeAbstractParameter::removeAllConnections() {
    assert(!hasInConnection() && !hasOutConnections() && !isScoped());
}
template<class T> struct Parameter : ofxOceanodeParameter<T> {
    void setRecord(const std::shared_ptr<Record>& record) {
        this->profilingRecord = record;
        this->profilingHook->setRecord(record);
    }
};
static std::int64_t ticks = 0;
static int clockReads = 0;
static std::int64_t now() { ++clockReads; return ticks; }
static void advance(int n) { ticks += n; }
static auto makeRecord(const std::shared_ptr<Session>& session, const char* name) {
    auto record = std::make_shared<Record>();
    record->identity = [name](){ return Record::Identity{name, "Root / Macro 1"}; };
    session->attach(record);
    return record;
}
static void equal(double actual, double expected) {
    if(std::abs(actual - expected) > 1e-6) {
        std::cerr << "Expected " << expected << ", got " << actual << '\n'; std::abort();
    }
}
'''

CASES = r'''
int main() {
    auto session = std::make_shared<Session>(now);
    auto a = makeRecord(session, "A"), b = makeRecord(session, "B"), c = makeRecord(session, "C");
    ofParameter<int> source("Source", 0), input("Input", 0), output("Output", 0), sink("Sink", 0);
    Parameter<int> pa, pb, po, pc;
    pa.bindParameter(source); pa.setRecord(a);
    pb.bindParameter(input); pb.setRecord(b);
    po.bindParameter(output); po.setRecord(b);
    pc.bindParameter(sink); pc.setRecord(c);
    ofEventListeners callbacks;
    callbacks.push(source.newListener([&](int& v){input = v;}));
    callbacks.push(input.newListener([&](int& v){ advance(30); output = v; advance(50); }));
    callbacks.push(output.newListener([&](int& v){sink = v;}));
    callbacks.push(sink.newListener([&](int&){advance(40);}));
    // Off by default: no added callbacks, clock reads or history allocation.
    assert(!session->isRecording());
    assert(source.getNumListeners() == 1 && input.getNumListeners() == 1);
    assert(a->history.empty());
    { Scope update(a, Phase::Update); source = 1; }
    session->nextFrame(); assert(clockReads == 0 && session->frameCount() == 0);

    session->reset(2); session->setRecording(true);
    assert(source.getNumListeners() == 3 && input.getNumListeners() == 3);
    // Ignore the partial frame between the UI button and the next root update.
    source = 2; assert(clockReads == 0);
    session->nextFrame();
    { Scope update(a, Phase::Update); advance(10); source = 3; advance(20); }
    session->nextFrame();
    equal(a->total.selfNs(), 30); equal(a->total.inclusiveNs(), 150);
    equal(b->total.selfNs(), 80); equal(b->total.inclusiveNs(), 120);
    equal(c->total.selfNs(), 40); equal(c->total.inclusiveNs(), 40);
    // Repeated notifications and same-node output scopes never double count.
    { Scope draw(a, Phase::Draw); advance(7); }
    { Scope gui(a, Phase::Gui); advance(3); }
    session->nextFrame(); assert(session->sampleFrames() == 2);
    equal(a->total.selfNs(), 40); equal(a->peakSelfNs(), 30);
    // Rolling eviction includes idle frames and removes the old peak.
    session->nextFrame(); equal(a->total.selfNs(), 10); equal(a->peakSelfNs(), 10);
    equal(b->total.selfNs(), 0);

    // Stop removes every profiling listener and preserves the last samples.
    session->setRecording(false);
    const auto frozen = a->total.selfNs(); const auto frozenFrames = session->frameCount();
    assert(source.getNumListeners() == 1 && sink.getNumListeners() == 1);
    const int reads = clockReads;
    { Scope update(a, Phase::Update); source = 4; }
    session->nextFrame();
    assert(clockReads == reads && session->frameCount() == frozenFrames);
    equal(a->total.selfNs(), frozen);
    session->setRecording(true); assert(session->frameCount() == 0 && session->windowFrames() == 2);
    session->nextFrame();

    // Copies in parameter groups share exactly one hook pair. Destroying the
    // original wrapper must not leave callbacks referring to its address.
    ofParameter<int> copiedValue("Copy", 0);
    std::shared_ptr<ofAbstractParameter> reference;
    {
        Parameter<int> original; original.bindParameter(copiedValue); original.setRecord(a);
        reference = original.newReference();
        assert(copiedValue.getNumListeners() == 2);
    }
    auto copyListener = copiedValue.newListener([&](int&){advance(9);});
    copiedValue = 1; session->nextFrame(); equal(a->total.selfNs(), 9);
    reference.reset(); assert(copiedValue.getNumListeners() == 1);

    // Group listeners can recursively notify the same group through another
    // parameter. The timing stack must preserve both scopes and avoid overlap.
    ofParameter<int> first("First", 0), second("Second", 0);
    ofParameterGroup group; group.add(first); group.add(second);
    auto groupHook = ParameterHook::bindGroup(group); groupHook->setRecord(b);
    auto groupListener = group.parameterChangedE().newListener([&](ofAbstractParameter& parameter){
        advance(5); if(parameter.getName() == "First") second = first.get(); advance(6);
    });
    first = 1; session->nextFrame(); equal(b->pending.selfNs(), 0);
    equal(b->total.selfNs(), 22); equal(b->total.inclusiveNs(), 22);

    // Void triggers, custom receive callbacks and notifications from workers.
    ofParameter<void> trigger("Trigger");
    Parameter<void> pv; pv.bindParameter(trigger); pv.setRecord(c);
    auto triggerListener = trigger.newListener([&](){advance(12);});
    trigger.trigger();
    pc.addReceiveFunc<int>([&](const int&){advance(15);});
    assert(pc.receiveParameter(&source));
    const auto workerReads = clockReads;
    std::thread worker([&](){ trigger.trigger(); }); worker.join();
    assert(clockReads == workerReads);
    session->nextFrame(); equal(c->total.selfNs(), 27);

    // An OF listener may consume an event, skipping our final callback.
    // Discard that frame, then recover without charging subsequent nodes.
    auto consumer = source.newListener([](int&)->bool { return true; }, -100);
    const auto completed = session->frameCount(); source = 5; session->nextFrame();
    assert(session->frameCount() == completed && session->incompleteSpans() > 0);
    consumer.reset(); source = 6; session->nextFrame();
    assert(session->frameCount() == completed + 1);
    equal(a->pending.selfNs(), 0);

    // Moving between sessions cannot leave duplicate registrations/history.
    auto other = std::make_shared<Session>(now);
    other->attach(c); session->attach(c);
    { Scope draw(c, Phase::Draw); advance(10); }
    session->nextFrame(); equal(c->total.selfNs(), 10);
    session->setRecording(false);
    assert(trigger.getNumListeners() == 1);
    assert(group.parameterChangedE().size() == 1);

    // Production ImGui controller: Profile! / Stop, sorting, filtering, narrow
    // window clipping and frozen snapshots after metadata/node changes.
    ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr;
    io.DisplaySize = {1600, 900}; io.DeltaTime = 1.0f/60;
    unsigned char* pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    auto root = std::make_shared<ofxOceanodeContainer>(); root->session = session;
    ofxOceanodeCanvas canvas; ofxOceanodeProfilingController panel(root, &canvas);
    ImVec2 button;
    float panelWidth = 1500;
    auto frame = [&](){
        ImGui::NewFrame(); ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({panelWidth,600});
        ImGui::Begin("Profiling"); button = ImGui::GetCursorScreenPos(); button.x += 10; button.y += 10;
        panel.draw(); ImGui::End(); ImGui::Render();
        for(const auto* list : ImGui::GetDrawData()->CmdLists) for(const auto& v : list->VtxBuffer)
            assert(std::isfinite(v.pos.x) && std::isfinite(v.pos.y));
    };
    auto click = [&](){
        io.AddMousePosEvent(button.x, button.y); frame();
        io.AddMouseButtonEvent(0, true); frame();
        io.AddMouseButtonEvent(0, false); frame();
    };
    frame(); assert(!session->isRecording() && panel.rows.empty());
    panel.pendingWindow = 60;
    click(); assert(session->isRecording() && session->windowFrames() == 60);
    session->nextFrame();
    { Scope update(a, Phase::Update); advance(40); }
    { Scope draw(b, Phase::Draw); advance(80); }
    session->nextFrame(); panel.nextRefresh = 0; panel.update(); frame();
    assert(panel.displayedFrames == 1 && panel.rows.size() == 3);
    assert(panel.rows.front().name == "B");
    click(); assert(!session->isRecording());
    const auto panelTotal = panel.measuredMs; const auto savedName = panel.rows.front().name;
    b->identity = [](){return Record::Identity{"Renamed", "Changed path"};};
    for(int i=0; i<10; ++i){ source = i + 50; session->nextFrame(); panel.update(); }
    equal(panel.measuredMs, panelTotal); assert(panel.rows.front().name == savedName);
    panelWidth = 280; frame();
    panel.filter.InputBuf[0] = 'Z'; panel.filter.InputBuf[1] = 0; panel.filter.Build(); frame();
    assert(!session->isRecording());
    ImGui::DestroyContext();
    std::cout << "Profiling regression passed: off-by-default/no listeners or clocks, nested self/inclusive, rolling samples, copy lifetime, groups/reentry, triggers, custom receive, workers, consumed-event recovery, session moves, capture buttons, frozen results and ImGui layout.\n";
}
'''

def main():
    of = of_root()
    imgui = of / "addons/ofxImGui/libs/imgui/src"
    includes = [ROOT / "src", ROOT / "src/Controls", imgui]
    for directory in ("src", "libs"):
        includes += sorted({p.parent for p in (of / "addons/ofxImGui" / directory).rglob("*.h")})
    includes += sorted({p.parent for p in (of / "libs/openFrameworks").rglob("*.h")})
    includes += [p for p in (of / "libs").glob("*/include") if p.is_dir()]
    includes += [of / "libs/freetype/include/freetype", of / "libs/cairo/include/cairo"]
    libdir = of / "libs/openFrameworksCompiled/lib/macos"
    libraries = [str(p) for p in sorted(libdir.glob("*.a")) if "openframeworks" not in p.name.lower()]
    frameworks = ["Accelerate", "AppKit", "ApplicationServices", "AudioToolbox", "AVFoundation", "Cocoa",
                  "CoreAudio", "CoreFoundation", "CoreMedia", "CoreServices", "CoreVideo", "IOKit", "OpenGL", "QuartzCore", "Security"]
    source = (ROOT / "src/Controls/ofxOceanodeProfilingController.cpp").read_text()
    production = "\nusing namespace oceanodeProfiling;\n" + "\n".join(block(source, signature) for signature in (
        "ofxOceanodeProfilingController::ofxOceanodeProfilingController(",
        "ofxOceanodeProfilingController::~ofxOceanodeProfilingController()",
        "void ofxOceanodeProfilingController::update()", "void ofxOceanodeProfilingController::refresh()",
        "void ofxOceanodeProfilingController::sortRows(", "void ofxOceanodeProfilingController::draw()"))
    with tempfile.TemporaryDirectory(prefix="oceanode-profiling-") as folder:
        folder = Path(folder)
        cpp = folder / "regression.cpp"; cpp.write_text(PREFIX + production + CASES)
        binary = folder / "regression"
        command = [os.environ.get("CXX", "clang++"), "-std=c++17", "-O0", "-g", "-w", "-DOF_NO_FMOD",
                   "-fsanitize=address,undefined", "-fno-omit-frame-pointer", *["-I" + str(p) for p in includes],
                   str(cpp), str(ROOT / "src/Controls/ofxOceanodeBaseController.cpp"),
                   *[str(imgui / s) for s in ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")],
                   str(of / "libs/openFrameworks/types/ofParameter.cpp"),
                   str(of / "libs/openFrameworks/types/ofParameterGroup.cpp"),
                   str(libdir / "openFrameworks.a"), *libraries, "-Wl,-dead_strip"]
        for framework in frameworks: command += ["-framework", framework]
        subprocess.run(command + ["-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})

NATIVE_CASES = r'''
class ProfileProbe : public ofxOceanodeNodeModel {
public:
    ProfileProbe() : ofxOceanodeNodeModel("Profile Probe") {
        addParameter(input.set("Input", 0.f));
        addOutputParameter(output.set("Output", 0.f));
        addInspectorParameter(option.set("Option", 0.f));
        listeners.push(input.newListener([this](float&){ work(); }));
        listeners.push(option.newListener([this](float&){ work(); }));
    }
    void setup() override {
        // Exercise the type-erased node parameter registration path too.
        ofAbstractParameter& erased = extra.set("Extra", 0.f);
        addParameter(erased);
        listeners.push(extra.newListener([this](float&){work();}));
    }
    void update(ofEventArgs&) override { work(); output = ++updates; }
    void draw(ofEventArgs&) override { work(); }
    static void work() { volatile int value = 0; for(int i=0; i<1000; ++i) value += i; }
    ofParameter<float> input, output, option, extra;
    ofEventListeners listeners;
    int updates = 0;
};
int main(int argc, char** argv) {
    using namespace oceanodeProfiling;
    assert(argc == 3); std::filesystem::current_path(argv[2]); ofDisableDataPath(); ofSeedRandom();
    auto window = std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry = std::make_shared<ofxOceanodeNodeRegistry>();
    auto types = ofxOceanodeTypesRegistry::getInstance(); types->registerType<float>("f");
    // An external transport excludes the fixture's deliberately stubbed
    // timeline service; profiling sessions are independent of that ownership.
    ofxOceanodeContainer root(registry, types, std::make_shared<ofxOceanodeTransport>());
    root.setCanvasID("ProfilingTest");
    auto& source = root.createNode(std::make_unique<ProfileProbe>(), 1);
    auto& sink = root.createNode(std::make_unique<ProfileProbe>(), 2, true);
    auto& a = static_cast<ProfileProbe&>(source.getNodeModel());
    auto& b = static_cast<ProfileProbe&>(sink.getNodeModel());
    root.createConnection(static_cast<ofxOceanodeAbstractParameter&>(source.getParameters().get("Output")),
                          static_cast<ofxOceanodeAbstractParameter&>(sink.getParameters().get("Input")));
    auto session = root.getProfilingSession();
    auto ar = a.getProfilingRecord(), br = b.getProfilingRecord();
    assert(ar->session.lock() == session && br->session.lock() == session);
    assert(!session->isRecording() && a.input.getNumListeners() == 1 && a.extra.getNumListeners() == 1);
    root.update(); root.draw(); assert(ar->pending.selfNs() == 0);
    session->setRecording(true); session->nextFrame();
    assert(a.input.getNumListeners() == 3 && a.option.getNumListeners() == 3 && a.extra.getNumListeners() == 3);
    a.option = 1; a.extra = 1;
    root.update(); root.draw(); session->nextFrame();
    assert(session->frameCount() == 1);
    for(const auto& record : {ar, br}) {
        assert(record->total.phases[static_cast<unsigned>(Phase::Update)].calls == 1);
        assert(record->total.phases[static_cast<unsigned>(Phase::Draw)].calls == 1);
        assert(record->total.phases[static_cast<unsigned>(Phase::Parameters)].calls > 0);
        assert(record->total.selfNs() > 0 && record->total.inclusiveNs() >= record->total.selfNs());
    }
    // Nodes created while recording, including nested Macro children, join the
    // same session immediately. Persistent nodes above are covered as well.
    auto& macroNode = root.createNode(std::make_unique<ofxOceanodeNodeMacro>(), 1);
    auto& macro = static_cast<ofxOceanodeNodeMacro&>(macroNode.getNodeModel());
    auto nested = macro.getContainer();
    assert(nested->getProfilingSession() == session);
    auto& child = nested->createNode(std::make_unique<ProfileProbe>(), 1);
    auto& childModel = static_cast<ProfileProbe&>(child.getNodeModel());
    auto cr = childModel.getProfilingRecord();
    assert(cr->session.lock() == session && childModel.input.getNumListeners() == 3);
    // Macro::newNodeCreated updates its child immediately, outside the Macro's
    // lifecycle scope. Compare only the later work nested inside the Macro.
    const auto creationTime = cr->pending.inclusiveNs();
    root.update(); root.draw(); session->nextFrame();
    assert(session->frameCount() == 2 && cr->total.selfNs() > 0);
    assert(macro.getProfilingRecord()->total.inclusiveNs() >= cr->total.inclusiveNs() - creationTime);
    session->setRecording(false);
    assert(a.input.getNumListeners() == 1 && a.option.getNumListeners() == 1 && a.extra.getNumListeners() == 1);
    assert(childModel.input.getNumListeners() == 1);
    const auto frozen = ar->total.selfNs(); root.update(); root.draw(); session->nextFrame();
    assert(ar->total.selfNs() == frozen && session->frameCount() == 2);
    // Captured records may outlive their model; metadata must not retain it.
    session->setRecording(true); session->nextFrame();
    nested->clearContainer(); assert(!cr->identity);
    root.clearContainer(); assert(!ar->identity && !br->identity);
    session->nextFrame(); session->setRecording(false);
    assert(session->liveRecords().empty());
    std::cout << "Native profiling wiring passed: lifecycle, typed/type-erased/Inspector listeners, persistent nodes, creation during capture, nested Macros, stop/freeze and graph teardown.\n";
}
'''

def native_main():
    import macro_mixer_regression as native
    native.CASES = NATIVE_CASES
    native.main()

if __name__ == "__main__":
    native_main() if "--native" in sys.argv else main()
