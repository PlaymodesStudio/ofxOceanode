#!/usr/bin/env python3
"""Exercise the Event Counter 2 production header and real ImGui without a host app."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = ROOT.parent / "ofxImGui/libs/imgui/src"

MODEL = r'''
#pragma once
#include <cfloat>
#include <climits>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
struct ofEventArgs {};
struct ofJson {};
struct ofEventListeners { void push(int) {} };
template<class T> struct ofParameter {
    T value{};
    bool notifying = false;
    std::vector<std::function<void(T&)>> callbacks;
    ofParameter& set(const std::string&, const T& v) { value = v; return *this; }
    ofParameter& set(const std::string& s, const T& v, const T&, const T&) { return set(s, v); }
    const T& get() const { return value; }
    ofParameter& operator=(const T& v) {
        value = v;
        if(!notifying) {
            notifying = true;
            for(auto& f : callbacks) f(value);
            notifying = false;
        }
        return *this;
    }
    template<class F> int newListener(F f) { callbacks.push_back(f); return 0; }
};
template<> struct ofParameter<void> {
    std::vector<std::function<void()>> callbacks;
    ofParameter& set(const std::string&) { return *this; }
    void trigger() { for(auto& f : callbacks) f(); }
    template<class F> int newListener(F f) { callbacks.push_back(f); return 0; }
};
inline std::vector<std::string> logs;
struct Logger {
    std::ostringstream text;
    template<class T> Logger& operator<<(const T& v) { text << v; return *this; }
    ~Logger() { logs.push_back(text.str()); }
};
inline Logger ofLogNotice(const std::string&) { return {}; }
inline std::string ofToString(unsigned int n) { return std::to_string(n); }
struct ofxOceanodeNodeModel {
    std::string description, canvasID = "Canvas";
    explicit ofxOceanodeNodeModel(const std::string&) {}
    virtual ~ofxOceanodeNodeModel() = default;
    virtual void draw(ofEventArgs&) {}
    virtual void presetWillBeLoaded() {}
    virtual void loadBeforeConnections(ofJson&) {}
    virtual void presetRecallBeforeSettingParameters(ofJson&) {}
    virtual void presetRecallAfterSettingParameters(ofJson&) {}
    virtual void presetHasLoaded() {}
    unsigned int getNumIdentifier() const { return 1; }
    template<class T> void addParameter(ofParameter<T>&) {}
    template<class T> void addOutputParameter(ofParameter<T>&) {}
};
'''

CASES = r'''
#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <limits>
#include <sstream>
#include <vector>
#include <cassert>
// Inspect private state to supply exact monotonic timestamps, without test hooks in production.
#define private public
#include "eventCounter2.h"
#undef private

static void near(float a, float b) { assert(std::abs(a - b) <= std::max(.001f, std::abs(b) * 1e-6f)); }
static auto at(int ms) { return eventCounter2::Clock::time_point(std::chrono::milliseconds(ms)); }
static void event(eventCounter2& n, int ms, std::vector<float> values = {1}) {
    n.input.value = values;
    n.recordEvent(n.input.get(), at(ms));
}
static ofEventArgs args;
static void frame(eventCounter2& n) {
    ImGui::NewFrame(); n.draw(args); ImGui::Render();
}
static void click(eventCounter2& n, ImVec2 p) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(p.x, p.y); frame(n); frame(n);
    io.AddMouseButtonEvent(0, true); frame(n);
    io.AddMouseButtonEvent(0, false); frame(n); frame(n);
}
int main() {
    eventCounter2 n;
    assert(!n.show.get() && n.sampling);
    int ticks = 0;
    n.tick.newListener([&] {
        ++ticks;
        // Every Tick must see the current count and both current timing outputs.
        assert(n.count.get() == std::vector<int>(n.vec.get() ? n.input.get().size() : 1, n.eventCount));
        near(n.fps.get(), n.intervalMs.get() > 0 ? 1000.f / n.intervalMs.get() : 0.f);
    });
    event(n, 1000, {.25f, .8f, 1.f});
    assert(n.count.get() == std::vector<int>{1}); near(n.intervalMs.get(), 0);
    event(n, 1020); near(n.intervalMs.get(), 20); near(n.fps.get(), 50);
    event(n, 1020); near(n.intervalMs.get(), 0); near(n.fps.get(), 0);
    assert(ticks == 3);
    n.resetNext.trigger(); event(n, 2000); assert(n.eventCount == 0); near(n.intervalMs.get(), 0);
    event(n, 2020); assert(n.eventCount == 1); near(n.fps.get(), 50);
    n.resetNext.trigger(); n.reset.trigger();
    assert(!n.resetOnNextEvent && n.eventCount == 0 && ticks == 5);
    event(n, 3000); assert(n.eventCount == 1); near(n.intervalMs.get(), 0);

    event(n, 3020, {1, 2, 3});
    int before = ticks;
    n.vec = true; assert(n.count.get() == std::vector<int>({2, 2, 2}) && ticks == before);
    n.reset.trigger(); assert(n.count.get() == std::vector<int>({0, 0, 0}) && ticks == before);
    event(n, 4000, {}); assert(n.count.get().empty());
    n.vec = false; assert(n.count.get() == std::vector<int>{1});
    n.verbose = true; event(n, 4020, {.25f, .8f, 1.f});
    assert(logs.back() == "Count: 2 | Interval: 20 ms | FPS: 50 | Input: [0.25, 0.8, 1]");
    n.verbose = false;
    // Test the actual listener too: identical notifications are separate events.
    before = ticks;
    n.input = std::vector<float>{.25f, .8f, 1.f};
    n.input = std::vector<float>{.25f, .8f, 1.f};
    assert(ticks == before + 2);

    auto frozen = n.history.size();
    n.sampling = false; event(n, 5000); event(n, 5020);
    assert(n.history.size() == frozen); near(n.fps.get(), 50);
    n.sampling = true; event(n, 6000); assert(n.history.back().timeMs > n.history.front().timeMs);
    for(int i = 0; i < 1100; ++i) event(n, 7000 + i);
    assert(n.history.size() == 1000);
    n.eventCount = INT_MAX; event(n, 9000); assert(n.eventCount == INT_MAX);

    ofJson json;
    before = ticks;
    n.presetWillBeLoaded();
    n.input = std::vector<float>{3}; assert(ticks == before);
    n.presetRecallBeforeSettingParameters(json);
    n.input = std::vector<float>{4};
    n.presetRecallAfterSettingParameters(json);
    n.input = std::vector<float>{5}; assert(ticks == before);
    n.presetHasLoaded();
    assert(n.eventCount == 0 && n.history.empty() && !n.hasPreviousEvent && n.sampling);
    event(n, 10000); assert(n.eventCount == 1); near(n.intervalMs.get(), 0);
    before = ticks;
    // New nodes join the container load before connections, without a will-load callback.
    n.loadBeforeConnections(json); n.input = std::vector<float>{6};
    n.presetHasLoaded(); assert(ticks == before && n.eventCount == 0);
    // Standalone node recall also finishes without waiting for a container callback.
    n.presetRecallBeforeSettingParameters(json); n.input = std::vector<float>{7};
    n.presetRecallAfterSettingParameters(json);
    n.input = std::vector<float>{7}; assert(ticks == before + 1 && n.eventCount == 1);

    ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = ImVec2(1200, 800); io.DeltaTime = 1.f / 60;
    unsigned char* pixels; int w, h; io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    const char* title = "Event Counter 2 1 - Datagram";
    frame(n); assert(!ImGui::FindWindowByName(title));
    n.show = true; frame(n); frame(n);
    auto* window = ImGui::FindWindowByName(title); assert(window && window->Active);
    const ImVec2 sampleButton(window->Pos.x + ImGui::GetStyle().WindowPadding.x + 6,
                             window->Pos.y + window->TitleBarHeight + ImGui::GetStyle().WindowPadding.y + 6);
    click(n, sampleButton); assert(!n.sampling);
    const auto historySize = n.history.size();
    const int countBefore = n.eventCount;
    event(n, 11000); assert(n.eventCount == countBefore + 1 && n.history.size() == historySize);
    click(n, sampleButton); assert(n.sampling);
    n.show = false; event(n, 11020); assert(n.history.size() == historySize + 1);
    n.show = true; frame(n); frame(n);
    const ImVec2 close(window->Pos.x + window->Size.x - ImGui::GetStyle().FramePadding.x - ImGui::GetFontSize() * .5f,
                       window->Pos.y + window->TitleBarHeight * .5f);
    click(n, close); assert(!n.show.get());

    // Render irregularly spaced samples, changing vector widths and nonfinite values.
    n.history.clear(); n.graphOrigin = at(0);
    event(n, 1000, {0, 1}); event(n, 1010, {.5f}); event(n, 1040, {1, std::numeric_limits<float>::quiet_NaN()});
    ImGui::NewFrame(); ImGui::SetNextWindowSize(ImVec2(680, 360)); ImGui::Begin("Plot test");
    auto origin = ImGui::GetCursorScreenPos(); auto available = ImGui::GetContentRegionAvail();
    auto* draw = ImGui::GetWindowDrawList(); int start = draw->VtxBuffer.Size;
    n.drawDatagram();
    const float middleX = origin.x + 70.f + (available.x - 82.f) * .25f;
    bool foundDot = false;
    for(int i = start; i < draw->VtxBuffer.Size; ++i) {
        const auto& v = draw->VtxBuffer[i];
        assert(std::isfinite(v.pos.x) && std::isfinite(v.pos.y));
        if(v.col == n.traceColor(0) && std::abs(v.pos.x - middleX) < 3) foundDot = true;
    }
    assert(foundDot); // 10 ms in a 40 ms span is one quarter across, not one half.
    ImGui::End(); ImGui::Render();

    // Dense 32-channel traces retain short peaks at their true times within a
    // shared point budget, while the full event history remains available.
    eventCounter2 dense;
    for(int i = 0; i < 1000; ++i) {
        std::vector<float> values(32, 0.0f);
        if(i == 11) values[0] = 100.0f;
        if(i == 12) values[0] = -80.0f;
        event(dense, i, values);
    }
    dense.rebuildPlot(600);
    assert(dense.history.size() == 1000 && dense.plotStrips.size() == 32 && !dense.plotMarkers);
    std::size_t points = 0;
    for(const auto &strip : dense.plotStrips) points += strip.points.size();
    assert(points <= dense.PlotPointBudget);
    bool positivePeak = false, negativePeak = false;
    float previousX = -1;
    for(const auto &point : dense.plotStrips[0].points) {
        assert(point.x >= previousX); previousX = point.x;
        if(std::abs(point.x - 11.f / 999.f) < 1e-6f && point.y > .9f) positivePeak = true;
        if(std::abs(point.x - 12.f / 999.f) < 1e-6f && point.y < .1f) negativePeak = true;
    }
    assert(positivePeak && negativePeak);
    assert(!dense.plotDirty);
    dense.sampling = false; event(dense, 2000, std::vector<float>(32, 1));
    assert(!dense.plotDirty); // Frozen/unchanged history reuses the prepared traces.
    dense.sampling = true; event(dense, 2100, std::vector<float>(32, 1));
    assert(dense.plotDirty); dense.rebuildPlot(600); near(dense.plotStrips[0].points.back().x, 1.f);

    eventCounter2 gaps;
    event(gaps, 0, {1, 0}); event(gaps, 10, {2}); event(gaps, 20, {3, 1});
    event(gaps, 30, {4, std::numeric_limits<float>::infinity()}); event(gaps, 40, {5, 2});
    gaps.rebuildPlot(600);
    assert(gaps.plotStrips.size() == 4); // One continuous first channel, three isolated second-channel values.
    for(std::size_t i = 1; i < gaps.plotStrips.size(); ++i) assert(gaps.plotStrips[i].points.size() == 1);
    ImGui::DestroyContext();
    puts("PASS Event Counter 2: count, timing/FPS, Tick ordering, resets, vectors, ms logs, repeated events, bounded history, preset recall, real ImGui Sample/Show, timestamps, dense point budget, peaks and channel gaps.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--header", type=Path, default=ROOT.parent / "ofxSantiNodes/src/eventCounter2.h")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="event-counter2-test-") as directory:
        work = Path(directory)
        (work / "ofxOceanodeNodeModel.h").write_text(MODEL)
        (work / "imconfig.h").write_text("")
        source = work / "test.cpp"
        source.write_text(CASES)
        binary = work / "test"
        subprocess.run([
            os.environ.get("CXX", "clang++"), "-std=c++17", "-O0", "-g",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I" + str(work), "-I" + str(args.header.resolve().parent), "-I" + str(IMGUI),
            str(source), *[str(IMGUI / name) for name in ("imgui.cpp", "imgui_draw.cpp", "imgui_widgets.cpp", "imgui_tables.cpp")],
            "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True, timeout=30,
                       env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})


if __name__ == "__main__":
    main()
