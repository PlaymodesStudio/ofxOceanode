#include "ofxOceanodeTimelineController.h"

#include "ofxOceanodeContainer.h"
#include "ofxOceanodeParameter.h"
#include "Timeline/ofxOceanodeTimeline.h"
#include "ofxOceanodeTransport.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <utility>

namespace {
constexpr double kPPQ = 24.0;
constexpr float kLabelWidth = 230.0f;
constexpr float kRulerHeight = 64.0f;
constexpr float kHeaderHeight = 25.0f;
constexpr float kRowHeight = 28.0f;
constexpr float kCollapsedHeight = 38.0f;
constexpr float kEdgePixels = 8.0f;
constexpr float kPianoKeyboardWidth = 38.0f;
constexpr float kPianoScrollbarWidth = 10.0f;
constexpr float kPianoZoomButtonHeight = 14.0f;
constexpr float kLaneResizeHandleHeight = 10.0f;
constexpr float kLaneEditorMinHeight = 90.0f;
constexpr float kLaneEditorMaxHeight = 640.0f;

using ofxOceanodeTimelineCurve::CurveInterpolationMode;
using ofxOceanodeTimelineCurve::curveInterpolationMode;
using ofxOceanodeTimelineCurve::curveSegmentShape;
using ofxOceanodeTimelineClipTime::cycleDuration;
using ofxOceanodeTimelineClipTime::sourceDuration;
using ofxOceanodeTimelineClipTime::sourceToTimelineBeat;
using ofxOceanodeTimelineClipTime::stretch;
using ofxOceanodeTimelineClipTime::timelineToSourceBeat;

constexpr const char* kCurveInterpolationNames[] = {
    "Step", "Linear", "Log / Exp", "Sigmoid"
};

struct DivisionOption {
    const char* label;
    double beats;
};

constexpr DivisionOption kDivisionOptions[] = {
    {"None", 0.0},
    {"Whole", 4.0}, {"Half", 2.0}, {"Quarter", 1.0},
    {"8th", 0.5}, {"8thD", 0.75}, {"8thT", 1.0 / 3.0},
    {"16th", 0.25}, {"16thD", 0.375}, {"16thT", 1.0 / 6.0},
    {"32nd", 0.125}, {"32ndD", 0.1875}, {"32ndT", 1.0 / 12.0},
    {"64th", 0.0625}
};

constexpr int kDivisionOptionCount = static_cast<int>(sizeof(kDivisionOptions) / sizeof(kDivisionOptions[0]));

int divisionIndexForBeats(double beats) {
    int best = 0;
    double bestDistance = std::abs(beats - kDivisionOptions[0].beats);
    for(int i = 1; i < kDivisionOptionCount; ++i) {
        const double distance = std::abs(beats - kDivisionOptions[i].beats);
        if(distance < bestDistance) { best = i; bestDistance = distance; }
    }
    return best;
}

std::string compactParameterName(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

const ImU32 kGrid = IM_COL32(70, 70, 70, 115);
const ImU32 kBar = IM_COL32(125, 125, 125, 190);
const ImU32 kPlayhead = IM_COL32(255, 85, 85, 255);

ImU32 mutedTrackColor(const ofColor& color, float brightness, float saturation = 0.34f) {
    const float luminance = 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
    auto channel = [&](float value) {
        const float desaturated = luminance + (value - luminance) * saturation;
        return static_cast<int>(ofClamp(9.0f + desaturated * brightness, 0.0f, 255.0f));
    };
    return IM_COL32(channel(color.r), channel(color.g), channel(color.b), 255);
}

const ofxOceanodeTimelineLane* laneForBinding(const ofxOceanodeTimelineClip& clip, const std::string& bindingId) {
    for(const auto& lane : clip.lanes) {
        if(std::find(lane.bindingIds.begin(), lane.bindingIds.end(), bindingId) != lane.bindingIds.end()) return &lane;
    }
    return nullptr;
}

const char* laneTypeName(ofxOceanodeTimelineLaneType type) {
    switch(type) {
        case ofxOceanodeTimelineLaneType::PianoRoll: return "Piano Roll";
        case ofxOceanodeTimelineLaneType::Curve: return "Curve";
        default: return "Step Sequencer";
    }
}

int insertLinearCurvePoint(ofxOceanodeTimelineLane& lane,
                           const ofxOceanodeTimelineCurvePoint& point) {
    const size_t oldPointCount = lane.curvePoints.size();
    auto oldTensions = lane.curveTensions;
    oldTensions.resize(oldPointCount > 0 ? oldPointCount - 1 : 0);
    const auto insertion = std::lower_bound(lane.curvePoints.begin(), lane.curvePoints.end(), point.beat,
        [](const auto& existing, double beat) { return existing.beat < beat; });
    const size_t insertionIndex = static_cast<size_t>(insertion - lane.curvePoints.begin());
    lane.curvePoints.insert(insertion, point);

    std::vector<ofxOceanodeTimelineCurveTension> newTensions(
        lane.curvePoints.size() > 0 ? lane.curvePoints.size() - 1 : 0);
    for(size_t segment = 0; segment < newTensions.size(); ++segment) {
        // Both segments touching the inserted point use the neutral sigmoid.
        if((insertionIndex > 0 && segment == insertionIndex - 1) || segment == insertionIndex) continue;
        const size_t oldSegment = segment < insertionIndex ? segment : segment - 1;
        if(oldSegment < oldTensions.size()) newTensions[segment] = oldTensions[oldSegment];
    }
    lane.curveTensions = std::move(newTensions);
    return static_cast<int>(insertionIndex);
}

void resetCurveTensions(ofxOceanodeTimelineLane& lane) {
    lane.curveTensions.assign(lane.curvePoints.size() > 0 ? lane.curvePoints.size() - 1 : 0,
                              ofxOceanodeTimelineCurveTension{});
}

void eraseCurvePointWithTensions(ofxOceanodeTimelineLane& lane, size_t pointIndex) {
    if(pointIndex >= lane.curvePoints.size()) return;
    lane.curvePoints.erase(lane.curvePoints.begin() + pointIndex);
    // Topology changes should not retain hidden shaping state from segments
    // that no longer exist. The resulting curve starts from a predictable,
    // neutral shape in every interpolation mode.
    resetCurveTensions(lane);
}

void finishAbsoluteLayout(const ImVec2& position) {
    // SetCursorPos alone cannot grow an ImGui child safely. Submitting a
    // zero-sized item clears that transient state and preserves the normal
    // item-spacing advance after an absolutely positioned docked section.
    ImGui::SetCursorScreenPos(position);
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}
}

ofxOceanodeTimelineController::ofxOceanodeTimelineController(std::shared_ptr<ofxOceanodeContainer> _container)
: ofxOceanodeBaseController("Timeline"), container(std::move(_container)) {}

double ofxOceanodeTimelineController::getContentEndBeat(const ofxOceanodeTimelineManager& timeline) const {
    const double barBeats = timeline.getBeatsPerBar();
    double endBeat = std::max(barBeats, static_cast<double>(visibleBars) * barBeats);
    if(timeline.isLoopEnabled()) endBeat = std::max(endBeat, timeline.getLoopEndBeat());
    for(const auto& point : timeline.getBpmAutomationPoints()) endBeat = std::max(endBeat, point.beat);
    for(const auto& track : timeline.getTracks()) {
        for(const auto& clip : track.clips) {
            endBeat = std::max(endBeat, clip.startBeat + clip.durationBeats);
        }
    }
    return endBeat;
}

double ofxOceanodeTimelineController::snapBeat(double beat) const {
    if(rulerSnapBeats <= 0.0) return std::max(0.0, beat);
    return std::max(0.0, std::round(beat / rulerSnapBeats) * rulerSnapBeats);
}

double ofxOceanodeTimelineController::editIncrement() const {
    return rulerSnapBeats > 0.0 ? rulerSnapBeats : 1.0 / kPPQ;
}

double ofxOceanodeTimelineController::displayGridBeats() const {
    return rulerSnapBeats > 0.0 ? rulerSnapBeats : 1.0;
}

float ofxOceanodeTimelineController::beatToPixels(const ofxOceanodeTimelineManager& timeline,
                                                  double beat, float fallbackBpm) const {
    return static_cast<float>(timeline.beatToSeconds(std::max(0.0, beat), fallbackBpm) * pixelsPerSecond);
}

double ofxOceanodeTimelineController::pixelsToBeat(const ofxOceanodeTimelineManager& timeline,
                                                   float pixels, float fallbackBpm,
                                                   double endBeatHint) const {
    const double targetSeconds = std::max(0.0f, pixels) / std::max(1.0f, pixelsPerSecond);
    double low = 0.0;
    double high = std::max(1.0, endBeatHint);
    while(timeline.beatToSeconds(high, fallbackBpm) < targetSeconds && high < 1000000.0) high *= 2.0;
    for(int iteration = 0; iteration < 40; ++iteration) {
        const double middle = (low + high) * 0.5;
        if(timeline.beatToSeconds(middle, fallbackBpm) < targetSeconds) low = middle;
        else high = middle;
    }
    return (low + high) * 0.5;
}

// Shared by every place a binding's blend mode can be changed: the
// per-binding row's own context menu, and the track-header menu (so the
// mode is reachable even while the track is collapsed and that row isn't
// drawn at all).
void ofxOceanodeTimelineController::drawBlendModeOptions(ofxOceanodeTimelineManager& timeline,
                                                         const std::string& trackId,
                                                         const ofxOceanodeTimelineParameterBinding& binding) {
    struct BlendModeOption {
        ofxOceanodeTimelineAutomationMode mode;
        const char* label;
        const char* hint;
    };
    static const BlendModeOption options[] = {
        {ofxOceanodeTimelineAutomationMode::Replace, "Replace",
         "Overrides any other binding driving this parameter"},
        {ofxOceanodeTimelineAutomationMode::Add, "Add",
         "Adds this binding's value to whatever else drives this parameter"},
        {ofxOceanodeTimelineAutomationMode::Multiply, "Multiply",
         "Multiplies this binding's value with whatever else drives this parameter"},
        {ofxOceanodeTimelineAutomationMode::Min, "Min",
         "Keeps the lowest value among all contributors"},
        {ofxOceanodeTimelineAutomationMode::Max, "Max",
         "Keeps the highest value among all contributors"},
    };
    for(const auto& option : options) {
        if(ImGui::MenuItem(option.label, nullptr, binding.mode == option.mode))
            timeline.setBindingMode(trackId, binding.id, option.mode);
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", option.hint);
    }
}

void ofxOceanodeTimelineController::draw() {
    if(container == nullptr) return;
    auto& timeline = container->getTimelineManager();
    auto transportState = container->getTransportState();
    auto transport = container->getTransport();

    if(transport != nullptr &&
       ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
       !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
       ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        transport->togglePlay();
        transportState = container->getTransportState();
    }

    std::string requestedRename;
    bool isNewTrack = false;
    if(timeline.consumePendingTrackRename(requestedRename, &isNewTrack)) {
        pendingTrackId = requestedRename;
        if(const auto* track = timeline.getTrack(pendingTrackId)) {
            std::strncpy(pendingTrackName, track->name.c_str(), sizeof(pendingTrackName) - 1);
            pendingTrackName[sizeof(pendingTrackName) - 1] = '\0';
            pendingNewTrackDialog = isNewTrack;
            if(isNewTrack) pendingNewTrackLaneType = 0;
            requestRenamePopup = true;
        }
    }

    if(transport != nullptr) {
        if(ImGui::Button(transportState.isPlaying ? "Pause" : "Play")) transport->setIsPlaying(!transportState.isPlaying);
        ImGui::SameLine();
        if(ImGui::Button("Stop")) transport->stop();
        ImGui::SameLine();
        if(ImGui::Button("Reset")) transport->seekToBeat(0.0);
        ImGui::SameLine();
        float bpm = transportState.bpm;
        if(timeline.isBpmAutomationEnabled()) {
            ImGui::Text("BPM %.1f (auto)", bpm);
        } else {
            ImGui::SetNextItemWidth(90.0f);
            if(ImGui::DragFloat("BPM", &bpm, 0.1f, 1.0f, 999.0f, "%.1f")) container->setBpm(bpm);
        }
        ImGui::SameLine();
        ImGui::Text("Beat %.3f", transportState.beatPosition);
    }
    ImGui::SetNextItemWidth(105.0f);
    ImGui::DragFloat("Px / sec", &pixelsPerSecond, 1.0f, 30.0f, 600.0f, "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::DragInt("Bars", &visibleBars, 1.0f, 1, 256);
    ImGui::SameLine();
    {
        // Time signature: numerator is free-typed (any meter is valid),
        // denominator is restricted to the conventional note-value set so
        // getBeatsPerBar() (numerator * 4/denominator, in quarter-note
        // beats) always lands on a sane grid.
        int tsNumerator = timeline.getTimeSignatureNumerator();
        int tsDenominator = timeline.getTimeSignatureDenominator();
        ImGui::TextUnformatted("Time Sig");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(38.0f);
        if(ImGui::DragInt("##timeSigNum", &tsNumerator, 0.1f, 1, 64))
            timeline.setTimeSignature(tsNumerator, tsDenominator);
        ImGui::SameLine(0.0f, 3.0f);
        ImGui::TextUnformatted("/");
        ImGui::SameLine(0.0f, 3.0f);
        ImGui::SetNextItemWidth(46.0f);
        char tsDenomLabel[8];
        std::snprintf(tsDenomLabel, sizeof(tsDenomLabel), "%d", tsDenominator);
        if(ImGui::BeginCombo("##timeSigDenom", tsDenomLabel)) {
            for(int option : {1, 2, 4, 8, 16, 32}) {
                char optionLabel[8];
                std::snprintf(optionLabel, sizeof(optionLabel), "%d", option);
                if(ImGui::Selectable(optionLabel, option == tsDenominator))
                    timeline.setTimeSignature(tsNumerator, option);
            }
            ImGui::EndCombo();
        }
    }
    ImGui::SameLine();
    int rulerDivision = divisionIndexForBeats(rulerSnapBeats);
    ImGui::SetNextItemWidth(82.0f);
    if(ImGui::BeginCombo("Snap", kDivisionOptions[rulerDivision].label)) {
        for(int i = 0; i < kDivisionOptionCount; ++i) {
            if(ImGui::Selectable(kDivisionOptions[i].label, i == rulerDivision))
                rulerSnapBeats = kDivisionOptions[i].beats;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    bool loopEnabled = timeline.isLoopEnabled();
    if(ImGui::Checkbox("Loop", &loopEnabled)) timeline.setLoopEnabled(loopEnabled);
    if(loopEnabled) {
        double loopStart = timeline.getLoopStartBeat();
        double loopEnd = timeline.getLoopEndBeat();
        ImGui::SameLine(); ImGui::SetNextItemWidth(62.0f);
        if(ImGui::InputDouble("##loopStart", &loopStart, editIncrement(), 1.0, "%.3g"))
            timeline.setLoopRange(snapBeat(loopStart), loopEnd);
        ImGui::SameLine(); ImGui::TextUnformatted("to");
        ImGui::SameLine(); ImGui::SetNextItemWidth(62.0f);
        if(ImGui::InputDouble("##loopEnd", &loopEnd, editIncrement(), 1.0, "%.3g"))
            timeline.setLoopRange(loopStart, std::max(loopStart + 1.0 / kPPQ, snapBeat(loopEnd)));
    }

    const double endBeat = getContentEndBeat(timeline);
    const float availableWidth = std::max(320.0f, ImGui::GetContentRegionAvail().x);
    const float timelineWidth = std::max(availableWidth - kLabelWidth,
                                         beatToPixels(timeline, endBeat, transportState.bpm));
    const float contentWidth = kLabelWidth + timelineWidth;
    auto beatOffset = [&](double beat) { return beatToPixels(timeline, beat, transportState.bpm); };
    auto beatAtOffset = [&](float pixels) { return pixelsToBeat(timeline, pixels, transportState.bpm, endBeat); };

    // Horizontal scrolling is fully manual (timelineScrollX) rather than a
    // native ImGui ScrollX, specifically so the label/properties column (the
    // first kLabelWidth pixels of every row) can stay visually pinned while
    // only the beat-based content to its right slides. A native ScrollX
    // would shift the whole child uniformly, dragging the labels off-screen
    // together with the timeline content.
    const float scrollbarHeight = ImGui::GetStyle().ScrollbarSize;
    ImGui::BeginChild("##TimelineViewport", ImVec2(0, -scrollbarHeight), false,
                      ImGuiWindowFlags_NoScrollWithMouse);
    const float zoneLeft = ImGui::GetWindowPos().x + kLabelWidth;
    const float maxTimelineScrollX = std::max(0.0f, timelineWidth - (availableWidth - kLabelWidth));
    timelineScrollX = ofClamp(timelineScrollX, 0.0f, maxTimelineScrollX);
    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                              ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
        const bool zoomGesture = std::abs(ImGui::GetIO().MouseWheel) > 0.001f;
        if(zoomGesture) {
            const float oldPixelsPerSecond = pixelsPerSecond;
            const float mouseViewportX = ImGui::GetIO().MousePos.x - ImGui::GetWindowPos().x;
            if(mouseViewportX >= kLabelWidth) {
                const float secondsAtMouse = std::max(0.0f, timelineScrollX + mouseViewportX - kLabelWidth) /
                                             std::max(1.0f, oldPixelsPerSecond);
                const float zoomFactor = static_cast<float>(std::pow(1.12f, ImGui::GetIO().MouseWheel));
                pixelsPerSecond = ofClamp(oldPixelsPerSecond * zoomFactor, 30.0f, 600.0f);
                timelineScrollX = ofClamp(kLabelWidth + secondsAtMouse * pixelsPerSecond - mouseViewportX,
                                          0.0f, maxTimelineScrollX);
            } else {
                ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() - ImGui::GetIO().MouseWheel * 55.0f));
            }
        }
        if(!zoomGesture && std::abs(ImGui::GetIO().MouseWheelH) > 0.001f)
            timelineScrollX = ofClamp(timelineScrollX - ImGui::GetIO().MouseWheelH * 55.0f, 0.0f, maxTimelineScrollX);
    }
    drawRuler(timeline, kLabelWidth, timelineWidth, endBeat, transportState.beatPosition, transportState.bpm);
    drawBpmLane(timeline, contentWidth, endBeat, transportState.beatPosition, transportState.bpm);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for(const auto& track : timeline.getTracks()) {
        const float headerY = ImGui::GetCursorPosY();
        // Keep layout state immutable for this frame. Clicking collapse mutates
        // the model, but mixing the old Dummy height with the new branch later
        // in this loop makes SetCursorPos extend the child and asserts in End().
        const bool trackCollapsed = track.collapsed;
        const float trackHeaderHeight = trackCollapsed ? kCollapsedHeight : kHeaderHeight;
        bool expandTrackForEditor = false;
        ImGui::SetCursorPos(ImVec2(0, headerY));
        // Keep the header as a visual/layout item only. A full-width
        // InvisibleButton steals the overlap from the color button on some
        // ImGui versions, making the picker look permanently disabled.
        ImGui::Dummy(ImVec2(contentWidth, trackHeaderHeight));
        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        if(trackCollapsed) {
            dl->AddRectFilled(headerMin, headerMax, mutedTrackColor(track.color, 0.17f));
            dl->AddRectFilled(headerMin, ImVec2(headerMin.x + kLabelWidth, headerMax.y),
                              mutedTrackColor(track.color, 0.27f, 0.42f));
        } else {
            dl->AddRectFilled(headerMin, headerMax, mutedTrackColor(track.color, 0.27f, 0.42f));
        }
        const float headerTextY = headerMin.y + (trackHeaderHeight - ImGui::GetTextLineHeight()) * 0.5f;
        dl->AddText(ImVec2(headerMin.x + 7, headerTextY), IM_COL32(235, 235, 235, 255), trackCollapsed ? ">" : "v");
        dl->AddText(ImVec2(headerMin.x + 22, headerTextY), IM_COL32(235, 235, 235, 255), track.name.c_str());
        // A binding's target parameter can disappear (node deleted, preset
        // loaded on a different graph, ...). The manager keeps the binding
        // and its data instead of dropping it, but that's invisible unless
        // the editor surfaces it somewhere - otherwise automation silently
        // stops applying with no clue why.
        const bool hasMissingTarget = std::any_of(track.bindings.begin(), track.bindings.end(),
            [](const auto& binding) { return binding.missingTarget && !binding.bypass; });
        if(hasMissingTarget) {
            const float nameWidth = ImGui::CalcTextSize(track.name.c_str()).x;
            dl->AddText(ImVec2(headerMin.x + 22 + nameWidth + 6, headerTextY),
                       IM_COL32(255, 165, 60, 255), "[missing target]");
        }

        const ImVec2 headerInteractionMax(trackCollapsed ? headerMin.x + kLabelWidth : headerMax.x,
                                          headerMax.y);
        if(ImGui::IsMouseHoveringRect(headerMin, headerInteractionMax)) {
            if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                pendingTrackId = track.id;
                pendingNewTrackDialog = false;
                std::strncpy(pendingTrackName, track.name.c_str(), sizeof(pendingTrackName) - 1);
                pendingTrackName[sizeof(pendingTrackName) - 1] = '\0';
                requestRenamePopup = true;
            } else if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetIO().MousePos.x < headerMin.x + 22) {
                if(auto* editTrack = timeline.getTrack(track.id)) editTrack->collapsed = !trackCollapsed;
            } else if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                pendingTrackId = track.id;
                ImGui::OpenPopup(("##trackMenu" + track.id).c_str());
            }
        }

        // Use a ColorButton + explicit picker popup. ColorEdit4 with a local
        // temporary swatch was easy to click past and appeared non-persistent.
        ImGui::SetCursorPos(ImVec2(kLabelWidth - 28, headerY + (trackHeaderHeight - 19.0f) * 0.5f));
        ImGui::SetNextItemAllowOverlap();
        const ImVec4 color(track.color.r / 255.0f, track.color.g / 255.0f, track.color.b / 255.0f, track.color.a / 255.0f);
        const std::string colorButtonId = "##trackColorButton" + track.id;
        if(ImGui::ColorButton(colorButtonId.c_str(), color, ImGuiColorEditFlags_NoTooltip, ImVec2(20, 19))) ImGui::OpenPopup(("##trackColorPopup" + track.id).c_str());
        if(ImGui::BeginPopup(("##trackColorPopup" + track.id).c_str())) {
            float rgba[4] = {color.x, color.y, color.z, color.w};
            if(ImGui::ColorPicker4("##picker", rgba, ImGuiColorEditFlags_AlphaBar)) {
                if(auto* editTrack = timeline.getTrack(track.id)) editTrack->color = ofColor(
                    static_cast<unsigned char>(ofClamp(rgba[0] * 255.0f, 0.0f, 255.0f)),
                    static_cast<unsigned char>(ofClamp(rgba[1] * 255.0f, 0.0f, 255.0f)),
                    static_cast<unsigned char>(ofClamp(rgba[2] * 255.0f, 0.0f, 255.0f)),
                    static_cast<unsigned char>(ofClamp(rgba[3] * 255.0f, 0.0f, 255.0f)));
            }
            ImGui::EndPopup();
        }
        if(ImGui::BeginPopup(("##trackMenu" + track.id).c_str())) {
            float menuColor[4] = {color.x, color.y, color.z, color.w};
            if(ImGui::ColorEdit4("Track color", menuColor, ImGuiColorEditFlags_AlphaBar)) {
                if(auto* editTrack = timeline.getTrack(track.id)) editTrack->color = ofColor(
                    static_cast<unsigned char>(ofClamp(menuColor[0] * 255.0f, 0.0f, 255.0f)),
                    static_cast<unsigned char>(ofClamp(menuColor[1] * 255.0f, 0.0f, 255.0f)),
                    static_cast<unsigned char>(ofClamp(menuColor[2] * 255.0f, 0.0f, 255.0f)),
                    static_cast<unsigned char>(ofClamp(menuColor[3] * 255.0f, 0.0f, 255.0f)));
            }
            if(ImGui::MenuItem(trackCollapsed ? "Expand track" : "Collapse track")) {
                if(auto* editTrack = timeline.getTrack(track.id)) editTrack->collapsed = !trackCollapsed;
            }
            if(ImGui::MenuItem("Rename track")) {
                pendingTrackId = track.id;
                pendingNewTrackDialog = false;
                std::strncpy(pendingTrackName, track.name.c_str(), sizeof(pendingTrackName) - 1);
                pendingTrackName[sizeof(pendingTrackName) - 1] = '\0';
                requestRenamePopup = true;
                ImGui::CloseCurrentPopup();
            }
            if(ImGui::MenuItem("New clip")) {
                pendingTrackId = track.id;
                pendingClipName[0] = '\0';
                pendingClipBindingId = track.bindings.empty() ? std::string() : track.bindings.front().id;
                pendingClipLaneType = track.bindings.empty() ? 0
                    : track.bindings.front().laneType == ofxOceanodeTimelineLaneType::Curve ? 1
                    : track.bindings.front().laneType == ofxOceanodeTimelineLaneType::PianoRoll ? 2 : 0;
                pendingStartBeat = snapBeat(transportState.beatPosition);
                pendingDurationBeats = timeline.getBeatsPerBar();
                requestClipPopup = true;
                ImGui::CloseCurrentPopup();
            }
            if(!track.bindings.empty()) {
                // Reachable here too (not just the per-binding row's own
                // menu) because that row only exists while the track is
                // expanded -- a collapsed track would otherwise have no way
                // to reach blend mode at all.
                if(ImGui::BeginMenu("Blend mode")) {
                    if(track.bindings.size() == 1) {
                        drawBlendModeOptions(timeline, track.id, track.bindings.front());
                    } else {
                        for(const auto& binding : track.bindings) {
                            const std::string label = compactParameterName(binding.parameterPath);
                            if(ImGui::BeginMenu(label.c_str())) {
                                drawBlendModeOptions(timeline, track.id, binding);
                                ImGui::EndMenu();
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                // Same reasoning as "Blend mode" above -- give a collapsed
                // track a way to unbind a parameter without having to
                // expand it first to reach the per-binding row's own menu.
                if(ImGui::BeginMenu("Remove from Timeline")) {
                    for(const auto& binding : track.bindings) {
                        const std::string label = compactParameterName(binding.parameterPath);
                        if(ImGui::Selectable(label.c_str())) {
                            removeBindingTrackId = track.id;
                            removeBindingId = binding.id;
                            requestRemoveBinding = true;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Remove track")) {
                trackDeletionId = track.id;
                requestTrackDeletion = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        auto drawGrid = [&](const ImVec2& min, const ImVec2& max) {
            if(timeline.isLoopEnabled()) {
                const float loopX1 = min.x + beatOffset(timeline.getLoopStartBeat());
                const float loopX2 = min.x + beatOffset(timeline.getLoopEndBeat());
                dl->AddRectFilled(ImVec2(std::max(min.x, loopX1), min.y), ImVec2(std::min(max.x, loopX2), max.y), IM_COL32(95, 105, 220, 22));
                dl->AddLine(ImVec2(loopX1, min.y), ImVec2(loopX1, max.y), IM_COL32(130, 145, 235, 95));
                dl->AddLine(ImVec2(loopX2, min.y), ImVec2(loopX2, max.y), IM_COL32(130, 145, 235, 95));
            }
            const double grid = displayGridBeats();
            const int lines = static_cast<int>(std::ceil(endBeat / grid));
            for(int i = 0; i <= lines; ++i) {
                const double beat = i * grid;
                const float x = min.x + beatOffset(beat);
                const bool isBar = std::fmod(beat, timeline.getBeatsPerBar()) < 0.001;
                const bool quarter = std::fmod(beat, 1.0) < 0.001;
                if(isBar || quarter || beatOffset(beat + grid) - beatOffset(beat) >= 4.0f)
                    dl->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), isBar ? kBar : quarter ? IM_COL32(92, 92, 92, 145) : kGrid, isBar ? 1.5f : 1.0f);
            }
        };

        auto drawClip = [&](const ofxOceanodeTimelineClip& clip, const ofxOceanodeTimelineLane* lane,
                            const ImVec2& min, const ImVec2& max, bool drawChrome = true) {
            const float x1 = min.x + beatOffset(clip.startBeat);
            const float x2 = min.x + beatOffset(clip.startBeat + clip.durationBeats);
            const float left = std::max(min.x, x1), right = std::min(max.x, x2);
            if(right <= left) return;
            if(drawChrome) {
                const ImU32 fill = IM_COL32(track.color.r, track.color.g, track.color.b, lane == nullptr ? 75 : 185);
                dl->AddRectFilled(ImVec2(left + 1, min.y + 3), ImVec2(right - 1, max.y - 3), fill, 3);
                dl->AddRect(ImVec2(left + 1, min.y + 3), ImVec2(right - 1, max.y - 3), IM_COL32(track.color.r, track.color.g, track.color.b, 245), 3);
                if(right - left > 45) {
                    std::string label = clip.name;
                    if(lane != nullptr) label += " [" + std::string(laneTypeName(lane->type)) + "]";
                    dl->AddText(ImVec2(left + 6, min.y + 6), IM_COL32(245, 250, 255, 255), label.c_str());
                }
            }
            if(lane == nullptr) return;
            if(lane->type == ofxOceanodeTimelineLaneType::Curve) {
                const auto interpolation = curveInterpolationMode(lane->curveInterpolation);
                auto points = lane->curvePoints;
                std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) { return a.beat < b.beat; });
                const int cycles = clip.repeatContent ? static_cast<int>(std::ceil(clip.durationBeats / cycleDuration(clip))) : 1;
                dl->PushClipRect(ImVec2(left + 1.0f, min.y + 3.0f), ImVec2(right - 1.0f, max.y - 3.0f), true);
                for(int cycle = 0; cycle < cycles; ++cycle) {
                    for(size_t i = 1; i < points.size(); ++i) {
                        const auto tension = i - 1 < lane->curveTensions.size()
                            ? lane->curveTensions[i - 1] : ofxOceanodeTimelineCurveTension{};
                        if(interpolation == CurveInterpolationMode::Step) {
                            const float x1 = min.x + beatOffset(sourceToTimelineBeat(clip, points[i - 1].beat, cycle));
                            const float x2 = min.x + beatOffset(sourceToTimelineBeat(clip, points[i].beat, cycle));
                            const float y1 = max.y - 5.0f - points[i - 1].value * (max.y - min.y - 10.0f);
                            const float y2 = max.y - 5.0f - points[i].value * (max.y - min.y - 10.0f);
                            dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y1), IM_COL32(245, 245, 245, 200), 1.5f);
                            dl->AddLine(ImVec2(x2, y1), ImVec2(x2, y2), IM_COL32(245, 245, 245, 200), 1.5f);
                            continue;
                        }
                        const int samples = 20;
                        for(int sample = 0; sample < samples; ++sample) {
                            const float t1 = sample / static_cast<float>(samples);
                            const float t2 = (sample + 1) / static_cast<float>(samples);
                            const double source1 = points[i - 1].beat + (points[i].beat - points[i - 1].beat) * t1;
                            const double source2 = points[i - 1].beat + (points[i].beat - points[i - 1].beat) * t2;
                            const float value1 = ofLerp(points[i - 1].value, points[i].value,
                                                        curveSegmentShape(t1, interpolation, tension));
                            const float value2 = ofLerp(points[i - 1].value, points[i].value,
                                                        curveSegmentShape(t2, interpolation, tension));
                            const float x1 = min.x + beatOffset(sourceToTimelineBeat(clip, source1, cycle));
                            const float x2 = min.x + beatOffset(sourceToTimelineBeat(clip, source2, cycle));
                            const float y1 = max.y - 5.0f - value1 * (max.y - min.y - 10.0f);
                            const float y2 = max.y - 5.0f - value2 * (max.y - min.y - 10.0f);
                            dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(245, 245, 245, 200), 1.5f);
                        }
                    }
                }
                dl->PopClipRect();
                return;
            }
            if(lane->type == ofxOceanodeTimelineLaneType::PianoRoll) {
                const int cycles = clip.repeatContent ? static_cast<int>(std::ceil(clip.durationBeats / cycleDuration(clip))) : 1;
                dl->PushClipRect(ImVec2(left + 1.0f, min.y + 3.0f), ImVec2(right - 1.0f, max.y - 3.0f), true);
                for(int cycle = 0; cycle < cycles; ++cycle) {
                    for(const auto& note : lane->pianoNotes) {
                        const float nx1 = min.x + beatOffset(sourceToTimelineBeat(clip, note.startBeat, cycle));
                        const float nx2 = min.x + beatOffset(sourceToTimelineBeat(clip, note.startBeat + note.durationBeats, cycle));
                        const float ny = max.y - 5.0f - (note.pitch % 12) / 12.0f * (max.y - min.y - 10.0f);
                        dl->AddRectFilled(ImVec2(nx1, ny - 3.0f), ImVec2(nx2, ny + 2.0f), IM_COL32(245, 245, 245, 180), 1.0f);
                    }
                }
                dl->PopClipRect();
                return;
            }
            const double patternLength = std::max(1.0 / kPPQ, lane->stepCount * lane->beatsPerStep);
            const double content = sourceDuration(clip);
            const int clipCycles = clip.repeatContent
                ? std::max(1, static_cast<int>(std::ceil(clip.durationBeats / cycleDuration(clip)))) : 1;
            const int patternCycles = std::max(1, static_cast<int>(std::ceil(content / patternLength)));
            dl->PushClipRect(ImVec2(left + 1.0f, min.y + 3.0f), ImVec2(right - 1.0f, max.y - 3.0f), true);
            for(int clipCycle = 0; clipCycle < clipCycles; ++clipCycle) {
                for(int patternCycle = 0; patternCycle < patternCycles; ++patternCycle) {
                    const double offset = patternCycle * patternLength;
                    if(clipCycle > 0 || patternCycle > 0) {
                        const float markerX = min.x + beatOffset(sourceToTimelineBeat(clip, offset, clipCycle));
                        dl->AddLine(ImVec2(markerX, min.y + 3.0f), ImVec2(markerX, max.y - 3.0f),
                                    IM_COL32(track.color.r, track.color.g, track.color.b, 245), 2.0f);
                    }
                    for(const auto& step : lane->step.steps) {
                        const double stepEnd = std::min(patternLength, step.startBeat +
                            (step.durationBeats > 0.0 ? step.durationBeats : lane->beatsPerStep));
                        const double startSource = offset + step.startBeat;
                        const double endSource = std::min(content, offset + stepEnd);
                        if(startSource >= content || endSource <= startSource) continue;
                        const float sx1 = min.x + beatOffset(sourceToTimelineBeat(clip, startSource, clipCycle));
                        const float sx2 = min.x + beatOffset(sourceToTimelineBeat(clip, endSource, clipCycle));
                        const float probability = ofClamp(step.probability, 0.0f, 1.0f);
                        const float top = max.y - 6.0f - probability * (max.y - min.y - 12.0f);
                        dl->AddRectFilled(ImVec2(std::max(sx1, min.x) + 2, top),
                                              ImVec2(std::min(sx2, max.x) - 2, max.y - 6),
                                              IM_COL32(245, 245, 245, 85), 2);
                    }
                }
            }
            dl->PopClipRect();
        };

        bool clipInteractionClaimedThisFrame = false;
        auto handleClip = [&](const ofxOceanodeTimelineClip& clip, const ofxOceanodeTimelineLane* lane,
                              const ImVec2& min, const ImVec2& max) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            // Never let a click that lands on the pinned label/properties
            // column be mistaken for a click on scrolled-away clip content
            // that happens to fall at the same screen x.
            if(mouse.x < zoneLeft) return;
            const float x1 = min.x + beatOffset(clip.startBeat);
            const float x2 = min.x + beatOffset(clip.startBeat + clip.durationBeats);
            if(!ImGui::IsMouseHoveringRect(min, max) || mouse.x < x1 || mouse.x > x2) return;
            const std::string menuId = "##clipMenu" + track.id + "_" + clip.id;
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                if(clipInteractionClaimedThisFrame) return;
                clipInteractionClaimedThisFrame = true;
                pendingTrackId = track.id;
                pendingClipId = clip.id;
                pendingLaneId = lane != nullptr ? lane->id : (clip.lanes.empty() ? "" : clip.lanes.front().id);
                ImGui::OpenPopup(menuId.c_str());
            }
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if(clipInteractionClaimedThisFrame || clipDragMode != ClipDragMode::None) return;
                clipInteractionClaimedThisFrame = true;
                pendingTrackId = track.id;
                pendingClipId = clip.id;
                pendingLaneId = lane != nullptr ? lane->id : (clip.lanes.empty() ? "" : clip.lanes.front().id);
                const double clickedBeat = beatAtOffset(mouse.x - min.x);
                if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && lane != nullptr) {
                    editorTrackId = track.id;
                    editorClipId = clip.id;
                    editorLaneId = lane->id;
                    clipEditorOpen = true;
                    if(trackCollapsed) {
                        // Layout stays collapsed for the rest of this frame,
                        // but the model expands now so the editor is docked
                        // directly below the track on the next frame.
                        if(auto* editTrack = timeline.getTrack(track.id)) editTrack->collapsed = false;
                        expandTrackForEditor = true;
                    }
                    pendingStartBeat = snapBeat(std::max(0.0, clickedBeat - clip.startBeat));
                    pendingDurationBeats = 1.0;
                } else {
                    const bool edge = std::abs(mouse.x - x2) <= kEdgePixels;
                    // On macOS Cmd is the stretch modifier. Ctrl is reserved
                    // by the canvas/ImGui interaction layer and can turn a
                    // drag into a context-menu gesture.
                    // ImGui deliberately maps macOS Cmd to its logical Ctrl
                    // modifier (and physical Ctrl to Super). Using KeySuper
                    // here was why Cmd+drag never selected Stretch.
                    const bool commandDown = ImGui::GetIO().KeyCtrl ||
                        (ImGui::GetIO().KeyMods & ImGuiMod_Ctrl) != 0;
                    clipDragMode = edge ? (ImGui::GetIO().KeyShift ? ClipDragMode::Resize
                                                                  : commandDown ? ClipDragMode::Stretch
                                                                                : ClipDragMode::Repeat)
                                        : ClipDragMode::Move;
                    draggingTrackId = track.id;
                    draggingClipId = clip.id;
                    dragOffsetBeats = clickedBeat - clip.startBeat;
                    dragInitialContentDuration = clip.contentDurationBeats;
                    dragInitialContentStretch = clip.contentStretch;
                    dragTimelineOriginX = min.x;
                }
            }
        };

        // Popups must be submitted every frame, even after the mouse leaves
        // the clip that opened them.
        auto drawClipMenu = [&](const ofxOceanodeTimelineClip& clip) {
            const std::string menuId = "##clipMenu" + track.id + "_" + clip.id;
            if(!ImGui::BeginPopup(menuId.c_str())) return;
            ImGui::TextUnformatted(clip.name.c_str());
            ImGui::Separator();
            // These values are edited on the selected clip, never on the
            // track or on the other clips in it.
            if(auto* editClip = timeline.getClip(track.id, clip.id)) {
                double start = editClip->startBeat;
                double duration = editClip->durationBeats;
                double contentDuration = editClip->contentDurationBeats;
                if(ImGui::InputDouble("Start", &start, editIncrement(), 1.0, "%.3f"))
                    timeline.setClipTiming(track.id, clip.id, snapBeat(start), editClip->durationBeats);
                if(ImGui::InputDouble("Duration", &duration, editIncrement(), 1.0, "%.3f")) {
                    const double newDuration = std::max(1.0 / kPPQ, snapBeat(duration));
                    timeline.setClipTiming(track.id, clip.id, editClip->startBeat, newDuration);
                    if(!editClip->repeatContent) {
                        editClip->contentStretch = newDuration /
                            std::max(1.0 / kPPQ, editClip->contentDurationBeats);
                    }
                }
                if(ImGui::InputDouble("Content", &contentDuration, editIncrement(), 1.0, "%.3f"))
                    timeline.setClipContentDuration(track.id, clip.id,
                                                    std::max(1.0 / kPPQ, snapBeat(contentDuration)),
                                                    editClip->repeatContent);
                bool repeat = editClip->repeatContent;
                if(ImGui::Checkbox("Repeat content", &repeat))
                    timeline.setClipContentDuration(track.id, clip.id, editClip->contentDurationBeats, repeat);
                ImGui::Separator();
                if(ImGui::MenuItem("Consolidate content"))
                    timeline.consolidateClipContent(track.id, clip.id);
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("Permanently remove source data hidden beyond the clip's right edge");
                ImGui::Separator();
            }
            auto* selectedLane = timeline.getLane(track.id, clip.id, pendingLaneId);
            const auto selectedType = selectedLane == nullptr ? ofxOceanodeTimelineLaneType::Step : selectedLane->type;
            if(ImGui::MenuItem("Step Sequencer", nullptr, selectedType == ofxOceanodeTimelineLaneType::Step) && selectedLane != nullptr) timeline.setClipLaneType(track.id, clip.id, selectedLane->id, ofxOceanodeTimelineLaneType::Step);
            if(ImGui::MenuItem("Piano Roll", nullptr, selectedType == ofxOceanodeTimelineLaneType::PianoRoll) && selectedLane != nullptr) timeline.setClipLaneType(track.id, clip.id, selectedLane->id, ofxOceanodeTimelineLaneType::PianoRoll);
            if(ImGui::MenuItem("Curve", nullptr, selectedType == ofxOceanodeTimelineLaneType::Curve) && selectedLane != nullptr) timeline.setClipLaneType(track.id, clip.id, selectedLane->id, ofxOceanodeTimelineLaneType::Curve);
            if(selectedLane != nullptr && ImGui::BeginMenu("Add parameter to this lane")) {
                for(const auto& candidate : track.bindings) {
                    const bool assigned = std::find(selectedLane->bindingIds.begin(), selectedLane->bindingIds.end(), candidate.id) != selectedLane->bindingIds.end();
                    if(ImGui::MenuItem(candidate.parameterPath.c_str(), nullptr, assigned, true)) {
                        if(assigned) timeline.removeBindingFromLane(track.id, clip.id, selectedLane->id, candidate.id);
                        else timeline.addBindingToLane(track.id, clip.id, selectedLane->id, candidate.id);
                    }
                }
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("New lane")) {
                for(const auto& candidate : track.bindings) {
                    const std::string label = compactParameterName(candidate.parameterPath);
                    if(ImGui::MenuItem(label.c_str())) {
                        const auto laneId = timeline.createLane(track.id, clip.id, candidate.parameterPath, candidate.laneType);
                        if(!laneId.empty()) {
                            timeline.addBindingToLane(track.id, clip.id, laneId, candidate.id);
                            pendingLaneId = laneId;
                        }
                    }
                    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", candidate.parameterPath.c_str());
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Delete clip")) {
                clipDeletionTrackId = track.id;
                clipDeletionClipId = clip.id;
                requestClipDeletion = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        };

        if(trackCollapsed) {
            const ImVec2 min = headerMin, max = headerMax, laneMin(min.x + kLabelWidth - timelineScrollX, min.y);
            dl->PushClipRect(ImVec2(zoneLeft, min.y), ImVec2(max.x, max.y), true);
            drawGrid(laneMin, max);
            for(const auto& clip : track.clips) {
                if(clip.lanes.empty()) {
                    drawClip(clip, nullptr, laneMin, max);
                } else {
                    for(size_t laneIndex = 0; laneIndex < clip.lanes.size(); ++laneIndex) {
                        drawClip(clip, &clip.lanes[laneIndex], laneMin, max, laneIndex == 0);
                    }
                }
            }
            // Later clips are drawn on top, so hit-test in reverse order and
            // let the visually topmost overlapping clip own the gesture.
            for(auto clipIt = track.clips.rbegin(); clipIt != track.clips.rend(); ++clipIt) {
                const auto* frontLane = clipIt->lanes.empty() ? nullptr : &clipIt->lanes.front();
                handleClip(*clipIt, frontLane, laneMin, max);
            }
            for(const auto& clip : track.clips) {
                drawClipMenu(clip);
            }
            const float px = laneMin.x + beatOffset(transportState.beatPosition);
            if(px >= zoneLeft && px <= max.x) dl->AddLine(ImVec2(px, min.y), ImVec2(px, max.y), kPlayhead, 2);
            dl->PopClipRect();
            ImGui::SetCursorPosY(headerY + kCollapsedHeight);
            // A collapsed track hides its rows entirely, so any clip editor
            // still open for it would float disconnected from what it's
            // editing -- close it rather than keep drawing it.
            if(clipEditorOpen && editorTrackId == track.id && !expandTrackForEditor) {
                clipEditorOpen = false;
                editorTrackId.clear();
                editorClipId.clear();
                editorLaneId.clear();
            }
        } else {
            int index = 0;
            // Rows sharing a clip.id (a clip with more than one lane,
            // combining e.g. a curve and a step pattern on this track) are
            // bracketed together below so it reads as one entity rather
            // than a coincidence.
            std::unordered_map<std::string, std::vector<std::pair<float, float>>> multiLaneClipRowSpans;
            for(const auto& binding : track.bindings) {
                const float y = headerY + kHeaderHeight + index * kRowHeight;
                ImGui::SetCursorPos(ImVec2(0, y));
                ImGui::InvisibleButton(("##binding" + track.id + binding.id).c_str(), ImVec2(contentWidth, kRowHeight));
                const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax(), laneMin(min.x + kLabelWidth - timelineScrollX, min.y);
                dl->AddRectFilled(min, max, mutedTrackColor(track.color, 0.17f));
                dl->PushClipRect(ImVec2(zoneLeft, min.y), ImVec2(max.x, max.y), true);
                drawGrid(laneMin, max);
                dl->PopClipRect();
                bool automated = false;
                for(const auto& clip : track.clips) automated = automated || laneForBinding(clip, binding.id) != nullptr;
                dl->AddRectFilled(min, ImVec2(min.x + kLabelWidth, max.y),
                                  mutedTrackColor(track.color, automated ? 0.34f : 0.23f, 0.48f));
                dl->AddRect(min, ImVec2(min.x + kLabelWidth, max.y), IM_COL32(track.color.r, track.color.g, track.color.b, 210));
                dl->PushClipRect(ImVec2(min.x + 5.0f, min.y), ImVec2(min.x + kLabelWidth - 7.0f, max.y), true);
                std::string bindingLabel = binding.parameterPath;
                if(binding.mode != ofxOceanodeTimelineAutomationMode::Replace)
                    bindingLabel += "  [" + ofxOceanodeTimelineManager::modeToString(binding.mode) + "]";
                dl->AddText(ImVec2(min.x + 7, min.y + 6), automated ? IM_COL32(245, 250, 255, 255) : IM_COL32(180, 180, 180, 255), bindingLabel.c_str());
                dl->PopClipRect();

                const std::string bindingMenuId = "##bindingMenu" + track.id + binding.id;
                if(ImGui::IsMouseHoveringRect(min, max) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    const bool overTimelineZone = ImGui::GetIO().MousePos.x >= zoneLeft;
                    const double clickedBeat = overTimelineZone
                        ? beatAtOffset(ImGui::GetIO().MousePos.x - laneMin.x)
                        : 0.0;
                    bool overClip = false;
                    if(overTimelineZone) {
                        for(const auto& clip : track.clips) {
                            if(laneForBinding(clip, binding.id) != nullptr &&
                               clickedBeat >= clip.startBeat && clickedBeat <= clip.startBeat + clip.durationBeats) {
                                overClip = true;
                                break;
                            }
                        }
                    }
                    if(!overClip) {
                        pendingStartBeat = snapBeat(std::max(0.0, clickedBeat));
                        ImGui::OpenPopup(bindingMenuId.c_str());
                    }
                }
                if(ImGui::BeginPopup(bindingMenuId.c_str())) {
                    if(ImGui::MenuItem("New clip here")) {
                        pendingTrackId = track.id;
                        pendingClipBindingId = binding.id;
                        pendingClipLaneType = binding.laneType == ofxOceanodeTimelineLaneType::Curve ? 1
                            : binding.laneType == ofxOceanodeTimelineLaneType::PianoRoll ? 2 : 0;
                        pendingClipName[0] = '\0';
                        pendingDurationBeats = timeline.getBeatsPerBar();
                        requestClipPopup = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::Separator();
                    // When more than one binding (possibly on other tracks)
                    // drives the same parameter, this decides how this one
                    // combines with the others -- e.g. a piano-roll gate set
                    // to Multiply onto a level curve driven by Replace.
                    if(ImGui::BeginMenu("Blend mode")) {
                        drawBlendModeOptions(timeline, track.id, binding);
                        ImGui::EndMenu();
                    }
                    ImGui::Separator();
                    if(ImGui::Selectable("Remove from Timeline")) {
                        removeBindingTrackId = track.id;
                        removeBindingId = binding.id;
                        requestRemoveBinding = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                dl->PushClipRect(ImVec2(zoneLeft, min.y), ImVec2(max.x, max.y), true);
                if(!track.bindings.empty() && binding.id == track.bindings.front().id) {
                    for(const auto& clip : track.clips) drawClipMenu(clip);
                }
                for(const auto& clip : track.clips) {
                    const auto* lane = laneForBinding(clip, binding.id);
                    if(lane == nullptr) continue;
                    drawClip(clip, lane, laneMin, max);
                    if(clip.lanes.size() > 1) multiLaneClipRowSpans[clip.id].push_back({min.y, max.y});
                }
                for(auto clipIt = track.clips.rbegin(); clipIt != track.clips.rend(); ++clipIt) {
                    const auto* lane = laneForBinding(*clipIt, binding.id);
                    if(lane != nullptr) handleClip(*clipIt, lane, laneMin, max);
                }
                const float px = laneMin.x + beatOffset(transportState.beatPosition);
                if(px >= zoneLeft && px <= max.x) dl->AddLine(ImVec2(px, min.y), ImVec2(px, max.y), kPlayhead, 2);
                dl->PopClipRect();
                ++index;
            }
            if(!multiLaneClipRowSpans.empty()) {
                const float contentOriginX = zoneLeft - timelineScrollX;
                const float rowRight = ImGui::GetWindowPos().x + contentWidth;
                for(const auto& [clipId, rowSpans] : multiLaneClipRowSpans) {
                    if(rowSpans.size() < 2) continue;
                    const auto clipIt = std::find_if(track.clips.begin(), track.clips.end(),
                        [&](const auto& c) { return c.id == clipId; });
                    if(clipIt == track.clips.end()) continue;
                    const float x1 = contentOriginX + beatOffset(clipIt->startBeat);
                    const float x2 = contentOriginX + beatOffset(clipIt->startBeat + clipIt->durationBeats);
                    if(x2 < zoneLeft || x1 > rowRight) continue;
                    const float barX = std::max(zoneLeft + 2.0f, x1 - 4.0f);
                    float top = rowSpans.front().first, bottom = rowSpans.front().second;
                    for(const auto& span : rowSpans) {
                        top = std::min(top, span.first);
                        bottom = std::max(bottom, span.second);
                    }
                    dl->AddLine(ImVec2(barX, top + 6.0f), ImVec2(barX, bottom - 6.0f), IM_COL32(255, 210, 90, 210), 2.0f);
                    for(const auto& span : rowSpans) {
                        const float midY = (span.first + span.second) * 0.5f;
                        dl->AddLine(ImVec2(barX, midY), ImVec2(barX + 4.0f, midY), IM_COL32(255, 210, 90, 210), 2.0f);
                    }
                }
            }
            ImGui::SetCursorPosY(headerY + kHeaderHeight + index * kRowHeight);
            if(clipEditorOpen && editorTrackId == track.id) drawLaneEditor(timeline, track, contentWidth, endBeat, transportState.beatPosition);
        }
    }

    if(requestTrackDeletion) {
        // Clear controller-only state before the manager destroys all of the
        // track's bindings, clips and lanes.
        if(const auto* deletedTrack = timeline.getTrack(trackDeletionId)) {
            for(const auto& clip : deletedTrack->clips) {
                for(const auto& lane : clip.lanes) {
                    collapsedLaneIds.erase(lane.id);
                    laneEditorHeights.erase(lane.id);
                }
            }
        }
        if(clipEditorOpen && editorTrackId == trackDeletionId) {
            clipEditorOpen = false;
            editorTrackId.clear();
            editorClipId.clear();
            editorLaneId.clear();
            pianoSelectedNoteIndices.clear();
            pianoDragSnapshot.clear();
            pianoValueDragSnapshot.clear();
        }
        if(draggingTrackId == trackDeletionId) {
            clipDragMode = ClipDragMode::None;
            draggingTrackId.clear();
            draggingClipId.clear();
        }
        if(pianoKeyboardPreviewTrackId == trackDeletionId) {
            pianoKeyboardPreviewActive = false;
            pianoKeyboardPreviewTrackId.clear();
            pianoKeyboardPreviewGateBindingId.clear();
            pianoKeyboardPreviewPitchBindingId.clear();
            pianoKeyboardPreviewPitch = -1;
        }
        timeline.removeTrack(trackDeletionId);
        requestTrackDeletion = false;
        trackDeletionId.clear();
    }

    if(requestClipDeletion) {
        if(const auto* deletedClip = timeline.getClip(clipDeletionTrackId, clipDeletionClipId)) {
            for(const auto& lane : deletedClip->lanes) {
                collapsedLaneIds.erase(lane.id);
                laneEditorHeights.erase(lane.id);
            }
        }
        if(clipEditorOpen && editorTrackId == clipDeletionTrackId && editorClipId == clipDeletionClipId) {
            clipEditorOpen = false;
            editorTrackId.clear();
            editorClipId.clear();
            editorLaneId.clear();
            pianoSelectedNoteIndices.clear();
            pianoDragSnapshot.clear();
            pianoValueDragSnapshot.clear();
        }
        timeline.removeClip(clipDeletionTrackId, clipDeletionClipId);
        requestClipDeletion = false;
        clipDeletionTrackId.clear();
        clipDeletionClipId.clear();
    }

    if(requestRemoveBinding) {
        timeline.removeBinding(removeBindingTrackId, removeBindingId);
        requestRemoveBinding = false;
        removeBindingTrackId.clear();
        removeBindingId.clear();
    }

    if(requestAddLane) {
        requestAddLane = false;
        const auto laneType = pendingAddLaneType == 1 ? ofxOceanodeTimelineLaneType::Curve
            : pendingAddLaneType == 2 ? ofxOceanodeTimelineLaneType::PianoRoll
            : ofxOceanodeTimelineLaneType::Step;
        const char* laneName = pendingAddLaneType == 1 ? "Curve"
            : pendingAddLaneType == 2 ? "Piano Roll" : "Step Sequencer";
        const auto newLaneId = timeline.createLane(editorTrackId, editorClipId, laneName, laneType);
        if(!newLaneId.empty()) editorLaneId = newLaneId;
    }

    if(requestRemoveLane) {
        requestRemoveLane = false;
        if(auto* clip = timeline.getClip(editorTrackId, editorClipId)) {
            if(clip->lanes.size() <= 1) {
                collapsedLaneIds.erase(pendingRemoveLaneId);
                laneEditorHeights.erase(pendingRemoveLaneId);
                timeline.removeClip(editorTrackId, editorClipId);
                clipEditorOpen = false;
                editorTrackId.clear();
                editorClipId.clear();
                editorLaneId.clear();
            } else {
                collapsedLaneIds.erase(pendingRemoveLaneId);
                laneEditorHeights.erase(pendingRemoveLaneId);
                timeline.removeLane(editorTrackId, editorClipId, pendingRemoveLaneId);
                if(editorLaneId == pendingRemoveLaneId) {
                    if(auto* clip2 = timeline.getClip(editorTrackId, editorClipId)) {
                        if(!clip2->lanes.empty()) editorLaneId = clip2->lanes.front().id;
                    }
                }
            }
        }
        pendingRemoveLaneId.clear();
    }

    if(clipDragMode != ClipDragMode::None) {
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if(auto* clip = timeline.getClip(draggingTrackId, draggingClipId)) {
                const double mouseBeat = beatAtOffset(ImGui::GetIO().MousePos.x - dragTimelineOriginX);
                if(clipDragMode == ClipDragMode::Move) {
                    timeline.setClipTiming(draggingTrackId, draggingClipId,
                                           snapBeat(mouseBeat - dragOffsetBeats), clip->durationBeats);
                }
                else {
                    const double duration = std::max(1.0 / kPPQ, snapBeat(mouseBeat - clip->startBeat));
                    timeline.setClipTiming(draggingTrackId, draggingClipId, clip->startBeat, duration);
                    if(clipDragMode == ClipDragMode::Repeat) timeline.setClipContentDuration(draggingTrackId, draggingClipId, dragInitialContentDuration, true);
                    else if(clipDragMode == ClipDragMode::Stretch) {
                        timeline.setClipContentDuration(draggingTrackId, draggingClipId, dragInitialContentDuration, false);
                        clip->contentStretch = duration / std::max(1.0 / kPPQ, dragInitialContentDuration);
                    }
                    else {
                        // Shift-resize adds/crops source space without
                        // changing an existing stretch ratio.
                        clip->contentStretch = std::max(1.0 / 1024.0, dragInitialContentStretch);
                        timeline.setClipContentDuration(draggingTrackId, draggingClipId,
                            duration / clip->contentStretch, false);
                    }
                }
            }
        } else {
            clipDragMode = ClipDragMode::None;
            draggingTrackId.clear();
            draggingClipId.clear();
        }
    }
    // Absolute positioning is used throughout the ruler, tracks and docked
    // editor. Always finish the scrolling child with a submitted item so a
    // state change on this frame (collapse/close/delete) cannot leave ImGui's
    // final cursor position uncommitted.
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::EndChild();

    // Horizontal scrolling is manual (timelineScrollX) so the label column
    // can stay pinned (see the comment above BeginChild), which meant losing
    // ImGui's native scrollbar along with native ScrollX. This redraws an
    // equivalent scrollbar by hand, in the strip reserved below the viewport,
    // spanning only the content zone (the label column never scrolls).
    {
        const ImVec2 viewportMin = ImGui::GetItemRectMin();
        const ImVec2 viewportMax = ImGui::GetItemRectMax();
        const float trackLeft = viewportMin.x + kLabelWidth;
        const float trackRight = viewportMax.x;
        const float trackWidth = std::max(1.0f, trackRight - trackLeft);
        ImDrawList* footerDl = ImGui::GetWindowDrawList();
        const ImVec2 trackMin(trackLeft, viewportMax.y);
        const ImVec2 trackMax(trackRight, viewportMax.y + scrollbarHeight);
        footerDl->AddRectFilled(trackMin, trackMax, IM_COL32(20, 20, 20, 255));

        const float thumbWidth = maxTimelineScrollX <= 0.0f ? trackWidth
            : std::max(24.0f, trackWidth * trackWidth / (trackWidth + maxTimelineScrollX));
        const float thumbTravel = std::max(0.0f, trackWidth - thumbWidth);
        const float thumbX = trackLeft + (maxTimelineScrollX > 0.0f
            ? thumbTravel * ofClamp(timelineScrollX / maxTimelineScrollX, 0.0f, 1.0f) : 0.0f);
        const ImVec2 thumbMin(thumbX, trackMin.y + 2.0f);
        const ImVec2 thumbMax(thumbX + thumbWidth, trackMax.y - 2.0f);

        ImGui::SetCursorScreenPos(trackMin);
        ImGui::InvisibleButton("##timelineHScrollbar", ImVec2(trackWidth, scrollbarHeight));
        const bool thumbHovered = ImGui::IsItemHovered();
        const bool thumbActive = ImGui::IsItemActive();
        if(maxTimelineScrollX > 0.0f) {
            if(ImGui::IsItemClicked() && !ImGui::IsMouseHoveringRect(thumbMin, thumbMax)) {
                const float targetThumbX = ofClamp(ImGui::GetIO().MousePos.x - thumbWidth * 0.5f,
                                                   trackLeft, trackLeft + thumbTravel);
                timelineScrollX = ofClamp((targetThumbX - trackLeft) / std::max(1.0f, thumbTravel) * maxTimelineScrollX,
                                          0.0f, maxTimelineScrollX);
            }
            if(thumbActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                timelineScrollX = ofClamp(timelineScrollX +
                    ImGui::GetIO().MouseDelta.x / std::max(1.0f, thumbTravel) * maxTimelineScrollX,
                    0.0f, maxTimelineScrollX);
            }
        }
        footerDl->AddRectFilled(thumbMin, thumbMax,
                                thumbActive ? IM_COL32(150, 150, 160, 255)
                                            : thumbHovered ? IM_COL32(120, 120, 130, 255) : IM_COL32(90, 90, 100, 255), 3.0f);
    }

    if(requestRenamePopup) {
        ImGui::OpenPopup(pendingNewTrackDialog ? "New Timeline Track" : "Rename Timeline Track");
        requestRenamePopup = false;
    }
    if(requestClipPopup) { ImGui::OpenPopup("New Timeline Clip"); requestClipPopup = false; }
    drawRenamePopup(timeline);
    drawClipPopup(timeline);
}

void ofxOceanodeTimelineController::drawRuler(ofxOceanodeTimelineManager& timeline, float labelWidth,
                                              float timelineWidth, double endBeat,
                                              double beatPosition, float bpm) {
    const float width = labelWidth + timelineWidth;
    ImGui::InvisibleButton("##timelineRuler", ImVec2(width, kRulerHeight));
    const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(min, max, IM_COL32(35, 35, 35, 255));
    const float rulerDividerY = min.y + 31.0f;
    dl->AddLine(ImVec2(min.x, rulerDividerY), ImVec2(max.x, rulerDividerY), IM_COL32(80, 80, 80, 180));
    dl->AddText(ImVec2(min.x + 7, min.y + 8), IM_COL32(165, 165, 165, 255), "SECONDS");
    dl->AddText(ImVec2(min.x + 7, rulerDividerY + 8), IM_COL32(220, 220, 220, 255), "BEATS");
    const float zoneLeft = min.x + labelWidth;
    const ImVec2 laneMin(zoneLeft - timelineScrollX, min.y);
    dl->PushClipRect(ImVec2(zoneLeft, min.y), ImVec2(max.x, max.y), true);
    const double grid = displayGridBeats();
    const int gridLines = static_cast<int>(std::ceil(endBeat / grid));
    for(int i = 0; i <= gridLines; ++i) {
        const double beat = i * grid;
        const float x = laneMin.x + beatToPixels(timeline, beat, bpm);
        const bool bar = std::fmod(beat, timeline.getBeatsPerBar()) < 0.001;
        const bool quarter = std::fmod(beat, 1.0) < 0.001;
        if(bar || quarter || beatToPixels(timeline, beat + grid, bpm) - beatToPixels(timeline, beat, bpm) >= 4.0f)
            dl->AddLine(ImVec2(x, rulerDividerY), ImVec2(x, max.y), bar ? kBar : quarter ? IM_COL32(95, 95, 95, 165) : kGrid, bar ? 1.5f : 1.0f);
        if(bar) {
            char beatLabel[16];
            std::snprintf(beatLabel, sizeof(beatLabel), "%d", static_cast<int>(beat / timeline.getBeatsPerBar()) + 1);
            dl->AddText(ImVec2(x + 5, rulerDividerY + 5), IM_COL32(235, 235, 235, 255), beatLabel);
        }
    }

    const int totalSeconds = static_cast<int>(std::floor(timeline.beatToSeconds(endBeat, bpm)));
    const int secondsStep = std::max(1, static_cast<int>(std::ceil(totalSeconds / 2048.0)));
    float lastSecondsLabelX = -1000.0f;
    for(int second = 0; second <= totalSeconds; second += secondsStep) {
        const float x = laneMin.x + second * pixelsPerSecond;
        dl->AddLine(ImVec2(x, min.y), ImVec2(x, rulerDividerY), IM_COL32(90, 90, 100, 145), 1.0f);
        if(x - lastSecondsLabelX >= 38.0f) {
            char secondsLabel[20];
            std::snprintf(secondsLabel, sizeof(secondsLabel), "%ds", second);
            dl->AddText(ImVec2(x + 4.0f, min.y + 5.0f), IM_COL32(175, 175, 185, 255), secondsLabel);
            lastSecondsLabelX = x;
        }
    }

    if(timeline.isLoopEnabled()) {
        const float loopX1 = laneMin.x + beatToPixels(timeline, timeline.getLoopStartBeat(), bpm);
        const float loopX2 = laneMin.x + beatToPixels(timeline, timeline.getLoopEndBeat(), bpm);
        dl->AddRectFilled(ImVec2(loopX1, min.y), ImVec2(loopX2, max.y), IM_COL32(95, 105, 220, 48));
        dl->AddRectFilled(ImVec2(loopX1 - 3.0f, min.y), ImVec2(loopX1 + 3.0f, max.y), IM_COL32(150, 165, 255, 230));
        dl->AddRectFilled(ImVec2(loopX2 - 3.0f, min.y), ImVec2(loopX2 + 3.0f, max.y), IM_COL32(150, 165, 255, 230));
    }
    const float px = laneMin.x + beatToPixels(timeline, beatPosition, bpm);
    if(px >= zoneLeft && px <= max.x) dl->AddLine(ImVec2(px, min.y), ImVec2(px, max.y), kPlayhead, 2.5f);
    dl->PopClipRect();

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool hovered = ImGui::IsItemHovered() && mouse.x >= zoneLeft;
    const double rawMouseBeat = ofClamp(
        pixelsToBeat(timeline, mouse.x - laneMin.x, bpm, endBeat), 0.0, endBeat);
    const double mouseBeat = snapBeat(rawMouseBeat);
    if(hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        const double loopLength = timeline.getBeatsPerBar();
        const double loopStart = snapBeat(std::max(0.0, mouseBeat - loopLength * 0.5));
        timeline.setLoopEnabled(true);
        timeline.setLoopRange(loopStart, loopStart + loopLength);
        loopDragMode = LoopDragMode::None;
    } else if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const float loopStartX = laneMin.x + beatToPixels(timeline, timeline.getLoopStartBeat(), bpm);
        const float loopEndX = laneMin.x + beatToPixels(timeline, timeline.getLoopEndBeat(), bpm);
        if(timeline.isLoopEnabled() && std::abs(mouse.x - loopStartX) <= 7.0f) {
            loopDragMode = LoopDragMode::Start;
        } else if(timeline.isLoopEnabled() && std::abs(mouse.x - loopEndX) <= 7.0f) {
            loopDragMode = LoopDragMode::End;
        } else if(timeline.isLoopEnabled() && ImGui::GetIO().KeyShift &&
                  mouseBeat > timeline.getLoopStartBeat() && mouseBeat < timeline.getLoopEndBeat()) {
            loopDragMode = LoopDragMode::Move;
            loopDragAnchorBeat = mouseBeat;
            loopDragStartBeat = timeline.getLoopStartBeat();
            loopDragEndBeat = timeline.getLoopEndBeat();
        } else if(container->getTransport() != nullptr) {
            loopDragMode = LoopDragMode::Scrub;
            container->getTransport()->seekToBeat(rawMouseBeat);
        }
    }
    if(loopDragMode != LoopDragMode::None && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if(loopDragMode == LoopDragMode::Scrub) {
            if(container->getTransport() != nullptr) container->getTransport()->seekToBeat(rawMouseBeat);
        } else if(loopDragMode == LoopDragMode::Start) {
            timeline.setLoopRange(std::min(mouseBeat, timeline.getLoopEndBeat() - 1.0 / kPPQ), timeline.getLoopEndBeat());
        } else if(loopDragMode == LoopDragMode::End) {
            timeline.setLoopRange(timeline.getLoopStartBeat(), std::max(mouseBeat, timeline.getLoopStartBeat() + 1.0 / kPPQ));
        } else if(loopDragMode == LoopDragMode::Move) {
            const double length = loopDragEndBeat - loopDragStartBeat;
            const double start = std::max(0.0, loopDragStartBeat + mouseBeat - loopDragAnchorBeat);
            timeline.setLoopRange(start, start + length);
        }
    }
    if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) loopDragMode = LoopDragMode::None;
}

void ofxOceanodeTimelineController::drawBpmLane(ofxOceanodeTimelineManager& timeline,
                                                float contentWidth, double endBeat,
                                                double beatPosition, float fallbackBpm) {
    const bool collapsed = timeline.isBpmLaneCollapsed();
    const float height = collapsed ? 29.0f : 132.0f;
    ImGui::Dummy(ImVec2(contentWidth, height));
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float zoneLeft = min.x + kLabelWidth;
    const ImVec2 timelineMin(zoneLeft - timelineScrollX, min.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(min, max, IM_COL32(27, 27, 30, 255));
    dl->AddRectFilled(min, ImVec2(zoneLeft, max.y), IM_COL32(85, 90, 150, 75));
    dl->AddLine(ImVec2(zoneLeft - 1.0f, min.y), ImVec2(zoneLeft - 1.0f, max.y), IM_COL32(125, 135, 225, 190));

    // Keep every BPM control inside the fixed properties column. The combo
    // used to continue the top row after the Auto/manual controls, which put
    // it over the automation canvas on narrower windows.
    ImGui::PushClipRect(min, ImVec2(zoneLeft, max.y), true);
    ImGui::SetCursorScreenPos(ImVec2(min.x + 5.0f, min.y + 4.0f));
    ImGui::SetNextItemAllowOverlap();
    if(ImGui::SmallButton(collapsed ? ">##bpmCollapse" : "v##bpmCollapse"))
        timeline.setBpmLaneCollapsed(!collapsed);
    ImGui::SameLine();
    ImGui::TextUnformatted("BPM");
    ImGui::SameLine();
    bool enabled = timeline.isBpmAutomationEnabled();
    if(ImGui::Checkbox("Auto##bpmAuto", &enabled)) timeline.setBpmAutomationEnabled(enabled);
    if(!enabled) {
        // With automation off, this lane still shows Oceanode's actual BPM
        // (the flat line below), but there was no way to type an exact
        // value here - only via the separate transport toolbar or the Time
        // controller. Both write through container->setBpm(), and this
        // reads transportState.bpm fresh every frame, so all three stay in
        // sync regardless of which one last changed it.
        ImGui::SameLine();
        float manualBpm = fallbackBpm;
        ImGui::SetNextItemWidth(70.0f);
        if(ImGui::DragFloat("##bpmManual", &manualBpm, 0.1f, 1.0f, 999.0f, "%.1f"))
            container->setBpm(manualBpm);
    }
    // Same 4 shapes as a curve lane (Step/Linear/Log-Exp/Sigmoid), reusing
    // the shared curve math so the BPM curve is evaluated, drawn and timed
    // the exact same way a regular automation curve is.
    const auto bpmInterpolation = curveInterpolationMode(timeline.getBpmInterpolation());
    if(!collapsed) {
        int interpolationMode = static_cast<int>(bpmInterpolation);
        ImGui::SetCursorScreenPos(ImVec2(min.x + 5.0f, min.y + 35.0f));
        ImGui::TextUnformatted("Mode");
        ImGui::SetCursorScreenPos(ImVec2(min.x + 55.0f, min.y + 31.0f));
        ImGui::SetNextItemWidth(std::max(1.0f, zoneLeft - min.x - 61.0f));
        if(ImGui::Combo("##bpmInterpolation", &interpolationMode, kCurveInterpolationNames, 4)) {
            timeline.setBpmInterpolation(kCurveInterpolationNames[interpolationMode]);
            bpmTensionSegment = -1;
        }
        float minimum = timeline.getBpmMinimum();
        float maximum = timeline.getBpmMaximum();
        const float rangeStartX = min.x + 55.0f;
        const float rangeAvailable = std::max(2.0f, zoneLeft - rangeStartX - 6.0f);
        const float rangeFieldWidth = std::max(1.0f, (rangeAvailable - 4.0f) * 0.5f);
        ImGui::SetCursorScreenPos(ImVec2(min.x + 5.0f, min.y + 64.0f));
        ImGui::TextUnformatted("Range");
        ImGui::SetCursorScreenPos(ImVec2(rangeStartX, min.y + 60.0f));
        ImGui::SetNextItemWidth(rangeFieldWidth);
        if(ImGui::DragFloat("##bpmMin", &minimum, 0.5f, 1.0f, 998.0f, "%.0f")) timeline.setBpmRange(minimum, maximum);
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Minimum BPM");
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::SetNextItemWidth(rangeFieldWidth);
        if(ImGui::DragFloat("##bpmMax", &maximum, 0.5f, minimum + 1.0f, 999.0f, "%.0f")) timeline.setBpmRange(minimum, maximum);
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Maximum BPM");
        ImGui::SetCursorScreenPos(ImVec2(min.x + 55.0f, min.y + 89.0f));
        if(bpmInterpolation == CurveInterpolationMode::LogExp) ImGui::TextDisabled("Alt-drag vertically");
        else if(bpmInterpolation == CurveInterpolationMode::Sigmoid) ImGui::TextDisabled("Alt-drag freely");
    }
    ImGui::PopClipRect();

    const float bpmCanvasStartX = std::max(zoneLeft, timelineMin.x);
    ImGui::SetCursorScreenPos(ImVec2(bpmCanvasStartX, timelineMin.y));
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##bpmAutomationCanvas",
                          ImVec2(std::max(1.0f, contentWidth - kLabelWidth - (bpmCanvasStartX - timelineMin.x)), height));
    const bool canvasHovered = ImGui::IsItemHovered() && ImGui::GetIO().MousePos.x >= zoneLeft;
    const float graphTop = min.y + (collapsed ? 4.0f : 8.0f);
    const float graphBottom = max.y - (collapsed ? 4.0f : 8.0f);
    dl->PushClipRect(ImVec2(zoneLeft, graphTop), ImVec2(max.x, graphBottom), true);
    if(timeline.isLoopEnabled()) {
        const float loopX1 = timelineMin.x + beatToPixels(timeline, timeline.getLoopStartBeat(), fallbackBpm);
        const float loopX2 = timelineMin.x + beatToPixels(timeline, timeline.getLoopEndBeat(), fallbackBpm);
        dl->AddRectFilled(ImVec2(std::max(timelineMin.x, loopX1), graphTop), ImVec2(std::min(max.x, loopX2), graphBottom), IM_COL32(95, 105, 220, 25));
    }
    const double grid = displayGridBeats();
    const int gridLines = static_cast<int>(std::ceil(endBeat / grid));
    for(int i = 0; i <= gridLines; ++i) {
        const double beat = i * grid;
        const float x = timelineMin.x + beatToPixels(timeline, beat, fallbackBpm);
        const bool bar = std::fmod(beat, timeline.getBeatsPerBar()) < 0.001;
        if(bar || beatToPixels(timeline, beat + grid, fallbackBpm) - beatToPixels(timeline, beat, fallbackBpm) >= 4.0f)
            dl->AddLine(ImVec2(x, graphTop), ImVec2(x, graphBottom), bar ? kBar : kGrid, bar ? 1.5f : 1.0f);
    }

    auto& sourcePoints = timeline.getBpmAutomationPoints();
    auto& sourceTensions = timeline.getBpmCurveTensions();
    if(bpmDragPointIndex < 0 && bpmTensionSegment < 0)
        std::sort(sourcePoints.begin(), sourcePoints.end(), [](const auto& a, const auto& b) { return a.beat < b.beat; });
    sourceTensions.resize(sourcePoints.empty() ? 0 : sourcePoints.size() - 1);
    std::vector<ofxOceanodeTimelineCurvePoint> points = sourcePoints;
    if(points.empty()) points.push_back({0.0, fallbackBpm});
    std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) { return a.beat < b.beat; });
    const float bpmSpan = std::max(1.0f, timeline.getBpmMaximum() - timeline.getBpmMinimum());
    auto bpmToY = [&](float value) {
        const float normalized = ofClamp((value - timeline.getBpmMinimum()) / bpmSpan, 0.0f, 1.0f);
        return graphBottom - normalized * (graphBottom - graphTop);
    };
    if(points.size() == 1) {
        dl->AddLine(ImVec2(timelineMin.x, bpmToY(points.front().value)),
                    ImVec2(max.x, bpmToY(points.front().value)), IM_COL32(155, 170, 255, 220), 2.0f);
    } else if(bpmInterpolation == CurveInterpolationMode::Step) {
        for(size_t i = 1; i < points.size(); ++i) {
            const ImVec2 a(timelineMin.x + beatToPixels(timeline, points[i - 1].beat, fallbackBpm), bpmToY(points[i - 1].value));
            const ImVec2 b(timelineMin.x + beatToPixels(timeline, points[i].beat, fallbackBpm), bpmToY(points[i].value));
            dl->AddLine(a, ImVec2(b.x, a.y), IM_COL32(155, 170, 255, 230), 2.0f);
            dl->AddLine(ImVec2(b.x, a.y), b, IM_COL32(155, 170, 255, 230), 2.0f);
        }
    } else {
        for(size_t i = 1; i < points.size(); ++i) {
            const auto tension = i - 1 < sourceTensions.size() ? sourceTensions[i - 1] : ofxOceanodeTimelineCurveTension{};
            const float x1 = timelineMin.x + beatToPixels(timeline, points[i - 1].beat, fallbackBpm);
            const float x2 = timelineMin.x + beatToPixels(timeline, points[i].beat, fallbackBpm);
            const int samples = std::max(16, static_cast<int>(std::abs(x2 - x1) / 4.0f));
            for(int sample = 0; sample < samples; ++sample) {
                const float t1 = sample / static_cast<float>(samples);
                const float t2 = (sample + 1) / static_cast<float>(samples);
                const double beat1 = points[i - 1].beat + (points[i].beat - points[i - 1].beat) * t1;
                const double beat2 = points[i - 1].beat + (points[i].beat - points[i - 1].beat) * t2;
                const float value1 = ofLerp(points[i - 1].value, points[i].value, curveSegmentShape(t1, bpmInterpolation, tension));
                const float value2 = ofLerp(points[i - 1].value, points[i].value, curveSegmentShape(t2, bpmInterpolation, tension));
                const ImVec2 a(timelineMin.x + beatToPixels(timeline, beat1, fallbackBpm), bpmToY(value1));
                const ImVec2 b(timelineMin.x + beatToPixels(timeline, beat2, fallbackBpm), bpmToY(value2));
                dl->AddLine(a, b, IM_COL32(155, 170, 255, 230), 2.0f);
            }
        }
    }
    if(!collapsed) for(const auto& point : points) {
        const ImVec2 p(timelineMin.x + beatToPixels(timeline, point.beat, fallbackBpm), bpmToY(point.value));
        dl->AddCircleFilled(p, 4.0f, IM_COL32(225, 230, 255, 245));
    }
    dl->PopClipRect();

    if(!collapsed && canvasHovered) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const double beat = snapBeat(pixelsToBeat(timeline, mouse.x - timelineMin.x, fallbackBpm, endBeat));
        const float value = ofClamp(timeline.getBpmMinimum() +
            (graphBottom - mouse.y) / std::max(1.0f, graphBottom - graphTop) * bpmSpan,
            timeline.getBpmMinimum(), timeline.getBpmMaximum());
        const double tolerance = std::max(1.0 / kPPQ,
            std::abs(pixelsToBeat(timeline, mouse.x - timelineMin.x + 5.0f, fallbackBpm, endBeat) - beat));
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            bpmValuePointIndex = -1;
            if(sourcePoints.empty() && std::abs(mouse.x - timelineMin.x) <= 8.0f &&
               std::abs(bpmToY(fallbackBpm) - mouse.y) <= 9.0f) {
                sourcePoints.push_back({0.0, ofClamp(fallbackBpm, timeline.getBpmMinimum(), timeline.getBpmMaximum())});
                bpmValuePointIndex = 0;
            }
            for(int i = static_cast<int>(sourcePoints.size()) - 1; i >= 0; --i) {
                const float pointX = timelineMin.x + beatToPixels(timeline, sourcePoints[i].beat, fallbackBpm);
                if(std::abs(pointX - mouse.x) <= 8.0f && std::abs(bpmToY(sourcePoints[i].value) - mouse.y) <= 9.0f) {
                    bpmValuePointIndex = i;
                    bpmNumericValue = sourcePoints[i].value;
                    break;
                }
            }
            if(bpmValuePointIndex >= 0) ImGui::OpenPopup("BPM point value");
        } else if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            bpmDragPointIndex = -1;
            bpmTensionSegment = -1;
            for(int i = static_cast<int>(sourcePoints.size()) - 1; i >= 0; --i) {
                if(std::abs(sourcePoints[i].beat - beat) <= tolerance && std::abs(bpmToY(sourcePoints[i].value) - mouse.y) <= 9.0f) {
                    bpmDragPointIndex = i;
                    break;
                }
            }
            if(bpmDragPointIndex < 0 && ImGui::GetIO().KeyAlt &&
               (bpmInterpolation == CurveInterpolationMode::LogExp || bpmInterpolation == CurveInterpolationMode::Sigmoid) &&
               sourcePoints.size() > 1) {
                for(size_t i = 1; i < sourcePoints.size(); ++i) {
                    const auto& a = sourcePoints[i - 1];
                    const auto& b = sourcePoints[i];
                    if(beat < a.beat || beat > b.beat) continue;
                    const float t = static_cast<float>((beat - a.beat) / std::max(1e-9, b.beat - a.beat));
                    const auto tension = i - 1 < sourceTensions.size() ? sourceTensions[i - 1] : ofxOceanodeTimelineCurveTension{};
                    const float segmentValue = ofLerp(a.value, b.value, curveSegmentShape(t, bpmInterpolation, tension));
                    const float segmentY = bpmToY(segmentValue);
                    if(std::abs(mouse.y - segmentY) <= 12.0f) {
                        bpmTensionSegment = static_cast<int>(i - 1);
                        bpmTensionDragStartX = mouse.x;
                        bpmTensionDragStartY = mouse.y;
                        bpmTensionStartInflection = tension.inflection;
                        bpmTensionStartSteepness = tension.steepness;
                    }
                    break;
                }
            }
            if(bpmDragPointIndex < 0 && bpmTensionSegment < 0 && !ImGui::GetIO().KeyAlt) {
                sourcePoints.push_back({beat, value});
                std::sort(sourcePoints.begin(), sourcePoints.end(), [](const auto& a, const auto& b) { return a.beat < b.beat; });
                sourceTensions.resize(sourcePoints.empty() ? 0 : sourcePoints.size() - 1);
                bpmDragPointIndex = static_cast<int>(std::min_element(sourcePoints.begin(), sourcePoints.end(), [&](const auto& a, const auto& b) {
                    return std::abs(a.beat - beat) < std::abs(b.beat - beat);
                }) - sourcePoints.begin());
            }
        }
        // Once a point or a tension handle has been grabbed, keep updating
        // it for as long as the mouse button is held, even if the cursor
        // strays outside the lane's rect, matching the curve lane editor.
        if((bpmDragPointIndex >= 0 || bpmTensionSegment >= 0) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if(bpmDragPointIndex >= 0 && bpmDragPointIndex < static_cast<int>(sourcePoints.size())) {
                const double minimumBeat = bpmDragPointIndex > 0
                    ? sourcePoints[bpmDragPointIndex - 1].beat + 1.0 / kPPQ : 0.0;
                const double maximumBeat = bpmDragPointIndex + 1 < static_cast<int>(sourcePoints.size())
                    ? sourcePoints[bpmDragPointIndex + 1].beat - 1.0 / kPPQ : endBeat;
                sourcePoints[bpmDragPointIndex].beat = std::max(minimumBeat,
                    std::min(std::max(minimumBeat, maximumBeat), beat));
                sourcePoints[bpmDragPointIndex].value = value;
            } else if(bpmTensionSegment >= 0 && bpmTensionSegment < static_cast<int>(sourceTensions.size())) {
                auto& tension = sourceTensions[bpmTensionSegment];
                if(bpmInterpolation == CurveInterpolationMode::Sigmoid) {
                    tension.inflection = ofClamp(bpmTensionStartInflection +
                        (mouse.x - bpmTensionDragStartX) / std::max(1.0f, max.x - timelineMin.x), 0.01f, 0.99f);
                } else {
                    tension.inflection = 0.5f;
                }
                const float steepnessDelta = -(mouse.y - bpmTensionDragStartY) /
                    std::max(1.0f, (graphBottom - graphTop) / 3.0f);
                tension.steepness = ofClamp(bpmTensionStartSteepness * std::exp(steepnessDelta * 0.5f), 0.1f, 10.0f);
            }
        }
    }
    if(ImGui::BeginPopup("BPM point value")) {
        if(bpmValuePointIndex >= 0 && bpmValuePointIndex < static_cast<int>(sourcePoints.size())) {
            ImGui::SetNextItemWidth(110.0f);
            if(ImGui::InputFloat("BPM", &bpmNumericValue, 0.1f, 1.0f, "%.3f")) {
                sourcePoints[bpmValuePointIndex].value = ofClamp(bpmNumericValue,
                    timeline.getBpmMinimum(), timeline.getBpmMaximum());
            }
            if(ImGui::Button("Delete point")) {
                sourcePoints.erase(sourcePoints.begin() + bpmValuePointIndex);
                sourceTensions.resize(sourcePoints.empty() ? 0 : sourcePoints.size() - 1);
                bpmValuePointIndex = -1;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
    if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if(bpmDragPointIndex >= 0)
            std::sort(sourcePoints.begin(), sourcePoints.end(), [](const auto& a, const auto& b) { return a.beat < b.beat; });
        bpmDragPointIndex = -1;
        bpmTensionSegment = -1;
    }
    if(collapsed && canvasHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        timeline.setBpmLaneCollapsed(false);

    const float playheadX = timelineMin.x + beatToPixels(timeline, beatPosition, fallbackBpm);
    if(playheadX >= zoneLeft && playheadX <= max.x)
        dl->AddLine(ImVec2(playheadX, min.y), ImVec2(playheadX, max.y), kPlayhead, 2.0f);
    finishAbsoluteLayout(ImVec2(min.x, max.y));
}

void ofxOceanodeTimelineController::drawLaneEditor(ofxOceanodeTimelineManager& timeline,
                                                  const ofxOceanodeTimelineTrack& track,
                                                  float contentWidth, double endBeat,
                                                  double beatPosition) {
    auto* clip = timeline.getClip(editorTrackId, editorClipId);
    if(clip == nullptr || clip->lanes.empty()) {
        clipEditorOpen = false;
        return;
    }
    if(timeline.getLane(editorTrackId, editorClipId, editorLaneId) == nullptr) {
        editorLaneId = clip->lanes.front().id;
    }
    const float fallbackBpm = container->getTransportState().bpm;
    auto beatOffset = [&](double beat) { return beatToPixels(timeline, beat, fallbackBpm); };
    auto beatAtOffset = [&](float pixels) { return pixelsToBeat(timeline, pixels, fallbackBpm, endBeat); };

    // A clip can hold more than one lane (e.g. a curve and a step pattern
    // combined together). The clip gets one header row (name / add lane /
    // close); each lane below it is its own independently collapsible
    // block, stacked vertically and indented to read as nested under the
    // clip. Only the focused lane (editorLaneId) reacts to canvas
    // clicks/drags -- the drag/selection state further down (piano note
    // drag, curve point drag, step painting) is shared single-lane state,
    // so letting two expanded lanes both respond to the mouse in the same
    // frame would let them stomp on each other's notes/points. Click a
    // lane's header (or its canvas) to focus it before editing it.
    const float clipIndent = 20.0f;
    ImGui::Dummy(ImVec2(contentWidth, 22.0f));
    {
        const ImVec2 clipHeaderMin = ImGui::GetItemRectMin();
        const ImVec2 clipHeaderMax = ImGui::GetItemRectMax();
        const float headerZoneLeft = clipHeaderMin.x + kLabelWidth;
        ImDrawList* headerDl = ImGui::GetWindowDrawList();
        headerDl->AddRectFilled(ImVec2(clipHeaderMin.x + clipIndent, clipHeaderMin.y), ImVec2(headerZoneLeft, clipHeaderMax.y),
                                mutedTrackColor(track.color, 0.25f, 0.44f));
        headerDl->AddLine(ImVec2(headerZoneLeft - 1.0f, clipHeaderMin.y), ImVec2(headerZoneLeft - 1.0f, clipHeaderMax.y),
                          IM_COL32(track.color.r, track.color.g, track.color.b, 210));
        ImGui::SetCursorScreenPos(ImVec2(clipHeaderMin.x + 5.0f + clipIndent, clipHeaderMin.y + 2.0f));
        ImGui::TextColored(ImVec4(track.color.r / 255.0f, track.color.g / 255.0f, track.color.b / 255.0f, 1.0f),
                           "%s", clip->name.c_str());
        ImGui::SameLine(kLabelWidth - 50.0f);
        if(ImGui::SmallButton("+##addLane")) ImGui::OpenPopup("##addLaneTypePopup");
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Add another lane to this clip");
        ImGui::SameLine();
        if(ImGui::SmallButton("x")) clipEditorOpen = false;
        if(ImGui::BeginPopup("##addLaneTypePopup")) {
            if(ImGui::MenuItem("Step Sequencer")) { requestAddLane = true; pendingAddLaneType = 0; }
            if(ImGui::MenuItem("Curve")) { requestAddLane = true; pendingAddLaneType = 1; }
            if(ImGui::MenuItem("Piano Roll")) { requestAddLane = true; pendingAddLaneType = 2; }
            ImGui::EndPopup();
        }
        finishAbsoluteLayout(ImVec2(clipHeaderMin.x, clipHeaderMax.y));
    }

    for(size_t laneIndex = 0; laneIndex < clip->lanes.size(); ++laneIndex) {
    auto* lane = &clip->lanes[laneIndex];
    const bool isFocused = lane->id == editorLaneId;
    std::string laneLabel = laneTypeName(lane->type);
    if(!lane->bindingIds.empty()) {
        if(const auto* laneBoundParam = timeline.getBinding(track.id, lane->bindingIds.front()))
            laneLabel += ": " + compactParameterName(laneBoundParam->parameterPath);
    }
    if(collapsedLaneIds.count(lane->id) > 0) {
        ImGui::Dummy(ImVec2(contentWidth, 22.0f));
        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        const float rowZoneLeft = rowMin.x + kLabelWidth;
        ImDrawList* rowDl = ImGui::GetWindowDrawList();
        rowDl->AddRectFilled(ImVec2(rowMin.x + clipIndent, rowMin.y), ImVec2(rowZoneLeft, rowMax.y), mutedTrackColor(track.color, 0.18f, 0.30f));
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x + 5.0f + clipIndent, rowMin.y + 2.0f));
        if(ImGui::SmallButton((">##laneExpand" + lane->id).c_str())) collapsedLaneIds.erase(lane->id);
        ImGui::SameLine();
        ImGui::Selectable((laneLabel + "##laneHeaderCollapsed" + lane->id).c_str(), isFocused,
                          ImGuiSelectableFlags_None, ImVec2(ImGui::GetContentRegionAvail().x - 4.0f, 0));
        if(ImGui::IsItemClicked()) editorLaneId = lane->id;
        finishAbsoluteLayout(ImVec2(rowMin.x, rowMax.y));
        continue;
    }

    const float defaultEditorHeight = lane->type == ofxOceanodeTimelineLaneType::PianoRoll
        ? 270.0f : 190.0f;
    const auto laneHeightIt = laneEditorHeights.find(lane->id);
    const float editorHeight = laneHeightIt != laneEditorHeights.end() ? laneHeightIt->second : defaultEditorHeight;
    // Reserve the whole dock row first, then give only the timeline half an
    // input item. This leaves the left configuration zone interactive.
    ImGui::Dummy(ImVec2(contentWidth, editorHeight));
    const ImVec2 editorMin = ImGui::GetItemRectMin();
    const ImVec2 editorMax = ImGui::GetItemRectMax();
    const float zoneLeft = editorMin.x + kLabelWidth;
    const ImVec2 timelineMin(zoneLeft - timelineScrollX, editorMin.y);
    const float editorCanvasStartX = std::max(zoneLeft, timelineMin.x);
    ImGui::SetCursorScreenPos(ImVec2(editorCanvasStartX, timelineMin.y));
    ImGui::InvisibleButton(("##clipEditorCanvas" + editorTrackId + editorClipId + lane->id).c_str(),
                           ImVec2(std::max(1.0f, contentWidth - kLabelWidth - (editorCanvasStartX - timelineMin.x)),
                                  std::max(1.0f, editorHeight - kLaneResizeHandleHeight)));
    const bool editorCanvasHovered = ImGui::IsItemHovered() && ImGui::GetIO().MousePos.x >= zoneLeft;
    if(!isFocused && editorCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) editorLaneId = lane->id;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Leave the hierarchy gutter to the left of the lane indentation
    // untouched; both the editor and its properties panel belong visually
    // below the indented clip header.
    dl->AddRectFilled(ImVec2(editorMin.x + clipIndent, editorMin.y), editorMax,
                      mutedTrackColor(track.color, 0.13f, 0.26f));
    dl->AddRectFilled(ImVec2(editorMin.x + clipIndent, editorMin.y), ImVec2(zoneLeft, editorMax.y),
                      mutedTrackColor(track.color, 0.25f, 0.44f));
    dl->AddLine(ImVec2(zoneLeft - 1.0f, editorMin.y), ImVec2(zoneLeft - 1.0f, editorMax.y), IM_COL32(track.color.r, track.color.g, track.color.b, isFocused ? 210 : 120));

    // The bottom strip is excluded from both the canvas item and properties
    // child. Track the drag directly from its rectangle so later editor
    // widgets cannot replace the handle's active ImGui id mid-drag.
    {
        const ImVec2 handleMin(editorMin.x + clipIndent, editorMax.y - kLaneResizeHandleHeight);
        const ImVec2 handleMax(editorMax.x, editorMax.y);
        const bool resizeHovered = ImGui::IsMouseHoveringRect(handleMin, handleMax);
        if(resizeHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            resizingLaneId = lane->id;
            laneResizeStartMouseY = ImGui::GetIO().MousePos.y;
            laneResizeStartHeight = editorHeight;
            editorLaneId = lane->id;
        }
        const bool resizeActive = resizingLaneId == lane->id && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if(resizeHovered || resizeActive) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if(resizeActive) {
            laneEditorHeights[lane->id] = ofClamp(laneResizeStartHeight + ImGui::GetIO().MousePos.y - laneResizeStartMouseY,
                                                  kLaneEditorMinHeight, kLaneEditorMaxHeight);
        }
        if(resizingLaneId == lane->id && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) resizingLaneId.clear();
        const float separatorY = editorMax.y - kLaneResizeHandleHeight * 0.5f;
        dl->AddLine(ImVec2(handleMin.x, separatorY), ImVec2(handleMax.x, separatorY),
                    resizeActive ? IM_COL32(205, 205, 215, 235)
                                 : resizeHovered ? IM_COL32(160, 160, 170, 205)
                                                 : IM_COL32(80, 80, 86, 110),
                    resizeActive ? 2.0f : 1.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(editorMin.x + 5.0f + clipIndent, editorMin.y + 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 4.0f));
    const float pianoKeyboardReserve = lane->type == ofxOceanodeTimelineLaneType::PianoRoll
        ? kPianoKeyboardWidth + kPianoScrollbarWidth + 9.0f : 0.0f;
    ImGui::BeginChild(("##clipProperties" + editorTrackId + editorClipId + lane->id).c_str(),
                      ImVec2(kLabelWidth - 10.0f - clipIndent - pianoKeyboardReserve,
                             editorHeight - 8.0f - kLaneResizeHandleHeight), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(ImGui::SmallButton(("v##laneCollapse" + lane->id).c_str())) collapsedLaneIds.insert(lane->id);
    ImGui::SameLine();
    if(ImGui::Selectable((laneLabel + "##laneHeader" + lane->id).c_str(), isFocused,
                         ImGuiSelectableFlags_None, ImVec2(ImGui::GetContentRegionAvail().x - 22.0f, 0))) {
        editorLaneId = lane->id;
    }
    ImGui::SameLine();
    if(ImGui::SmallButton(("x##removeLane" + lane->id).c_str())) {
        requestRemoveLane = true;
        pendingRemoveLaneId = lane->id;
    }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Remove this lane (deletes the whole clip if it's the only one)");

    if(ImGui::BeginTabBar("##clipPropertyTabs")) {
        if(ImGui::BeginTabItem("Clip")) {
            const std::string parameterPreview = lane->bindingIds.empty()
                ? "No bindings" : ofToString(lane->bindingIds.size()) + " binding" + (lane->bindingIds.size() == 1 ? "" : "s");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if(ImGui::BeginCombo("##bindings", parameterPreview.c_str())) {
                for(const auto& candidate : track.bindings) {
                    const bool assigned = std::find(lane->bindingIds.begin(), lane->bindingIds.end(), candidate.id) != lane->bindingIds.end();
                    const std::string compactName = compactParameterName(candidate.parameterPath);
                    if(ImGui::Selectable(compactName.c_str(), assigned)) {
                        if(assigned) timeline.removeBindingFromLane(track.id, clip->id, lane->id, candidate.id);
                        else timeline.addBindingToLane(track.id, clip->id, lane->id, candidate.id);
                    }
                    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", candidate.parameterPath.c_str());
                }
                ImGui::EndCombo();
            }

            float start = static_cast<float>(clip->startBeat);
            float duration = static_cast<float>(clip->durationBeats);
            const bool compactPianoProperties = lane->type == ofxOceanodeTimelineLaneType::PianoRoll;
            ImGui::TextUnformatted("Start"); ImGui::SameLine(compactPianoProperties ? 45.0f : 58.0f);
            ImGui::SetNextItemWidth(compactPianoProperties ? 48.0f : 62.0f);
            if(ImGui::DragFloat("##clipStart", &start, static_cast<float>(editIncrement()), 0.0f, 9999.0f, "%.3g"))
                timeline.setClipTiming(track.id, clip->id, snapBeat(start), clip->durationBeats);
            ImGui::SameLine(); ImGui::TextUnformatted("Len"); ImGui::SameLine();
            ImGui::SetNextItemWidth(compactPianoProperties ? 42.0f : 55.0f);
            if(ImGui::DragFloat("##clipLength", &duration, static_cast<float>(editIncrement()), static_cast<float>(1.0 / kPPQ), 9999.0f, "%.3g")) {
                const double newLength = std::max(1.0 / kPPQ, snapBeat(duration));
                const double clipStretch = stretch(*clip);
                timeline.setClipTiming(track.id, clip->id, clip->startBeat, newLength);
                timeline.setClipContentDuration(track.id, clip->id, newLength / clipStretch, false);
            }

            if(lane->type != ofxOceanodeTimelineLaneType::PianoRoll) {
                ImGui::TextUnformatted("Range"); ImGui::SameLine(58.0f); ImGui::SetNextItemWidth(62.0f);
                ImGui::DragFloat("##rangeMin", &lane->valueMin, 0.01f, -99999.0f, 99999.0f, "%.4g");
                ImGui::SameLine(); ImGui::TextUnformatted("to"); ImGui::SameLine(); ImGui::SetNextItemWidth(55.0f);
                ImGui::DragFloat("##rangeMax", &lane->valueMax, 0.01f, -99999.0f, 99999.0f, "%.4g");
            }
            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Editor")) {
            int divisionIndex = divisionIndexForBeats(lane->beatsPerStep);
            auto drawDivision = [&]() {
                ImGui::SetNextItemWidth(94.0f);
                if(ImGui::BeginCombo("Grid", kDivisionOptions[divisionIndex].label)) {
                    for(int i = 1; i < kDivisionOptionCount; ++i) {
                        if(ImGui::Selectable(kDivisionOptions[i].label, i == divisionIndex)) {
                            divisionIndex = i;
                            lane->beatsPerStep = kDivisionOptions[i].beats;
                        }
                    }
                    ImGui::EndCombo();
                }
            };

            if(lane->type == ofxOceanodeTimelineLaneType::Step) {
                int steps = std::max(1, lane->stepCount);
                ImGui::SetNextItemWidth(94.0f);
                if(ImGui::DragInt("Steps", &steps, 0.2f, 1, 128)) {
                    lane->stepCount = steps;
                }
                drawDivision();
                int behavior = lane->behavior == "Always" ? 1 : lane->behavior == "Mute" ? 2 : 0;
                const char* behaviors[] = {"Probability", "Always", "Mute"};
                ImGui::SetNextItemWidth(94.0f);
                if(ImGui::Combo("Mode", &behavior, behaviors, 3)) lane->behavior = behaviors[behavior];
                ImGui::Checkbox("Probability", &lane->probabilityEnabled);
            } else if(lane->type == ofxOceanodeTimelineLaneType::Curve) {
                int interpolationMode = static_cast<int>(curveInterpolationMode(lane->curveInterpolation));
                ImGui::SetNextItemWidth(105.0f);
                if(ImGui::Combo("Mode##curveInterpolation", &interpolationMode, kCurveInterpolationNames, 4)) {
                    lane->curveInterpolation = kCurveInterpolationNames[interpolationMode];
                    resetCurveTensions(*lane);
                    curveDragPointIndex = -1;
                    curveTensionSegment = -1;
                }
                const auto selectedInterpolation = curveInterpolationMode(lane->curveInterpolation);
                if(selectedInterpolation == CurveInterpolationMode::LogExp) ImGui::TextDisabled("Alt-drag vertically");
                else if(selectedInterpolation == CurveInterpolationMode::Sigmoid) ImGui::TextDisabled("Alt-drag freely");
                ImGui::Checkbox("Clamp 0..1", &lane->curveClamp);
            } else {
                auto drawPianoRole = [&](const char* label, std::string& roleId) {
                    const auto* currentBinding = roleId.empty() ? nullptr : timeline.getBinding(track.id, roleId);
                    const std::string preview = currentBinding == nullptr ? "None" : compactParameterName(currentBinding->parameterPath);
                    ImGui::SetNextItemWidth(112.0f);
                    if(ImGui::BeginCombo(label, preview.c_str())) {
                        if(ImGui::Selectable("None", currentBinding == nullptr)) {
                            const std::string previousRoleId = roleId;
                            roleId.clear();
                            const bool stillUsed = lane->pianoPitchBindingId == previousRoleId ||
                                lane->pianoGateBindingId == previousRoleId ||
                                lane->pianoVelocityBindingId == previousRoleId;
                            if(!previousRoleId.empty() && !stillUsed)
                                timeline.removeBindingFromLane(track.id, clip->id, lane->id, previousRoleId);
                        }
                        for(const auto& candidate : track.bindings) {
                            const bool selected = candidate.id == roleId;
                            const std::string compactName = compactParameterName(candidate.parameterPath);
                            if(ImGui::Selectable(compactName.c_str(), selected)) {
                                const std::string previousRoleId = roleId;
                                if(&roleId != &lane->pianoPitchBindingId && lane->pianoPitchBindingId == candidate.id) lane->pianoPitchBindingId.clear();
                                if(&roleId != &lane->pianoGateBindingId && lane->pianoGateBindingId == candidate.id) lane->pianoGateBindingId.clear();
                                if(&roleId != &lane->pianoVelocityBindingId && lane->pianoVelocityBindingId == candidate.id) lane->pianoVelocityBindingId.clear();
                                roleId = candidate.id;
                                timeline.addBindingToLane(track.id, clip->id, lane->id, candidate.id);
                                const bool previousStillUsed = lane->pianoPitchBindingId == previousRoleId ||
                                    lane->pianoGateBindingId == previousRoleId ||
                                    lane->pianoVelocityBindingId == previousRoleId;
                                if(!previousRoleId.empty() && previousRoleId != candidate.id && !previousStillUsed)
                                    timeline.removeBindingFromLane(track.id, clip->id, lane->id, previousRoleId);
                            }
                            if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", candidate.parameterPath.c_str());
                        }
                        ImGui::EndCombo();
                    }
                };
                drawPianoRole("Pitch", lane->pianoPitchBindingId);
                drawPianoRole("Gate", lane->pianoGateBindingId);
                drawPianoRole("Velocity", lane->pianoVelocityBindingId);
                drawDivision();
                ImGui::SetNextItemWidth(94.0f);
                ImGui::DragInt("Low", &lane->pianoLowPitch, 0.25f, 0, 127);
                ImGui::SetNextItemWidth(94.0f);
                ImGui::DragInt("High", &lane->pianoHighPitch, 0.25f, 0, 127);
                lane->pianoHighPitch = std::max(lane->pianoLowPitch, lane->pianoHighPitch);
                ImGui::SetNextItemWidth(94.0f);
                ImGui::SliderFloat("Vel", &lane->pianoDefaultVelocity, 0.0f, 1.0f, "%.2f");
                ImGui::Checkbox("Snap", &lane->pianoSnapToGrid);
                ImGui::SameLine(); ImGui::Checkbox("Mono", &lane->pianoMonophonic);
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    finishAbsoluteLayout(ImVec2(editorMin.x, editorMax.y));

    const float clipX1 = timelineMin.x + beatOffset(clip->startBeat);
    const float clipX2 = timelineMin.x + beatOffset(clip->startBeat + clip->durationBeats);
    const float left = std::max(zoneLeft, clipX1);
    const float right = std::min(editorMax.x, clipX2);
    if(right <= left) {
        finishAbsoluteLayout(ImVec2(editorMin.x, editorMax.y));
        continue;
    }

    // Everything below draws actual clip content (notes / curve / steps),
    // which must never visually bleed into the pinned properties column
    // (and, for a piano roll, its pinned keyboard strip) to its left.
    dl->PushClipRect(ImVec2(zoneLeft - pianoKeyboardReserve, editorMin.y), ImVec2(editorMax.x, editorMax.y), true);

    if(lane->type == ofxOceanodeTimelineLaneType::PianoRoll) {
        // The note selection is keyed by index into this lane's pianoNotes,
        // so switching to a different lane must drop it -- otherwise stale
        // indices could highlight (or, worse, delete) unrelated notes.
        if(isFocused && pianoSelectionLaneId != lane->id) {
            pianoSelectionLaneId = lane->id;
            pianoSelectedNoteIndices.clear();
            pianoDragSnapshot.clear();
            pianoMarqueeActive = false;
        }
        const float rollTop = editorMin.y + 8.0f;
        const float rollBottom = editorMax.y - 66.0f;
        const float velocityTop = rollBottom + 5.0f;
        const float velocityBottom = velocityTop + 25.0f;
        const float probabilityTop = velocityBottom + 4.0f;
        const float probabilityBottom = editorMax.y - 6.0f;
        const float keyboardRight = zoneLeft - 1.0f;
        const float keyboardLeft = keyboardRight - kPianoKeyboardWidth;
        const float pianoScrollbarRight = keyboardLeft - 4.0f;
        const float pianoScrollbarLeft = pianoScrollbarRight - kPianoScrollbarWidth;

        const int lowPitch = ofClamp(lane->pianoLowPitch, 0, 127);
        const int highPitch = ofClamp(lane->pianoHighPitch, lowPitch, 127);
        dl->AddRectFilled(ImVec2(keyboardLeft, rollTop), ImVec2(keyboardRight, rollBottom),
                          IM_COL32(205, 205, 200, 255));
        const float pitchHeight = (rollBottom - rollTop) / static_cast<float>(highPitch - lowPitch + 1);
        // Black keys are drawn on the side away from the note grid (like the
        // shank of a real key, which doesn't reach all the way to the front
        // edge). Each row is clipped to its own bounds so a black key can
        // never visually bleed into the semitone above or below it.
        const float blackKeyRight = keyboardLeft + kPianoKeyboardWidth * 0.70f;
        for(int keyboardPitch = lowPitch; keyboardPitch <= highPitch; ++keyboardPitch) {
            const float keyBottom = rollBottom - (keyboardPitch - lowPitch) * pitchHeight;
            const float keyTop = keyBottom - pitchHeight;
            const int noteClass = keyboardPitch % 12;
            const bool blackKey = noteClass == 1 || noteClass == 3 || noteClass == 6 || noteClass == 8 || noteClass == 10;
            // Clicking (and, while held, dragging across) a key previews it:
            // the lane's Gate/Pitch bindings get a live override for as long
            // as the mouse is down, so the bound parameters sound the note
            // immediately -- like pressing a real keyboard -- without
            // writing anything into the clip.
            const bool keyPressedHere = pianoKeyboardPreviewActive && pianoKeyboardPreviewTrackId == track.id &&
                pianoKeyboardPreviewPitch == keyboardPitch;
            const bool keyHovered = ImGui::IsMouseHoveringRect(ImVec2(keyboardLeft, keyTop), ImVec2(keyboardRight, keyBottom));
            if(keyHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                // Match the canvas's own convention (see editorCanvasHovered
                // above): clicking anywhere interactive on an unfocused
                // lane focuses it, rather than silently doing nothing.
                if(!isFocused) editorLaneId = lane->id;
                pianoKeyboardPreviewActive = true;
                pianoKeyboardPreviewTrackId = track.id;
                pianoKeyboardPreviewGateBindingId = lane->pianoGateBindingId;
                pianoKeyboardPreviewPitchBindingId = lane->pianoPitchBindingId;
                pianoKeyboardPreviewPitch = keyboardPitch;
                if(!lane->pianoGateBindingId.empty()) timeline.setLiveOverride(track.id, lane->pianoGateBindingId, "1");
                if(!lane->pianoPitchBindingId.empty()) timeline.setLiveOverride(track.id, lane->pianoPitchBindingId, ofToString(keyboardPitch));
            } else if(pianoKeyboardPreviewActive && pianoKeyboardPreviewTrackId == track.id && keyHovered &&
                      ImGui::IsMouseDown(ImGuiMouseButton_Left) && pianoKeyboardPreviewPitch != keyboardPitch) {
                pianoKeyboardPreviewPitch = keyboardPitch;
                if(!pianoKeyboardPreviewPitchBindingId.empty()) timeline.setLiveOverride(track.id, pianoKeyboardPreviewPitchBindingId, ofToString(keyboardPitch));
            }
            dl->PushClipRect(ImVec2(keyboardLeft, keyTop), ImVec2(keyboardRight, keyBottom), true);
            if(blackKey) {
                dl->AddRectFilled(ImVec2(keyboardLeft, keyTop),
                                  ImVec2(blackKeyRight, keyBottom), keyPressedHere ? IM_COL32(90, 150, 230, 255) : IM_COL32(30, 31, 34, 255));
            } else {
                if(keyPressedHere) dl->AddRectFilled(ImVec2(keyboardLeft, keyTop), ImVec2(keyboardRight, keyBottom), IM_COL32(140, 185, 240, 255));
                dl->AddLine(ImVec2(keyboardLeft, keyTop), ImVec2(keyboardRight, keyTop),
                            IM_COL32(75, 75, 75, 185));
            }
            if(noteClass == 0 && pitchHeight >= 13.0f) {
                char noteLabel[8];
                std::snprintf(noteLabel, sizeof(noteLabel), "C%d", keyboardPitch / 12 - 1);
                const float textHeight = ImGui::GetFontSize();
                const float textY = keyTop + (pitchHeight - textHeight) * 0.5f;
                dl->AddText(ImVec2(keyboardLeft + 2.0f, textY), IM_COL32(45, 45, 45, 230), noteLabel);
            }
            dl->PopClipRect();
        }
        dl->AddRect(ImVec2(keyboardLeft, rollTop), ImVec2(keyboardRight, rollBottom),
                    IM_COL32(track.color.r, track.color.g, track.color.b, 210));
        if(pianoKeyboardPreviewActive && pianoKeyboardPreviewTrackId == track.id && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if(!pianoKeyboardPreviewGateBindingId.empty()) timeline.clearLiveOverride(track.id, pianoKeyboardPreviewGateBindingId);
            if(!pianoKeyboardPreviewPitchBindingId.empty()) timeline.clearLiveOverride(track.id, pianoKeyboardPreviewPitchBindingId);
            pianoKeyboardPreviewActive = false;
            pianoKeyboardPreviewTrackId.clear();
            pianoKeyboardPreviewGateBindingId.clear();
            pianoKeyboardPreviewPitchBindingId.clear();
            pianoKeyboardPreviewPitch = -1;
        }

        // Vertical scrollbar: a thumb sized to the fraction of the full
        // 0-127 pitch range currently visible, positioned so pitch 127 is at
        // the top and pitch 0 at the bottom (matching the keyboard above).
        // Dragging pans without changing zoom; clicking the track jumps the
        // window there -- the same conventions as the horizontal timeline
        // scrollbar drawn by hand below the whole viewport.
        if(isFocused) {
            const int scrollRangeSize = highPitch - lowPitch + 1;
            const int maxLowPitch = std::max(0, 128 - scrollRangeSize);
            // The zoom buttons below occupy a strip at the top and bottom of
            // the scrollbar's own column; the draggable track fills what's
            // left between them.
            const float pianoTrackTop = std::min(rollBottom, rollTop + kPianoZoomButtonHeight + 2.0f);
            const float pianoTrackBottom = std::max(pianoTrackTop, rollBottom - kPianoZoomButtonHeight - 2.0f);
            const float scrollTrackHeight = std::max(1.0f, pianoTrackBottom - pianoTrackTop);
            const float pianoThumbHeight = std::min(scrollTrackHeight,
                std::max(16.0f, scrollTrackHeight * static_cast<float>(scrollRangeSize) / 128.0f));
            const float pianoThumbTravel = std::max(0.0f, scrollTrackHeight - pianoThumbHeight);
            const float pianoLowFraction = maxLowPitch > 0 ? static_cast<float>(lowPitch) / static_cast<float>(maxLowPitch) : 0.0f;
            const float pianoThumbTop = pianoTrackTop + pianoThumbTravel * (1.0f - pianoLowFraction);
            const ImVec2 pianoScrollThumbMin(pianoScrollbarLeft, pianoThumbTop);
            const ImVec2 pianoScrollThumbMax(pianoScrollbarRight, pianoThumbTop + pianoThumbHeight);

            dl->AddRectFilled(ImVec2(pianoScrollbarLeft, pianoTrackTop), ImVec2(pianoScrollbarRight, pianoTrackBottom),
                              IM_COL32(20, 20, 20, 255));

            ImGui::SetCursorScreenPos(ImVec2(pianoScrollbarLeft, pianoTrackTop));
            ImGui::InvisibleButton(("##pianoVScrollbar" + lane->id).c_str(),
                                   ImVec2(std::max(1.0f, pianoScrollbarRight - pianoScrollbarLeft), scrollTrackHeight));
            const bool pianoThumbHovered = ImGui::IsItemHovered();
            const bool pianoThumbActive = ImGui::IsItemActive();
            if(maxLowPitch > 0) {
                if(ImGui::IsItemClicked() && !ImGui::IsMouseHoveringRect(pianoScrollThumbMin, pianoScrollThumbMax)) {
                    const float targetThumbTop = ofClamp(ImGui::GetIO().MousePos.y - pianoThumbHeight * 0.5f,
                                                         pianoTrackTop, pianoTrackTop + pianoThumbTravel);
                    const float targetFraction = 1.0f - (targetThumbTop - pianoTrackTop) / std::max(1.0f, pianoThumbTravel);
                    lane->pianoLowPitch = ofClamp(static_cast<int>(std::lround(targetFraction * maxLowPitch)), 0, maxLowPitch);
                    lane->pianoHighPitch = lane->pianoLowPitch + scrollRangeSize - 1;
                }
                if(pianoThumbActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    const float deltaFraction = -ImGui::GetIO().MouseDelta.y / std::max(1.0f, pianoThumbTravel);
                    const int newLow = ofClamp(lane->pianoLowPitch + static_cast<int>(std::lround(deltaFraction * maxLowPitch)),
                                               0, maxLowPitch);
                    lane->pianoLowPitch = newLow;
                    lane->pianoHighPitch = newLow + scrollRangeSize - 1;
                }
            }
            dl->AddRectFilled(pianoScrollThumbMin, pianoScrollThumbMax,
                              pianoThumbActive ? IM_COL32(150, 150, 160, 255)
                                                : pianoThumbHovered ? IM_COL32(120, 120, 130, 255) : IM_COL32(90, 90, 100, 255), 3.0f);

            // "+"/"-" buttons flanking the scrollbar zoom the visible pitch
            // range (i.e. change note height) around its current center.
            // This replaces an earlier Ctrl+wheel zoom gesture: the piano
            // roll canvas is drawn inside the same ImGui window as the rest
            // of the timeline, so any wheel motion over it also triggered
            // the timeline's own horizontal zoom regardless of modifier
            // keys, and the two clashed instead of resizing the notes.
            const float pianoZoomButtonWidth = pianoScrollbarRight - pianoScrollbarLeft;
            const int pianoZoomCenterPitch = (lowPitch + highPitch) / 2;
            const auto applyPianoZoom = [&](float factor) {
                const int newRangeSize = std::clamp(static_cast<int>(std::lround(scrollRangeSize * factor)), 1, 128);
                int newHigh = pianoZoomCenterPitch + newRangeSize / 2;
                newHigh = ofClamp(newHigh, newRangeSize - 1, 127);
                lane->pianoLowPitch = newHigh - newRangeSize + 1;
                lane->pianoHighPitch = newHigh;
            };
            // Drawn by hand (InvisibleButton + manual fill/text) rather
            // than ImGui::Button, matching every other absolutely-positioned
            // control on this canvas -- a themed Button here came out
            // invisible: it's added to the same window draw list as the
            // hand-drawn scrollbar, and at this 10x14 size ImGui's own
            // frame padding could make it decide there was nothing worth
            // submitting.
            const auto drawPianoZoomButton = [&](const std::string& idSuffix, const ImVec2& buttonMin, const char* glyph, float factor) {
                const ImVec2 buttonMax(buttonMin.x + pianoZoomButtonWidth, buttonMin.y + kPianoZoomButtonHeight);
                ImGui::SetCursorScreenPos(buttonMin);
                ImGui::InvisibleButton(("##pianoZoom" + idSuffix + lane->id).c_str(), ImVec2(pianoZoomButtonWidth, kPianoZoomButtonHeight));
                const bool zoomHovered = ImGui::IsItemHovered();
                const bool zoomActive = ImGui::IsItemActive();
                dl->AddRectFilled(buttonMin, buttonMax,
                                  zoomActive ? IM_COL32(150, 150, 160, 255) : zoomHovered ? IM_COL32(110, 110, 120, 255) : IM_COL32(60, 60, 66, 255), 2.0f);
                const ImVec2 textSize = ImGui::CalcTextSize(glyph);
                dl->AddText(ImVec2(buttonMin.x + (pianoZoomButtonWidth - textSize.x) * 0.5f,
                                   buttonMin.y + (kPianoZoomButtonHeight - textSize.y) * 0.5f),
                           IM_COL32(230, 230, 235, 255), glyph);
                if(ImGui::IsItemClicked()) applyPianoZoom(factor);
            };
            drawPianoZoomButton("In", ImVec2(pianoScrollbarLeft, rollTop), "+", 1.0f / 1.2f);
            drawPianoZoomButton("Out", ImVec2(pianoScrollbarLeft, rollBottom - kPianoZoomButtonHeight), "-", 1.2f);
        }

        dl->AddRectFilled(ImVec2(left, velocityTop), ImVec2(right, velocityBottom), IM_COL32(28, 28, 31, 255));
        dl->AddRectFilled(ImVec2(left, probabilityTop), ImVec2(right, probabilityBottom), IM_COL32(24, 24, 27, 255));
        dl->AddText(ImVec2(left + 4.0f, velocityTop + 5.0f), IM_COL32(145, 145, 150, 210), "VEL");
        dl->AddText(ImVec2(left + 4.0f, probabilityTop + 5.0f), IM_COL32(145, 145, 150, 210), "PROB");
        for(int pitch = lowPitch; pitch <= highPitch; ++pitch) {
            const float y = rollBottom - (pitch - lowPitch + 1) * pitchHeight;
            const int noteClass = pitch % 12;
            const bool blackKey = noteClass == 1 || noteClass == 3 || noteClass == 6 || noteClass == 8 || noteClass == 10;
            if(blackKey) dl->AddRectFilled(ImVec2(left, y), ImVec2(right, y + pitchHeight), IM_COL32(255, 255, 255, 8));
            // `y` is the top edge of this pitch's row, i.e. the boundary with
            // the row above it. The octave separator belongs below C (the
            // B/C boundary), which is the top edge of B's own row (noteClass
            // 11) -- not the top edge of C's row (that would be the C/C#
            // boundary, one semitone too high).
            dl->AddLine(ImVec2(left, y), ImVec2(right, y), noteClass == 11 ? IM_COL32(125, 125, 125, 165) : IM_COL32(65, 65, 65, 100));
        }
        const double grid = std::max(1.0 / kPPQ, lane->beatsPerStep);
        const double content = sourceDuration(*clip);
        const int cycles = clip->repeatContent ? std::max(1, static_cast<int>(std::ceil(clip->durationBeats / cycleDuration(*clip)))) : 1;
        const int gridCount = static_cast<int>(std::ceil(content / grid));
        for(int cycle = 0; cycle < cycles; ++cycle) {
            for(int i = 0; i <= gridCount; ++i) {
                const double source = std::min(content, i * grid);
                const float x = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, source, cycle));
                if(x < left || x > right) continue;
                const bool contentBoundary = clip->repeatContent && i == 0 && cycle > 0;
                dl->AddLine(ImVec2(x, rollTop), ImVec2(x, probabilityBottom), contentBoundary ? IM_COL32(track.color.r, track.color.g, track.color.b, 225) : kGrid, contentBoundary ? 2.0f : 1.0f);
            }
        }

        dl->PushClipRect(ImVec2(left, rollTop), ImVec2(right, probabilityBottom), true);
        for(int cycle = 0; cycle < cycles; ++cycle) {
            for(size_t noteIndex = 0; noteIndex < lane->pianoNotes.size(); ++noteIndex) {
                const auto& note = lane->pianoNotes[noteIndex];
                if(note.pitch < lowPitch || note.pitch > highPitch) continue;
                const float x1 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, note.startBeat, cycle));
                const float x2 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, note.startBeat + note.durationBeats, cycle));
                const float y2 = rollBottom - (note.pitch - lowPitch) * pitchHeight;
                const float y1 = y2 - pitchHeight + 1.0f;
                const bool selected = isFocused && pianoSelectedNoteIndices.count(static_cast<int>(noteIndex)) > 0;
                dl->AddRectFilled(ImVec2(x1 + 1.0f, y1), ImVec2(x2 - 1.0f, y2),
                                  selected ? IM_COL32(250, 210, 90, 245) : IM_COL32(track.color.r, track.color.g, track.color.b, cycle == 0 ? 225 : 135), 2.0f);
                dl->AddLine(ImVec2(x2 - 3.0f, y1 + 1.0f), ImVec2(x2 - 3.0f, y2 - 1.0f), IM_COL32(255, 255, 255, 180), 1.0f);
                const float handleX = x1 + 3.0f;
                const float velocityY = velocityBottom - ofClamp(note.velocity, 0.0f, 1.0f) * (velocityBottom - velocityTop - 3.0f);
                const float probabilityY = probabilityBottom - ofClamp(note.probability, 0.0f, 1.0f) * (probabilityBottom - probabilityTop - 3.0f);
                dl->AddLine(ImVec2(handleX, velocityBottom - 2.0f), ImVec2(handleX, velocityY), IM_COL32(track.color.r, track.color.g, track.color.b, cycle == 0 ? 235 : 125), 3.0f);
                dl->AddCircleFilled(ImVec2(handleX, velocityY), 3.5f, IM_COL32(240, 240, 245, cycle == 0 ? 245 : 135));
                dl->AddLine(ImVec2(handleX, probabilityBottom - 2.0f), ImVec2(handleX, probabilityY), IM_COL32(245, 185, 80, cycle == 0 ? 235 : 125), 3.0f);
                dl->AddCircleFilled(ImVec2(handleX, probabilityY), 3.5f, IM_COL32(250, 225, 150, cycle == 0 ? 245 : 135));
            }
        }
        dl->PopClipRect();

        if(isFocused) {
        const bool hovered = ImGui::IsMouseHoveringRect(ImVec2(left, rollTop), ImVec2(right, rollBottom));
        const bool velocityHovered = ImGui::IsMouseHoveringRect(ImVec2(left, velocityTop), ImVec2(right, velocityBottom));
        const bool probabilityHovered = ImGui::IsMouseHoveringRect(ImVec2(left, probabilityTop), ImVec2(right, probabilityBottom));
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const double timelineBeat = beatAtOffset(mouse.x - timelineMin.x);
        double sourceBeat = timelineToSourceBeat(*clip, timelineBeat);
        const int pitch = ofClamp(highPitch - static_cast<int>((mouse.y - rollTop) / pitchHeight), lowPitch, highPitch);
        const double snappedSource = lane->pianoSnapToGrid ? std::round(sourceBeat / grid) * grid : sourceBeat;
        auto hitPianoValueHandle = [&]() {
            int result = -1;
            float nearestDistance = 8.0f;
            for(int cycle = 0; cycle < cycles; ++cycle) {
                for(int i = static_cast<int>(lane->pianoNotes.size()) - 1; i >= 0; --i) {
                    const float handleX = timelineMin.x + beatOffset(
                        sourceToTimelineBeat(*clip, lane->pianoNotes[i].startBeat, cycle)) + 3.0f;
                    const float distance = std::abs(mouse.x - handleX);
                    if(distance <= nearestDistance) {
                        nearestDistance = distance;
                        result = i;
                    }
                }
            }
            return result;
        };

        if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            int hitIndex = -1;
            for(int i = static_cast<int>(lane->pianoNotes.size()) - 1; i >= 0; --i) {
                const auto& note = lane->pianoNotes[i];
                if(note.pitch == pitch && sourceBeat >= note.startBeat && sourceBeat <= note.startBeat + note.durationBeats) {
                    hitIndex = i;
                    break;
                }
            }
            if(hitIndex >= 0) {
                // Right-clicking a note that's part of a multi-selection
                // deletes the whole group; right-clicking any other note
                // (selected alone, or not selected at all) deletes just it.
                std::set<int> toDelete;
                if(pianoSelectedNoteIndices.count(hitIndex) > 0 && pianoSelectedNoteIndices.size() > 1) {
                    toDelete = pianoSelectedNoteIndices;
                } else {
                    toDelete.insert(hitIndex);
                }
                for(auto it = toDelete.rbegin(); it != toDelete.rend(); ++it) {
                    if(*it >= 0 && *it < static_cast<int>(lane->pianoNotes.size()))
                        lane->pianoNotes.erase(lane->pianoNotes.begin() + *it);
                }
                pianoSelectedNoteIndices.clear();
            }
        }
        if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            pianoDragNoteIndex = -1;
            int hitIndex = -1;
            for(int i = static_cast<int>(lane->pianoNotes.size()) - 1; i >= 0; --i) {
                const auto& note = lane->pianoNotes[i];
                if(note.pitch == pitch && sourceBeat >= note.startBeat && sourceBeat <= note.startBeat + note.durationBeats) {
                    hitIndex = i;
                    break;
                }
            }
            const bool ctrlDown = ImGui::GetIO().KeyCtrl;
            if(hitIndex >= 0) {
                if(ctrlDown) {
                    // Ctrl+click only toggles membership -- it never starts a
                    // drag, so a whole selection can be built up click by
                    // click before moving any of it.
                    if(pianoSelectedNoteIndices.count(hitIndex) > 0) pianoSelectedNoteIndices.erase(hitIndex);
                    else pianoSelectedNoteIndices.insert(hitIndex);
                } else {
                    if(pianoSelectedNoteIndices.count(hitIndex) == 0) {
                        // Clicking a note outside the current selection
                        // replaces it, matching the old single-note behaviour.
                        pianoSelectedNoteIndices.clear();
                        pianoSelectedNoteIndices.insert(hitIndex);
                    }
                    // Otherwise the click landed on a note that's already
                    // part of the selection -- keep the whole group selected
                    // and drag it together.
                    const auto& note = lane->pianoNotes[hitIndex];
                    pianoDragNoteIndex = hitIndex;
                    const double edgeThreshold = std::max(grid * 0.2,
                        std::abs(beatAtOffset(mouse.x - timelineMin.x + 6.0f) - timelineBeat));
                    pianoDragMode = std::abs(sourceBeat - note.startBeat - note.durationBeats) <= edgeThreshold
                        ? PianoDragMode::Resize : PianoDragMode::Move;
                    pianoDragBeatOffset = sourceBeat - note.startBeat;
                    if(pianoDragMode == PianoDragMode::Move) {
                        pianoDragSnapshot.clear();
                        for(int index : pianoSelectedNoteIndices) {
                            if(index >= 0 && index < static_cast<int>(lane->pianoNotes.size()))
                                pianoDragSnapshot.push_back({index, lane->pianoNotes[index].startBeat, lane->pianoNotes[index].pitch});
                        }
                        pianoDragAnchorStartBeat = note.startBeat;
                        pianoDragAnchorPitch = note.pitch;
                        if(ImGui::GetIO().KeyAlt) {
                            // Alt+drag duplicates the whole selection in
                            // place and drags the copies instead, leaving
                            // the originals untouched where they were.
                            // "note" and hitIndex must not be dereferenced
                            // into lane->pianoNotes again after this point --
                            // push_back below can reallocate the vector.
                            std::unordered_map<int, int> originalToDuplicate;
                            for(int index : pianoSelectedNoteIndices) {
                                if(index < 0 || index >= static_cast<int>(lane->pianoNotes.size())) continue;
                                const auto copy = lane->pianoNotes[index];
                                lane->pianoNotes.push_back(copy);
                                originalToDuplicate[index] = static_cast<int>(lane->pianoNotes.size()) - 1;
                            }
                            pianoSelectedNoteIndices.clear();
                            for(const auto& [originalIndex, duplicateIndex] : originalToDuplicate)
                                pianoSelectedNoteIndices.insert(duplicateIndex);
                            const auto foundDuplicate = originalToDuplicate.find(hitIndex);
                            if(foundDuplicate != originalToDuplicate.end()) pianoDragNoteIndex = foundDuplicate->second;
                            pianoDragSnapshot.clear();
                            for(int index : pianoSelectedNoteIndices) {
                                if(index >= 0 && index < static_cast<int>(lane->pianoNotes.size()))
                                    pianoDragSnapshot.push_back({index, lane->pianoNotes[index].startBeat, lane->pianoNotes[index].pitch});
                            }
                        }
                    }
                }
            } else if(ctrlDown) {
                // Ctrl+drag on empty space marquee-selects instead of
                // drawing a new note. Without Shift the previous selection
                // is replaced once the drag resolves on release.
                pianoMarqueeActive = true;
                pianoMarqueeStartX = mouse.x;
                pianoMarqueeStartY = mouse.y;
                if(!ImGui::GetIO().KeyShift) pianoSelectedNoteIndices.clear();
            } else {
                pianoSelectedNoteIndices.clear();
                const double duration = grid;
                if(lane->pianoMonophonic) {
                    lane->pianoNotes.erase(std::remove_if(lane->pianoNotes.begin(), lane->pianoNotes.end(), [&](const auto& note) {
                        return note.startBeat < snappedSource + duration && note.startBeat + note.durationBeats > snappedSource;
                    }), lane->pianoNotes.end());
                }
                lane->pianoNotes.push_back({std::max(0.0, snappedSource), duration, pitch, lane->pianoDefaultVelocity, 1.0f});
                pianoDragNoteIndex = static_cast<int>(lane->pianoNotes.size()) - 1;
                pianoSelectedNoteIndices.insert(pianoDragNoteIndex);
                pianoDragMode = PianoDragMode::Resize;
            }
        }
        if(pianoMarqueeActive) {
            if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                const ImVec2 marqueeMin(std::min(pianoMarqueeStartX, mouse.x), std::min(pianoMarqueeStartY, mouse.y));
                const ImVec2 marqueeMax(std::max(pianoMarqueeStartX, mouse.x), std::max(pianoMarqueeStartY, mouse.y));
                dl->PushClipRect(ImVec2(left, rollTop), ImVec2(right, rollBottom), true);
                dl->AddRectFilled(marqueeMin, marqueeMax, IM_COL32(250, 210, 90, 40));
                dl->AddRect(marqueeMin, marqueeMax, IM_COL32(250, 210, 90, 200));
                dl->PopClipRect();
            } else {
                const float marqueeMinX = std::min(pianoMarqueeStartX, mouse.x);
                const float marqueeMaxX = std::max(pianoMarqueeStartX, mouse.x);
                const float marqueeMinY = std::min(pianoMarqueeStartY, mouse.y);
                const float marqueeMaxY = std::max(pianoMarqueeStartY, mouse.y);
                for(int cycle = 0; cycle < cycles; ++cycle) {
                    for(size_t noteIndex = 0; noteIndex < lane->pianoNotes.size(); ++noteIndex) {
                        const auto& note = lane->pianoNotes[noteIndex];
                        if(note.pitch < lowPitch || note.pitch > highPitch) continue;
                        const float x1 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, note.startBeat, cycle));
                        const float x2 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, note.startBeat + note.durationBeats, cycle));
                        const float y2 = rollBottom - (note.pitch - lowPitch) * pitchHeight;
                        const float y1 = y2 - pitchHeight + 1.0f;
                        if(x2 >= marqueeMinX && x1 <= marqueeMaxX && y2 >= marqueeMinY && y1 <= marqueeMaxY) {
                            pianoSelectedNoteIndices.insert(static_cast<int>(noteIndex));
                        }
                    }
                }
                pianoMarqueeActive = false;
            }
        }
        if(hovered && !pianoSelectedNoteIndices.empty() && !ImGui::IsAnyItemActive() &&
           (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
            std::vector<int> sortedSelection(pianoSelectedNoteIndices.begin(), pianoSelectedNoteIndices.end());
            std::sort(sortedSelection.rbegin(), sortedSelection.rend());
            for(int index : sortedSelection) {
                if(index >= 0 && index < static_cast<int>(lane->pianoNotes.size()))
                    lane->pianoNotes.erase(lane->pianoNotes.begin() + index);
            }
            pianoSelectedNoteIndices.clear();
        }
        if(velocityHovered || probabilityHovered) {
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                pianoNumericNoteIndex = hitPianoValueHandle();
                if(pianoNumericNoteIndex >= 0) {
                    pianoNumericField = velocityHovered
                        ? PianoNumericField::Velocity : PianoNumericField::Probability;
                    const auto& note = lane->pianoNotes[pianoNumericNoteIndex];
                    pianoNumericValue = pianoNumericField == PianoNumericField::Velocity
                        ? note.velocity : note.probability;
                    ImGui::OpenPopup("Piano note value");
                }
            } else if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                pianoDragNoteIndex = hitPianoValueHandle();
                if(pianoDragNoteIndex >= 0) {
                    pianoDragMode = velocityHovered ? PianoDragMode::Velocity : PianoDragMode::Probability;
                    if(pianoSelectedNoteIndices.count(pianoDragNoteIndex) == 0) {
                        pianoSelectedNoteIndices.clear();
                        pianoSelectedNoteIndices.insert(pianoDragNoteIndex);
                    }
                    const auto& anchorNote = lane->pianoNotes[pianoDragNoteIndex];
                    pianoValueDragAnchorValue = pianoDragMode == PianoDragMode::Velocity
                        ? anchorNote.velocity : anchorNote.probability;
                    pianoValueDragSnapshot.clear();
                    for(int index : pianoSelectedNoteIndices) {
                        if(index < 0 || index >= static_cast<int>(lane->pianoNotes.size())) continue;
                        const auto& selectedNote = lane->pianoNotes[index];
                        pianoValueDragSnapshot.push_back({index,
                            pianoDragMode == PianoDragMode::Velocity
                                ? selectedNote.velocity : selectedNote.probability});
                    }
                }
            }
        }
        if(pianoDragNoteIndex >= 0 && pianoDragNoteIndex < static_cast<int>(lane->pianoNotes.size()) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            auto& note = lane->pianoNotes[pianoDragNoteIndex];
            if(pianoDragMode == PianoDragMode::Resize) {
                const double end = lane->pianoSnapToGrid ? std::round(sourceBeat / grid) * grid : sourceBeat;
                note.durationBeats = std::max(grid, end - note.startBeat);
            } else if(pianoDragMode == PianoDragMode::Move) {
                // Move every selected note by the same delta from where it
                // started the drag, rather than snapping each one straight
                // to the mouse -- that's what keeps a multi-note selection's
                // shape intact while dragging it.
                const double newAnchorStart = sourceBeat - pianoDragBeatOffset;
                const double snappedAnchorStart = std::max(0.0, lane->pianoSnapToGrid
                    ? std::round(newAnchorStart / grid) * grid : newAnchorStart);
                const double deltaBeat = snappedAnchorStart - pianoDragAnchorStartBeat;
                const int deltaPitch = pitch - pianoDragAnchorPitch;
                for(const auto& entry : pianoDragSnapshot) {
                    if(entry.index < 0 || entry.index >= static_cast<int>(lane->pianoNotes.size())) continue;
                    auto& dragged = lane->pianoNotes[entry.index];
                    dragged.startBeat = std::max(0.0, entry.startBeat + deltaBeat);
                    dragged.pitch = ofClamp(entry.pitch + deltaPitch, lowPitch, highPitch);
                }
            } else if(pianoDragMode == PianoDragMode::Velocity) {
                const float targetValue = ofClamp((velocityBottom - mouse.y) /
                    std::max(1.0f, velocityBottom - velocityTop - 3.0f), 0.0f, 1.0f);
                const float delta = targetValue - pianoValueDragAnchorValue;
                for(const auto& entry : pianoValueDragSnapshot) {
                    if(entry.index < 0 || entry.index >= static_cast<int>(lane->pianoNotes.size())) continue;
                    lane->pianoNotes[entry.index].velocity = ofClamp(entry.value + delta, 0.0f, 1.0f);
                }
            } else if(pianoDragMode == PianoDragMode::Probability) {
                const float targetValue = ofClamp((probabilityBottom - mouse.y) /
                    std::max(1.0f, probabilityBottom - probabilityTop - 3.0f), 0.0f, 1.0f);
                const float delta = targetValue - pianoValueDragAnchorValue;
                for(const auto& entry : pianoValueDragSnapshot) {
                    if(entry.index < 0 || entry.index >= static_cast<int>(lane->pianoNotes.size())) continue;
                    lane->pianoNotes[entry.index].probability = ofClamp(entry.value + delta, 0.0f, 1.0f);
                }
            }
        }
        if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            pianoDragMode = PianoDragMode::None;
            pianoDragNoteIndex = -1;
            pianoDragSnapshot.clear();
            pianoValueDragSnapshot.clear();
        }
        if(ImGui::BeginPopup("Piano note value")) {
            if(pianoNumericNoteIndex >= 0 &&
               pianoNumericNoteIndex < static_cast<int>(lane->pianoNotes.size()) &&
               pianoNumericField != PianoNumericField::None) {
                ImGui::SetNextItemWidth(110.0f);
                const char* label = pianoNumericField == PianoNumericField::Velocity
                    ? "Velocity" : "Probability";
                if(ImGui::InputFloat(label, &pianoNumericValue, 0.01f, 0.1f, "%.3f")) {
                    pianoNumericValue = ofClamp(pianoNumericValue, 0.0f, 1.0f);
                    auto& note = lane->pianoNotes[pianoNumericNoteIndex];
                    if(pianoNumericField == PianoNumericField::Velocity) note.velocity = pianoNumericValue;
                    else note.probability = pianoNumericValue;
                }
            }
            ImGui::EndPopup();
        }
        } // isFocused (piano roll interaction)
        const float playheadX = timelineMin.x + beatOffset(beatPosition);
        if(playheadX >= zoneLeft && playheadX <= editorMax.x) dl->AddLine(ImVec2(playheadX, editorMin.y), ImVec2(playheadX, editorMax.y), kPlayhead, 2.0f);
        dl->PopClipRect();
        finishAbsoluteLayout(ImVec2(editorMin.x, editorMax.y));
        continue;
    }

    if(lane->type == ofxOceanodeTimelineLaneType::Curve) {
        const auto interpolation = curveInterpolationMode(lane->curveInterpolation);
        const float curveTop = editorMin.y + 8.0f, curveBottom = editorMax.y - 8.0f;
        for(int row = 0; row <= 4; ++row) {
            const float y = curveTop + (curveBottom - curveTop) * row / 4.0f;
            dl->AddLine(ImVec2(timelineMin.x, y), ImVec2(editorMax.x, y), IM_COL32(75, 75, 75, 120));
        }
        const double curveGrid = displayGridBeats();
        const int curveGridLines = static_cast<int>(std::ceil(endBeat / curveGrid));
        for(int gridIndex = 0; gridIndex <= curveGridLines; ++gridIndex) {
            const double beat = gridIndex * curveGrid;
            const float x = timelineMin.x + beatOffset(beat);
            const bool bar = std::fmod(beat, timeline.getBeatsPerBar()) < 0.001;
            if(bar || beatOffset(beat + curveGrid) - beatOffset(beat) >= 4.0f)
                dl->AddLine(ImVec2(x, curveTop), ImVec2(x, curveBottom), bar ? kBar : kGrid, bar ? 1.5f : 1.0f);
        }
        std::sort(lane->curvePoints.begin(), lane->curvePoints.end(), [](const auto& a, const auto& b) { return a.beat < b.beat; });
        lane->curveTensions.resize(lane->curvePoints.empty() ? 0 : lane->curvePoints.size() - 1);
        const auto& points = lane->curvePoints;
        const double content = sourceDuration(*clip);
        const double repeatCycleDuration = cycleDuration(*clip);
        const int cycles = clip->repeatContent ? std::max(1, static_cast<int>(std::ceil(clip->durationBeats / repeatCycleDuration))) : 1;
        dl->PushClipRect(ImVec2(left, curveTop), ImVec2(right, curveBottom), true);
        for(int cycle = 0; cycle < cycles; ++cycle) {
            if(cycle > 0) {
                const float markerX = timelineMin.x + beatOffset(clip->startBeat + cycle * repeatCycleDuration);
                dl->AddLine(ImVec2(markerX, curveTop), ImVec2(markerX, curveBottom), IM_COL32(track.color.r, track.color.g, track.color.b, 220), 2.0f);
            }
            for(size_t i = 1; i < points.size(); ++i) {
                const auto tension = lane->curveTensions[i - 1];
                if(interpolation == CurveInterpolationMode::Step) {
                    const float x1 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, points[i - 1].beat, cycle));
                    const float x2 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, points[i].beat, cycle));
                    const float y1 = curveBottom - points[i - 1].value * (curveBottom - curveTop);
                    const float y2 = curveBottom - points[i].value * (curveBottom - curveTop);
                    const ImU32 color = IM_COL32(track.color.r, track.color.g, track.color.b, cycle == 0 ? 240 : 130);
                    dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y1), color, 2.0f);
                    dl->AddLine(ImVec2(x2, y1), ImVec2(x2, y2), color, 2.0f);
                    continue;
                }
                const int samples = std::max(16, static_cast<int>(std::abs(
                    beatOffset(sourceToTimelineBeat(*clip, points[i].beat, cycle)) -
                    beatOffset(sourceToTimelineBeat(*clip, points[i - 1].beat, cycle))) / 4.0f));
                for(int sample = 0; sample < samples; ++sample) {
                    const float t1 = sample / static_cast<float>(samples);
                    const float t2 = (sample + 1) / static_cast<float>(samples);
                    const double source1 = points[i - 1].beat + (points[i].beat - points[i - 1].beat) * t1;
                    const double source2 = points[i - 1].beat + (points[i].beat - points[i - 1].beat) * t2;
                    const float value1 = ofLerp(points[i - 1].value, points[i].value,
                                                curveSegmentShape(t1, interpolation, tension));
                    const float value2 = ofLerp(points[i - 1].value, points[i].value,
                                                curveSegmentShape(t2, interpolation, tension));
                    const ImVec2 a(timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, source1, cycle)),
                                   curveBottom - value1 * (curveBottom - curveTop));
                    const ImVec2 b(timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, source2, cycle)),
                                   curveBottom - value2 * (curveBottom - curveTop));
                    dl->AddLine(a, b, IM_COL32(track.color.r, track.color.g, track.color.b, cycle == 0 ? 240 : 130), 2.0f);
                }
            }
            for(const auto& point : points) {
                const ImVec2 p(timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, point.beat, cycle)), curveBottom - point.value * (curveBottom - curveTop));
                dl->AddCircleFilled(p, cycle == 0 ? 4.0f : 3.0f, IM_COL32(245, 245, 245, cycle == 0 ? 230 : 115));
            }
        }
        dl->PopClipRect();
        // Require the editor canvas item itself to be hoverable, not merely
        // geometric overlap. This prevents clicks in the numeric popup from
        // passing through and creating a point underneath it.
        if(isFocused) {
        const bool curveHovered = editorCanvasHovered && !ImGui::IsPopupOpen("Curve point value") && right > left &&
            ImGui::IsMouseHoveringRect(ImVec2(left, curveTop), ImVec2(right, curveBottom));
        const ImVec2 curveMouse = ImGui::GetIO().MousePos;
        const double curveTimelineBeat = beatAtOffset(curveMouse.x - timelineMin.x);
        const double curveSourceBeat = timelineToSourceBeat(*clip, curveTimelineBeat);
        const int hoveredCycle = clip->repeatContent
            ? std::max(0, static_cast<int>(std::floor(std::max(0.0, curveTimelineBeat - clip->startBeat) / repeatCycleDuration))) : 0;
        const float curveValue = lane->curveClamp
            ? ofClamp((curveBottom - curveMouse.y) / (curveBottom - curveTop), 0.0f, 1.0f)
            : (curveBottom - curveMouse.y) / (curveBottom - curveTop);
        auto hitPoint = [&]() {
            for(int i = static_cast<int>(lane->curvePoints.size()) - 1; i >= 0; --i) {
                const auto& point = lane->curvePoints[i];
                const float pointX = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, point.beat, hoveredCycle));
                const float pointY = curveBottom - point.value * (curveBottom - curveTop);
                if(std::abs(curveMouse.x - pointX) <= 8.0f && std::abs(curveMouse.y - pointY) <= 8.0f) return i;
            }
            return -1;
        };

        if(curveHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            curveValuePointIndex = hitPoint();
            if(curveValuePointIndex >= 0) {
                curveNumericValue = lane->valueMin + lane->curvePoints[curveValuePointIndex].value *
                    (lane->valueMax - lane->valueMin);
                ImGui::OpenPopup("Curve point value");
            }
        }
        if(curveHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            curveDragPointIndex = hitPoint();
            curveTensionSegment = -1;
            if(curveDragPointIndex >= 0 && ImGui::GetIO().KeyShift) {
                eraseCurvePointWithTensions(*lane, static_cast<size_t>(curveDragPointIndex));
                curveDragPointIndex = -1;
                curveValuePointIndex = -1;
            } else if(curveDragPointIndex < 0 && ImGui::GetIO().KeyAlt &&
                      (interpolation == CurveInterpolationMode::LogExp || interpolation == CurveInterpolationMode::Sigmoid) &&
                      lane->curvePoints.size() > 1) {
                for(size_t i = 1; i < lane->curvePoints.size(); ++i) {
                    const auto& a = lane->curvePoints[i - 1];
                    const auto& b = lane->curvePoints[i];
                    if(curveSourceBeat < a.beat || curveSourceBeat > b.beat) continue;
                    const float t = static_cast<float>((curveSourceBeat - a.beat) / std::max(1e-9, b.beat - a.beat));
                    const auto tension = lane->curveTensions[i - 1];
                    const float segmentValue = ofLerp(a.value, b.value,
                        curveSegmentShape(t, interpolation, tension));
                    const float segmentY = curveBottom - segmentValue * (curveBottom - curveTop);
                    if(std::abs(curveMouse.y - segmentY) <= 12.0f) {
                        curveTensionSegment = static_cast<int>(i - 1);
                        curveTensionDragStartX = curveMouse.x;
                        curveTensionDragStartY = curveMouse.y;
                        curveTensionStartInflection = tension.inflection;
                        curveTensionStartSteepness = tension.steepness;
                    }
                    break;
                }
            }
            if(curveDragPointIndex < 0 && curveTensionSegment < 0 &&
               !ImGui::GetIO().KeyAlt && !ImGui::GetIO().KeyShift) {
                curveDragPointIndex = insertLinearCurvePoint(*lane, {
                    std::max(0.0, std::min(content, snapBeat(curveSourceBeat))), curveValue
                });
            }
        }
        // Once a point or a tension handle has been grabbed, keep updating
        // it for as long as the mouse button is held, even if the cursor
        // strays outside the curve editor's rect - a normal, fast Alt-drag
        // easily overshoots the lane's vertical bounds. Re-requiring hover
        // here made the drag silently stop applying the moment that happened.
        if((curveDragPointIndex >= 0 || curveTensionSegment >= 0) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if(curveDragPointIndex >= 0 && curveDragPointIndex < static_cast<int>(lane->curvePoints.size())) {
                const double minimumBeat = curveDragPointIndex > 0 ? lane->curvePoints[curveDragPointIndex - 1].beat + 1.0 / kPPQ : 0.0;
                const double maximumBeat = curveDragPointIndex + 1 < static_cast<int>(lane->curvePoints.size())
                    ? lane->curvePoints[curveDragPointIndex + 1].beat - 1.0 / kPPQ : content;
                lane->curvePoints[curveDragPointIndex].beat = std::max(minimumBeat,
                    std::min(std::max(minimumBeat, maximumBeat), snapBeat(curveSourceBeat)));
                lane->curvePoints[curveDragPointIndex].value = curveValue;
            } else if(curveTensionSegment >= 0 && curveTensionSegment < static_cast<int>(lane->curveTensions.size())) {
                auto& tension = lane->curveTensions[curveTensionSegment];
                if(interpolation == CurveInterpolationMode::Sigmoid) {
                    tension.inflection = ofClamp(curveTensionStartInflection +
                        (curveMouse.x - curveTensionDragStartX) / std::max(1.0f, right - left), 0.01f, 0.99f);
                } else {
                    tension.inflection = 0.5f;
                }
                const float steepnessDelta = -(curveMouse.y - curveTensionDragStartY) /
                    std::max(1.0f, (curveBottom - curveTop) / 3.0f);
                tension.steepness = ofClamp(curveTensionStartSteepness * std::exp(steepnessDelta * 0.5f), 0.1f, 10.0f);
            }
        }
        if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            curveDragPointIndex = -1;
            curveTensionSegment = -1;
        }
        if(ImGui::BeginPopup("Curve point value")) {
            if(curveValuePointIndex >= 0 && curveValuePointIndex < static_cast<int>(lane->curvePoints.size())) {
                ImGui::SetNextItemWidth(110.0f);
                if(ImGui::InputFloat("Value", &curveNumericValue, 0.001f, 0.01f, "%.6g")) {
                    const float span = lane->valueMax - lane->valueMin;
                    const float normalized = std::abs(span) < 1e-9f ? 0.0f
                        : (curveNumericValue - lane->valueMin) / span;
                    lane->curvePoints[curveValuePointIndex].value = lane->curveClamp
                        ? ofClamp(normalized, 0.0f, 1.0f) : normalized;
                }
                if(ImGui::Button("Delete point")) {
                    eraseCurvePointWithTensions(*lane, static_cast<size_t>(curveValuePointIndex));
                    curveValuePointIndex = -1;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        } // isFocused (curve interaction)
        const float playheadX = timelineMin.x + beatOffset(beatPosition);
        if(playheadX >= zoneLeft && playheadX <= editorMax.x) dl->AddLine(ImVec2(playheadX, editorMin.y), ImVec2(playheadX, editorMax.y), kPlayhead, 2.0f);
        if(isFocused && editorCanvasHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) clipEditorOpen = false;
        dl->PopClipRect();
        finishAbsoluteLayout(ImVec2(editorMin.x, editorMax.y));
        continue;
    }

    const int stepCount = std::max(1, lane->stepCount);
    const double beatsPerStep = std::max(1.0 / kPPQ, lane->beatsPerStep);
    const double patternLength = stepCount * beatsPerStep;
    const float cellTop = editorMin.y + 10.0f;
    const float cellBottom = editorMax.y - 10.0f;
    if(right > left) {
        dl->AddRectFilled(ImVec2(left, editorMin.y + 3),
                          ImVec2(right, editorMax.y - kLaneResizeHandleHeight),
                          IM_COL32(track.color.r, track.color.g, track.color.b, 50), 2);
        const double content = sourceDuration(*clip);
        const int clipCycles = clip->repeatContent
            ? std::max(1, static_cast<int>(std::ceil(clip->durationBeats / cycleDuration(*clip)))) : 1;
        const int patternCycles = std::max(1, static_cast<int>(std::ceil(content / patternLength)));
        dl->PushClipRect(ImVec2(left, cellTop), ImVec2(right, cellBottom), true);
        for(int clipCycle = 0; clipCycle < clipCycles; ++clipCycle) {
            for(int patternCycle = 0; patternCycle < patternCycles; ++patternCycle) {
                const double sourceOffset = patternCycle * patternLength;
                const double cycleSourceEnd = std::min(content, sourceOffset + patternLength);
                const float cycleX1 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, sourceOffset, clipCycle));
                const float cycleX2 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, cycleSourceEnd, clipCycle));
                const bool firstPattern = clipCycle == 0 && patternCycle == 0;
                if(!firstPattern) {
                    dl->AddRectFilled(ImVec2(cycleX1, cellTop), ImVec2(cycleX2, cellBottom),
                                      IM_COL32(255, 255, 255, patternCycle % 2 ? 10 : 18));
                    dl->AddLine(ImVec2(cycleX1, cellTop), ImVec2(cycleX1, cellBottom),
                                IM_COL32(track.color.r, track.color.g, track.color.b, 245), 2.0f);
                }
                for(int index = 0; index < stepCount; ++index) {
                    const double dataBeat = index * beatsPerStep;
                    const double sourceStart = sourceOffset + dataBeat;
                    const double sourceEnd = std::min(content, sourceStart + beatsPerStep);
                    if(sourceStart >= content || sourceEnd <= sourceStart) continue;
                    const float x1 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, sourceStart, clipCycle));
                    const float x2 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, sourceEnd, clipCycle));
                    if(x2 < left || x1 > right) continue;
                    dl->AddLine(ImVec2(x1, cellTop), ImVec2(x1, cellBottom),
                                index == 0 ? kBar : kGrid, index == 0 ? 1.5f : 1.0f);
                    const auto stepIt = std::find_if(lane->step.steps.begin(), lane->step.steps.end(), [&](const auto& step) {
                        return std::abs(step.startBeat - dataBeat) < 1.0 / kPPQ;
                    });
                    const float probability = stepIt == lane->step.steps.end() ? 0.0f : ofClamp(stepIt->probability, 0.0f, 1.0f);
                    if(probability > 0.0f) {
                        const float fillTop = cellBottom - probability * (cellBottom - cellTop);
                        dl->AddRectFilled(ImVec2(std::max(x1, left) + 1, fillTop),
                                              ImVec2(std::min(x2, right) - 1, cellBottom),
                                              IM_COL32(240, 240, 240, firstPattern ? 215 : 125), 2);
                    } else {
                        dl->AddRect(ImVec2(std::max(x1, left) + 1, cellTop),
                                    ImVec2(std::min(x2, right) - 1, cellBottom),
                                    IM_COL32(130, 130, 130, firstPattern ? 110 : 65), 1.0f);
                    }
                    if(firstPattern && x2 - x1 > 28.0f && stepIt != lane->step.steps.end()) {
                        char probabilityLabel[16];
                        std::snprintf(probabilityLabel, sizeof(probabilityLabel), "%.0f%%", probability * 100.0f);
                        dl->AddText(ImVec2(std::max(x1, left) + 3, cellTop + 3),
                                    IM_COL32(35, 35, 35, 230), probabilityLabel);
                    }
                }
            }
        }
        dl->PopClipRect();
        if(isFocused) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool inStepEditor = ImGui::IsMouseHoveringRect(ImVec2(left, cellTop), ImVec2(right, cellBottom));
        if(inStepEditor && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
            const double timelineBeat = beatAtOffset(mouse.x - timelineMin.x);
            const double sourceBeat = timelineToSourceBeat(*clip, timelineBeat);
            const double patternBeat = std::fmod(sourceBeat, patternLength);
            const int index = ofClamp(static_cast<int>(std::floor(patternBeat / beatsPerStep)), 0, stepCount - 1);
            const double cellBeat = index * beatsPerStep;
            std::string value = "1";
            auto stepIt = std::find_if(lane->step.steps.begin(), lane->step.steps.end(), [&](const auto& step) {
                return std::abs(step.startBeat - cellBeat) < 1.0 / kPPQ;
            });
            if(stepIt == lane->step.steps.end()) {
                timeline.setClipStep(editorTrackId, editorClipId, lane->id, cellBeat, value, beatsPerStep);
                stepIt = std::find_if(lane->step.steps.begin(), lane->step.steps.end(), [&](const auto& step) {
                    return std::abs(step.startBeat - cellBeat) < 1.0 / kPPQ;
                });
            }
            if(stepIt != lane->step.steps.end()) {
                stepIt->probability = ofClamp((cellBottom - mouse.y) / (cellBottom - cellTop), 0.0f, 1.0f);
            }
        }
        if(inStepEditor && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            const double timelineBeat = beatAtOffset(mouse.x - timelineMin.x);
            const double sourceBeat = timelineToSourceBeat(*clip, timelineBeat);
            const double patternBeat = std::fmod(sourceBeat, patternLength);
            const int index = ofClamp(static_cast<int>(std::floor(patternBeat / beatsPerStep)), 0, stepCount - 1);
            const double cellBeat = index * beatsPerStep;
            auto stepIt = std::find_if(lane->step.steps.begin(), lane->step.steps.end(), [&](const auto& step) {
                return std::abs(step.startBeat - cellBeat) < 1.0 / kPPQ;
            });
            std::string value = "1";
            if(stepIt == lane->step.steps.end()) {
                timeline.setClipStep(editorTrackId, editorClipId, lane->id, cellBeat, value, beatsPerStep);
                stepIt = std::find_if(lane->step.steps.begin(), lane->step.steps.end(), [&](const auto& step) {
                    return std::abs(step.startBeat - cellBeat) < 1.0 / kPPQ;
                });
            }
            if(stepIt != lane->step.steps.end()) stepIt->probability = stepIt->probability > 0.5f ? 0.0f : 1.0f;
        }
        } // isFocused (step interaction)
    }
    const float playheadX = timelineMin.x + beatOffset(beatPosition);
    if(playheadX >= zoneLeft && playheadX <= editorMax.x) dl->AddLine(ImVec2(playheadX, editorMin.y), ImVec2(playheadX, editorMax.y), kPlayhead, 2.0f);
    if(isFocused && editorCanvasHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) clipEditorOpen = false;
    dl->PopClipRect();
    finishAbsoluteLayout(ImVec2(editorMin.x, editorMax.y));
    } // end for(laneIndex : clip->lanes)
}

void ofxOceanodeTimelineController::drawRenamePopup(ofxOceanodeTimelineManager& timeline) {
    const char* popupTitle = pendingNewTrackDialog ? "New Timeline Track" : "Rename Timeline Track";
    if(!ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::InputText("Name", pendingTrackName, sizeof(pendingTrackName));
    if(pendingNewTrackDialog) {
        const char* editorTypes[] = {"Step Sequencer", "Curve", "Piano Roll"};
        ImGui::Combo("Automation type", &pendingNewTrackLaneType, editorTypes, 3);
    }
    if(ImGui::Button(pendingNewTrackDialog ? "Create" : "Save")) {
        timeline.renameTrack(pendingTrackId, pendingTrackName);
        if(pendingNewTrackDialog) {
            const auto laneType = pendingNewTrackLaneType == 1 ? ofxOceanodeTimelineLaneType::Curve
                : pendingNewTrackLaneType == 2 ? ofxOceanodeTimelineLaneType::PianoRoll
                : ofxOceanodeTimelineLaneType::Step;
            if(auto* track = timeline.getTrack(pendingTrackId)) {
                std::vector<std::string> bindingIds;
                for(const auto& binding : track->bindings) bindingIds.push_back(binding.id);
                for(const auto& bindingId : bindingIds) timeline.setLaneType(pendingTrackId, bindingId, laneType);
            }
        }
        pendingNewTrackDialog = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void ofxOceanodeTimelineController::drawClipPopup(ofxOceanodeTimelineManager& timeline) {
    if(!ImGui::BeginPopupModal("New Timeline Clip", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::InputText("Name", pendingClipName, sizeof(pendingClipName));
    if(const auto* track = timeline.getTrack(pendingTrackId)) {
        const auto* selectedBinding = pendingClipBindingId.empty()
            ? nullptr : timeline.getBinding(pendingTrackId, pendingClipBindingId);
        const std::string bindingPreview = selectedBinding == nullptr
            ? "No parameter" : compactParameterName(selectedBinding->parameterPath);
        if(ImGui::BeginCombo("Parameter", bindingPreview.c_str())) {
            for(const auto& binding : track->bindings) {
                const std::string label = compactParameterName(binding.parameterPath);
                if(ImGui::Selectable(label.c_str(), binding.id == pendingClipBindingId)) {
                    pendingClipBindingId = binding.id;
                    pendingClipLaneType = binding.laneType == ofxOceanodeTimelineLaneType::Curve ? 1
                        : binding.laneType == ofxOceanodeTimelineLaneType::PianoRoll ? 2 : 0;
                }
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", binding.parameterPath.c_str());
            }
            ImGui::EndCombo();
        }
    }
    const char* editorTypes[] = {"Step Sequencer", "Curve", "Piano Roll"};
    ImGui::Combo("Automation type", &pendingClipLaneType, editorTypes, 3);
    ImGui::InputDouble("Start beat", &pendingStartBeat, editIncrement(), 1.0, "%.4f");
    ImGui::InputDouble("Duration", &pendingDurationBeats, editIncrement(), 1.0, "%.4f");
    if(ImGui::Button("Create Clip")) {
        const auto clipId = timeline.createClip(pendingTrackId, pendingClipName[0] == '\0' ? "Clip" : pendingClipName,
                                                snapBeat(pendingStartBeat), std::max(1.0 / kPPQ, snapBeat(pendingDurationBeats)));
        if(const auto* binding = timeline.getBinding(pendingTrackId, pendingClipBindingId)) {
            const auto laneType = pendingClipLaneType == 1 ? ofxOceanodeTimelineLaneType::Curve
                : pendingClipLaneType == 2 ? ofxOceanodeTimelineLaneType::PianoRoll
                : ofxOceanodeTimelineLaneType::Step;
            const auto laneId = timeline.createLane(pendingTrackId, clipId, binding->parameterPath, laneType);
            if(!laneId.empty()) {
                timeline.addBindingToLane(pendingTrackId, clipId, laneId, binding->id);
                if(auto* lane = timeline.getLane(pendingTrackId, clipId, laneId)) {
                    if(laneType != ofxOceanodeTimelineLaneType::PianoRoll) {
                        if(auto* parameter = container->findCustomGuiParameter(binding->parameterPath)) {
                            if(binding->valueType == typeid(float).name()) {
                                lane->valueMin = parameter->cast<float>().getParameter().getMin();
                                lane->valueMax = parameter->cast<float>().getParameter().getMax();
                            } else if(binding->valueType == typeid(int).name()) {
                                lane->valueMin = static_cast<float>(parameter->cast<int>().getParameter().getMin());
                                lane->valueMax = static_cast<float>(parameter->cast<int>().getParameter().getMax());
                            }
                        }
                    }
                }
            }
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
