#!/usr/bin/env python3
"""Exercise log keyboard copying with real ImGui and the macOS system clipboard.

Leaves a short UTF-8 test log on the clipboard, then verifies it with pbpaste.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMGUI = Path(os.environ.get("OCEANODE_TEST_IMGUI_SRC", ROOT.parent / "ofxImGui/libs/imgui/src"))

SOURCE = r'''
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <cstdarg>
#include <deque>
#include <iostream>
#include <string>
using namespace std;
#define ofxOceanodeBaseController_h
class ofxOceanodeBaseController {
public:
    ofxOceanodeBaseController(string) {}
    virtual ~ofxOceanodeBaseController() = default;
};
enum ofLogLevel { OF_LOG_NOTICE };
class ofBaseLoggerChannel {
public:
    virtual ~ofBaseLoggerChannel() = default;
    virtual void log(ofLogLevel, const string&, const string&) = 0;
    virtual void log(ofLogLevel, const string&, const char*, ...) = 0;
    virtual void log(ofLogLevel, const string&, const char*, va_list) = 0;
};
template<class T> struct ofThreadChannel {
    deque<T> values;
    void send(const T& value) { values.push_back(value); }
    bool tryReceive(T& value) {
        if(values.empty()) return false;
        value = values.front(); values.pop_front(); return true;
    }
};
#include "ofxOceanodeLogController.h"
static ImVec2 selectionToggle;
static ImVec2 copyButton;
static char pasted[512]{};
static bool focusPaste = false;
static void frame(ofxOceanodeLogController& log) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({700,400});
    ImGui::Begin("Log");
    log.draw();
    auto* window = ImGui::GetCurrentWindow();
    // Resolve the checkbox from the actual toolbar sizing, without stubbing widgets.
    const float clearWidth = ImGui::CalcTextSize("[Clear]").x + ImGui::GetStyle().FramePadding.x * 2;
    const float copyWidth = ImGui::CalcTextSize("Copy all").x + ImGui::GetStyle().FramePadding.x * 2;
    copyButton = {window->Pos.x + window->WindowPadding.x + clearWidth +
        ImGui::GetStyle().ItemSpacing.x + 5, window->Pos.y + window->TitleBarHeight + window->WindowPadding.y + 5};
    selectionToggle = {window->Pos.x + window->WindowPadding.x + clearWidth + copyWidth +
        ImGui::GetStyle().ItemSpacing.x * 2 + 5, window->Pos.y + window->TitleBarHeight + window->WindowPadding.y + 5};
    ImGui::End();
    ImGui::SetNextWindowPos({710,0}); ImGui::SetNextWindowSize({250,400});
    ImGui::Begin("Other");
    if(focusPaste) { ImGui::SetKeyboardFocusHere(); focusPaste=false; }
    ImGui::InputTextMultiline("Paste target",pasted,sizeof(pasted));
    ImGui::End();
    ImGui::Render();
}
static void key(ofxOceanodeLogController& log, ImGuiKey key) {
    auto& io = ImGui::GetIO();
    io.AddKeyEvent(key,true); frame(log); frame(log);
    io.AddKeyEvent(key,false); frame(log);
}
int main() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.DisplaySize={1000,600}; io.DeltaTime=1.0f/60; io.IniFilename=nullptr;
    io.ConfigMacOSXBehaviors=true; io.ConfigInputTrickleEventQueue=false;
    unsigned char* pixels; int width,height;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    ofxOceanodeClipboard::install();
    // A single bad byte previously made NSString reject the entire log.
    assert(ofxOceanodeClipboard::setText("before\xff" "after"));
    assert(ofxOceanodeClipboard::getText()=="before�after");
    assert(ofxOceanodeClipboard::setText(string("before\0after",12)));
    assert(ofxOceanodeClipboard::getText()=="before�after");
    assert(ofxOceanodeClipboard::validText("truncated \xc3")=="truncated �");
    ofxOceanodeLogController log;
    const string first="Català: àèíòú — log";
    log.log(OF_LOG_NOTICE,"test",first);
    log.log(OF_LOG_NOTICE,"test",string("second line"));
    frame(log); frame(log);
    ImGui::FocusWindow(ImGui::FindWindowByName("Log"));
    io.AddKeyEvent(ImGuiMod_Super,true); frame(log);
    key(log,ImGuiKey_C);
    assert(ofxOceanodeClipboard::getText()==first+"\nsecond line\n");
    // Copying in a different window must leave the clipboard alone.
    ofxOceanodeClipboard::setText("other text");
    ImGui::FocusWindow(ImGui::FindWindowByName("Other")); frame(log);
    key(log,ImGuiKey_C); assert(ofxOceanodeClipboard::getText()=="other text");
    io.AddKeyEvent(ImGuiMod_Super,false); frame(log);
    // Copy all writes the same system clipboard even without a keyboard shortcut.
    io.AddMousePosEvent(copyButton.x,copyButton.y);
    io.AddMouseButtonEvent(0,true); frame(log);
    io.AddMouseButtonEvent(0,false); frame(log);
    assert(ofxOceanodeClipboard::getText()==first+"\nsecond line\n");
    // Open the snapshot through its real checkbox, then Cmd+A/C without another click.
    io.AddMousePosEvent(selectionToggle.x,selectionToggle.y);
    io.AddMouseButtonEvent(0,true); frame(log);
    io.AddMouseButtonEvent(0,false); frame(log); frame(log);
    io.AddKeyEvent(ImGuiMod_Super,true); frame(log);
    key(log,ImGuiKey_A); key(log,ImGuiKey_C);
    assert(ofxOceanodeClipboard::getText()==first+"\nsecond line\n");
    // Selecting a fragment must preserve that selection, rather than copying all.
    auto* state=ImGui::GetInputTextState(ImGui::GetActiveID());
    assert(state); state->SelectAll();
    io.AddKeyEvent(ImGuiMod_Super,false); frame(log);
    key(log,ImGuiKey_LeftArrow);
    io.AddKeyEvent(ImGuiMod_Shift,true); frame(log);
    key(log,ImGuiKey_RightArrow);
    io.AddKeyEvent(ImGuiMod_Shift,false);
    io.AddKeyEvent(ImGuiMod_Super,true); frame(log);
    key(log,ImGuiKey_C); assert(ofxOceanodeClipboard::getText()=="C");
    // Incoming messages do not alter the snapshot or its selection.
    log.log(OF_LOG_NOTICE,"test",string("new message")); frame(log);
    key(log,ImGuiKey_A); key(log,ImGuiKey_C);
    assert(ofxOceanodeClipboard::getText()==first+"\nsecond line\n");
    io.AddKeyEvent(ImGuiMod_Super,false); frame(log);
    focusPaste=true; frame(log); frame(log);
    io.AddKeyEvent(ImGuiMod_Super,true); frame(log);
    key(log,ImGuiKey_V);
    assert(string(pasted)==first+"\nsecond line\n");
    ImGui::DestroyContext();
    cout << "Log clipboard regression passed: Copy all, Cmd+C live, focus isolation, snapshot Cmd+A/C, fragment, Cmd+V, UTF-8.\n";
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix="log-clipboard-") as folder:
        folder = Path(folder)
        (folder / "imconfig.h").write_text("#pragma once\n")
        source = folder / "regression.cpp"
        source.write_text(SOURCE)
        executable = folder / "regression"
        command = [os.environ.get("CXX", "clang++"), "-std=c++17", "-g", "-O0",
                   "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                   "-I" + str(folder), "-I" + str(IMGUI),
                   "-I" + str(ROOT / "src/Controls"), "-I" + str(ROOT / "src"),
                   "-I" + str(ROOT.parent.parent / "libs/utf8/include"), str(source)]
        command += [str(IMGUI / name) for name in ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")]
        subprocess.run(command + ["-framework", "AppKit", "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        pasted = subprocess.check_output(["/usr/bin/pbpaste"], text=True)
        assert pasted == "Català: àèíòú — log\nsecond line\n", repr(pasted)
        print("macOS clipboard verified from a separate process with pbpaste.")


if __name__ == "__main__":
    main()
