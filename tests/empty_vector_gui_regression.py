#!/usr/bin/env python3
"""Exercise production numeric-vector widgets with real ImGui and ASan/UBSan.

Only Oceanode parameter/drag adapters are stubbed. Covers the SVG loader's
initial empty, populated and cleared outputs without launching the host app.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get("OCEANODE_TEST_IMGUI_SRC", ROOT.parent / "ofxImGui/libs/imgui/src"))

PREFIX = r'''
#include "imgui.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
using namespace std;
template<class T, class U> U ofClamp(T v, U lo, U hi) { return std::max(lo, std::min(U(v), hi)); }
template<class T> struct Parameter {
    struct State { vector<T> value, minimum, maximum; };
    shared_ptr<State> state = make_shared<State>();
    const vector<T>& get() const { return state->value; }
    const vector<T>* operator->() const { return &state->value; }
    const vector<T>& getMin() const { return state->minimum; }
    const vector<T>& getMax() const { return state->maximum; }
    Parameter& operator=(const vector<T>& value) { state->value=value; return *this; }
};
template<class T> struct AbstractParameter {
    Parameter<T> parameter;
    vector<string> options;
    template<class U> AbstractParameter& cast() {
        static_assert(is_same<U, vector<T>>::value, "wrong parameter type");
        return *this;
    }
    Parameter<T> getParameter() { return parameter; }
    vector<string> getDropdownOptions() { return options; }
    vector<T> getDefaultValue() { return {T(0)}; }
    void applyPrecisionDrag(float) {}
    void applySpeedDrag(float) {}
    void applyNormalDrag(float) {}
};
// Stands in for the ofxOceanodeNodeGui member the widgets read.
bool selected = true;
'''
CASES = r'''
template<class T, class Draw> int drawVertices(Draw draw, AbstractParameter<T>& absParam) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0,0});
    ImGui::SetNextWindowSize({400,200});
    ImGui::Begin("Node");
    ImGui::SetNextItemWidth(150);
    draw(absParam);
    ImGui::End();
    ImGui::Render();
    int count=0;
    for(const ImDrawList* list : ImGui::GetDrawData()->CmdLists) count += list->VtxBuffer.Size;
    return count;
}
template<class T, class Draw> void check(Draw draw) {
    AbstractParameter<T> absParam;
    // An unselected node draws a large vector with fewer bars than its pixel
    // width; the selected node keeps the full plot.
    vector<T> large(1000);
    for(size_t i = 0; i < large.size(); ++i) large[i] = T(int(i % 5) - 2);
    for(bool bounded : {false, true}) {
        absParam.parameter.state->minimum = {bounded ? T(-2) : numeric_limits<T>::lowest()};
        absParam.parameter.state->maximum = {bounded ? T(2) : numeric_limits<T>::max()};
        absParam.parameter = large;
        selected = true;
        const int full = drawVertices<T>(draw, absParam);
        selected = false;
        const int reduced = drawVertices<T>(draw, absParam);
        assert(reduced < full);
        // Up to the bin count the plot is unchanged whatever the selection.
        absParam.parameter = vector<T>(large.begin(), large.begin() + kReducedPlotBins);
        const int smallUnselected = drawVertices<T>(draw, absParam);
        selected = true;
        assert(drawVertices<T>(draw, absParam) == smallUnselected);
    }
    for(bool full : {true, false}) {
    selected = full;
    for(bool bounded : {false, true}) {
        absParam.parameter.state->minimum = {bounded ? T(-1) : numeric_limits<T>::lowest()};
        absParam.parameter.state->maximum = {bounded ? T(1) : numeric_limits<T>::max()};
        for(const vector<T>& values : vector<vector<T>>{{}, {T(0)}, {T(0),T(1),T(-1)}, {}, {T(1),T(1)}}) {
            absParam.parameter = values;
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0,0});
            ImGui::SetNextWindowSize({400,200});
            ImGui::Begin("Node");
            ImGui::SetNextItemWidth(150);
            draw(absParam);
            // Empty output still occupies a widget and provides a valid pin row.
            auto size=ImGui::GetItemRectSize();
            assert(size.x > 0 && size.y > 0);
            assert(absParam.parameter.get() == values);
            ImGui::End();
            ImGui::Render();
            for(const ImDrawList* list : ImGui::GetDrawData()->CmdLists)
                for(const auto& vertex : list->VtxBuffer) {
                    assert(isfinite(vertex.pos.x) && isfinite(vertex.pos.y));
                }
        }
    }
    }
    // Empty values with no range metadata must not index either limits vector.
    absParam.parameter = vector<T>{};
    absParam.parameter.state->minimum.clear();
    absParam.parameter.state->maximum.clear();
    ImGui::NewFrame();
    ImGui::Begin("Node");
    draw(absParam);
    ImGui::End();
    ImGui::Render();
}
int main() {
    ImGui::CreateContext();
    auto &io=ImGui::GetIO();
    io.IniFilename=nullptr;
    io.DisplaySize={800,600};
    io.DeltaTime=1.0f/60;
    unsigned char* pixels; int width,height;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    check<float>(drawFloat);
    check<int>(drawInt);
    ImGui::DestroyContext();
    // Each reduced bar keeps the largest-magnitude value of its bucket, so a
    // -1 separator and a lone peak both survive, and the range is exact.
    {
        vector<float> spikes(200, 0.f);
        spikes[100] = -1.f;
        spikes[150] = 0.5f;
        float bins[kReducedPlotBins], minimum, maximum;
        reduceVectorPlot(spikes, bins, minimum, maximum);
        assert(minimum == -1.f && maximum == 0.5f);
        for(int bin = 0; bin < kReducedPlotBins; ++bin)
            assert(bins[bin] == (bin == 32 ? -1.f : bin == 48 ? 0.5f : 0.f));
        vector<int> ints(130, 3);
        ints[129] = -7;
        reduceVectorPlot(ints, bins, minimum, maximum);
        assert(bins[0] == 3.f && bins[kReducedPlotBins - 1] == -7.f && minimum == -7.f && maximum == 3.f);
    }
    cout << "Empty vector GUI regression passed: float/int, bounded/unbounded, empty/populated transitions, reduced plots.\n";
}
'''


def widget(source, value_type):
    marker = 'else if(absParam.valueType() == typeid(vector<' + value_type + '>).name())'
    start = source.index('{', source.index(marker))
    end, depth = start + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def helpers(source):
    """The file-local plot helpers the vector widgets call."""
    marker = 'namespace { // vector pin plots'
    start = source.index(marker)
    end, depth = source.index('{', start) + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    source = (ROOT / 'src/Nodes/ofxOceanodeNodeGui.cpp').read_text()
    fixture = PREFIX + helpers(source) + '\n'
    for value_type, name in [('float', 'drawFloat'), ('int', 'drawInt')]:
        fixture += f'void {name}(AbstractParameter<{value_type}>& absParam) {{\n'
        fixture += 'float drag=0; bool resetValue=false, isItemEditableByText=false; string hiddenUniqueId="##value";\n'
        fixture += widget(source, value_type) + '\n}\n'
    fixture += CASES
    with tempfile.TemporaryDirectory(prefix='empty-vector-gui-') as folder:
        folder = Path(folder)
        (folder / 'imconfig.h').write_text('#pragma once\n')
        cpp = folder / 'regression.cpp'
        cpp.write_text(fixture)
        executable = folder / 'regression'
        command = [os.environ.get('CXX', 'clang++'), '-std=c++17', '-g', '-O0',
                   '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                   '-I' + str(folder), '-I' + str(IMGUI), str(cpp)]
        command += [str(IMGUI / name) for name in ('imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp')]
        subprocess.run(command + ['-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
