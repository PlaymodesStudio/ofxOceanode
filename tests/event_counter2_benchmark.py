#!/usr/bin/env python3
"""Measure Event Counter 2's real ImGui CPU drawing cost (no GPU/host app)."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

from event_counter2_regression import IMGUI, MODEL, ROOT

CODE = r'''
#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <limits>
#include <sstream>
#include <vector>
#define private public
#include "eventCounter2.h"
#undef private

int main() {
    ImGui::CreateContext(); auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.DisplaySize = ImVec2(1200, 800); io.DeltaTime = 1.f / 60;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    unsigned char* pixels; int w, h; io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    for(int channels : {1, 32, 128}) {
        eventCounter2 n; n.graphOrigin = eventCounter2::Clock::time_point{};
        auto sample = [&](int i) {
            std::vector<float> v(channels);
            for(int c = 0; c < channels; ++c) v[c] = std::sin(i * .05f + c * .2f);
            n.input.value = v;
            n.recordEvent(v, n.graphOrigin + std::chrono::milliseconds(i * 16));
        };
        for(int i = 0; i < 1000; ++i) sample(i);
        std::vector<double> timings;
        int vertices = 0, indices = 0;
        for(int i = 0; i < 220; ++i) {
            sample(1000 + i);
            auto start = std::chrono::steady_clock::now();
            ImGui::NewFrame(); ImGui::SetNextWindowSize(ImVec2(680, 360)); ImGui::Begin("Benchmark");
            n.drawDatagram(); ImGui::End(); ImGui::Render();
            double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
            if(i >= 20) timings.push_back(us);
            vertices = ImGui::GetDrawData()->TotalVtxCount; indices = ImGui::GetDrawData()->TotalIdxCount;
        }
        std::sort(timings.begin(), timings.end());
        std::printf("%3d channels: median %.1f us/frame, p95 %.1f us/frame, %d vertices, %d indices\n",
                    channels, timings[timings.size()/2], timings[timings.size()*95/100], vertices, indices);
    }
    ImGui::DestroyContext();
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--header", type=Path, default=ROOT.parent / "ofxSantiNodes/src/eventCounter2.h")
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="event-counter2-bench-") as directory:
        work = Path(directory)
        (work / "ofxOceanodeNodeModel.h").write_text(MODEL)
        (work / "imconfig.h").write_text("")
        (work / "eventCounter2.h").write_text(args.header.read_text())
        source = work / "benchmark.cpp"
        source.write_text(CODE)
        binary = work / "benchmark"
        subprocess.run([
            os.environ.get("CXX", "clang++"), "-std=c++17", "-O0" if args.debug else "-O2",
            "-I" + str(work), "-I" + str(IMGUI), str(source),
            *[str(IMGUI / name) for name in ("imgui.cpp", "imgui_draw.cpp", "imgui_widgets.cpp", "imgui_tables.cpp")],
            "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True, timeout=40)


if __name__ == "__main__":
    main()
