#!/usr/bin/env python3
"""Exercise the production snapshot inspector across a synchronous map reload."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/Nodes/ofxOceanodeNodeMacroGui.cpp"
START = "void ofxOceanodeNodeMacro::renderInspectorInterface() {"
END = "// syncParameterGroupToSortOrder"

FIXTURE = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
using std::string;
struct ImVec2 { ImVec2(float, float) {} };
constexpr int ImGuiWindowFlags_AlwaysAutoResize = 1;
constexpr int ImGuiInputTextFlags_EnterReturnsTrue = 1;
namespace ImGui {
    enum Mode { Rename, Clear };
    Mode mode;
    int slot = -1;
    int rows = 0;
    bool popupOpen = false;
    void Text(const char*, ...) {}
    void Separator() {}
    void OpenPopup(const char*) { popupOpen = true; }
    void CloseCurrentPopup() { popupOpen = false; }
    void EndPopup() {}
    bool BeginPopupModal(const char*, void*, int) { return false; }
    bool BeginPopup(const char*) { return popupOpen; }
    void PushID(int id) { slot = id; ++rows; }
    void PopID() {}
    void SetNextItemWidth(float) {}
    void SameLine() {}
    bool InputText(const char*, char* buffer, size_t size, int) {
        if(mode != Rename || slot != 1) return false;
        std::snprintf(buffer, size, "%s", "Renamed");
        return true;
    }
    bool Button(const char* label, ImVec2 = ImVec2(0, 0)) {
        const string text(label);
        return mode == Clear && slot == 1 && (text == "Clear" || text == "Yes");
    }
}
struct SnapshotData { string name; };
struct SnapshotSystem {
    std::map<int, SnapshotData> snapshots = {{1, {string(300, 'x')}}, {2, {"Two"}}};
    int currentSlot = 1;
    int saves = 0;
    bool isEmpty() const { return snapshots.empty(); }
    std::map<int, SnapshotData>& getSnapshots() { return snapshots; }
    int getCurrentSlot() const { return currentSlot; }
    void setCurrentSlot(int value) { currentSlot = value; }
    void clearAll() { snapshots.clear(); }
    void save(bool, const string&, const string&, const string&, int) {
        ++saves;
        // Simulate the synchronous snapshotUpdated listener: replace all map nodes.
        std::map<int, SnapshotData> previous;
        previous.swap(snapshots);
        snapshots = previous;
    }
};
struct PresetManager {
    bool isLocal() const { return false; }
    string getCurrentMacroPath() const { return "macro"; }
    string getPresetPath() const { return "preset"; }
};
struct GuiState { bool showClearAllSnapshotsPopup = false; };
struct ofxOceanodeNodeMacro {
    SnapshotSystem snapshotSystem;
    PresetManager presetManager;
    GuiState guiState;
    string nodeName() const { return "Macro"; }
    int getNumIdentifier() const { return 1; }
    void loadRouterSnapshot(int) {}
    void renderInspectorInterface();
};
'''

CASES = r'''
int main() {
    ofxOceanodeNodeMacro macro;
    ImGui::mode = ImGui::Rename;
    macro.renderInspectorInterface();
    assert(macro.snapshotSystem.saves == 1);
    assert(macro.snapshotSystem.getSnapshots().at(1).name == "Renamed");
    assert(ImGui::rows == 2);

    ImGui::mode = ImGui::Clear;
    ImGui::rows = 0;
    macro.renderInspectorInterface();
    assert(macro.snapshotSystem.saves == 2);
    assert(macro.snapshotSystem.getSnapshots().count(1) == 0);
    assert(macro.snapshotSystem.getSnapshots().count(2) == 1);
    assert(macro.snapshotSystem.getCurrentSlot() == -1);
    assert(ImGui::rows == 2);
}
'''


def main():
    source = SOURCE.read_text()
    function = source[source.index(START):source.index(END)]
    with tempfile.TemporaryDirectory(prefix="oceanode-snapshot-inspector-") as tmp:
        cpp = Path(tmp) / "inspector.cpp"
        binary = Path(tmp) / "inspector"
        cpp.write_text(FIXTURE + function + CASES)
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-g", "-O1",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print("Macro Snapshot inspector regression passed (rename, clear, synchronous reload)")


if __name__ == "__main__":
    main()
