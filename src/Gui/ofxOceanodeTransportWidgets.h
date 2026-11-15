#pragma once

#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

// Transport widgets drawn the same wherever a transport is shown: the host
// menu bar here, and the timeline toolbar in ofxOceanodeTimeline.
namespace ofxOceanodeTransportWidgets {

constexpr float positionReadoutWidth = 170.0f;

// Position readout: bar . beat . sixteenth on the left (green while playing)
// and the song time on the right. It is one item, `id`: hover, tooltip and
// dragging are the caller's to add after the call.
inline void positionReadout(const char* id, double beat, double beatsPerBar, double seconds,
                            bool isPlaying, float height){
    beat = std::max(0.0, beat);
    beatsPerBar = std::max(1e-6, beatsPerBar);
    seconds = std::max(0.0, seconds);
    const int bar = static_cast<int>(std::floor(beat / beatsPerBar)) + 1;
    const double inBar = beat - (bar - 1) * beatsPerBar;
    const int beatInBar = static_cast<int>(std::floor(inBar)) + 1;
    const int sixteenth = static_cast<int>(std::floor((inBar - std::floor(inBar)) * 4.0)) + 1;
    const int minutes = static_cast<int>(seconds / 60.0);
    char positionText[32];
    char timeText[32];
    std::snprintf(positionText, sizeof(positionText), "%3d. %d. %d", bar, beatInBar, sixteenth);
    std::snprintf(timeText, sizeof(timeText), "%d:%06.3f", minutes, seconds - minutes * 60.0);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(positionReadoutWidth, height);
    ImGui::InvisibleButton(id, size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 corner(pos.x + size.x, pos.y + size.y);
    dl->AddRectFilled(pos, corner, IM_COL32(12, 14, 18, 255), 3.0f);
    dl->AddRect(pos, corner, IM_COL32(255, 255, 255, 25), 3.0f);
    const float textY = pos.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f;
    dl->AddText(ImVec2(pos.x + 8.0f, textY),
                isPlaying ? IM_COL32(120, 235, 160, 255) : IM_COL32(230, 232, 238, 255), positionText);
    const float timeWidth = ImGui::CalcTextSize(timeText).x;
    dl->AddText(ImVec2(pos.x + size.x - timeWidth - 8.0f, textY), IM_COL32(150, 155, 165, 255), timeText);
}

// The tap tempo icon: a ring around a dot. `s` is the icon size (about half
// the button's height), as for the other transport icons.
inline void drawTapIcon(ImDrawList* dl, ImVec2 center, float s, ImU32 col){
    dl->AddCircle(center, s * 0.4f, col, 16, 1.6f);
    dl->AddCircleFilled(center, s * 0.16f, col);
}

// Tap tempo: call on each tap, with the time in seconds. The tempo is the
// average of the last taps (8 at most); a pause of 2 s starts over. True
// when it set `bpm` (from the second tap on).
inline bool tapTempo(std::vector<double>& tapTimes, double now, float& bpm){
    if(!tapTimes.empty() && now - tapTimes.back() > 2.0) tapTimes.clear();
    tapTimes.push_back(now);
    if(tapTimes.size() > 8) tapTimes.erase(tapTimes.begin());
    if(tapTimes.size() < 2) return false;
    const double interval = (tapTimes.back() - tapTimes.front()) / (tapTimes.size() - 1);
    if(interval <= 0.0) return false;
    bpm = std::clamp(static_cast<float>(60.0 / interval), 20.0f, 400.0f);
    return true;
}

}
