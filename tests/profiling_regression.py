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
from of_environment import ROOT, of_root
from new_preset_regression import block

PREFIX = r'''
#include "ofMain.h"
#include "ofxOceanodeParameter.h"
#include "ofxOceanodeGlDiagnostics.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <unordered_set>
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
ofxOceanodeAbstractParameter::~ofxOceanodeAbstractParameter() = default;
template<class T> struct Parameter : ofxOceanodeParameter<T> {
    void setRecord(const std::shared_ptr<Record>& record) {
        this->profilingRecord = record;
        this->profilingHook->setRecord(record);
    }
    auto publication() const { return this->profilingHook->publication; }
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
static void publicationChecks() {
    constexpr int frameNs = 16666667;
    auto session = std::make_shared<Session>(now);
    auto record = makeRecord(session, "Publications");
    ofParameter<int> first("First", 0), second("Second", 0), input("Input", 0);
    Parameter<int> a, b, in;
    a.bindParameter(first); a.setFlags(ofxOceanodeParameterFlags_DisableInConnection); a.setRecord(record);
    b.bindParameter(second); b.setFlags(ofxOceanodeParameterFlags_DisableInConnection); b.setRecord(record);
    in.bindParameter(input); in.setFlags(ofxOceanodeParameterFlags_DisableOutConnection); in.setRecord(record);
    ofParameterGroup group; group.add(first);
    auto groupHook = ParameterHook::bindGroup(group); groupHook->setRecord(record);
    // Multiple receivers and group events do not multiply publications.
    int receiver1 = 0, receiver2 = 0;
    auto l1 = first.newListener([&](int& v){receiver1 = v;});
    auto l2 = first.newListener([&](int& v){receiver2 = v;});
    assert(first.getNumListeners() == 2 && a.publication()->history.empty());
    const int offReads = clockReads; first = 7; session->nextFrame(); assert(clockReads == offReads);
    session->reset(60); session->setRecording(true); session->nextFrame();
    for(int i=0; i<60; ++i){
        first = 7; second = 7; input = 7; // Same value still emits a notification.
        advance(frameNs); session->nextFrame();
    }
    auto stats = session->publicationStats(*a.publication());
    assert(stats.count == 60 && receiver1 == 7 && receiver2 == 7);
    equal(stats.perFrame, 1); equal(stats.perSecond, 1e9 / frameNs); equal(stats.intervalMs, frameNs / 1e6);
    assert(session->publicationStats(*b.publication()).count == 60);
    assert(session->publicationStats(*in.publication()).count == 0);
    {
        auto root = std::make_shared<ofxOceanodeContainer>(); root->session = session;
        ofxOceanodeCanvas canvas; ofxOceanodeProfilingController panel(root, &canvas);
        panel.refresh(); assert(panel.rows.size() == 1 && panel.rows[0].outputs.size() == 2);
        equal(panel.rows[0].outputsPerFrame, 2);
        equal(panel.rows[0].outputsPerSecond, 2e9 / frameNs);
        assert(panel.rows[0].outputIntervalMs < 0); // Two independent output pins.
        equal(panel.rows[0].outputs[0].stats.intervalMs, frameNs / 1e6);
        equal(panel.rows[0].outputs[1].stats.perSecond, 1e9 / frameNs);
        // This scope owns the controller, so restore capture after its destructor.
    }
    // The controller destructor stopped capture; start a new 60-frame window.
    session->setRecording(true); session->nextFrame();
    for(int i=0; i<60; ++i){advance(frameNs); session->nextFrame();}
    stats = session->publicationStats(*a.publication());
    assert(stats.count == 0 && stats.intervalMs < 0); equal(stats.perSecond, 0);
    // Idle frames stay in the denominator; each alternate frame is 30 Hz.
    session->reset(); session->nextFrame();
    for(int i=0; i<60; ++i){ if(i%2 == 0) first = 7; advance(frameNs); session->nextFrame(); }
    stats = session->publicationStats(*a.publication());
    equal(stats.perFrame, .5); equal(stats.perSecond, 1e9 / frameNs / 2); equal(stats.intervalMs, 2 * frameNs / 1e6);
    session->reset(); session->nextFrame();
    for(int i=0; i<30; ++i){first = 7; advance(2 * frameNs); session->nextFrame();}
    stats = session->publicationStats(*a.publication());
    equal(stats.perFrame, 1); equal(stats.perSecond, 1e9 / frameNs / 2); equal(stats.intervalMs, 2 * frameNs / 1e6);
    // Bursts in the same frame, zero-time intervals, and shared wrapper copies.
    session->reset(3); session->nextFrame();
    auto copy = a.newReference(); first = 7; first = 7;
    advance(frameNs); session->nextFrame();
    stats = session->publicationStats(*a.publication());
    assert(stats.count == 2); equal(stats.intervalMs, 0); equal(stats.perFrame, 2);
    // Void pins share the same base direction flags and publication machinery.
    ofParameter<void> trigger("Trigger"); Parameter<void> v;
    v.bindParameter(trigger); v.setFlags(ofxOceanodeParameterFlags_DisableInConnection); v.setRecord(record);
    assert(v.getFlags() == static_cast<ofxOceanodeAbstractParameter&>(v).getFlags());
    trigger.trigger(); advance(5000000); trigger.trigger(); advance(frameNs); session->nextFrame();
    auto triggerStats = session->publicationStats(*v.publication());
    assert(triggerStats.count == 2); equal(triggerStats.intervalMs, 5);
    a.setFlags(ofxOceanodeParameterFlags_DisableOutConnection);
    first = 8; assert(a.publication()->pending.count == 0);
    a.setFlags(ofxOceanodeParameterFlags_DisableInConnection);
    first = 9; assert(a.publication()->pending.count == 1);
    // A discarded frame breaks the interval chain, not the whole capture.
    session->reset(10); session->nextFrame(); first = 1; advance(frameNs); session->nextFrame();
    auto consume = first.newListener([](int&)->bool{return true;}, -100);
    first = 2; advance(frameNs); session->nextFrame(); consume.reset();
    first = 3; advance(frameNs); session->nextFrame();
    stats = session->publicationStats(*a.publication());
    assert(stats.count == 2 && stats.intervalMs < 0);
    first = 4; advance(frameNs); session->nextFrame();
    equal(session->publicationStats(*a.publication()).intervalMs, frameNs / 1e6);
    auto otherOwner = makeRecord(session, "Other owner");
    a.setRecord(otherOwner); first = 8;
    a.setRecord(record); first = 9; advance(frameNs); session->nextFrame();
    assert(session->publicationStats(*a.publication()).count == 1);
    unsigned registered = 0; record->forEachPublication([&](Publication&){++registered;});
    assert(registered == 4);
    unsigned oldOwner = 0; otherOwner->forEachPublication([&](Publication&){++oldOwner;}); assert(oldOwner == 0);
    // Worker notifications and stopped captures never change the counters.
    const auto beforeWorker = a.publication()->pending.count;
    const int workerReads = clockReads;
    std::thread worker([&](){first = 5;}); worker.join();
    assert(clockReads == workerReads && a.publication()->pending.count == beforeWorker);
    session->setRecording(false); stats = session->publicationStats(*a.publication());
    const int stoppedReads = clockReads; first = 6; advance(frameNs); session->nextFrame();
    assert(clockReads == stoppedReads && session->publicationStats(*a.publication()).count == stats.count);
    equal(session->publicationStats(*a.publication()).perSecond, stats.perSecond);
    session->setRecording(true); session->nextFrame(); advance(frameNs); session->nextFrame();
    assert(session->publicationStats(*a.publication()).count == 0);
    session->setRecording(false);
    ticks = 0; clockReads = 0;
}

int main() {
    publicationChecks();
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
    oceanodeGlDiagnostics::enabled() = false;
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr;
    io.DisplaySize = {1600, 900}; io.DeltaTime = 1.0f/60;
    unsigned char* pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    auto root = std::make_shared<ofxOceanodeContainer>(); root->session = session;
    ofxOceanodeCanvas canvas; ofxOceanodeProfilingController panel(root, &canvas);
    ImVec2 button;
    float panelWidth = 1500;
    auto frame = [&](){
        ImGui::NewFrame(); ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({panelWidth,600});
        ImGui::Begin("Profiling"); button = ImGui::GetCursorScreenPos(); button.x += 10;
        button.y += ImGui::GetFrameHeightWithSpacing() + 10; // GL trace checkbox precedes Profile!.
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
    for(auto& row : panel.rows) row.expanded = true;
    frame(); // Expanded output rows use the same clipper height as node rows.
    click(); assert(!session->isRecording());
    const auto panelTotal = panel.measuredMs; const auto savedName = panel.rows.front().name;
    b->identity = [](){return Record::Identity{"Renamed", "Changed path"};};
    for(int i=0; i<10; ++i){ source = i + 50; session->nextFrame(); panel.update(); }
    equal(panel.measuredMs, panelTotal); assert(panel.rows.front().name == savedName);
    assert(!panel.rows.front().outputs.empty());
    const auto savedRate = panel.rows.front().outputs.front().stats.perSecond;
    panelWidth = 280; frame();
    panel.filter.InputBuf[0] = 'Z'; panel.filter.InputBuf[1] = 0; panel.filter.Build(); frame();
    assert(!session->isRecording());
    equal(panel.rows.front().outputs.front().stats.perSecond, savedRate);
    ImGui::DestroyContext();
    std::cout << "Profiling regression passed: disabled capture, nested CPU accounting, output publication rates/intervals, idle frames, bursts, direction flags, copies/fanout, workers, discarded frames, owner moves, capture buttons, frozen results and expanded ImGui rows.\n";
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
        addOutputParameter(trigger.set("Trigger"));
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
    void update(ofEventArgs&) override { work(); output = ++updates; trigger.trigger(); }
    void draw(ofEventArgs&) override { work(); }
    static void work() { volatile int value = 0; for(int i=0; i<1000; ++i) value += i; }
    ofParameter<float> input, output, option, extra;
    ofParameter<void> trigger;
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
        bool outputSeen = false, triggerSeen = false;
        record->forEachPublication([&](Publication& publication){
            assert(publication.name() != "Option"); // Inspector timing is not output publication.
            if(publication.name() == "Output" || publication.name() == "Trigger"){
                assert(publication.outputCapable && !publication.bidirectional);
                assert(session->publicationStats(publication).count == 1);
                outputSeen |= publication.name() == "Output";
                triggerSeen |= publication.name() == "Trigger";
            }
        });
        assert(outputSeen && triggerSeen);
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
