#include "ofxOceanodeTimelineController.h"

#include "ofxOceanodeContainer.h"
#include "ofxOceanodeParameter.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeNodeModel.h"
#include "Timeline/ofxOceanodeTimeline.h"
#ifdef OFXOCEANODE_USE_MIDI
#include "Managers/ofxOceanodeMidiClock.h"
#endif
#include "ofxOceanodeTransport.h"
#include "imgui.h"

#include <algorithm>
#include <limits>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <utility>

namespace {
constexpr double kPPQ = 24.0;
// Keep long timelines practical: at the minimum zoom, one hour occupies
// about 900 px while still leaving enough scale for beat-based editing.
constexpr float kMinPixelsPerSecond = 0.25f;
constexpr float kMaxPixelsPerSecond = 600.0f;
constexpr float kSnapMinimumSpacingPixels = 5.0f;
constexpr float kLabelWidth = 230.0f;
constexpr float kRulerHeight = 64.0f;
constexpr float kHeaderHeight = 25.0f;
constexpr float kRowHeight = 28.0f;
constexpr float kCollapsedHeight = 38.0f;
constexpr float kWaveTrackMinHeight = 48.0f;
constexpr float kWaveTrackMaxHeight = 360.0f;
constexpr float kWaveTrackResizeHandleHeight = 8.0f;
constexpr float kEdgePixels = 8.0f;
// Parameter rows are indented under their track header by half the clip
// editor's indent (20 px), so the hierarchy reads track > row > editor.
constexpr float kParameterRowIndent = 10.0f;
constexpr float kPianoKeyboardWidth = 38.0f;
constexpr float kPianoScrollbarWidth = 10.0f;
constexpr float kPianoZoomButtonHeight = 14.0f;
constexpr float kLaneResizeHandleHeight = 10.0f;
constexpr float kLaneEditorMinHeight = 90.0f;
constexpr float kLaneEditorMaxHeight = 640.0f;

using ofxOceanodeTimelineCurve::CurveInterpolationMode;
using ofxOceanodeTimelineCurve::curveInterpolationMode;
using ofxOceanodeTimelineCurve::curveSegmentShape;
using ofxOceanodeTimelineCurve::valueAtBeat;
using ofxOceanodeTimelineClipTime::cycleDuration;
using ofxOceanodeTimelineClipTime::sourceDuration;
using ofxOceanodeTimelineClipTime::sourceToTimelineBeat;
using ofxOceanodeTimelineClipTime::stretch;
using ofxOceanodeTimelineClipTime::timelineToSourceBeat;

constexpr const char* kCurveInterpolationNames[] = {
    "Step", "Linear", "Log / Exp", "Sigmoid"
};
// LFO lanes add "Value": one constant for the whole lane, set numerically.
constexpr const char* kLfoInterpolationNames[] = {
    "Value", "Step", "Linear", "Log / Exp", "Sigmoid"
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

double nextWiderSnapDivision(double beats, double beatsPerBar) {
    // Keep the musical widening predictable: 16th -> 8th -> quarter ->
    // half -> bar, then double the number of bars each time.
    if(beats < 1.0) return beats * 2.0;
    if(beats < 2.0) return 2.0;
    if(beatsPerBar > beats) return beatsPerBar;
    return beats * 2.0;
}

double adaptiveSnapDivision(double baseBeats, double beatsPerBar,
                            float bpm, float pixelsPerSecond) {
    if(baseBeats <= 0.0) return baseBeats;

    const double pixelsPerBeat = 60.0 / std::max(1.0f, bpm) *
                                 std::max(kMinPixelsPerSecond, pixelsPerSecond);
    double beats = baseBeats;
    while(beats * pixelsPerBeat < kSnapMinimumSpacingPixels) {
        const double wider = nextWiderSnapDivision(beats, std::max(1.0, beatsPerBar));
        if(wider <= beats) break;
        beats = wider;
    }
    return beats;
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

// A rectangle test that respects what is on top: ImGui::IsMouseHoveringRect
// alone ignores popups and overlapping windows, so a click on a menu item
// sitting over a clip also started a drag on that clip.
bool hoverRect(const ImVec2& min, const ImVec2& max) {
    return ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
           ImGui::IsMouseHoveringRect(min, max);
}

const ofxOceanodeTimelineLane* laneForBinding(const ofxOceanodeTimelineClip& clip, const std::string& bindingId) {
    if(clip.isLfo && clip.lfoOutputBindingId == bindingId)
        return clip.lanes.empty() ? nullptr : &clip.lanes.front();
    for(const auto& lane : clip.lanes) {
        if(std::find(lane.bindingIds.begin(), lane.bindingIds.end(), bindingId) != lane.bindingIds.end()) return &lane;
    }
    return nullptr;
}

const char* laneTypeName(ofxOceanodeTimelineLaneType type) {
    switch(type) {
        case ofxOceanodeTimelineLaneType::PianoRoll: return "Piano Roll";
        case ofxOceanodeTimelineLaneType::Curve: return "Curve";
        case ofxOceanodeTimelineLaneType::MultiValue: return "Multi Value";
        case ofxOceanodeTimelineLaneType::MultiSlider: return "Multi Slider";
        case ofxOceanodeTimelineLaneType::MultiGate: return "Multi Gate";
        case ofxOceanodeTimelineLaneType::Wave: return "Wave";
        default: return "Step Sequencer";
    }
}

// Every type the user can pick from a combo or menu in this file, with the
// little index ints those sites use mapped to/from it in one place instead
// of a hand-maintained ternary chain per site.
//
// Two counts, one table. Wave is absent from both: it is a track type whose
// clips carry audio files, and a Wave *lane* no longer exists at all. LFO is only
// reachable through kClipCreationOptionCount, i.e. only where a whole clip
// is being created -- an LFO is a self-contained modulator clip with its own
// oscillator-control lanes, so it is a kind of clip to create (alongside a
// Curve or Step Sequencer one, which is how it reads to the user) rather
// than a type an individual lane can be switched to. Selectors that change
// or add a lane pass kLaneTypeOptionCount and so never offer it.
constexpr const char* kLaneTypeOptions[] = {
    "Step Sequencer", "Curve", "Piano Roll", "Multi Value", "Multi Slider", "Multi Gate", "LFO"
};
constexpr int kClipCreationOptionCount = static_cast<int>(sizeof(kLaneTypeOptions) / sizeof(kLaneTypeOptions[0]));
constexpr int kLaneTypeOptionCount = kClipCreationOptionCount - 1;
constexpr int kLfoClipOptionIndex = kClipCreationOptionCount - 1;

ofxOceanodeTimelineLaneType laneTypeFromOptionIndex(int index) {
    switch(index) {
        case 1: return ofxOceanodeTimelineLaneType::Curve;
        case 2: return ofxOceanodeTimelineLaneType::PianoRoll;
        case 3: return ofxOceanodeTimelineLaneType::MultiValue;
        case 4: return ofxOceanodeTimelineLaneType::MultiSlider;
        case 5: return ofxOceanodeTimelineLaneType::MultiGate;
        default: return ofxOceanodeTimelineLaneType::Step;
    }
}

int optionIndexForLaneType(ofxOceanodeTimelineLaneType type) {
    switch(type) {
        case ofxOceanodeTimelineLaneType::Curve: return 1;
        case ofxOceanodeTimelineLaneType::PianoRoll: return 2;
        case ofxOceanodeTimelineLaneType::MultiValue: return 3;
        case ofxOceanodeTimelineLaneType::MultiSlider: return 4;
        case ofxOceanodeTimelineLaneType::MultiGate: return 5;
        default: return 0;
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

// Curve lanes: active number of snap levels (0 = snapping off).
int curveSnapLevels(const ofxOceanodeTimelineLane& lane) {
    return lane.valueSnap && lane.valueQuantizeSteps >= 2 ? lane.valueQuantizeSteps : 0;
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

// Removes one point and merges its two segments into a neutral one, keeping the
// shape of every other segment (used by the LFO editor).
void eraseCurvePointKeepingTensions(ofxOceanodeTimelineLane& lane, size_t pointIndex) {
    if(pointIndex >= lane.curvePoints.size()) return;
    auto tensions = lane.curveTensions;
    tensions.resize(lane.curvePoints.size() > 0 ? lane.curvePoints.size() - 1 : 0);
    lane.curvePoints.erase(lane.curvePoints.begin() + pointIndex);
    if(pointIndex < tensions.size()) tensions.erase(tensions.begin() + pointIndex);
    else if(!tensions.empty()) tensions.pop_back();
    if(pointIndex > 0 && pointIndex - 1 < tensions.size()) tensions[pointIndex - 1] = ofxOceanodeTimelineCurveTension{};
    tensions.resize(lane.curvePoints.size() > 0 ? lane.curvePoints.size() - 1 : 0);
    lane.curveTensions = std::move(tensions);
}

// Thick divider between a track's rows and its docked clip editor.
void drawEditorSeparator(const ofxOceanodeTimelineTrack& track, float contentWidth) {
    ImGui::Dummy(ImVec2(contentWidth, 6.0f));
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(min, max, IM_COL32(12, 12, 14, 255));
    dl->AddRectFilled(ImVec2(min.x, min.y + 2.0f), ImVec2(max.x, min.y + 4.0f),
                      IM_COL32(track.color.r, track.color.g, track.color.b, 230));
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

bool ofxOceanodeTimelineController::createWaveClipFromFile(ofxOceanodeTimelineManager& timeline,
                                                            const std::string& trackId,
                                                            const std::string& filePath,
                                                            double startBeat,
                                                            double durationBeats) {
    const auto* track = timeline.getTrack(trackId);
    if(track == nullptr || !track->isWaveTrack || filePath.empty()) return false;

    const auto clipId = timeline.createClip(trackId, "Clip", snapBeat(startBeat),
                                             std::max(1.0 / kPPQ, snapBeat(durationBeats)));
    if(clipId.empty()) return false;

    if(auto* clip = timeline.getClip(trackId, clipId)) {
        clip->waveFilePath = filePath;
        clip->waveGain = 1.0f;
        const bool loaded = timeline.reloadWaveform(trackId, clipId);
        if(loaded && clip->waveFileDurationMs > 0.0 && container != nullptr) {
            const double bpm = std::max(1.0f, container->getTransportState().bpm);
            const double sourceBeats = std::max(1.0 / kPPQ,
                clip->waveFileDurationMs * bpm / 60000.0);
            // Audio clips are content-sized by the selected file. This also
            // gives the envelope lane a meaningful source range instead of
            // leaving every file at the one-bar placeholder length.
            clip->contentDurationBeats = sourceBeats;
            clip->durationBeats = sourceBeats;
            clip->repeatContent = false;
            clip->contentStretch = 1.0;
            clip->waveSourceStartBeat = 0.0;
            clip->waveFileDurationBeats = sourceBeats;
            clip->waveReverse = false;
        }
        return true;
    }
    timeline.removeClip(trackId, clipId);
    return false;
}

bool ofxOceanodeTimelineController::replaceWaveClipFromFile(ofxOceanodeTimelineManager& timeline,
                                                            const std::string& trackId,
                                                            const std::string& clipId,
                                                            const std::string& filePath) {
    auto* track = timeline.getTrack(trackId);
    auto* clip = timeline.getClip(trackId, clipId);
    if(track == nullptr || clip == nullptr || !track->isWaveTrack || filePath.empty()) return false;

    const double visibleDuration = std::max(1.0 / kPPQ, clip->durationBeats);
    clip->waveFilePath = filePath;
    clip->waveGain = std::max(0.0f, clip->waveGain);
    if(!timeline.reloadWaveform(trackId, clipId)) return false;

    const double bpm = container != nullptr
        ? std::max(1.0f, container->getTransportState().bpm) : 120.0;
    const double sourceBeats = std::max(1.0 / kPPQ,
        clip->waveFileDurationMs * bpm / 60000.0);
    clip->contentDurationBeats = sourceBeats;
    clip->durationBeats = visibleDuration;
    clip->repeatContent = false;
    clip->contentStretch = visibleDuration / sourceBeats;
    clip->waveSourceStartBeat = 0.0;
    clip->waveFileDurationBeats = sourceBeats;
    clip->waveReverse = false;
    return true;
}

bool ofxOceanodeTimelineController::trimWaveClipStart(ofxOceanodeTimelineManager& timeline,
                                                      const std::string& trackId, const std::string& clipId,
                                                      double newStartBeat) {
    const auto* track = timeline.getTrack(trackId);
    if(track == nullptr) return false;
    std::set<std::string> before;
    for(const auto& clip : track->clips) before.insert(clip.id);
    if(!splitWaveClipAtPlayhead(timeline, trackId, clipId, newStartBeat)) return false;
    std::string rightId;
    if(const auto* after = timeline.getTrack(trackId))
        for(const auto& clip : after->clips) if(before.count(clip.id) == 0) rightId = clip.id;
    if(rightId.empty()) return false;
    const std::string name = timeline.getClip(trackId, clipId) != nullptr ? timeline.getClip(trackId, clipId)->name : std::string();
    timeline.removeClip(trackId, clipId);
    if(!name.empty()) timeline.renameClip(trackId, rightId, name);
    selectedClips.erase({trackId, clipId});
    selectedClips.insert({trackId, rightId});
    if(editorTrackId == trackId && editorClipId == clipId) editorClipId = rightId;
    return true;
}

bool ofxOceanodeTimelineController::splitWaveClipAtPlayhead(ofxOceanodeTimelineManager& timeline,
                                                            const std::string& trackId,
                                                            const std::string& clipId,
                                                            double playheadBeat) {
    const auto* track = timeline.getTrack(trackId);
    const auto* originalClip = timeline.getClip(trackId, clipId);
    if(track == nullptr || originalClip == nullptr || !track->isWaveTrack ||
       originalClip->waveFilePath.empty()) return false;

    const double epsilon = 1.0 / kPPQ * 0.25;
    const double clipStart = originalClip->startBeat;
    const double clipEnd = clipStart + std::max(1.0 / kPPQ, originalClip->durationBeats);
    if(playheadBeat <= clipStart + epsilon || playheadBeat >= clipEnd - epsilon) return false;

    // A repeated clip can only be represented as two independent source
    // ranges when the cut is in its first cycle. New Wave clips are
    // non-repeating, but this guard avoids silently producing a different
    // arrangement for an older preset that had Repeat content enabled.
    const double localTimelineBeat = playheadBeat - clipStart;
    if(originalClip->repeatContent && localTimelineBeat >= cycleDuration(*originalClip) - epsilon)
        return false;

    const ofxOceanodeTimelineClip original = *originalClip;
    const double sourceLength = std::max(1.0 / kPPQ, original.contentDurationBeats);
    const double sourceSplit = ofClamp(timelineToSourceBeat(original, playheadBeat),
                                       epsilon, sourceLength - epsilon);
    const double leftTimelineDuration = localTimelineBeat;
    const double rightTimelineDuration = clipEnd - playheadBeat;
    if(leftTimelineDuration <= epsilon || rightTimelineDuration <= epsilon) return false;

    // Curve points are stored in source-beat space. Add an interpolated cut
    // point to both halves so a volume envelope remains continuous at the
    // split, then shift the right half back to source beat zero.
    auto splitCurveLane = [&](ofxOceanodeTimelineLane& leftLane,
                              ofxOceanodeTimelineLane& rightLane) {
        if(leftLane.curvePoints.empty()) return;
        std::sort(leftLane.curvePoints.begin(), leftLane.curvePoints.end(),
                  [](const auto& a, const auto& b) { return a.beat < b.beat; });
        const auto points = leftLane.curvePoints;
        const auto tensions = leftLane.curveTensions;
        const float splitValue = valueAtBeat(points, tensions, leftLane.curveInterpolation,
                                             sourceSplit, points.front().value);
        std::vector<ofxOceanodeTimelineCurvePoint> leftPoints;
        std::vector<ofxOceanodeTimelineCurvePoint> rightPoints;
        for(const auto& point : points) {
            if(point.beat < sourceSplit - epsilon) leftPoints.push_back(point);
            if(point.beat > sourceSplit + epsilon)
                rightPoints.push_back({point.beat - sourceSplit, point.value});
        }
        leftPoints.push_back({sourceSplit, splitValue});
        rightPoints.insert(rightPoints.begin(), {0.0, splitValue});
        leftLane.curvePoints = std::move(leftPoints);
        rightLane.curvePoints = std::move(rightPoints);
        resetCurveTensions(leftLane);
        resetCurveTensions(rightLane);
    };

    auto splitLaneData = [&](ofxOceanodeTimelineLane& leftLane,
                             ofxOceanodeTimelineLane& rightLane) {
        if(leftLane.type == ofxOceanodeTimelineLaneType::Curve) {
            splitCurveLane(leftLane, rightLane);
            return;
        }
        // Wave clips currently expose only a Curve volume lane, but preserve
        // the source positions of any future auxiliary lane rather than
        // copying points at the wrong location into the right slice.
        for(auto& note : rightLane.pianoNotes) note.startBeat -= sourceSplit;
        for(auto& step : rightLane.step.steps) step.startBeat -= sourceSplit;
        for(auto& row : rightLane.multiValueRows)
            for(auto& region : row) region.startBeat -= sourceSplit;
        for(auto& row : rightLane.multiGateRows)
            for(auto& region : row) region.startBeat -= sourceSplit;
    };

    // Splitting intentionally dissolves a group: the two new clips are now
    // independent edit targets and should not drag an unrelated slice with
    // them.
    if(timeline.getGroupForClip(trackId, clipId) != nullptr)
        timeline.ungroupClip(trackId, clipId);

    const std::string rightClipId = timeline.createClip(trackId, original.name + " slice",
                                                        playheadBeat, rightTimelineDuration);
    if(rightClipId.empty()) return false;

    auto* rightClip = timeline.getClip(trackId, rightClipId);
    if(rightClip == nullptr) return false;
    rightClip->waveFilePath = original.waveFilePath;
    rightClip->waveGain = original.waveGain;
    rightClip->wavePlaybackRate = original.wavePlaybackRate;
    // For forward playback the right slice begins after the cut. For reverse
    // playback the timeline starts at the high end of the source range, so
    // the left slice occupies the high range and the right slice the low one.
    rightClip->waveSourceStartBeat = original.waveReverse
        ? original.waveSourceStartBeat
        : original.waveSourceStartBeat + sourceSplit;
    rightClip->waveFileDurationBeats = original.waveFileDurationBeats;
    rightClip->waveReverse = original.waveReverse;
    rightClip->waveNumChannels = original.waveNumChannels;
    rightClip->waveFileDurationMs = original.waveFileDurationMs;
    rightClip->waveformPeaks = original.waveformPeaks;
    rightClip->contentDurationBeats = sourceLength - sourceSplit;
    rightClip->contentStretch = original.contentStretch;
    rightClip->repeatContent = false;

    std::vector<ofxOceanodeTimelineLane> leftLanes;
    leftLanes.reserve(original.lanes.size());
    for(const auto& sourceLane : original.lanes) {
        auto leftLane = sourceLane;
        auto rightLane = leftLane;
        splitLaneData(leftLane, rightLane);
        leftLanes.push_back(leftLane);
        const std::string newLaneId = timeline.createLane(trackId, rightClipId,
                                                           leftLane.name, leftLane.type);
        if(newLaneId.empty()) continue;
        if(auto* createdLane = timeline.getLane(trackId, rightClipId, newLaneId)) {
            *createdLane = std::move(rightLane);
            createdLane->id = newLaneId;
        }
    }

    if(auto* leftClip = timeline.getClip(trackId, clipId)) {
        leftClip->durationBeats = leftTimelineDuration;
        leftClip->contentDurationBeats = sourceSplit;
        if(original.waveReverse)
            leftClip->waveSourceStartBeat = original.waveSourceStartBeat + sourceLength - sourceSplit;
        leftClip->repeatContent = false;
        leftClip->lanes = std::move(leftLanes);
    }

    if(auto* sortedTrack = timeline.getTrack(trackId)) {
        std::sort(sortedTrack->clips.begin(), sortedTrack->clips.end(),
                  [](const auto& a, const auto& b) {
                      if(std::abs(a.startBeat - b.startBeat) > 1e-9) return a.startBeat < b.startBeat;
                      return a.id < b.id;
                  });
    }
    // A split creates two new edit targets, but should not leave either slice
    // selected. This prevents an immediate group/drag/delete action from
    // affecting both halves just because the split command was invoked.
    selectedClips.clear();
    return true;
}

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
    if(!snapEnabled || effectiveRulerSnapBeats <= 0.0) return std::max(0.0, beat);
    return std::max(0.0, std::round(beat / effectiveRulerSnapBeats) * effectiveRulerSnapBeats);
}

double ofxOceanodeTimelineController::editIncrement() const {
    return effectiveRulerSnapBeats > 0.0 ? effectiveRulerSnapBeats : 1.0 / kPPQ;
}

double ofxOceanodeTimelineController::displayGridBeats() const {
    return effectiveRulerSnapBeats > 0.0 ? effectiveRulerSnapBeats : 1.0;
}

float ofxOceanodeTimelineController::beatToPixels(const ofxOceanodeTimelineManager& timeline,
                                                  double beat, float fallbackBpm) const {
    return static_cast<float>(timeline.beatToSeconds(std::max(0.0, beat), fallbackBpm) *
                              timeline.getViewState().pixelsPerSecond);
}

double ofxOceanodeTimelineController::pixelsToBeat(const ofxOceanodeTimelineManager& timeline,
                                                   float pixels, float fallbackBpm,
                                                   double endBeatHint) const {
    const double targetSeconds = std::max(0.0f, pixels) /
                                 std::max(kMinPixelsPerSecond, timeline.getViewState().pixelsPerSecond);
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
    ImGui::Separator();
    // Add/Multiply/Min/Max have no natural ceiling of their own -- several
    // bindings combining onto one parameter can drive it past its min/max
    // even though each contributor individually stayed in range. This lives
    // right below the blend modes because it's only ever relevant once one
    // of those non-Replace modes is actually in play. It only takes effect
    // via this parameter's first (non-bypassed) binding, the same one whose
    // value type/default already act as the shared source of truth for it.
    if(ImGui::MenuItem("Clamp to parameter range", nullptr, binding.clampToParameterRange))
        timeline.setBindingClamp(trackId, binding.id, !binding.clampToParameterRange);
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("Keeps the combined value from Add/Multiply/Min/Max bindings within this parameter's own min/max");
}

void ofxOceanodeTimelineController::draw() {
    if(container == nullptr) return;
    editorScreenValid = false;
    auto& timeline = container->getTimelineManager();
    auto& viewState = timeline.getViewState();
    float& pixelsPerSecond = viewState.pixelsPerSecond;
    float& timelineScrollX = viewState.scrollX;
    auto transportState = container->getTransportState();
    auto transport = container->getTransport();
    const bool externalClock = transport != nullptr && transport->hasExternalClock();
    effectiveRulerSnapBeats = adaptiveSnapDivision(rulerSnapBeats,
                                                   timeline.getBeatsPerBar(),
                                                   transportState.bpm,
                                                   pixelsPerSecond);

    if(transport != nullptr &&
       ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
       !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
       ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        transport->togglePlay();
        transportState = container->getTransportState();
    }

    // S is deliberately a timeline-level shortcut rather than a text-field
    // shortcut: it cuts the one selected clip (Wave or automation) at the current transport
    // beat and leaves both resulting slices independently editable.
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
       !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
       !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
       ImGui::IsKeyPressed(ImGuiKey_S, false) && selectedClips.size() == 1) {
        const auto selected = *selectedClips.begin();
        const auto* selectedTrack = timeline.getTrack(selected.first);
        if(selectedTrack != nullptr && selectedTrack->isWaveTrack)
            splitWaveClipAtPlayhead(timeline, selected.first, selected.second,
                                    transportState.beatPosition);
        else if(selectedTrack != nullptr)
            timeline.splitClip(selected.first, selected.second, transportState.beatPosition);
    }

    // ======================= Toolbar =======================
    // Two rows of grouped controls. Row 1: transport, position, tempo, meter,
    // sync. Row 2: loop, grid, view, output timing, tracks. Groups are split
    // by thin vertical rules; transport and toggles are drawn as icons.
    // Darker panel behind both rows: widgets draw on channel 1, the panel is
    // filled on channel 0 once the rows' height is known.
    auto* toolbarDl = ImGui::GetWindowDrawList();
    toolbarDl->ChannelsSplit(2);
    toolbarDl->ChannelsSetCurrent(1);
    const float toolbarPad = 5.0f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + toolbarPad);
    const float toolbarTop = ImGui::GetCursorScreenPos().y - toolbarPad;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
    const float toolbarHeight = ImGui::GetFrameHeight();
    enum class ToolIcon { Play, Pause, Stop, ToStart, Loop, Follow, Fit, ZoomIn, ZoomOut, Dots, Tap, Clock, Magnet };
    auto drawIcon = [&](ImDrawList* dl, ToolIcon icon, ImVec2 c, float s, ImU32 col) {
        switch(icon) {
            case ToolIcon::Play:
                dl->AddTriangleFilled(ImVec2(c.x - s * 0.35f, c.y - s * 0.45f), ImVec2(c.x - s * 0.35f, c.y + s * 0.45f),
                                      ImVec2(c.x + s * 0.45f, c.y), col);
                break;
            case ToolIcon::Pause:
                dl->AddRectFilled(ImVec2(c.x - s * 0.38f, c.y - s * 0.42f), ImVec2(c.x - s * 0.1f, c.y + s * 0.42f), col, 1.0f);
                dl->AddRectFilled(ImVec2(c.x + s * 0.1f, c.y - s * 0.42f), ImVec2(c.x + s * 0.38f, c.y + s * 0.42f), col, 1.0f);
                break;
            case ToolIcon::Stop:
                dl->AddRectFilled(ImVec2(c.x - s * 0.36f, c.y - s * 0.36f), ImVec2(c.x + s * 0.36f, c.y + s * 0.36f), col, 1.5f);
                break;
            case ToolIcon::ToStart:
                dl->AddRectFilled(ImVec2(c.x - s * 0.42f, c.y - s * 0.4f), ImVec2(c.x - s * 0.26f, c.y + s * 0.4f), col);
                dl->AddTriangleFilled(ImVec2(c.x + s * 0.42f, c.y - s * 0.42f), ImVec2(c.x + s * 0.42f, c.y + s * 0.42f),
                                      ImVec2(c.x - s * 0.22f, c.y), col);
                break;
            case ToolIcon::Loop: {
                const float r = s * 0.42f;
                dl->PathArcTo(c, r, 0.35f * 3.14159265f, 1.85f * 3.14159265f, 16);
                dl->PathStroke(col, 0, 1.8f);
                const ImVec2 tip(c.x + r * std::cos(0.35f * 3.14159265f), c.y + r * std::sin(0.35f * 3.14159265f));
                dl->AddTriangleFilled(ImVec2(tip.x + s * 0.2f, tip.y - s * 0.02f), ImVec2(tip.x - s * 0.12f, tip.y - s * 0.18f),
                                      ImVec2(tip.x - s * 0.02f, tip.y + s * 0.2f), col);
                break;
            }
            case ToolIcon::Follow: // playhead line with a chevron pulling it along
                dl->AddLine(ImVec2(c.x - s * 0.3f, c.y - s * 0.45f), ImVec2(c.x - s * 0.3f, c.y + s * 0.45f), col, 1.8f);
                dl->AddLine(ImVec2(c.x - s * 0.02f, c.y - s * 0.28f), ImVec2(c.x + s * 0.28f, c.y), col, 1.8f);
                dl->AddLine(ImVec2(c.x + s * 0.28f, c.y), ImVec2(c.x - s * 0.02f, c.y + s * 0.28f), col, 1.8f);
                break;
            case ToolIcon::Fit: { // four corners
                const float o = s * 0.42f, l = s * 0.2f;
                for(int sx = -1; sx <= 1; sx += 2)
                    for(int sy = -1; sy <= 1; sy += 2) {
                        const ImVec2 k(c.x + sx * o, c.y + sy * o);
                        dl->AddLine(k, ImVec2(k.x - sx * l, k.y), col, 1.6f);
                        dl->AddLine(k, ImVec2(k.x, k.y - sy * l), col, 1.6f);
                    }
                break;
            }
            case ToolIcon::ZoomIn:
                dl->AddLine(ImVec2(c.x - s * 0.36f, c.y), ImVec2(c.x + s * 0.36f, c.y), col, 1.8f);
                dl->AddLine(ImVec2(c.x, c.y - s * 0.36f), ImVec2(c.x, c.y + s * 0.36f), col, 1.8f);
                break;
            case ToolIcon::ZoomOut:
                dl->AddLine(ImVec2(c.x - s * 0.36f, c.y), ImVec2(c.x + s * 0.36f, c.y), col, 1.8f);
                break;
            case ToolIcon::Dots:
                for(int i = -1; i <= 1; ++i) dl->AddCircleFilled(ImVec2(c.x + i * s * 0.3f, c.y), s * 0.09f, col);
                break;
            case ToolIcon::Tap:
                dl->AddCircle(c, s * 0.4f, col, 16, 1.6f);
                dl->AddCircleFilled(c, s * 0.16f, col);
                break;
            case ToolIcon::Magnet: { // horseshoe magnet, poles up
                const float r = s * 0.3f;
                const float legTop = c.y - s * 0.42f;
                const float bendY = c.y + s * 0.08f;
                dl->PathArcTo(ImVec2(c.x, bendY), r, 0.0f, 3.14159265f, 14);
                dl->PathStroke(col, 0, s * 0.2f);
                dl->AddLine(ImVec2(c.x - r, bendY), ImVec2(c.x - r, legTop), col, s * 0.2f);
                dl->AddLine(ImVec2(c.x + r, bendY), ImVec2(c.x + r, legTop), col, s * 0.2f);
                // Pole tips.
                const float tip = s * 0.14f;
                dl->AddRectFilled(ImVec2(c.x - r - s * 0.11f, legTop - tip * 0.3f), ImVec2(c.x - r + s * 0.11f, legTop + tip), col);
                dl->AddRectFilled(ImVec2(c.x + r - s * 0.11f, legTop - tip * 0.3f), ImVec2(c.x + r + s * 0.11f, legTop + tip), col);
                break;
            }
            case ToolIcon::Clock:
                dl->AddCircle(c, s * 0.42f, col, 20, 1.6f);
                dl->AddLine(c, ImVec2(c.x, c.y - s * 0.28f), col, 1.6f);
                dl->AddLine(c, ImVec2(c.x + s * 0.2f, c.y + s * 0.08f), col, 1.6f);
                break;
        }
    };
    // Square icon button. `on` tints it with `accent` (toggles, playing).
    auto iconButton = [&](const char* id, ToolIcon icon, const char* tooltip, bool on = false,
                          ImU32 accent = IM_COL32(95, 150, 235, 255), bool enabled = true) {
        const ImVec2 size(toolbarHeight + 2.0f, toolbarHeight);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton(id, size) && enabled;
        const bool hovered = ImGui::IsItemHovered() && enabled;
        const bool held = ImGui::IsItemActive() && enabled;
        auto* dl = ImGui::GetWindowDrawList();
        ImU32 bg = on ? accent : ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
        if(on && hovered) bg = (accent & 0x00FFFFFF) | 0xE6000000;
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, 3.0f);
        const ImU32 fg = !enabled ? IM_COL32(120, 120, 120, 160) : on ? IM_COL32(15, 18, 24, 255) : IM_COL32(225, 228, 235, 255);
        drawIcon(dl, icon, ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), size.y * 0.52f, fg);
        if(tooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", tooltip);
        return pressed;
    };
    // Thin vertical rule between groups.
    auto groupSeparator = [&]() {
        ImGui::SameLine(0.0f, 9.0f);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + 2.0f), ImVec2(pos.x, pos.y + toolbarHeight - 2.0f),
                                            IM_COL32(255, 255, 255, 38), 1.0f);
        ImGui::Dummy(ImVec2(1.0f, toolbarHeight));
        ImGui::SameLine(0.0f, 9.0f);
    };
    // Small dim caption in front of a group (kept to a word).
    auto caption = [&](const char* text) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", text);
        ImGui::SameLine(0.0f, 5.0f);
    };
    // ---- Row 1: transport | position | tempo | meter | sync ----
    if(transport != nullptr) {
        // Following an external clock: position, play state and tempo come from it.
        const char* lockedTip = "Following an external clock";
        if(iconButton("##tbToStart", ToolIcon::ToStart, externalClock ? lockedTip : "Back to the start", false, 0, !externalClock))
            transport->seekToBeat(0.0);
        ImGui::SameLine(0.0f, 2.0f);
        if(iconButton("##tbStop", ToolIcon::Stop, externalClock ? lockedTip : "Stop (back to the start)", false, 0, !externalClock))
            transport->stop();
        ImGui::SameLine(0.0f, 2.0f);
        if(iconButton("##tbPlay", transportState.isPlaying ? ToolIcon::Pause : ToolIcon::Play,
                      externalClock ? lockedTip : (transportState.isPlaying ? "Pause (Space)" : "Play (Space)"),
                      transportState.isPlaying, IM_COL32(90, 205, 125, 255), !externalClock))
            transport->setIsPlaying(!transportState.isPlaying);

        groupSeparator();
        {
            // Position readout: bar . beat . sixteenth, and the song time.
            const double beatsPerBar = std::max(1e-6, timeline.getBeatsPerBar());
            const double beat = std::max(0.0, transportState.beatPosition);
            const int bar = static_cast<int>(std::floor(beat / beatsPerBar)) + 1;
            const double inBar = beat - (bar - 1) * beatsPerBar;
            const int beatInBar = static_cast<int>(std::floor(inBar)) + 1;
            const int sixteenth = static_cast<int>(std::floor((inBar - std::floor(inBar)) * 4.0)) + 1;
            const double seconds = timeline.beatToSeconds(beat, transportState.bpm);
            const int minutes = static_cast<int>(seconds / 60.0);
            char positionText[32];
            char timeText[32];
            std::snprintf(positionText, sizeof(positionText), "%3d. %d. %d", bar, beatInBar, sixteenth);
            std::snprintf(timeText, sizeof(timeText), "%d:%06.3f", minutes, seconds - minutes * 60.0);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImVec2 size(170.0f, toolbarHeight);
            ImGui::Dummy(size);
            auto* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(12, 14, 18, 255), 3.0f);
            dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(255, 255, 255, 25), 3.0f);
            const float textY = pos.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f;
            dl->AddText(ImVec2(pos.x + 8.0f, textY),
                        transportState.isPlaying ? IM_COL32(120, 235, 160, 255) : IM_COL32(230, 232, 238, 255), positionText);
            const float timeWidth = ImGui::CalcTextSize(timeText).x;
            dl->AddText(ImVec2(pos.x + size.x - timeWidth - 8.0f, textY), IM_COL32(150, 155, 165, 255), timeText);
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Bar . beat . sixteenth    song time\nBeat %.3f", beat);
        }

        groupSeparator();
        {
            float bpm = transportState.bpm;
            if(externalClock || timeline.isBpmAutomationEnabled()) {
                // Read-only: the tempo comes from the clock or the tempo lane.
                ImGui::BeginDisabled();
                ImGui::SetNextItemWidth(78.0f);
                ImGui::DragFloat("##tbBpmRead", &bpm, 0.0f, 0.0f, 0.0f, "%.1f BPM");
                ImGui::EndDisabled();
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(externalClock ? ImVec4(0.45f, 0.95f, 0.5f, 1.0f) : ImVec4(0.95f, 0.75f, 0.35f, 1.0f),
                                   externalClock ? "EXT" : "AUTO");
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip(externalClock ? "Tempo from the external clock" : "Tempo from the tempo lane");
            } else {
                ImGui::SetNextItemWidth(78.0f);
                if(ImGui::DragFloat("##tbBpm", &bpm, 0.1f, 1.0f, 999.0f, "%.1f BPM")) container->setBpm(bpm);
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("Tempo (drag, or double-click to type)");
                ImGui::SameLine(0.0f, 2.0f);
                // Tap tempo: the average of the last taps; a pause of 2 s starts over.
                if(iconButton("##tbTap", ToolIcon::Tap, "Tap tempo: tap on the beat")) {
                    const double now = ofGetElapsedTimef();
                    if(!tapTimes.empty() && now - tapTimes.back() > 2.0) tapTimes.clear();
                    tapTimes.push_back(now);
                    if(tapTimes.size() > 8) tapTimes.erase(tapTimes.begin());
                    if(tapTimes.size() >= 2) {
                        const double interval = (tapTimes.back() - tapTimes.front()) / (tapTimes.size() - 1);
                        if(interval > 0.0) container->setBpm(ofClamp(static_cast<float>(60.0 / interval), 20.0f, 400.0f));
                    }
                }
            }
        }

        groupSeparator();
        {
            // Meter: numerator is free (any meter is valid), the denominator is
            // a note value so getBeatsPerBar() always lands on a sane grid.
            int tsNumerator = timeline.getTimeSignatureNumerator();
            int tsDenominator = timeline.getTimeSignatureDenominator();
            ImGui::SetNextItemWidth(28.0f);
            if(ImGui::DragInt("##timeSigNum", &tsNumerator, 0.1f, 1, 64))
                timeline.setTimeSignature(tsNumerator, tsDenominator);
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Time signature");
            ImGui::SameLine(0.0f, 3.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("/");
            ImGui::SameLine(0.0f, 3.0f);
            ImGui::SetNextItemWidth(30.0f);
            char tsDenomLabel[8];
            std::snprintf(tsDenomLabel, sizeof(tsDenomLabel), "%d", tsDenominator);
            if(ImGui::BeginCombo("##timeSigDenom", tsDenomLabel, ImGuiComboFlags_NoArrowButton)) {
                for(int option : {1, 2, 4, 8, 16, 32}) {
                    char optionLabel[8];
                    std::snprintf(optionLabel, sizeof(optionLabel), "%d", option);
                    if(ImGui::Selectable(optionLabel, option == tsDenominator))
                        timeline.setTimeSignature(tsNumerator, option);
                }
                ImGui::EndCombo();
            }
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Time signature");
        }
#ifdef OFXOCEANODE_USE_MIDI
        if(auto* clockSync = container->getMidiClockSync()) {
            groupSeparator();
            caption("Sync");
            const char* syncModes[] = {"Internal", "MIDI Clock"};
            int syncMode = container->isMidiClockSyncEnabled() ? 1 : 0;
            ImGui::SetNextItemWidth(92.0f);
            if(ImGui::Combo("##transportSync", &syncMode, syncModes, 2))
                container->setMidiClockSyncEnabled(syncMode == 1);
            if(ImGui::IsItemHovered())
                ImGui::SetTooltip("Internal: Oceanode runs its own transport.\n"
                                  "MIDI Clock: follow an external MIDI clock (play/stop, song position, tempo).");
            ImGui::SameLine(0.0f, 2.0f);
            if(iconButton("##midiClockSyncSettingsButton", ToolIcon::Dots, "MIDI clock port and options"))
                ImGui::OpenPopup("##midiClockSyncSettings");
            // Status: a coloured dot and a word; details in the tooltip.
            ImU32 statusColor = 0;
            std::string statusText;
            std::string statusTip;
            if(container->isMidiClockSyncEnabled()) {
                const auto clockStatus = clockSync->status();
                if(clockSync->getPortName().empty()) {
                    statusColor = IM_COL32(255, 165, 50, 255); statusText = "no port"; statusTip = "Choose a MIDI port in the options (...)";
                } else if(!clockSync->isOpen()) {
                    statusColor = IM_COL32(255, 165, 50, 255); statusText = "offline"; statusTip = "Port not available (retrying): " + clockSync->getPortName();
                } else if(!clockStatus.receiving) {
                    statusColor = IM_COL32(255, 90, 90, 255); statusText = "no clock"; statusTip = "No clock arriving on " + clockSync->getPortName();
                } else {
                    statusColor = IM_COL32(110, 240, 130, 255);
                    statusText = clockStatus.playing ? "play" : "stop";
                    statusTip = clockSync->getPortName() + ": " + ofToString(clockStatus.bpm, 1) + " BPM";
                }
                if(!clockSync->isDrivingTransport()) statusTip += "\n(another source drives the transport)";
            } else if(externalClock) {
                statusColor = IM_COL32(110, 240, 130, 255); statusText = "node"; statusTip = "Transport driven by a MIDI Clock node";
            }
            if(!statusText.empty()) {
                ImGui::SameLine(0.0f, 6.0f);
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const float textWidth = ImGui::CalcTextSize(statusText.c_str()).x;
                ImGui::Dummy(ImVec2(14.0f + textWidth, toolbarHeight));
                auto* dl = ImGui::GetWindowDrawList();
                dl->AddCircleFilled(ImVec2(pos.x + 5.0f, pos.y + toolbarHeight * 0.5f), 4.0f, statusColor);
                dl->AddText(ImVec2(pos.x + 13.0f, pos.y + (toolbarHeight - ImGui::GetTextLineHeight()) * 0.5f),
                            IM_COL32(200, 204, 212, 255), statusText.c_str());
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", statusTip.c_str());
            }
            if(ImGui::BeginPopup("##midiClockSyncSettings")) {
                if(ImGui::IsWindowAppearing()) midiPortChoices = ofxOceanodeMidiClock::availablePorts();
                const auto& portChoices = midiPortChoices;
                const std::string currentPort = clockSync->getPortName();
                ImGui::SetNextItemWidth(220.0f);
                if(ImGui::BeginCombo("Port", currentPort.empty() ? "None" : currentPort.c_str())) {
                    if(ImGui::Selectable("None", currentPort.empty())) container->setMidiClockSyncPort("");
                    for(const auto& port : portChoices) {
                        if(ImGui::Selectable(port.c_str(), port == currentPort)) container->setMidiClockSyncPort(port);
                    }
                    ImGui::EndCombo();
                }
                bool reaper = clockSync->core().getReaperMode();
                if(ImGui::Checkbox("REAPER mode", &reaper)) clockSync->core().setReaperMode(reaper);
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("A Song Position sent just before Start is honoured\n(start from that position instead of the top).");
                bool clockOnly = clockSync->core().getClockOnly();
                if(ImGui::Checkbox("Clock only", &clockOnly)) clockSync->core().setClockOnly(clockOnly);
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("For devices that send clock but no Start/Stop:\nplay while ticks arrive, stop when they stop.");
                bool stopOnLoss = clockSync->core().getStopOnClockLoss();
                if(ImGui::Checkbox("Stop when the clock is lost", &stopOnLoss)) clockSync->core().setStopOnClockLoss(stopOnLoss);
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("When ticks stop arriving while playing (cable pulled, master gone):\non = stop after half a second, off = hold at the last position.");
                float offsetMs = clockSync->core().getOffsetMs();
                ImGui::SetNextItemWidth(120.0f);
                if(ImGui::DragFloat("Offset (ms)", &offsetMs, 0.5f, -250.0f, 250.0f, "%.1f"))
                    clockSync->core().setOffsetMs(ofClamp(offsetMs, -250.0f, 250.0f));
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("Positive runs ahead of the incoming clock (compensates output latency).");
                int tempoWindow = clockSync->core().getTempoWindow();
                ImGui::SetNextItemWidth(120.0f);
                if(ImGui::DragInt("Tempo window (ticks)", &tempoWindow, 0.2f, 2, 96))
                    clockSync->core().setTempoWindow(tempoWindow);
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("Clock ticks averaged for the tempo estimate (24 = one beat).");
                ImGui::EndPopup();
            }
        }
#endif
    }

    // ---- Row 2: loop | grid | view | output timing | tracks ----
    {
        bool loopEnabled = timeline.isLoopEnabled();
        const bool localLoopAvailable = !externalClock;
        const char* loopTip = localLoopAvailable ? (loopEnabled ? "Loop on" : "Loop off")
            : "Local loop unavailable while following an external clock";
        if(iconButton("##tbLoop", ToolIcon::Loop, loopTip,
                      loopEnabled && localLoopAvailable,
                      IM_COL32(120, 135, 240, 255), localLoopAvailable))
            timeline.setLoopEnabled(!loopEnabled);
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::BeginDisabled(!loopEnabled || !localLoopAvailable);
        double loopStart = timeline.getLoopStartBeat();
        double loopEnd = timeline.getLoopEndBeat();
        ImGui::SetNextItemWidth(56.0f);
        if(ImGui::InputDouble("##loopStart", &loopStart, 0.0, 0.0, "%.3g"))
            timeline.setLoopRange(snapBeat(loopStart), loopEnd);
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Loop start (beats)");
        ImGui::SameLine(0.0f, 3.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("-");
        ImGui::SameLine(0.0f, 3.0f);
        ImGui::SetNextItemWidth(56.0f);
        if(ImGui::InputDouble("##loopEnd", &loopEnd, 0.0, 0.0, "%.3g"))
            timeline.setLoopRange(loopStart, std::max(loopStart + 1.0 / kPPQ, snapBeat(loopEnd)));
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Loop end (beats)");
        ImGui::EndDisabled();
    }

    groupSeparator();
    if(iconButton("##tbSnapToggle", ToolIcon::Magnet,
                  snapEnabled ? "Snap to grid: on (clips, markers, loop, playhead)" : "Snap to grid: off",
                  snapEnabled, IM_COL32(235, 95, 95, 255)))
        snapEnabled = !snapEnabled;
    ImGui::SameLine(0.0f, 4.0f);
    {
        int rulerDivision = divisionIndexForBeats(rulerSnapBeats);
        ImGui::SetNextItemWidth(64.0f);
        if(ImGui::BeginCombo("##tbSnap", kDivisionOptions[rulerDivision].label)) {
            for(int i = 0; i < kDivisionOptionCount; ++i) {
                if(ImGui::Selectable(kDivisionOptions[i].label, i == rulerDivision)) {
                    rulerSnapBeats = kDivisionOptions[i].beats;
                    effectiveRulerSnapBeats = adaptiveSnapDivision(rulerSnapBeats,
                                                                   timeline.getBeatsPerBar(),
                                                                   transportState.bpm,
                                                                   pixelsPerSecond);
                }
            }
            ImGui::EndCombo();
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Grid and snap division");
    }

    groupSeparator();
    {
        if(iconButton("##tbZoomOut", ToolIcon::ZoomOut, "Zoom out"))
            pixelsPerSecond = ofClamp(pixelsPerSecond / 1.4f, kMinPixelsPerSecond, kMaxPixelsPerSecond);
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::SetNextItemWidth(58.0f);
        ImGui::DragFloat("##tbPxPerSec", &pixelsPerSecond, 1.0f, kMinPixelsPerSecond, kMaxPixelsPerSecond, "%.0f px/s");
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Zoom: pixels per second (mouse wheel over the timeline also zooms)");
        ImGui::SameLine(0.0f, 2.0f);
        if(iconButton("##tbZoomIn", ToolIcon::ZoomIn, "Zoom in"))
            pixelsPerSecond = ofClamp(pixelsPerSecond * 1.4f, kMinPixelsPerSecond, kMaxPixelsPerSecond);
        ImGui::SameLine(0.0f, 4.0f);
        if(iconButton("##tbFit", ToolIcon::Fit, "Zoom to fit every clip and marker")) requestZoomToFit = true;
        ImGui::SameLine(0.0f, 4.0f);
        if(iconButton("##tbFollow", ToolIcon::Follow, followPlayhead ? "Follow playhead: on" : "Follow playhead: off",
                      followPlayhead, IM_COL32(240, 185, 80, 255)))
            followPlayhead = !followPlayhead;
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::SetNextItemWidth(62.0f);
        ImGui::DragInt("##tbBars", &visibleBars, 0.25f, 1, 256, "%d bars");
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Timeline length");
    }

    groupSeparator();
    {
        // Timestamped events: discrete lanes (steps, notes, gates, multi-value
        // blocks) are handed to a backend that can execute them at an exact
        // instant, a lookahead ahead of the playhead. Off sends everything on
        // the frame that crosses it.
        const bool timeStamped = timeline.isSchedulingEnabled();
        if(iconButton("##tbTimeStamped", ToolIcon::Clock,
                      "Timestamped: send steps, notes and gates to SuperCollider with their exact time\n"
                      "(timetagged OSC bundles) instead of on the GUI frame that crosses them.\n"
                      "Curves and parameters without a timestamping backend are unaffected.",
                      timeStamped, IM_COL32(90, 205, 205, 255)))
            timeline.setSchedulingEnabled(!timeStamped);
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::BeginDisabled(!timeStamped);
        float lookahead = static_cast<float>(timeline.getSchedulingLookaheadMs());
        ImGui::SetNextItemWidth(78.0f);
        if(ImGui::DragFloat("##schedulingLookahead", &lookahead, 1.0f, 0.0f, 1000.0f, "%.0f ms ahead"))
            timeline.setSchedulingLookaheadMs(lookahead);
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Lookahead window. It must be longer than the worst frame interval\n"
                              "that still has to sound on time (120 ms covers a several-frame stall).");
        ImGui::EndDisabled();
    }

    groupSeparator();
    // A Wave Track is created together with its first audio clip. Selecting
    // the file here avoids leaving an empty track behind and makes the
    // waveform visible immediately after the track is added.
    if(ImGui::Button("+ Wave Track")) {
        waveFileRequest = WaveFileRequest::NewTrack;
        waveFileRequestBeat = transportState.beatPosition;
    }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("New track with an audio file");
    ImGui::PopStyleVar(3);
    {
        // Stop just short of the next row (the style's item spacing below the rows).
        const float bottom = ImGui::GetItemRectMax().y + std::max(1.0f, ImGui::GetStyle().ItemSpacing.y - 1.0f);
        const float left = ImGui::GetWindowPos().x;
        const float right = left + ImGui::GetWindowWidth();
        toolbarDl->ChannelsSetCurrent(0);
        toolbarDl->AddRectFilled(ImVec2(left, toolbarTop), ImVec2(right, bottom), IM_COL32(17, 18, 21, 255));
        toolbarDl->AddLine(ImVec2(left, bottom), ImVec2(right, bottom), IM_COL32(0, 0, 0, 200), 1.0f);
        toolbarDl->ChannelsMerge();
    }

    // Stale entries can accumulate in selectedClips if their clip was
    // removed some other way (e.g. loading a different preset) without
    // going through this controller's own delete paths below -- filter
    // them out here, once per frame, before anything reads the selection.
    for(auto it = selectedClips.begin(); it != selectedClips.end();) {
        if(timeline.getClip(it->first, it->second) == nullptr) it = selectedClips.erase(it);
        else ++it;
    }
    // Delete acts on the timeline selection, but only while the Timeline
    // window owns keyboard focus and no text/widget is editing. Defer the
    // actual removals until after the track iteration below, just like the
    // context-menu delete path, because removing a clip can reallocate its
    // track's clip vector.
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
       !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
       !selectedClips.empty() && !lastClickInEditor && // in the editor, Delete edits the clip's content
       !(clipEditorOpen && lfoSelectedPointIndex >= 0 && lfoSelectedClipId == editorClipId) && // Delete removes the selected LFO point instead

       (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))) {
        keyboardClipDeletionRequests.assign(selectedClips.begin(), selectedClips.end());
        selectedClips.clear();
    }
    // Grouping/ungrouping is reachable two ways, per the request that
    // prompted this: a toolbar button here for whatever is currently
    // Shift+click-selected, and (see drawClipMenu below) a right-click
    // menu item on the clip itself.
    if(selectedClips.size() >= 2) {
        ImGui::SameLine();
        const std::string label = "Group (" + ofToString(selectedClips.size()) + ")##groupSelectedClips";
        if(ImGui::Button(label.c_str())) {
            timeline.groupClips(std::vector<std::pair<std::string, std::string>>(
                selectedClips.begin(), selectedClips.end()));
            // Selection is left as-is (not cleared) -- the clips just
            // grouped are exactly what's still highlighted, which is the
            // most useful confirmation that the group now contains them.
        }
    } else if(selectedClips.size() == 1 && timeline.getGroupForClip(selectedClips.begin()->first, selectedClips.begin()->second) != nullptr) {
        ImGui::SameLine();
        if(ImGui::Button("Ungroup##ungroupSelectedClip")) {
            timeline.ungroupClip(selectedClips.begin()->first, selectedClips.begin()->second);
        }
    }

    const double endBeat = getContentEndBeat(timeline);
    const float availableWidth = std::max(320.0f, ImGui::GetContentRegionAvail().x);
    if(requestZoomToFit) {
        requestZoomToFit = false;
        double lastBeat = 0.0;
        for(const auto& fitTrack : timeline.getTracks())
            for(const auto& fitClip : fitTrack.clips)
                lastBeat = std::max(lastBeat, fitClip.startBeat + fitClip.durationBeats);
        for(const auto& marker : timeline.getMarkers()) lastBeat = std::max(lastBeat, marker.beat);
        if(lastBeat <= 0.0) lastBeat = timeline.getBeatsPerBar() * 4.0;
        const double seconds = std::max(0.001, timeline.beatToSeconds(lastBeat, transportState.bpm));
        const float usable = std::max(40.0f, availableWidth - kLabelWidth - 24.0f);
        pixelsPerSecond = ofClamp(static_cast<float>(usable / seconds), kMinPixelsPerSecond, kMaxPixelsPerSecond);
        timelineScrollX = 0.0f;
    }
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
    const float maxTimelineScrollX = std::max(0.0f, timelineWidth - (availableWidth - kLabelWidth));
    if(followPlayhead && transportState.isPlaying) {
        // Page along with the playhead: when it nears the right edge (or is
        // off screen to the left), bring it back near the left side.
        const float visibleWidth = std::max(1.0f, availableWidth - kLabelWidth);
        const float playheadX = beatOffset(transportState.beatPosition);
        if(playheadX > timelineScrollX + visibleWidth * 0.9f || playheadX < timelineScrollX)
            timelineScrollX = playheadX - visibleWidth * 0.1f;
    }
    timelineScrollX = ofClamp(timelineScrollX, 0.0f, maxTimelineScrollX);
    // Mouse wheel over the timeline zooms around the mouse; over the label column
    // it scrolls vertically (tracks area only). Shared by the pinned ruler strip
    // and the scrolling tracks viewport; call inside the child it applies to.
    auto handleTimelineWheel = [&](bool allowVerticalScroll) {
        if(!ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                                   ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) return;
        const bool zoomGesture = std::abs(ImGui::GetIO().MouseWheel) > 0.001f;
        if(zoomGesture) {
            const float oldPixelsPerSecond = pixelsPerSecond;
            const float mouseViewportX = ImGui::GetIO().MousePos.x - ImGui::GetWindowPos().x;
            if(mouseViewportX >= kLabelWidth) {
                const float secondsAtMouse = std::max(0.0f, timelineScrollX + mouseViewportX - kLabelWidth) /
                                             std::max(kMinPixelsPerSecond, oldPixelsPerSecond);
                const float zoomFactor = static_cast<float>(std::pow(1.12f, ImGui::GetIO().MouseWheel));
                pixelsPerSecond = ofClamp(oldPixelsPerSecond * zoomFactor,
                                          kMinPixelsPerSecond, kMaxPixelsPerSecond);
                timelineScrollX = ofClamp(kLabelWidth + secondsAtMouse * pixelsPerSecond - mouseViewportX,
                                          0.0f, maxTimelineScrollX);
            } else if(allowVerticalScroll) {
                ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() - ImGui::GetIO().MouseWheel * 55.0f));
            }
        }
        if(!zoomGesture && std::abs(ImGui::GetIO().MouseWheelH) > 0.001f)
            timelineScrollX = ofClamp(timelineScrollX - ImGui::GetIO().MouseWheelH * 55.0f, 0.0f, maxTimelineScrollX);
    };

    // The ruler (seconds / beats) lives in its own non-scrolling strip so it stays
    // pinned at the top while the tracks below scroll vertically.
    ImGui::BeginChild("##TimelineRulerStrip", ImVec2(0, kRulerHeight), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    handleTimelineWheel(false);
    effectiveRulerSnapBeats = adaptiveSnapDivision(rulerSnapBeats,
                                                   timeline.getBeatsPerBar(),
                                                   transportState.bpm,
                                                   pixelsPerSecond);
    drawRuler(timeline, kLabelWidth, timelineWidth, endBeat, transportState.beatPosition, transportState.bpm);
    ImGui::EndChild();

    ImGui::BeginChild("##TimelineViewport", ImVec2(0, -scrollbarHeight), false,
                      ImGuiWindowFlags_NoScrollWithMouse);
    const float zoneLeft = ImGui::GetWindowPos().x + kLabelWidth;
    handleTimelineWheel(true);
    // The wheel gesture may have changed the scale this frame. Refresh the
    // derived snap before handling timeline input.
    effectiveRulerSnapBeats = adaptiveSnapDivision(rulerSnapBeats,
                                                   timeline.getBeatsPerBar(),
                                                   transportState.bpm,
                                                   pixelsPerSecond);
    if(timeline.isBpmLaneVisible())
        drawBpmLane(timeline, contentWidth, endBeat, transportState.beatPosition, transportState.bpm);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Reset once per frame, before any track's handleClip can set it --
    // unlike the per-track clipInteractionClaimedThisFrame below, this one
    // has to survive across every track's iteration so the "clicked empty
    // space" deselect check after the loop knows whether *any* clip on
    // *any* track already claimed this frame's click.
    anyClipInteractionClaimedThisFrame = false;
    bool emptyTrackAreaClickedThisFrame = false;
    clipScreenRects.clear(); // refilled by drawClip below, for box selection
    clipDropRows.clear();    // refilled per clip row below, for Cmd+drag duplicate

    for(const auto& track : timeline.getTracks()) {
        const float headerY = ImGui::GetCursorPosY();
        // Keep layout state immutable for this frame. Clicking collapse mutates
        // the model, but mixing the old Dummy height with the new branch later
        // in this loop makes SetCursorPos extend the child and asserts in End().
        const bool trackCollapsed = track.collapsed;
        // A Wave Track has no per-binding rows to expand into (see
        // ofxOceanodeTimelineTrack::isWaveTrack) -- it always renders as the
        // same single always-visible clip row a collapsed track would use,
        // regardless of its own collapsed flag.
        const bool singleRowTrack = trackCollapsed || track.isWaveTrack;
        const float trackHeaderHeight = track.isWaveTrack
            ? ofClamp(track.waveTrackHeight, kWaveTrackMinHeight, kWaveTrackMaxHeight)
            : (singleRowTrack ? kCollapsedHeight : kHeaderHeight);
        bool expandTrackForEditor = false;
        ImGui::SetCursorPos(ImVec2(0, headerY));
        // Keep the header as a visual/layout item only. A full-width
        // InvisibleButton steals the overlap from the color button on some
        // ImGui versions, making the picker look permanently disabled.
        ImGui::Dummy(ImVec2(contentWidth, trackHeaderHeight));
        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        if(singleRowTrack) {
            dl->AddRectFilled(headerMin, headerMax, mutedTrackColor(track.color, 0.17f));
            dl->AddRectFilled(headerMin, ImVec2(headerMin.x + kLabelWidth, headerMax.y),
                              mutedTrackColor(track.color, 0.27f, 0.42f));
        } else {
            dl->AddRectFilled(headerMin, headerMax, mutedTrackColor(track.color, 0.27f, 0.42f));
        }
        const float headerTextY = headerMin.y + (trackHeaderHeight - ImGui::GetTextLineHeight()) * 0.5f;
        // A Wave Track has nothing to expand/collapse, so it gets no
        // chevron -- just its name, flush where the chevron would be.
        if(!track.isWaveTrack)
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

        const ImVec2 headerInteractionMax(singleRowTrack ? headerMin.x + kLabelWidth : headerMax.x,
                                          headerMax.y);
        if(hoverRect(headerMin, headerInteractionMax)) {
            if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                pendingTrackId = track.id;
                pendingNewTrackDialog = false;
                std::strncpy(pendingTrackName, track.name.c_str(), sizeof(pendingTrackName) - 1);
                pendingTrackName[sizeof(pendingTrackName) - 1] = '\0';
                requestRenamePopup = true;
            } else if(!track.isWaveTrack && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetIO().MousePos.x < headerMin.x + 22) {
                if(auto* editTrack = timeline.getTrack(track.id)) editTrack->collapsed = !trackCollapsed;
            } else if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                pendingTrackId = track.id;
                pendingWaveClipBeat = transportState.beatPosition;
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
            if(!track.isWaveTrack && ImGui::MenuItem(trackCollapsed ? "Expand track" : "Collapse track")) {
                if(auto* editTrack = timeline.getTrack(track.id)) editTrack->collapsed = !trackCollapsed;
            }
            if(track.isWaveTrack) {
                ImGui::TextDisabled("Volume automation applies to the whole track");
                if(ImGui::MenuItem("Edit track volume automation")) {
                    if(!track.clips.empty()) {
                        timeline.createWaveVolumeLane(track.id, track.clips.front().id);
                        editorTrackId = track.id;
                        editorClipId = track.clips.front().id;
                        editorLaneId.clear();
                        clipEditorOpen = true;
                    foldedClipEditors.erase(editorClipId);
                    }
                }
            }
            if(ImGui::MenuItem("Rename track")) {
                pendingTrackId = track.id;
                pendingNewTrackDialog = false;
                std::strncpy(pendingTrackName, track.name.c_str(), sizeof(pendingTrackName) - 1);
                pendingTrackName[sizeof(pendingTrackName) - 1] = '\0';
                requestRenamePopup = true;
                ImGui::CloseCurrentPopup();
            }
            if(ImGui::MenuItem(track.isWaveTrack ? "Add new wave clip" : "New clip")) {
                if(track.isWaveTrack) {
                    // Wave clips need a file before they are useful, so this
                    // asks for one and creates the clip around it.
                    waveFileRequest = WaveFileRequest::NewClip;
                    waveFileRequestTrackId = track.id;
                    waveFileRequestBeat = pendingWaveClipBeat;
                } else {
                    pendingTrackId = track.id;
                    pendingClipName[0] = '\0';
                    pendingClipBindingId = track.bindings.empty() ? std::string() : track.bindings.front().id;
                    pendingClipLaneType = track.bindings.empty() ? 1 : optionIndexForLaneType(track.bindings.front().laneType);
                    pendingStartBeat = snapBeat(transportState.beatPosition);
                    pendingDurationBeats = timeline.getBeatsPerBar();
                    requestClipPopup = true;
                }
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
            // Only the lines that are on screen.
            const double visibleFrom = beatAtOffset(std::max(0.0f, zoneLeft - min.x));
            const double visibleTo = std::min(endBeat, beatAtOffset(std::max(0.0f, max.x - min.x)) + grid);
            const int firstLine = std::max(0, static_cast<int>(std::floor(visibleFrom / grid)));
            const int lines = static_cast<int>(std::ceil(visibleTo / grid));
            for(int i = firstLine; i <= lines; ++i) {
                const double beat = i * grid;
                const float x = min.x + beatOffset(beat);
                const bool isBar = std::fmod(beat, timeline.getBeatsPerBar()) < 0.001;
                const bool quarter = std::fmod(beat, 1.0) < 0.001;
                if(isBar || quarter || beatOffset(beat + grid) - beatOffset(beat) >= 4.0f)
                    dl->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), isBar ? kBar : quarter ? IM_COL32(92, 92, 92, 145) : kGrid, isBar ? 1.5f : 1.0f);
            }
        };

        auto drawClip = [&](const ofxOceanodeTimelineClip& clip, const ofxOceanodeTimelineLane* lane,
                            const ImVec2& min, const ImVec2& max, bool drawChrome = true,
                            bool drawContent = true) {
            const float x1 = min.x + beatOffset(clip.startBeat);
            const float x2 = min.x + beatOffset(clip.startBeat + clip.durationBeats);
            const float left = std::max(min.x, x1), right = std::min(max.x, x2);
            if(right <= left) return;
            if(drawChrome) {
                clipScreenRects.push_back({track.id, clip.id, left + 1.0f, min.y + 3.0f, right - 1.0f, max.y - 3.0f});
                // Square, no outline; a selected clip is shown by a brighter fill.
                const bool clipSelected = selectedClips.count({track.id, clip.id}) > 0;
                auto lift = [&](unsigned char c) { return static_cast<int>(c + (255 - c) * (clipSelected ? 0.45f : 0.0f)); };
                const ImU32 fill = IM_COL32(lift(track.color.r), lift(track.color.g), lift(track.color.b),
                                            clipSelected ? 235 : (lane == nullptr ? 75 : 185));
                dl->AddRectFilled(ImVec2(left + 1, min.y + 3), ImVec2(right - 1, max.y - 3), fill);
                // A clip's persistent group and its (separate, ephemeral)
                // Shift+click selection are drawn as extra outlines on top
                // of the normal fill/border above, so a grouped clip that's
                // also currently selected shows both at once.
                if(const auto* group = timeline.getGroupForClip(track.id, clip.id)) {
                    // Colour by a hash of the group id so every member of
                    // the same group reads as one thing across every track
                    // it touches, while a second group visible at the same
                    // time gets a visibly different colour instead of
                    // reusing one fixed colour for "grouped".
                    const size_t hash = std::hash<std::string>{}(group->id);
                    const ImU32 groupColor = IM_COL32(180 + (hash % 60), 150 + ((hash >> 8) % 90), 255 - ((hash >> 16) % 110), 235);
                    dl->AddRect(ImVec2(left - 1, min.y + 1), ImVec2(right + 1, max.y - 1), groupColor, 0.0f, 0, 2.5f);
                    dl->AddText(ImVec2(right - 13, min.y + 3), groupColor, "G");
                }
                if(right - left > 45) {
                    std::string label = clip.name;
                    if(clip.isLfo) label += " [LFO]";
                    else if(lane != nullptr) label += " [" + std::string(laneTypeName(lane->type)) + "]";
                    dl->AddText(ImVec2(left + 6, min.y + 6), IM_COL32(245, 250, 255, 255), label.c_str());
                }
            }
            if(!drawContent) return;
            // Repeats that are off screen are not drawn (a short repeating
            // source on a long clip can have thousands of them).
            int firstVisibleCycle = 0;
            int lastVisibleCycle = std::numeric_limits<int>::max();
            if(clip.repeatContent) {
                const double cycleLength = std::max(1e-6, cycleDuration(clip));
                const double visibleFrom = beatAtOffset(left - min.x) - clip.startBeat;
                const double visibleTo = beatAtOffset(right - min.x) - clip.startBeat;
                firstVisibleCycle = std::max(0, static_cast<int>(std::floor(visibleFrom / cycleLength)));
                lastVisibleCycle = static_cast<int>(std::min(1e9, std::ceil(visibleTo / cycleLength)));
            }
            if(track.isWaveTrack) {
                dl->PushClipRect(ImVec2(left + 1.0f, min.y + 3.0f), ImVec2(right - 1.0f, max.y - 3.0f), true);
                if(clip.waveNumChannels > 0 && !clip.waveformPeaks.empty()) {
                    constexpr int kMiniPointsPerChannel = 2000;
                    const double miniContent = sourceDuration(clip);
                    const double fileContent = std::max(miniContent + clip.waveSourceStartBeat,
                        clip.waveFileDurationBeats > 0.0
                            ? clip.waveFileDurationBeats
                            : clip.waveFileDurationMs > 0.0
                            ? clip.waveFileDurationMs * std::max(1.0f, transportState.bpm) / 60000.0
                            : miniContent);
                    const double sourceStart = ofClamp(clip.waveSourceStartBeat / fileContent, 0.0, 1.0);
                    const double sourceSpan = ofClamp(miniContent / fileContent, 0.0, 1.0 - sourceStart);
                    const int miniCycles = clip.repeatContent
                        ? std::max(1, static_cast<int>(std::ceil(clip.durationBeats / cycleDuration(clip)))) : 1;
                    const int channels = std::max(1, clip.waveNumChannels);
                    const float channelHeight = (max.y - min.y - 6.0f) / channels;
                    for(int cycle = firstVisibleCycle; cycle < miniCycles && cycle <= lastVisibleCycle; ++cycle) {
                        for(int ch = 0; ch < channels; ++ch) {
                            const float channelTop = min.y + 3.0f + ch * channelHeight;
                            const float midY = channelTop + channelHeight * 0.5f;
                            const float half = channelHeight * 0.45f;
                            for(int pt = 0; pt < kMiniPointsPerChannel; pt += 4) {
                                const double sourceBeatAtPoint = miniContent * (static_cast<double>(pt) / kMiniPointsPerChannel);
                                const float x = min.x + beatOffset(sourceToTimelineBeat(clip, sourceBeatAtPoint, cycle));
                                if(x < left || x > right) continue;
                                const double normalizedPoint = static_cast<double>(pt) / kMiniPointsPerChannel;
                                const double filePoint = clip.waveReverse
                                    ? sourceStart + (1.0 - normalizedPoint) * sourceSpan
                                    : sourceStart + normalizedPoint * sourceSpan;
                                const int peakPoint = ofClamp(static_cast<int>(filePoint * (kMiniPointsPerChannel - 1)),
                                                              0, kMiniPointsPerChannel - 1);
                                const size_t index = static_cast<size_t>(ch) * kMiniPointsPerChannel + peakPoint;
                                if(index >= clip.waveformPeaks.size()) continue;
                                const float peak = ofClamp(clip.waveformPeaks[index] * clip.waveGain, -1.0f, 1.0f);
                                dl->AddLine(ImVec2(x, midY), ImVec2(x, midY - peak * half), IM_COL32(245, 245, 245, 170), 1.0f);
                            }
                        }
                    }
                }
                dl->PopClipRect();
                return;
            }
            if(clip.isLfo) {
                dl->PushClipRect(ImVec2(left + 1.0f, min.y + 3.0f), ImVec2(right - 1.0f, max.y - 3.0f), true);
                const double content = sourceDuration(clip);
                const int samples = std::max(24, static_cast<int>((right - left) / 3.0f));
                const float graphTop = min.y + 5.0f;
                const float graphBottom = max.y - 5.0f;
                for(int sample = 0; sample < samples; ++sample) {
                    const double source1 = content * sample / static_cast<double>(samples);
                    const double source2 = content * (sample + 1) / static_cast<double>(samples);
                    const float value1 = ofxOceanodeTimelineLfo::evaluate(clip, source1);
                    const float value2 = ofxOceanodeTimelineLfo::evaluate(clip, source2);
                    const float sx1 = min.x + beatOffset(sourceToTimelineBeat(clip, source1, 0));
                    const float sx2 = min.x + beatOffset(sourceToTimelineBeat(clip, source2, 0));
                    dl->AddLine(ImVec2(sx1, graphBottom - value1 * (graphBottom - graphTop)),
                                ImVec2(sx2, graphBottom - value2 * (graphBottom - graphTop)),
                                IM_COL32(245, 245, 245, 200), 1.5f);
                }
                dl->PopClipRect();
                return;
            }
            if(lane == nullptr) return;
            if(lane->type == ofxOceanodeTimelineLaneType::Curve) {
                const auto interpolation = curveInterpolationMode(lane->curveInterpolation);
                const auto& points = lane->curvePoints; // kept sorted by beat
                const int cycles = clip.repeatContent ? static_cast<int>(std::ceil(clip.durationBeats / cycleDuration(clip))) : 1;
                dl->PushClipRect(ImVec2(left + 1.0f, min.y + 3.0f), ImVec2(right - 1.0f, max.y - 3.0f), true);
                for(int cycle = firstVisibleCycle; cycle < cycles && cycle <= lastVisibleCycle; ++cycle) {
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
                for(int cycle = firstVisibleCycle; cycle < cycles && cycle <= lastVisibleCycle; ++cycle) {
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
            if(lane->type == ofxOceanodeTimelineLaneType::MultiSlider) {
                const double miniPatternLength = std::max(1.0 / kPPQ, lane->stepCount * lane->beatsPerStep);
                const double miniContent = sourceDuration(clip);
                const int miniClipCycles = clip.repeatContent
                    ? std::max(1, static_cast<int>(std::ceil(clip.durationBeats / cycleDuration(clip)))) : 1;
                const int miniPatternCycles = std::max(1, static_cast<int>(std::ceil(miniContent / miniPatternLength)));
                const float span = lane->valueMax - lane->valueMin;
                dl->PushClipRect(ImVec2(left + 1.0f, min.y + 3.0f), ImVec2(right - 1.0f, max.y - 3.0f), true);
                for(int clipCycle = firstVisibleCycle; clipCycle < miniClipCycles && clipCycle <= lastVisibleCycle; ++clipCycle) {
                    for(int patternCycle = 0; patternCycle < miniPatternCycles; ++patternCycle) {
                        for(int index = 0; index < lane->stepCount && index < static_cast<int>(lane->multiSliderValues.size()); ++index) {
                            const double sourceStart = patternCycle * miniPatternLength + index * lane->beatsPerStep;
                            if(sourceStart >= miniContent) continue;
                            const float x1 = min.x + beatOffset(sourceToTimelineBeat(clip, sourceStart, clipCycle));
                            const float x2 = min.x + beatOffset(sourceToTimelineBeat(clip, sourceStart + lane->beatsPerStep, clipCycle));
                            const float normalized = std::abs(span) < 1e-9f ? 0.0f
                                : ofClamp((lane->multiSliderValues[index] - lane->valueMin) / span, 0.0f, 1.0f);
                            const float barTop = max.y - 5.0f - normalized * (max.y - min.y - 10.0f);
                            dl->AddRectFilled(ImVec2(x1 + 1, barTop), ImVec2(x2 - 1, max.y - 5.0f), IM_COL32(245, 245, 245, 190));
                        }
                    }
                }
                dl->PopClipRect();
                return;
            }
            if(lane->type == ofxOceanodeTimelineLaneType::MultiValue || lane->type == ofxOceanodeTimelineLaneType::MultiGate) {
                const bool isValueRow = lane->type == ofxOceanodeTimelineLaneType::MultiValue;
                const int rowCount = std::max(1, lane->multiRowCount);
                const int miniCycles = clip.repeatContent
                    ? std::max(1, static_cast<int>(std::ceil(clip.durationBeats / cycleDuration(clip)))) : 1;
                const float rowHeight = std::max(2.0f, (max.y - min.y - 10.0f) / rowCount);
                dl->PushClipRect(ImVec2(left + 1.0f, min.y + 3.0f), ImVec2(right - 1.0f, max.y - 3.0f), true);
                for(int r = 0; r < rowCount; ++r) {
                    const float rTop = min.y + 5.0f + r * rowHeight;
                    const float rBottom = rTop + rowHeight - 1.0f;
                    for(int cycle = firstVisibleCycle; cycle < miniCycles && cycle <= lastVisibleCycle; ++cycle) {
                        if(isValueRow && r < static_cast<int>(lane->multiValueRows.size())) {
                            for(const auto& region : lane->multiValueRows[r]) {
                                const float x1 = min.x + beatOffset(sourceToTimelineBeat(clip, region.startBeat, cycle));
                                const float x2 = min.x + beatOffset(sourceToTimelineBeat(clip, region.end(), cycle));
                                dl->AddRectFilled(ImVec2(x1, rTop), ImVec2(x2, rBottom), IM_COL32(245, 245, 245, 170));
                            }
                        } else if(!isValueRow && r < static_cast<int>(lane->multiGateRows.size())) {
                            for(const auto& region : lane->multiGateRows[r]) {
                                const float x1 = min.x + beatOffset(sourceToTimelineBeat(clip, region.startBeat, cycle));
                                const float x2 = min.x + beatOffset(sourceToTimelineBeat(clip, region.end(), cycle));
                                dl->AddRectFilled(ImVec2(x1, rTop), ImVec2(x2, rBottom), IM_COL32(245, 245, 245, 170));
                            }
                        }
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
            for(int clipCycle = firstVisibleCycle; clipCycle < clipCycles && clipCycle <= lastVisibleCycle; ++clipCycle) {
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
            if(!hoverRect(min, max) || mouse.x < x1 || mouse.x > x2) return;
            const std::string menuId = "##clipMenu" + track.id + "_" + clip.id;
            const std::pair<std::string, std::string> memberKey(track.id, clip.id);
            if(clipDragMode == ClipDragMode::None) {
                const float hoverEdgeZone = std::min(kEdgePixels, (x2 - x1) * 0.25f);
                if(mouse.x >= x2 - hoverEdgeZone || mouse.x <= x1 + hoverEdgeZone)
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                if(clipInteractionClaimedThisFrame) return;
                clipInteractionClaimedThisFrame = true;
                anyClipInteractionClaimedThisFrame = true;
                pendingTrackId = track.id;
                pendingClipId = clip.id;
                pendingLaneId = lane != nullptr ? lane->id : (clip.lanes.empty() ? "" : clip.lanes.front().id);
                clipMenuBeat = snapBeat(beatAtOffset(mouse.x - min.x)); // "Split here"
                // Right-clicking a clip that's part of the current multi-
                // selection keeps that whole selection (so the popup's
                // "Group" action groups all of it); right-clicking outside
                // it collapses the selection down to just this clip first,
                // the same convention the left-click branch below uses.
                if(selectedClips.count(memberKey) == 0 || selectedClips.size() <= 1) {
                    selectedClips.clear();
                    selectedClips.insert(memberKey);
                }
                ImGui::OpenPopup(menuId.c_str());
            }
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if(clipInteractionClaimedThisFrame || clipDragMode != ClipDragMode::None) return;
                // Shift is already the edge-drag modifier for Resize below,
                // so Shift+click only takes over the *non-edge* case: a
                // pure multi-selection toggle, not the start of any drag.
                // Edge zones shrink on narrow clips so a short clip can still be moved.
                const float edgeZone = std::min(kEdgePixels, (x2 - x1) * 0.25f);
                const bool edge = mouse.x >= x2 - edgeZone;
                // The left edge trims; with Shift it can also extend the
                // clip to the left (adding empty time in front).
                const bool leftEdge = !edge && mouse.x <= x1 + edgeZone;
                if(ImGui::GetIO().KeyShift && !edge && !leftEdge) {
                    clipInteractionClaimedThisFrame = true;
                    anyClipInteractionClaimedThisFrame = true;
                    if(selectedClips.count(memberKey) > 0) selectedClips.erase(memberKey);
                    else selectedClips.insert(memberKey);
                    return;
                }
                clipInteractionClaimedThisFrame = true;
                anyClipInteractionClaimedThisFrame = true;
                pendingTrackId = track.id;
                pendingClipId = clip.id;
                pendingLaneId = lane != nullptr ? lane->id : (clip.lanes.empty() ? "" : clip.lanes.front().id);
                const double clickedBeat = beatAtOffset(mouse.x - min.x);
                const bool editorShowingThisClip = clipEditorOpen && editorTrackId == track.id &&
                    editorClipId == clip.id && foldedClipEditors.count(clip.id) == 0;
                if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && editorShowingThisClip) {
                    // Double-clicking the clip whose editor is open closes it again.
                    clipEditorOpen = false;
                } else if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    editorTrackId = track.id;
                    editorClipId = clip.id;
                    editorLaneId = lane != nullptr ? lane->id : std::string();
                    editorAnchorBindingId = currentRowBindingId;
                    clipEditorOpen = true;
                    foldedClipEditors.erase(editorClipId);
                    if(singleRowTrack) {
                        // Layout stays single-row for the rest of this frame,
                        // but the model expands now (irrelevant for a Wave
                        // Track, which stays single-row regardless -- see
                        // singleRowTrack above) so the editor is docked
                        // directly below the track on the next frame.
                        if(!track.isWaveTrack) {
                            if(auto* editTrack = timeline.getTrack(track.id)) editTrack->collapsed = false;
                        }
                        expandTrackForEditor = true;
                    }
                    pendingStartBeat = snapBeat(std::max(0.0, clickedBeat - clip.startBeat));
                    pendingDurationBeats = 1.0;
                } else {
                    // A plain click on a clip outside the current multi-
                    // selection starts a fresh, single-clip selection;
                    // clicking a clip that's already part of a bigger
                    // selection keeps the whole thing so it can be dragged
                    // together below instead of collapsing mid-drag.
                    if(selectedClips.count(memberKey) == 0 || selectedClips.size() <= 1) {
                        selectedClips.clear();
                        selectedClips.insert(memberKey);
                    }
                    // On macOS Cmd is the stretch modifier. Ctrl is reserved
                    // by the canvas/ImGui interaction layer and can turn a
                    // drag into a context-menu gesture.
                    // ImGui deliberately maps macOS Cmd to its logical Ctrl
                    // modifier (and physical Ctrl to Super). Using KeySuper
                    // here was why Cmd+drag never selected Stretch.
                    const bool commandDown = ImGui::GetIO().KeyCtrl ||
                        (ImGui::GetIO().KeyMods & ImGuiMod_Ctrl) != 0;
                    // Wave clips are complete audio slices: every edge drag
                    // (plain, Cmd or Shift) stretches the whole source.
                    // Cmd on the body (not on an edge) drags out a copy instead.
                    clipDragMode = edge ? (track.isWaveTrack ? ClipDragMode::Stretch
                                           : ImGui::GetIO().KeyShift ? ClipDragMode::Resize
                                           : commandDown ? ClipDragMode::Stretch
                                                         : ClipDragMode::Repeat)
                                        : leftEdge ? ClipDragMode::TrimStart
                                        : commandDown ? ClipDragMode::Duplicate
                                        : ClipDragMode::Move;
                    clipDragStartMouseX = mouse.x;
                    clipDragStartMouseY = mouse.y;
                    trimExtends = leftEdge && ImGui::GetIO().KeyShift;
                    dragSourceBindingId = currentRowBindingId;
                    dragSourceLaneId = lane != nullptr ? lane->id : (clip.lanes.empty() ? std::string() : clip.lanes.front().id);
                    clipDragCommitted = false;
                    trimPreviewBeat = clip.startBeat;
                    dragRowTop = min.y;
                    dragRowBottom = max.y;
                    draggingTrackId = track.id;
                    draggingClipId = clip.id;
                    dragOffsetBeats = clickedBeat - clip.startBeat;
                    dragInitialContentDuration = clip.contentDurationBeats;
                    dragInitialContentStretch = clip.contentStretch;
                    dragTimelineOriginX = min.x;

                    // A clip that's grouped, or that's part of a multi-
                    // selection bigger than itself, drags/stretches as one
                    // unit: snapshot every member's current timing now so
                    // the whole set is scaled from this one shared
                    // reference each frame (see groupDragSnapshot's
                    // declaration in the header for why).
                    groupDragSnapshot.clear();
                    std::vector<std::pair<std::string, std::string>> dragMembers;
                    if(const auto* group = timeline.getGroupForClip(track.id, clip.id)) {
                        dragMembers = group->members;
                    } else if(selectedClips.count(memberKey) > 0 && selectedClips.size() > 1) {
                        dragMembers.assign(selectedClips.begin(), selectedClips.end());
                    }
                    if(dragMembers.size() > 1) {
                        double anchorBeat = clip.startBeat;
                        for(const auto& member : dragMembers) {
                            if(const auto* memberClip = timeline.getClip(member.first, member.second)) {
                                groupDragSnapshot.push_back({member.first, member.second, memberClip->startBeat,
                                    memberClip->durationBeats, memberClip->contentDurationBeats, memberClip->contentStretch});
                                anchorBeat = std::min(anchorBeat, memberClip->startBeat);
                            }
                        }
                        groupDragAnchorBeat = anchorBeat;
                    }
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
            if(ImGui::MenuItem("Rename clip")) {
                requestClipRename(track.id, clip);
                ImGui::CloseCurrentPopup();
            }
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
                if(!track.isWaveTrack &&
                   ImGui::InputDouble("Content", &contentDuration, editIncrement(), 1.0, "%.3f"))
                    timeline.setClipContentDuration(track.id, clip.id,
                                                    std::max(1.0 / kPPQ, snapBeat(contentDuration)),
                                                    editClip->repeatContent);
                if(track.isWaveTrack) {
                    ImGui::TextDisabled("Wave clips always play their full source when resized.");
                } else {
                    bool repeat = editClip->repeatContent;
                    if(ImGui::Checkbox("Repeat content", &repeat))
                        timeline.setClipContentDuration(track.id, clip.id, editClip->contentDurationBeats, repeat);
                }
                ImGui::Separator();
                if(ImGui::MenuItem("Consolidate content"))
                    timeline.consolidateClipContent(track.id, clip.id);
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("Permanently remove source data hidden beyond the clip's right edge");
                ImGui::Separator();
            }
            auto* selectedLane = timeline.getLane(track.id, clip.id, pendingLaneId);
            if(!track.isWaveTrack) {
                // Any automation or LFO clip can be cut in two at the playhead (S).
                const double playheadBeat = container->getTransportState().beatPosition;
                const bool playheadInside = playheadBeat > clip.startBeat + 1.0 / 96.0 &&
                    playheadBeat < clip.startBeat + clip.durationBeats - 1.0 / 96.0;
                const bool mouseInside = clipMenuBeat > clip.startBeat + 1.0 / 96.0 &&
                    clipMenuBeat < clip.startBeat + clip.durationBeats - 1.0 / 96.0;
                if(ImGui::MenuItem("Split here", nullptr, false, mouseInside)) {
                    requestClipSplit = true;
                    clipSplitTrackId = track.id;
                    clipSplitClipId = clip.id;
                    clipSplitBeat = clipMenuBeat;
                    ImGui::CloseCurrentPopup();
                }
                if(ImGui::MenuItem("Split at playhead", "S", false, playheadInside)) {
                    requestClipSplit = true;
                    clipSplitTrackId = track.id;
                    clipSplitClipId = clip.id;
                    clipSplitBeat = playheadBeat;
                    ImGui::CloseCurrentPopup();
                }
                if(!playheadInside && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Move the playhead inside the clip to split it there");
                ImGui::Separator();
            }
            if(track.isWaveTrack) {
                const double playheadBeat = container->getTransportState().beatPosition;
                const bool mouseInside = clipMenuBeat > clip.startBeat + 1.0 / 96.0 &&
                    clipMenuBeat < clip.startBeat + clip.durationBeats - 1.0 / 96.0;
                const bool playheadInside = playheadBeat > clip.startBeat + 1.0 / 96.0 &&
                    playheadBeat < clip.startBeat + clip.durationBeats - 1.0 / 96.0;
                if(ImGui::MenuItem("Split here", nullptr, false, mouseInside)) {
                    requestWaveSplit = true;
                    waveSplitTrackId = track.id;
                    waveSplitClipId = clip.id;
                    clipSplitBeat = clipMenuBeat;
                    ImGui::CloseCurrentPopup();
                }
                if(ImGui::MenuItem("Split at playhead", "S", false, playheadInside)) {
                    requestWaveSplit = true;
                    waveSplitTrackId = track.id;
                    waveSplitClipId = clip.id;
                    clipSplitBeat = playheadBeat;
                    ImGui::CloseCurrentPopup();
                }
                auto* menuEditClip = timeline.getClip(track.id, clip.id);
                bool reverse = menuEditClip != nullptr && menuEditClip->waveReverse;
                if(ImGui::MenuItem("Reverse", nullptr, reverse) && menuEditClip != nullptr)
                    menuEditClip->waveReverse = !reverse;
                ImGui::Separator();
                if(ImGui::MenuItem("Edit track volume automation")) {
                    timeline.createWaveVolumeLane(track.id, clip.id);
                    editorTrackId = track.id;
                    editorClipId = clip.id;
                    editorLaneId.clear();
                    clipEditorOpen = true;
                    foldedClipEditors.erase(editorClipId);
                }
            } else if(clip.isLfo) {
                // An LFO clip's lanes are its oscillator controls (frequency,
                // skew, pulse width, ...), not automation lanes the user
                // points at a parameter. Retyping one, or adding a sibling to
                // it, would quietly break the clip, so this menu offers none
                // of that -- the LFO editor (double-click the clip) is where
                // it is edited.
                ImGui::TextDisabled("LFO clip -- double-click to edit");
            } else {
                const auto selectedType = selectedLane == nullptr ? ofxOceanodeTimelineLaneType::Step : selectedLane->type;
                for(int optionIndex = 0; optionIndex < kLaneTypeOptionCount; ++optionIndex) {
                    const auto candidateType = laneTypeFromOptionIndex(optionIndex);
                    if(ImGui::MenuItem(kLaneTypeOptions[optionIndex], nullptr, selectedType == candidateType) && selectedLane != nullptr)
                        timeline.setClipLaneType(track.id, clip.id, selectedLane->id, candidateType);
                }
            }
            if(selectedLane != nullptr && !clip.isLfo && !track.bindings.empty() && ImGui::BeginMenu("Add parameter to this lane")) {
                for(const auto& candidate : track.bindings) {
                    const bool assigned = std::find(selectedLane->bindingIds.begin(), selectedLane->bindingIds.end(), candidate.id) != selectedLane->bindingIds.end();
                    if(ImGui::MenuItem(candidate.parameterPath.c_str(), nullptr, assigned, true)) {
                        if(assigned) timeline.removeBindingFromLane(track.id, clip.id, selectedLane->id, candidate.id);
                        else timeline.addBindingToLane(track.id, clip.id, selectedLane->id, candidate.id);
                    }
                }
                ImGui::EndMenu();
            }
            if(!clip.isLfo && !track.bindings.empty() && ImGui::BeginMenu("New lane")) {
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
            // handleClip already made sure this clip's own key is part of
            // selectedClips by the time either popup that opens this menu
            // (this one, or the binding-row menu) is shown, so "Group"
            // always groups the clip(s) actually selected right now.
            if(selectedClips.size() >= 2 && selectedClips.count({track.id, clip.id}) > 0) {
                const std::string groupLabel = "Group " + ofToString(selectedClips.size()) + " clips";
                if(ImGui::MenuItem(groupLabel.c_str())) {
                    timeline.groupClips(std::vector<std::pair<std::string, std::string>>(
                        selectedClips.begin(), selectedClips.end()));
                    ImGui::CloseCurrentPopup();
                }
            }
            if(timeline.getGroupForClip(track.id, clip.id) != nullptr) {
                if(ImGui::MenuItem("Ungroup")) {
                    timeline.ungroupClip(track.id, clip.id);
                    ImGui::CloseCurrentPopup();
                }
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

        if(singleRowTrack) {
            const ImVec2 min = headerMin, max = headerMax, laneMin(min.x + kLabelWidth - timelineScrollX, min.y);
            dl->PushClipRect(ImVec2(zoneLeft, min.y), ImVec2(max.x, max.y), true);
            drawGrid(laneMin, max);
            // Two passes: every clip's box/label first, then every lane's content on
            // top, so overlapping clips' translucent boxes never dim curves drawn
            // earlier -- all curves in a collapsed track read equally bright.
            for(const auto& clip : track.clips) {
                drawClip(clip, clip.lanes.empty() ? nullptr : &clip.lanes.front(), laneMin, max, true, false);
            }
            for(const auto& clip : track.clips) {
                if(clip.lanes.empty() || clip.isLfo || track.isWaveTrack) {
                    // An LFO clip's lanes are its oscillator controls, not
                    // separate curves: its waveform is drawn once (drawing it
                    // per lane stacked it into a brighter, thicker line).
                    drawClip(clip, clip.lanes.empty() ? nullptr : &clip.lanes.front(), laneMin, max, false, true);
                } else {
                    for(size_t laneIndex = 0; laneIndex < clip.lanes.size(); ++laneIndex) {
                        drawClip(clip, &clip.lanes[laneIndex], laneMin, max, false, true);
                    }
                }
            }
            // Later clips are drawn on top, so hit-test in reverse order and
            // let the visually topmost overlapping clip own the gesture.
            clipDropRows.push_back({track.id, std::string(), min.y, max.y});
            for(auto clipIt = track.clips.rbegin(); clipIt != track.clips.rend(); ++clipIt) {
                const auto* frontLane = clipIt->lanes.empty() ? nullptr : &clipIt->lanes.front();
                handleClip(*clipIt, frontLane, laneMin, max);
            }
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
               !clipInteractionClaimedThisFrame &&
               hoverRect(min, max) &&
               ImGui::GetIO().MousePos.x >= zoneLeft &&
               !(track.isWaveTrack && ImGui::GetIO().MousePos.y >= max.y - kWaveTrackResizeHandleHeight)) {
                emptyTrackAreaClickedThisFrame = true;
            }
            // Wave tracks have no binding-row item to own the empty timeline
            // area. Let a right-click anywhere in that row open the track
            // menu, with the file clip placed at the clicked beat. A clip
            // that was actually hit has already claimed the gesture above
            // and keeps its own clip menu.
            if(track.isWaveTrack && !clipInteractionClaimedThisFrame &&
               hoverRect(min, max) &&
               ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
               ImGui::GetIO().MousePos.x >= zoneLeft) {
                pendingTrackId = track.id;
                pendingWaveClipBeat = snapBeat(std::max(0.0,
                    beatAtOffset(ImGui::GetIO().MousePos.x - laneMin.x)));
                ImGui::OpenPopup(("##trackMenu" + track.id).c_str());
            }
            for(const auto& clip : track.clips) {
                drawClipMenu(clip);
            }
            const float px = laneMin.x + beatOffset(transportState.beatPosition);
            if(px >= zoneLeft && px <= max.x) dl->AddLine(ImVec2(px, min.y), ImVec2(px, max.y), kPlayhead, 2);
            dl->PopClipRect();
            if(track.isWaveTrack) {
                const ImVec2 resizeMin(headerMin.x, headerMax.y - kWaveTrackResizeHandleHeight);
                const ImVec2 resizeMax(headerMax.x, headerMax.y);
                const bool resizeHovered = hoverRect(resizeMin, resizeMax);
                if(resizeHovered || waveTrackResizeId == track.id) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    dl->AddLine(ImVec2(headerMin.x, headerMax.y - 1.0f),
                                ImVec2(headerMax.x, headerMax.y - 1.0f),
                                resizeHovered || waveTrackResizeId == track.id
                                    ? IM_COL32(220, 220, 245, 220)
                                    : IM_COL32(150, 150, 170, 150), 2.0f);
                }
                if(resizeHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    waveTrackResizeId = track.id;
                    waveTrackResizeStartMouseY = ImGui::GetIO().MousePos.y;
                    waveTrackResizeStartHeight = trackHeaderHeight;
                }
                if(waveTrackResizeId == track.id) {
                    if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        if(auto* editTrack = timeline.getTrack(track.id)) {
                            editTrack->waveTrackHeight = ofClamp(
                                waveTrackResizeStartHeight +
                                    (ImGui::GetIO().MousePos.y - waveTrackResizeStartMouseY),
                                kWaveTrackMinHeight, kWaveTrackMaxHeight);
                        }
                    }
                    if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) waveTrackResizeId.clear();
                }
            }
            ImGui::SetCursorPosY(headerY + trackHeaderHeight);
            if(track.isWaveTrack) {
                if(clipEditorOpen && editorTrackId == track.id) {
                    editorScreenTop = ImGui::GetCursorScreenPos().y;
                    drawEditorSeparator(track, contentWidth);
                    drawWaveTrackEditor(timeline, track, contentWidth, endBeat, transportState.beatPosition);
                    editorScreenBottom = ImGui::GetCursorScreenPos().y;
                    editorScreenValid = true;
                }
            } else if(clipEditorOpen && editorTrackId == track.id && !expandTrackForEditor) {
                // A collapsed track hides its rows entirely, so any clip editor
                // still open for it would float disconnected from what it's
                // editing -- close it rather than keep drawing it.
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
            // Each movable unit (a parameter row, or a note group with its
            // member rows) with its screen extent, for reordering.
            struct RowUnit {
                std::vector<std::string> bindingIds;
                float top = 0.0f;
                float bottom = 0.0f;
            };
            std::vector<RowUnit> rowUnits;
            // Model changes requested from row menus are applied after the
            // rows are drawn (not while iterating them).
            int deferredMoveUnitFrom = -1;
            int deferredMoveUnitTo = -1;
            std::string deferredGroupId;
            enum class DeferredGroupAction { None, Ungroup, Remove, NewClip, LeaveGroup };
            DeferredGroupAction deferredGroupAction = DeferredGroupAction::None;
            std::string deferredLeaveBindingId;
            double deferredClipBeat = 0.0;
            // Row height drag (applied to the model before this frame's layout).
            if(rowResizeTrackId == track.id) {
                if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    const float newHeight = ofClamp(rowResizeStartHeight + ImGui::GetIO().MousePos.y - rowResizeStartY, 20.0f, 300.0f);
                    if(auto* editTrack = timeline.getTrack(track.id)) {
                        for(auto& editBinding : editTrack->bindings)
                            if(editBinding.id == rowResizeBindingId) editBinding.rowHeight = newHeight;
                        for(auto& editGroup : editTrack->noteGroups)
                            if(editGroup.id == rowResizeBindingId) editGroup.rowHeight = newHeight;
                    }
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                } else {
                    rowResizeTrackId.clear();
                    rowResizeBindingId.clear();
                }
            }
            // Which row the open clip editor docks under: the row it was
            // opened from, else the first row automated by the edited clip.
            const bool editorOnThisTrack = clipEditorOpen && editorTrackId == track.id;
            std::string editorAnchorId;
            if(editorOnThisTrack) {
                auto hasBinding = [&](const std::string& id) {
                    return !id.empty() && std::any_of(track.bindings.begin(), track.bindings.end(),
                                                      [&](const auto& b) { return b.id == id; });
                };
                if(hasBinding(editorAnchorBindingId)) editorAnchorId = editorAnchorBindingId;
                else {
                    const auto clipIt = std::find_if(track.clips.begin(), track.clips.end(),
                        [&](const auto& c) { return c.id == editorClipId; });
                    if(clipIt != track.clips.end()) {
                        for(const auto& b : track.bindings) {
                            if(laneForBinding(*clipIt, b.id) != nullptr) { editorAnchorId = b.id; break; }
                        }
                    }
                }
            }
            bool editorDrawn = false;
            std::pair<float, float> editorSpanScreen{0.0f, 0.0f};
            float rowY = headerY + kHeaderHeight;
            for(const auto& binding : track.bindings) {
                const auto* noteGroup = ofxOceanodeTimelineManager::findNoteGroupForBinding(track, binding.id);
                const auto groupMembers = noteGroup != nullptr ? noteGroup->members() : std::vector<std::string>();
                const bool firstGroupMember = noteGroup != nullptr && groupMembers.front() == binding.id;
                const bool lastGroupMember = noteGroup != nullptr && groupMembers.back() == binding.id;
                const bool anchorIsHere = noteGroup != nullptr
                    ? (lastGroupMember && noteGroup->contains(editorAnchorId))
                    : binding.id == editorAnchorId;
                auto dockEditorHere = [&]() {
                    if(!editorOnThisTrack || editorDrawn || !anchorIsHere) return;
                    // Dock the clip editor directly under the row it edits;
                    // the remaining rows continue below it.
                    ImGui::SetCursorPos(ImVec2(0, rowY));
                    const float editorTopScreen = ImGui::GetCursorScreenPos().y;
                    drawEditorSeparator(track, contentWidth);
                    drawLaneEditor(timeline, track, contentWidth, endBeat, transportState.beatPosition);
                    rowY = ImGui::GetCursorPosY();
                    editorSpanScreen = {editorTopScreen, ImGui::GetCursorScreenPos().y};
                    editorScreenTop = editorSpanScreen.first;
                    editorScreenBottom = editorSpanScreen.second;
                    editorScreenValid = true;
                    editorDrawn = true;
                };
                if(firstGroupMember) {
                    // ---- Note group row: one row for a piano roll's pitch/gate/velocity ----
                    const auto& group = *noteGroup;
                    const float groupHeight = ofClamp(group.rowHeight, 20.0f, 300.0f);
                    ImGui::SetCursorPos(ImVec2(0, rowY));
                    ImGui::InvisibleButton(("##noteGroup" + track.id + group.id).c_str(), ImVec2(contentWidth, groupHeight));
                    rowY += groupHeight;
                    const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax(), laneMin(min.x + kLabelWidth - timelineScrollX, min.y);
                    const ImVec2 labelMin(min.x + kParameterRowIndent, min.y); // rows sit indented under their track
                    rowUnits.push_back({groupMembers, min.y, max.y});
                    constexpr float kGroupResizeGrip = 5.0f;
                    const ImVec2 gripMin(labelMin.x, max.y - kGroupResizeGrip), gripMax(min.x + kLabelWidth, max.y);
                    const bool popupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
                    const bool gripHovered = rowResizeTrackId.empty() && bindingDragTrackId.empty() && !popupOpen &&
                        hoverRect(gripMin, gripMax);
                    dl->AddRectFilled(labelMin, max, mutedTrackColor(track.color, 0.17f));
                    dl->PushClipRect(ImVec2(zoneLeft, min.y), ImVec2(max.x, max.y), true);
                    drawGrid(laneMin, max);
                    dl->PopClipRect();
                    dl->AddRectFilled(labelMin, ImVec2(min.x + kLabelWidth, max.y), mutedTrackColor(track.color, 0.38f, 0.55f));
                    dl->AddLine(ImVec2(labelMin.x, max.y - 0.5f), ImVec2(min.x + kLabelWidth, max.y - 0.5f), IM_COL32(0, 0, 0, 140), 1.0f); // row divider, on the resize edge
                    // Expand arrow: shows the member parameter rows underneath.
                    const ImVec2 arrowMin(labelMin.x + 2.0f, min.y + 2.0f), arrowMax(labelMin.x + 20.0f, min.y + 24.0f);
                    const bool arrowHovered = !popupOpen && hoverRect(arrowMin, arrowMax);
                    const ImU32 arrowColor = arrowHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(215, 220, 230, 230);
                    const ImVec2 arrowCenter(labelMin.x + 11.0f, min.y + 13.0f);
                    if(group.expanded)
                        dl->AddTriangleFilled(ImVec2(arrowCenter.x - 5, arrowCenter.y - 3), ImVec2(arrowCenter.x + 5, arrowCenter.y - 3),
                                              ImVec2(arrowCenter.x, arrowCenter.y + 4), arrowColor);
                    else
                        dl->AddTriangleFilled(ImVec2(arrowCenter.x - 3, arrowCenter.y - 5), ImVec2(arrowCenter.x - 3, arrowCenter.y + 5),
                                              ImVec2(arrowCenter.x + 4, arrowCenter.y), arrowColor);
                    const bool arrowClicked = arrowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                    if(arrowClicked) {
                        if(auto* editGroup = timeline.getNoteGroup(track.id, group.id)) editGroup->expanded = !editGroup->expanded;
                    }
                    // Name, and which parameters it drives.
                    const float labelRight = min.x + kLabelWidth - 6.0f;
                    dl->PushClipRect(ImVec2(labelMin.x + 20.0f, min.y), ImVec2(labelRight, max.y), true);
                    dl->AddText(ImVec2(labelMin.x + 22.0f, min.y + 6.0f), IM_COL32(245, 250, 255, 255), group.name.c_str());
                    struct RoleTag { const char* label; const std::string* id; };
                    const RoleTag roleTags[3] = {{"pitch", &group.pitchBindingId}, {"gate", &group.gateBindingId}, {"vel", &group.velocityBindingId}};
                    const bool tagsOnOwnLine = groupHeight >= 40.0f;
                    float tagX = tagsOnOwnLine ? labelMin.x + 22.0f : labelMin.x + 30.0f + ImGui::CalcTextSize(group.name.c_str()).x;
                    const float tagY = tagsOnOwnLine ? min.y + 23.0f : min.y + 6.0f;
                    for(const auto& tag : roleTags) {
                        if(tag.id->empty()) continue;
                        const auto* member = timeline.getBinding(track.id, *tag.id);
                        const bool missing = member == nullptr || member->missingTarget;
                        const ImVec2 textSize = ImGui::CalcTextSize(tag.label);
                        const ImVec2 chipMin(tagX, tagY - 1.0f), chipMax(tagX + textSize.x + 8.0f, tagY + textSize.y + 1.0f);
                        dl->AddRectFilled(chipMin, chipMax, missing ? IM_COL32(170, 50, 50, 220) : IM_COL32(20, 20, 24, 120), 3.0f);
                        dl->AddText(ImVec2(tagX + 4.0f, tagY), missing ? IM_COL32(255, 225, 225, 255) : IM_COL32(215, 222, 235, 235), tag.label);
                        tagX = chipMax.x + 4.0f;
                    }
                    dl->PopClipRect();
                    const bool labelHovered = !popupOpen && hoverRect(min, ImVec2(min.x + kLabelWidth, max.y));
                    if(labelHovered && !arrowHovered && !gripHovered && rowResizeTrackId.empty() && bindingDragTrackId.empty()) {
                        std::string tip = group.name + " (piano roll)";
                        const char* roleNames[3] = {"Pitch", "Gate", "Velocity"};
                        for(int role = 0; role < 3; ++role) {
                            const auto* member = roleTags[role].id->empty() ? nullptr : timeline.getBinding(track.id, *roleTags[role].id);
                            tip += std::string("\n") + roleNames[role] + ": " +
                                (member == nullptr ? std::string("none") : member->parameterPath + (member->missingTarget ? "  (missing)" : ""));
                        }
                        tip += "\nRight-click for targets, clips and more";
                        ImGui::SetTooltip("%s", tip.c_str());
                    }
                    // Resize from the bottom edge (double-click: default height).
                    if(gripHovered || (rowResizeTrackId == track.id && rowResizeBindingId == group.id)) {
                        dl->AddLine(ImVec2(gripMin.x, max.y - 1.5f), ImVec2(gripMax.x, max.y - 1.5f), IM_COL32(255, 210, 90, 220), 3.0f);
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    }
                    if(gripHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        rowResizeTrackId = track.id;
                        rowResizeBindingId = group.id;
                        rowResizeStartY = ImGui::GetIO().MousePos.y;
                        rowResizeStartHeight = groupHeight;
                    }
                    if(gripHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        if(auto* editGroup = timeline.getNoteGroup(track.id, group.id)) editGroup->rowHeight = 48.0f;
                        rowResizeTrackId.clear();
                        rowResizeBindingId.clear();
                    }
                    // Press on the name and drag vertically to move the whole group.
                    if(track.bindings.size() > groupMembers.size() && labelHovered && !arrowHovered && !gripHovered &&
                       rowResizeTrackId.empty() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        bindingDragTrackId = track.id;
                        bindingDragId = groupMembers.front();
                        bindingDragStartY = ImGui::GetIO().MousePos.y;
                        bindingDragActive = false;
                    }
                    const std::string groupMenuId = "##noteGroupMenu" + track.id + group.id;
                    if(hoverRect(min, max) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        const bool overTimelineZone = ImGui::GetIO().MousePos.x >= zoneLeft;
                        const double clickedBeat = overTimelineZone ? beatAtOffset(ImGui::GetIO().MousePos.x - laneMin.x) : 0.0;
                        bool overClip = false;
                        if(overTimelineZone) {
                            for(const auto& clip : track.clips) {
                                for(const auto& lane : clip.lanes) {
                                    if(lane.type != ofxOceanodeTimelineLaneType::PianoRoll) continue;
                                    if(std::none_of(lane.bindingIds.begin(), lane.bindingIds.end(),
                                                    [&](const auto& id) { return group.contains(id); })) continue;
                                    if(clickedBeat >= clip.startBeat && clickedBeat <= clip.startBeat + clip.durationBeats) overClip = true;
                                }
                            }
                        }
                        if(!overClip) {
                            pendingStartBeat = snapBeat(std::max(0.0, overTimelineZone ? clickedBeat : transportState.beatPosition));
                            ImGui::OpenPopup(groupMenuId.c_str());
                        }
                    }
                    if(ImGui::BeginPopup(groupMenuId.c_str())) {
                        ImGui::TextDisabled("%s", group.name.c_str());
                        ImGui::Separator();
                        if(ImGui::MenuItem("New piano roll clip here")) {
                            deferredGroupId = group.id;
                            deferredGroupAction = DeferredGroupAction::NewClip;
                            deferredClipBeat = pendingStartBeat;
                        }
                        if(ImGui::MenuItem("Targets...")) {
                            openNoteGroupSetup(timeline, nullptr, track.id, group.id);
                        }
                        if(ImGui::MenuItem(group.expanded ? "Hide parameter rows" : "Show parameter rows")) {
                            if(auto* editGroup = timeline.getNoteGroup(track.id, group.id)) editGroup->expanded = !editGroup->expanded;
                        }
                        {
                            const auto* pitchMember = timeline.getBinding(track.id, groupMembers.front());
                            auto* canvasParameter = pitchMember == nullptr ? nullptr : container->findTimelineParameter(pitchMember->parameterPath);
                            if(ImGui::MenuItem("Show in Canvas", nullptr, false, canvasParameter != nullptr) && canvasParameter != nullptr)
                                container->showParameterInCanvas(*canvasParameter);
                        }
                        ImGui::Separator();
                        const int unitIndex = static_cast<int>(rowUnits.size()) - 1;
                        if(ImGui::MenuItem("Move up", nullptr, false, unitIndex > 0)) {
                            deferredMoveUnitFrom = unitIndex;
                            deferredMoveUnitTo = unitIndex - 1;
                        }
                        if(ImGui::MenuItem("Move down", nullptr, false, groupMembers.back() != track.bindings.back().id)) {
                            deferredMoveUnitFrom = unitIndex;
                            deferredMoveUnitTo = unitIndex + 2; // slot after the next unit
                        }
                        ImGui::Separator();
                        if(ImGui::MenuItem("Ungroup (keep parameter rows)")) {
                            deferredGroupId = group.id;
                            deferredGroupAction = DeferredGroupAction::Ungroup;
                        }
                        if(ImGui::MenuItem("Remove from Timeline")) {
                            deferredGroupId = group.id;
                            deferredGroupAction = DeferredGroupAction::Remove;
                        }
                        ImGui::EndPopup();
                    }
                    // The group's piano-roll clips, drawn once.
                    auto groupLane = [&](const ofxOceanodeTimelineClip& clip) -> const ofxOceanodeTimelineLane* {
                        for(const auto& lane : clip.lanes) {
                            if(lane.type != ofxOceanodeTimelineLaneType::PianoRoll) continue;
                            if(std::any_of(lane.bindingIds.begin(), lane.bindingIds.end(),
                                           [&](const auto& id) { return group.contains(id); })) return &lane;
                        }
                        return nullptr;
                    };
                    dl->PushClipRect(ImVec2(zoneLeft, min.y), ImVec2(max.x, max.y), true);
                    for(const auto& clip : track.clips) {
                        if(const auto* lane = groupLane(clip)) drawClip(clip, lane, laneMin, max);
                    }
                    clipDropRows.push_back({track.id, groupMembers.front(), min.y, max.y});
                    currentRowBindingId = groupMembers.front();
                    for(auto clipIt = track.clips.rbegin(); clipIt != track.clips.rend(); ++clipIt) {
                        if(const auto* lane = groupLane(*clipIt)) handleClip(*clipIt, lane, laneMin, max);
                    }
                    currentRowBindingId.clear();
                    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !clipInteractionClaimedThisFrame &&
                       hoverRect(min, max) && ImGui::GetIO().MousePos.x >= zoneLeft) {
                        emptyTrackAreaClickedThisFrame = true;
                    }
                    const float px = laneMin.x + beatOffset(transportState.beatPosition);
                    if(px >= zoneLeft && px <= max.x) dl->AddLine(ImVec2(px, min.y), ImVec2(px, max.y), kPlayhead, 2);
                    dl->PopClipRect();
                }
                if(noteGroup != nullptr && !noteGroup->expanded) {
                    if(lastGroupMember) dockEditorHere();
                    continue;
                }
                const float rowHeight = ofClamp(binding.rowHeight, 20.0f, 300.0f);
                ImGui::SetCursorPos(ImVec2(0, rowY));
                ImGui::InvisibleButton(("##binding" + track.id + binding.id).c_str(), ImVec2(contentWidth, rowHeight));
                rowY += rowHeight;
                const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax(), laneMin(min.x + kLabelWidth - timelineScrollX, min.y);
                const ImVec2 labelMin(min.x + kParameterRowIndent, min.y); // rows sit indented under their track
                if(noteGroup != nullptr && !rowUnits.empty()) rowUnits.back().bottom = max.y;
                else rowUnits.push_back({{binding.id}, min.y, max.y});
                // Member rows of an expanded note group: indented, and they
                // show only automation other than the group's piano roll.
                const float memberIndent = noteGroup != nullptr ? 14.0f : 0.0f;
                auto rowLane = [&](const ofxOceanodeTimelineClip& clip) -> const ofxOceanodeTimelineLane* {
                    const auto* lane = laneForBinding(clip, binding.id);
                    if(lane != nullptr && noteGroup != nullptr && lane->type == ofxOceanodeTimelineLaneType::PianoRoll) return nullptr;
                    return lane;
                };
                // Bottom edge of the label column: drag to resize the row.
                constexpr float kResizeGrip = 5.0f;
                const ImVec2 gripMin(labelMin.x, max.y - kResizeGrip), gripMax(min.x + kLabelWidth, max.y);
                const bool gripHovered = rowResizeTrackId.empty() && bindingDragTrackId.empty() &&
                    hoverRect(gripMin, gripMax) &&
                    !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
                dl->AddRectFilled(labelMin, max, mutedTrackColor(track.color, 0.17f));
                dl->PushClipRect(ImVec2(zoneLeft, min.y), ImVec2(max.x, max.y), true);
                drawGrid(laneMin, max);
                dl->PopClipRect();
                bool automated = false;
                for(const auto& clip : track.clips) automated = automated || rowLane(clip) != nullptr;
                dl->AddRectFilled(labelMin, ImVec2(min.x + kLabelWidth, max.y),
                                  mutedTrackColor(track.color, automated ? 0.34f : 0.23f, 0.48f));
                if(noteGroup != nullptr) {
                    // Tie the member row to its group row above.
                    dl->AddLine(ImVec2(labelMin.x + 7.0f, min.y), ImVec2(labelMin.x + 7.0f, lastGroupMember ? min.y + 14.0f : max.y),
                                IM_COL32(track.color.r, track.color.g, track.color.b, 200), 2.0f);
                    dl->AddLine(ImVec2(labelMin.x + 7.0f, min.y + 14.0f), ImVec2(labelMin.x + 12.0f, min.y + 14.0f),
                                IM_COL32(track.color.r, track.color.g, track.color.b, 200), 2.0f);
                }
                dl->AddLine(ImVec2(labelMin.x, max.y - 0.5f), ImVec2(min.x + kLabelWidth, max.y - 0.5f), IM_COL32(0, 0, 0, 140), 1.0f); // row divider, on the resize edge
                // Blend mode used to be findable only by right-clicking this
                // row (or the track header) and opening a nested "Blend
                // mode" submenu -- easy to lose track of, which is also why
                // the in-label "[Mode]" text this chip replaces wasn't a
                // real fix: it lived inside the PushClipRect below and
                // simply didn't render once parameterPath filled the label
                // column. This chip sits in its own fixed, never-clipped
                // spot at the row's right edge, always shows the current
                // mode, and opens the same options in a single click.
                const float blendChipWidth = 80.0f;
                const ImVec2 blendChipMin(min.x + kLabelWidth - blendChipWidth - 6.0f, min.y + 4.0f);
                const ImVec2 blendChipMax(min.x + kLabelWidth - 6.0f, min.y + std::min(rowHeight, kRowHeight) - 4.0f);
                const bool blendChipActive = binding.mode != ofxOceanodeTimelineAutomationMode::Replace;
                dl->AddRectFilled(blendChipMin, blendChipMax,
                                  blendChipActive ? IM_COL32(235, 165, 65, 220) : IM_COL32(90, 90, 95, 150), 3.0f);
                dl->PushClipRect(ImVec2(labelMin.x + 5.0f, min.y), ImVec2(blendChipMin.x - 4.0f, max.y), true);
                {
                    std::string rowLabel = binding.parameterPath;
                    if(noteGroup != nullptr) {
                        const char* role = noteGroup->pitchBindingId == binding.id ? "pitch"
                            : noteGroup->gateBindingId == binding.id ? "gate" : "vel";
                        rowLabel = std::string(role) + ": " + rowLabel;
                    }
                    if(binding.bypass) rowLabel += "  (bypassed)";
                    dl->AddText(ImVec2(labelMin.x + 7 + memberIndent, min.y + 6),
                                automated || noteGroup != nullptr ? IM_COL32(245, 250, 255, 255) : IM_COL32(180, 180, 180, 255),
                                rowLabel.c_str());
                }
                dl->PopClipRect();
                {
                    const std::string blendChipLabel = ofxOceanodeTimelineManager::modeToString(binding.mode) + " v";
                    dl->PushClipRect(blendChipMin, blendChipMax, true);
                    dl->AddText(ImVec2(blendChipMin.x + 4.0f, blendChipMin.y + 3.0f),
                               blendChipActive ? IM_COL32(30, 20, 5, 255) : IM_COL32(225, 225, 230, 235), blendChipLabel.c_str());
                    dl->PopClipRect();
                }
                if(hoverRect(blendChipMin, blendChipMax))
                    ImGui::SetTooltip("Blend mode -- how this parameter combines with other clips/tracks driving it");
                if(gripHovered || (rowResizeTrackId == track.id && rowResizeBindingId == binding.id)) {
                    dl->AddLine(ImVec2(gripMin.x, max.y - 1.5f), ImVec2(gripMax.x, max.y - 1.5f), IM_COL32(255, 210, 90, 220), 3.0f);
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                }
                if(gripHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    rowResizeTrackId = track.id;
                    rowResizeBindingId = binding.id;
                    rowResizeStartY = ImGui::GetIO().MousePos.y;
                    rowResizeStartHeight = rowHeight;
                }
                if(gripHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    // Double-click the edge to restore the default height.
                    if(auto* editTrack = timeline.getTrack(track.id))
                        for(auto& editBinding : editTrack->bindings)
                            if(editBinding.id == binding.id) editBinding.rowHeight = kRowHeight;
                    rowResizeTrackId.clear();
                    rowResizeBindingId.clear();
                }
                // Press on the parameter name and drag vertically to reorder the
                // rows (a group's member rows move with their group).
                if(noteGroup == nullptr && track.bindings.size() > 1 && !gripHovered && rowResizeTrackId.empty() &&
                   hoverRect(min, ImVec2(blendChipMin.x - 2.0f, max.y)) &&
                   ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    bindingDragTrackId = track.id;
                    bindingDragId = binding.id;
                    bindingDragStartY = ImGui::GetIO().MousePos.y;
                    bindingDragActive = false;
                }
                const std::string blendChipPopupId = "##blendModeChip" + track.id + binding.id;
                if(hoverRect(blendChipMin, blendChipMax) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    anyClipInteractionClaimedThisFrame = true; // not a clip click, but not empty timeline space either
                    ImGui::OpenPopup(blendChipPopupId.c_str());
                }
                if(ImGui::BeginPopup(blendChipPopupId.c_str())) {
                    // Flattened to just this one binding -- the track-header
                    // entry point still nests a per-binding submenu because
                    // it has to ask *which* binding first; this chip already
                    // belongs to exactly one.
                    drawBlendModeOptions(timeline, track.id, binding);
                    ImGui::EndPopup();
                }

                const std::string bindingMenuId = "##bindingMenu" + track.id + binding.id;
                if(hoverRect(min, max) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
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
                    {
                        // Same as the Custom GUI's "Show in Canvas": select the owning node and centre on it.
                        auto* boundParameter = container->findTimelineParameter(binding.parameterPath);
                        if(ImGui::MenuItem("Show in Canvas", nullptr, false, boundParameter != nullptr) && boundParameter != nullptr)
                            container->showParameterInCanvas(*boundParameter);
                        ImGui::Separator();
                    }
                    if(noteGroup == nullptr) {
                        const int unitIndex = static_cast<int>(rowUnits.size()) - 1;
                        if(ImGui::MenuItem("Move up", nullptr, false, unitIndex > 0)) {
                            deferredMoveUnitFrom = unitIndex;
                            deferredMoveUnitTo = unitIndex - 1;
                        }
                        if(ImGui::MenuItem("Move down", nullptr, false, binding.id != track.bindings.back().id)) {
                            deferredMoveUnitFrom = unitIndex;
                            deferredMoveUnitTo = unitIndex + 2; // slot after the next unit
                        }
                        ImGui::Separator();
                    } else {
                        ImGui::TextDisabled("Part of \"%s\"", noteGroup->name.c_str());
                        if(ImGui::MenuItem("Take out of the group")) {
                            deferredGroupId = noteGroup->id;
                            deferredGroupAction = DeferredGroupAction::LeaveGroup;
                            deferredLeaveBindingId = binding.id;
                        }
                        ImGui::Separator();
                    }
                    if(ImGui::MenuItem("New clip here")) {
                        pendingTrackId = track.id;
                        pendingClipBindingId = binding.id;
                        pendingClipLaneType = optionIndexForLaneType(binding.laneType);
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
                    if(ImGui::MenuItem("Bypass", nullptr, binding.bypass)) {
                        // Stop automating this parameter (clips stay; the value is left alone).
                        if(auto* editBinding = timeline.getBinding(track.id, binding.id)) editBinding->bypass = !editBinding->bypass;
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
                for(const auto& clip : track.clips) {
                    const auto* lane = rowLane(clip);
                    if(lane == nullptr) continue;
                    drawClip(clip, lane, laneMin, max);
                    if(clip.lanes.size() > 1) multiLaneClipRowSpans[clip.id].push_back({min.y, max.y});
                }
                clipDropRows.push_back({track.id, binding.id, min.y, max.y});
                currentRowBindingId = binding.id;
                for(auto clipIt = track.clips.rbegin(); clipIt != track.clips.rend(); ++clipIt) {
                    const auto* lane = rowLane(*clipIt);
                    if(lane != nullptr) handleClip(*clipIt, lane, laneMin, max);
                }
                currentRowBindingId.clear();
                if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                   !clipInteractionClaimedThisFrame &&
                   hoverRect(min, max) &&
                   ImGui::GetIO().MousePos.x >= zoneLeft) {
                    emptyTrackAreaClickedThisFrame = true;
                }
                const float px = laneMin.x + beatOffset(transportState.beatPosition);
                if(px >= zoneLeft && px <= max.x) dl->AddLine(ImVec2(px, min.y), ImVec2(px, max.y), kPlayhead, 2);
                dl->PopClipRect();
                ++index;
                if(noteGroup == nullptr || lastGroupMember) dockEditorHere();
            }
            // Clip context menus (opened from any row's clip), once per track.
            for(const auto& clip : track.clips) drawClipMenu(clip);
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
                    // One bar per group of rows on the same side of an inline clip editor.
                    for(int side = 0; side < 2; ++side) {
                        bool any = false;
                        float top = 0.0f, bottom = 0.0f;
                        for(const auto& span : rowSpans) {
                            const bool below = editorDrawn && span.first >= editorSpanScreen.second - 0.5f;
                            if(below != (side == 1)) continue;
                            top = any ? std::min(top, span.first) : span.first;
                            bottom = any ? std::max(bottom, span.second) : span.second;
                            any = true;
                        }
                        if(any) dl->AddLine(ImVec2(barX, top + 6.0f), ImVec2(barX, bottom - 6.0f), IM_COL32(255, 210, 90, 210), 2.0f);
                    }
                    for(const auto& span : rowSpans) {
                        const float midY = (span.first + span.second) * 0.5f;
                        dl->AddLine(ImVec2(barX, midY), ImVec2(barX + 4.0f, midY), IM_COL32(255, 210, 90, 210), 2.0f);
                    }
                }
            }
            // Moves a unit (a row, or a note group with its members) to an
            // insertion slot between units, by rewriting the binding order.
            auto moveUnit = [&](int from, int slot) {
                const int unitCount = static_cast<int>(rowUnits.size());
                if(from < 0 || from >= unitCount) return;
                slot = std::max(0, std::min(slot, unitCount));
                const int to = slot > from ? slot - 1 : slot;
                if(to == from) return;
                std::vector<RowUnit> order = rowUnits;
                const RowUnit moved = order[from];
                order.erase(order.begin() + from);
                order.insert(order.begin() + to, moved);
                std::vector<std::string> ids;
                for(const auto& unit : order) ids.insert(ids.end(), unit.bindingIds.begin(), unit.bindingIds.end());
                timeline.setBindingOrder(track.id, ids);
            };
            if(deferredMoveUnitFrom >= 0) moveUnit(deferredMoveUnitFrom, deferredMoveUnitTo);
            if(deferredGroupAction != DeferredGroupAction::None) {
                switch(deferredGroupAction) {
                    case DeferredGroupAction::Ungroup:
                        timeline.removeNoteGroup(track.id, deferredGroupId, false);
                        break;
                    case DeferredGroupAction::Remove:
                        if(clipEditorOpen && editorTrackId == track.id) clipEditorOpen = false;
                        timeline.removeNoteGroup(track.id, deferredGroupId, true);
                        break;
                    case DeferredGroupAction::LeaveGroup:
                        if(const auto* group = timeline.getNoteGroup(track.id, deferredGroupId)) {
                            auto roles = *group;
                            for(auto* role : {&roles.pitchBindingId, &roles.gateBindingId, &roles.velocityBindingId})
                                if(*role == deferredLeaveBindingId) role->clear();
                            timeline.setNoteGroupRoles(track.id, deferredGroupId,
                                                       roles.pitchBindingId, roles.gateBindingId, roles.velocityBindingId);
                        }
                        break;
                    case DeferredGroupAction::NewClip: {
                        std::string laneId;
                        const std::string clipId = timeline.createNoteGroupClip(
                            track.id, deferredGroupId, std::string(), deferredClipBeat, timeline.getBeatsPerBar(), &laneId);
                        if(!clipId.empty()) {
                            if(const auto* group = timeline.getNoteGroup(track.id, deferredGroupId)) {
                                const auto members = group->members();
                                editorAnchorBindingId = members.empty() ? std::string() : members.front();
                            }
                            editorTrackId = track.id;
                            editorClipId = clipId;
                            editorLaneId = laneId;
                            clipEditorOpen = true;
                            foldedClipEditors.erase(clipId);
                        }
                        break;
                    }
                    default: break;
                }
            }
            // Row drag: show where the row (or group) will land, apply on release.
            if(bindingDragTrackId == track.id && !track.bindings.empty()) {
                const float mouseY = ImGui::GetIO().MousePos.y;
                const int unitCount = static_cast<int>(rowUnits.size());
                auto dropSlot = [&]() {
                    int slot = 0;
                    for(const auto& unit : rowUnits) if(mouseY > (unit.top + unit.bottom) * 0.5f) ++slot;
                    return std::min(slot, unitCount);
                };
                if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    if(std::abs(mouseY - bindingDragStartY) > 4.0f) bindingDragActive = true;
                    if(bindingDragActive && unitCount > 0) {
                        const int slot = dropSlot();
                        const float lineY = slot < unitCount ? rowUnits[slot].top : rowUnits.back().bottom;
                        const float left = ImGui::GetWindowPos().x;
                        dl->AddLine(ImVec2(left, lineY), ImVec2(left + contentWidth, lineY),
                                    IM_COL32(255, 210, 90, 235), 2.0f);
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    }
                } else {
                    if(bindingDragActive) {
                        int from = -1;
                        for(int i = 0; i < unitCount; ++i) {
                            const auto& ids = rowUnits[i].bindingIds;
                            if(std::find(ids.begin(), ids.end(), bindingDragId) != ids.end()) from = i;
                        }
                        moveUnit(from, dropSlot());
                    }
                    bindingDragTrackId.clear();
                    bindingDragId.clear();
                    bindingDragActive = false;
                }
            }
            ImGui::SetCursorPosY(rowY);
            if(editorOnThisTrack && !editorDrawn) {
                editorScreenTop = ImGui::GetCursorScreenPos().y;
                drawEditorSeparator(track, contentWidth);
                drawLaneEditor(timeline, track, contentWidth, endBeat, transportState.beatPosition);
                editorScreenBottom = ImGui::GetCursorScreenPos().y;
                editorScreenValid = true;
            }
        }
    }

    // Empty track space: a click moves the playhead (on release), a drag
    // draws a box that selects the clips it touches (Shift adds to the
    // selection).
    if(emptyTrackAreaClickedThisFrame) {
        marqueePending = true;
        marqueeActive = false;
        marqueeStartX = ImGui::GetIO().MousePos.x;
        marqueeStartY = ImGui::GetIO().MousePos.y;
        marqueeAdditive = ImGui::GetIO().KeyShift;
    }
    if(marqueePending) {
        const ImVec2 mouseNow = ImGui::GetIO().MousePos;
        const ImVec2 boxMin(std::min(marqueeStartX, mouseNow.x), std::min(marqueeStartY, mouseNow.y));
        const ImVec2 boxMax(std::max(marqueeStartX, mouseNow.x), std::max(marqueeStartY, mouseNow.y));
        auto clipsInBox = [&]() {
            std::vector<std::pair<std::string, std::string>> hits;
            for(const auto& rect : clipScreenRects) {
                if(rect.x2 < boxMin.x || rect.x1 > boxMax.x || rect.y2 < boxMin.y || rect.y1 > boxMax.y) continue;
                hits.emplace_back(rect.trackId, rect.clipId);
            }
            return hits;
        };
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if(!marqueeActive && (std::abs(mouseNow.x - marqueeStartX) >= std::max(4.0f, ImGui::GetIO().MouseDragThreshold) ||
                                  std::abs(mouseNow.y - marqueeStartY) >= std::max(4.0f, ImGui::GetIO().MouseDragThreshold))) {
                marqueeActive = true;
            }
            if(marqueeActive) {
                auto* boxDl = ImGui::GetWindowDrawList();
                boxDl->PushClipRectFullScreen();
                // Highlight what the box will select.
                for(const auto& rect : clipScreenRects) {
                    if(rect.x2 < boxMin.x || rect.x1 > boxMax.x || rect.y2 < boxMin.y || rect.y1 > boxMax.y) continue;
                    boxDl->AddRectFilled(ImVec2(rect.x1, rect.y1), ImVec2(rect.x2, rect.y2), IM_COL32(255, 255, 255, 70));
                }
                boxDl->AddRectFilled(boxMin, boxMax, IM_COL32(120, 170, 255, 40));
                boxDl->AddRect(boxMin, boxMax, IM_COL32(150, 190, 255, 200));
                boxDl->PopClipRect();
            }
        } else {
            if(marqueeActive) {
                if(!marqueeAdditive) selectedClips.clear();
                for(const auto& hit : clipsInBox()) selectedClips.insert(hit);
            } else if(container->getTransport() != nullptr) {
                const float timelineMouseX = marqueeStartX - zoneLeft + timelineScrollX;
                const double rawBeat = ofClamp(
                    pixelsToBeat(timeline, timelineMouseX, transportState.bpm, endBeat), 0.0, endBeat);
                container->getTransport()->seekToBeat(snapBeat(rawBeat));
            }
            marqueePending = false;
            marqueeActive = false;
        }
    }

    // A plain click that landed on no clip at all (every clip's handleClip
    // leaves anyClipInteractionClaimedThisFrame false) drops the current
    // multi-selection, the same way clicking empty space deselects in any
    // selection-based editor. Gated to this child window so it doesn't fire
    // for the Group/Ungroup toolbar buttons above it (those are outside
    // "##TimelineViewport" and handle the click themselves).
    // A click inside the clip editor is editing that clip, not empty space:
    // keep the selection so S / Delete / Group still act on it.
    const bool clickInEditor = editorScreenValid &&
        ImGui::GetIO().MousePos.y >= editorScreenTop && ImGui::GetIO().MousePos.y <= editorScreenBottom;
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
        lastClickInEditor = clickInEditor;
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !anyClipInteractionClaimedThisFrame && !clickInEditor &&
       !ImGui::GetIO().KeyShift && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
        selectedClips.clear();
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
            groupDragSnapshot.clear();
        }
        if(pianoKeyboardPreviewTrackId == trackDeletionId) {
            pianoKeyboardPreviewActive = false;
            pianoKeyboardPreviewTrackId.clear();
            pianoKeyboardPreviewGateBindingId.clear();
            pianoKeyboardPreviewPitchBindingId.clear();
            pianoKeyboardPreviewPitch = -1;
        }
        // Persistent group membership for this track's clips is cleaned up
        // inside timeline.removeTrack() itself; only the ephemeral
        // selection is this controller's own responsibility.
        for(auto it = selectedClips.begin(); it != selectedClips.end();) {
            if(it->first == trackDeletionId) it = selectedClips.erase(it); else ++it;
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
        if(draggingTrackId == clipDeletionTrackId && draggingClipId == clipDeletionClipId) {
            clipDragMode = ClipDragMode::None;
            draggingTrackId.clear();
            draggingClipId.clear();
            groupDragSnapshot.clear();
        }
        selectedClips.erase({clipDeletionTrackId, clipDeletionClipId});
        // removeClip() itself drops this clip from any group and dissolves
        // that group if it falls below two members -- nothing further to
        // clean up on the model side here.
        timeline.removeClip(clipDeletionTrackId, clipDeletionClipId);
        requestClipDeletion = false;
        clipDeletionTrackId.clear();
        clipDeletionClipId.clear();
    }

    if(!keyboardClipDeletionRequests.empty()) {
        for(const auto& deletion : keyboardClipDeletionRequests) {
            const auto& trackId = deletion.first;
            const auto& clipId = deletion.second;
            const auto* deletedClip = timeline.getClip(trackId, clipId);
            if(deletedClip == nullptr) continue;
            for(const auto& lane : deletedClip->lanes) {
                collapsedLaneIds.erase(lane.id);
                laneEditorHeights.erase(lane.id);
            }
            if(clipEditorOpen && editorTrackId == trackId && editorClipId == clipId) {
                clipEditorOpen = false;
                editorTrackId.clear();
                editorClipId.clear();
                editorLaneId.clear();
            }
            if(draggingTrackId == trackId && draggingClipId == clipId) {
                clipDragMode = ClipDragMode::None;
                draggingTrackId.clear();
                draggingClipId.clear();
                groupDragSnapshot.clear();
            }
            timeline.removeClip(trackId, clipId);
        }
        keyboardClipDeletionRequests.clear();
    }

    if(waveFileRequest != WaveFileRequest::None) {
        const auto request = waveFileRequest;
        const std::string requestTrackId = waveFileRequestTrackId;
        const std::string requestClipId = waveFileRequestClipId;
        const double requestBeat = waveFileRequestBeat;
        waveFileRequest = WaveFileRequest::None;
        waveFileRequestTrackId.clear();
        waveFileRequestClipId.clear();
        const auto result = ofSystemLoadDialog(
            request == WaveFileRequest::ReplaceClip ? "Replace wave file" : "Load wave file", false);
        if(result.bSuccess) {
            if(request == WaveFileRequest::NewTrack) {
                // The track is only worth creating once a file is chosen --
                // cancelling used to be the difference between an empty track
                // and none at all.
                const auto trackId = timeline.createWaveTrack();
                createWaveClipFromFile(timeline, trackId, result.filePath,
                                       requestBeat, timeline.getBeatsPerBar());
            } else if(request == WaveFileRequest::NewClip) {
                createWaveClipFromFile(timeline, requestTrackId, result.filePath,
                                       requestBeat, timeline.getBeatsPerBar());
            } else {
                replaceWaveClipFromFile(timeline, requestTrackId, requestClipId, result.filePath);
            }
        }
    }

    if(requestClipSplit) {
        requestClipSplit = false;
        timeline.splitClip(clipSplitTrackId, clipSplitClipId, clipSplitBeat);
        clipSplitTrackId.clear();
        clipSplitClipId.clear();
    }
    if(requestWaveSplit) {
        const std::string splitTrackId = waveSplitTrackId;
        const std::string splitClipId = waveSplitClipId;
        requestWaveSplit = false;
        waveSplitTrackId.clear();
        waveSplitClipId.clear();
        splitWaveClipAtPlayhead(timeline, splitTrackId, splitClipId, clipSplitBeat);
    }

    if(requestRemoveBinding) {
        timeline.removeBinding(removeBindingTrackId, removeBindingId);
        requestRemoveBinding = false;
        removeBindingTrackId.clear();
        removeBindingId.clear();
    }

    if(requestAddLane) {
        requestAddLane = false;
        const int optionIndex = ofClamp(pendingAddLaneType, 0, kLaneTypeOptionCount - 1);
        const auto laneType = laneTypeFromOptionIndex(optionIndex);
        const char* laneName = kLaneTypeOptions[optionIndex];
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
        // A duplicate can be dropped straight up or down onto another row,
        // so vertical movement starts that drag too.
        const float dragThreshold = std::max(3.0f, ImGui::GetIO().MouseDragThreshold);
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && !clipDragCommitted &&
           (std::abs(ImGui::GetIO().MousePos.x - clipDragStartMouseX) >= dragThreshold ||
            (clipDragMode == ClipDragMode::Duplicate &&
             std::abs(ImGui::GetIO().MousePos.y - clipDragStartMouseY) >= dragThreshold))) {
            clipDragCommitted = true;
        }
        if(clipDragMode == ClipDragMode::Duplicate) {
            // Where the copy would land: the row under the mouse, at the
            // snapped beat the grabbed point is dragged to.
            const ImVec2 mousePos = ImGui::GetIO().MousePos;
            const ClipDropRow* dropRow = nullptr;
            for(const auto& row : clipDropRows) {
                if(mousePos.y >= row.top && mousePos.y < row.bottom) { dropRow = &row; break; }
            }
            const auto* sourceClip = timeline.getClip(draggingTrackId, draggingClipId);
            const double dropStart = std::max(0.0, snapBeat(beatAtOffset(mousePos.x - dragTimelineOriginX) - dragOffsetBeats));
            // Same row: the whole clip. Any other row: the grabbed lane, now
            // driving that row's parameter. Another track's single row: the
            // whole clip, mapped onto that track's matching parameters.
            const bool sameRow = dropRow != nullptr && dropRow->trackId == draggingTrackId &&
                (dropRow->bindingId.empty() || dropRow->bindingId == dragSourceBindingId);
            const std::string dropBindingId = dropRow == nullptr || sameRow ? std::string() : dropRow->bindingId;
            if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                if(clipDragCommitted && sourceClip != nullptr && dropRow != nullptr) {
                    ImDrawList* ghostDl = ImGui::GetWindowDrawList();
                    const float x1 = dragTimelineOriginX + beatOffset(dropStart);
                    const float x2 = dragTimelineOriginX + beatOffset(dropStart + sourceClip->durationBeats);
                    const ImVec2 ghostMin(x1, dropRow->top + 2.0f), ghostMax(x2, dropRow->bottom - 2.0f);
                    ghostDl->AddRectFilled(ghostMin, ghostMax, IM_COL32(255, 255, 255, 40), 3.0f);
                    ghostDl->AddRect(ghostMin, ghostMax, IM_COL32(255, 255, 255, 200), 3.0f, 0, 1.5f);
                    ghostDl->AddText(ImVec2(x1 + 5.0f, dropRow->top + 4.0f), IM_COL32(255, 255, 255, 220),
                                     ("+ " + sourceClip->name).c_str());
                }
            } else {
                if(clipDragCommitted && sourceClip != nullptr && dropRow != nullptr) {
                    const std::string newClipId = timeline.duplicateClip(draggingTrackId, draggingClipId,
                        dropRow->trackId, dropStart, dragSourceLaneId, dropBindingId);
                    if(newClipId.empty()) {
                        ofLogWarning("ofxOceanodeTimelineController")
                            << "Cannot duplicate \"" << sourceClip->name << "\" there: "
                            << "no matching parameter on that track, or a piano roll dropped outside a note group, "
                            << "or a Wave clip outside a Wave track";
                    } else {
                        selectedClips.clear();
                        selectedClips.insert({dropRow->trackId, newClipId});
                    }
                }
                clipDragMode = ClipDragMode::None;
                clipDragCommitted = false;
                draggingTrackId.clear();
                draggingClipId.clear();
                groupDragSnapshot.clear();
            }
        } else if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && clipDragMode == ClipDragMode::TrimStart) {
            // Preview only; the trim is applied on release.
            if(clipDragCommitted) {
                if(const auto* clip = timeline.getClip(draggingTrackId, draggingClipId)) {
                    const auto* trimTrack = timeline.getTrack(draggingTrackId);
                    const double mouseBeat = beatAtOffset(ImGui::GetIO().MousePos.x - dragTimelineOriginX);
                    // Repeating automation can grow to the left (its content repeats);
                    // one-shot and Wave clips can only be shortened.
                    // Shift extends any (non-Wave, non-LFO) clip with empty time.
                    const bool canGrow = trimTrack != nullptr && !trimTrack->isWaveTrack &&
                        (clip->repeatContent || (trimExtends && !clip->isLfo));
                    const double lowest = canGrow ? 0.0 : clip->startBeat;
                    const double highest = clip->startBeat + clip->durationBeats - 1.0 / kPPQ;
                    trimPreviewBeat = ofClamp(snapBeat(mouseBeat), lowest, highest);
                    ImDrawList* trimDl = ImGui::GetWindowDrawList();
                    const float oldX = dragTimelineOriginX + beatOffset(clip->startBeat);
                    const float newX = dragTimelineOriginX + beatOffset(trimPreviewBeat);
                    if(newX > oldX)
                        trimDl->AddRectFilled(ImVec2(oldX, dragRowTop), ImVec2(newX, dragRowBottom), IM_COL32(0, 0, 0, 140));
                    else if(newX < oldX)
                        trimDl->AddRectFilled(ImVec2(newX, dragRowTop + 3.0f), ImVec2(oldX, dragRowBottom - 3.0f),
                                              trimTrack != nullptr ? IM_COL32(trimTrack->color.r, trimTrack->color.g, trimTrack->color.b, 70)
                                                                   : IM_COL32(200, 200, 200, 60));
                    trimDl->AddLine(ImVec2(newX, dragRowTop), ImVec2(newX, dragRowBottom), IM_COL32(255, 210, 90, 240), 2.0f);
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                }
            }
        } else if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && !clipDragCommitted) {
            // Not a drag yet.
        } else if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if(auto* clip = timeline.getClip(draggingTrackId, draggingClipId)) {
                const double mouseBeat = beatAtOffset(ImGui::GetIO().MousePos.x - dragTimelineOriginX);
                // groupDragSnapshot holds the anchor clip itself too when a
                // drag started on a grouped/multi-selected clip -- look up
                // its own snapshot entry so the group-wide math below is
                // relative to where *everything* was when the drag began,
                // not just where the anchor is right now.
                const auto anchorSnapshot = groupDragSnapshot.empty() ? groupDragSnapshot.end()
                    : std::find_if(groupDragSnapshot.begin(), groupDragSnapshot.end(), [&](const auto& entry) {
                        return entry.trackId == draggingTrackId && entry.clipId == draggingClipId;
                    });
                if(clipDragMode == ClipDragMode::Move) {
                    const double newStart = snapBeat(mouseBeat - dragOffsetBeats);
                    if(!groupDragSnapshot.empty() && anchorSnapshot != groupDragSnapshot.end()) {
                        // Clamp the shared offset once, so the earliest member
                        // stops at beat 0 and the spacing between members holds.
                        double earliest = anchorSnapshot->startBeat;
                        for(const auto& entry : groupDragSnapshot) earliest = std::min(earliest, entry.startBeat);
                        const double deltaBeats = std::max(newStart - anchorSnapshot->startBeat, -earliest);
                        for(const auto& entry : groupDragSnapshot) {
                            timeline.setClipTiming(entry.trackId, entry.clipId,
                                                   std::max(0.0, entry.startBeat + deltaBeats), entry.durationBeats);
                        }
                    } else {
                        timeline.setClipTiming(draggingTrackId, draggingClipId, newStart, clip->durationBeats);
                    }
                }
                else {
                    const double duration = std::max(1.0 / kPPQ, snapBeat(mouseBeat - clip->startBeat));
                    if(!groupDragSnapshot.empty() && anchorSnapshot != groupDragSnapshot.end()) {
                        // Express the drag as a scale factor on the anchor
                        // clip's own original span, then apply that same
                        // factor to every member's original span (from the
                        // snapshot) around the group's shared left edge --
                        // the group-wide equivalent of what the single-clip
                        // branch below does for just one clip.
                        const double scale = duration / std::max(1.0 / kPPQ, anchorSnapshot->durationBeats);
                        for(const auto& entry : groupDragSnapshot) {
                            const double newMemberStart = groupDragAnchorBeat + (entry.startBeat - groupDragAnchorBeat) * scale;
                            const double newMemberDuration = std::max(1.0 / kPPQ, entry.durationBeats * scale);
                            timeline.setClipTiming(entry.trackId, entry.clipId, std::max(0.0, newMemberStart), newMemberDuration);
                            const auto* memberTrack = timeline.getTrack(entry.trackId);
                            if(memberTrack != nullptr && memberTrack->isWaveTrack) {
                                // setClipTiming already mapped the full source
                                // over the new duration.
                                continue;
                            }
                            if(clipDragMode == ClipDragMode::Repeat) {
                                timeline.setClipContentDuration(entry.trackId, entry.clipId, entry.contentDurationBeats, true);
                            } else if(clipDragMode == ClipDragMode::Stretch) {
                                timeline.setClipContentDuration(entry.trackId, entry.clipId, entry.contentDurationBeats, false);
                                if(auto* memberClip = timeline.getClip(entry.trackId, entry.clipId))
                                    memberClip->contentStretch = newMemberDuration / std::max(1.0 / kPPQ, entry.contentDurationBeats);
                            } else {
                                // Shift-resize adds/crops source space
                                // without changing an existing stretch
                                // ratio -- same rule as the single-clip case.
                                if(auto* memberClip = timeline.getClip(entry.trackId, entry.clipId)) {
                                    memberClip->contentStretch = std::max(1.0 / 1024.0, entry.contentStretch);
                                    timeline.setClipContentDuration(entry.trackId, entry.clipId,
                                        newMemberDuration / memberClip->contentStretch, false);
                                }
                            }
                        }
                    } else {
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
            }
        } else {
            if(clipDragMode == ClipDragMode::TrimStart && clipDragCommitted) {
                const auto* trimTrack = timeline.getTrack(draggingTrackId);
                const auto* trimClip = timeline.getClip(draggingTrackId, draggingClipId);
                if(trimTrack != nullptr && trimClip != nullptr && std::abs(trimPreviewBeat - trimClip->startBeat) > 1e-9) {
                    if(trimTrack->isWaveTrack) trimWaveClipStart(timeline, draggingTrackId, draggingClipId, trimPreviewBeat);
                    else if(trimExtends) timeline.extendClipStart(draggingTrackId, draggingClipId, trimPreviewBeat);
                    else timeline.trimClipStart(draggingTrackId, draggingClipId, trimPreviewBeat);
                }
            }
            clipDragMode = ClipDragMode::None;
            clipDragCommitted = false;
            draggingTrackId.clear();
            draggingClipId.clear();
            groupDragSnapshot.clear();
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
            if(ImGui::IsItemClicked() && !hoverRect(thumbMin, thumbMax)) {
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

    if(requestClipPopup) { ImGui::OpenPopup("New Timeline Clip"); requestClipPopup = false; }
    drawClipPopup(timeline);
    if(requestClipRenamePopup) {
        ImGui::OpenPopup("Rename Timeline Clip");
        requestClipRenamePopup = false;
    }
    drawClipRenamePopup(timeline);
}

void ofxOceanodeTimelineController::drawPendingTrackPopup() {
    // Deliberately independent of draw() and this controller's own window:
    // see the declaration's comment in the header for why. This must work
    // even on a frame where this controller's ImGui::Begin() is never
    // called at all.
    if(container == nullptr) return;
    auto& timeline = container->getTimelineManager();

    std::string requestedRename;
    bool isNewTrack = false;
    if(timeline.consumePendingTrackRename(requestedRename, &isNewTrack)) {
        pendingTrackId = requestedRename;
        if(const auto* track = timeline.getTrack(pendingTrackId)) {
            std::strncpy(pendingTrackName, track->name.c_str(), sizeof(pendingTrackName) - 1);
            pendingTrackName[sizeof(pendingTrackName) - 1] = '\0';
            pendingNewTrackDialog = isNewTrack;
            requestRenamePopup = true;
        }
    }
    if(requestRenamePopup) {
        // Pin it to the main viewport and center it, regardless of which
        // window/viewport happens to be focused when this fires -- see
        // drawRenamePopup for the SetNextWindowPos/SetNextWindowViewport
        // calls that actually do this.
        ImGui::OpenPopup(pendingNewTrackDialog ? "New Timeline Track" : "Rename Timeline Track");
        requestRenamePopup = false;
    }
    drawRenamePopup(timeline);

    // "Add to Timeline as Piano Roll..." from a parameter's right-click menu.
    if(auto* noteSource = timeline.consumePendingNoteGroupSetup())
        openNoteGroupSetup(timeline, noteSource, std::string(), std::string());
    if(requestNoteSetupPopup) {
        ImGui::OpenPopup("Piano Roll##noteGroupSetup");
        requestNoteSetupPopup = false;
    }
    drawNoteGroupSetupPopup(timeline);
}

void ofxOceanodeTimelineController::jumpToBeat(double beat) {
    auto transport = container == nullptr ? nullptr : container->getTransport();
    if(transport == nullptr || transport->hasExternalClock()) return;
    const auto state = transport->getState();
    if(quantizeJumps && state.isPlaying) {
        // Wait for the next bar line; the jump keeps the position within the bar.
        const double beatsPerBar = std::max(1.0, container->getTimelineManager().getBeatsPerBar());
        double at = std::ceil(state.beatPosition / beatsPerBar - 1e-9) * beatsPerBar;
        if(at <= state.beatPosition + 1e-6) at += beatsPerBar;
        pendingJump = true;
        pendingJumpTarget = std::max(0.0, beat);
        pendingJumpAt = at;
        pendingJumpLoopCount = state.loopCount;
        return;
    }
    pendingJump = false;
    transport->seekToBeat(std::max(0.0, beat));
}

void ofxOceanodeTimelineController::drawMarkerRenamePopup(ofxOceanodeTimelineManager& timeline) {
    if(requestMarkerRename) {
        ImGui::OpenPopup("Rename Marker");
        requestMarkerRename = false;
    }
    if(!ImGui::BeginPopupModal("Rename Marker", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool entered = ImGui::InputText("Name", markerRenameBuffer, sizeof(markerRenameBuffer),
                                          ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    const bool valid = markerRenameBuffer[0] != '\0';
    ImGui::BeginDisabled(!valid);
    const bool saved = ImGui::Button("Save");
    ImGui::EndDisabled();
    if((entered || saved) && valid) {
        timeline.renameMarker(markerRenameId, markerRenameBuffer);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void ofxOceanodeTimelineController::drawRuler(ofxOceanodeTimelineManager& timeline, float labelWidth,
                                              float timelineWidth, double endBeat,
                                              double beatPosition, float bpm) {
    const auto& viewState = timeline.getViewState();
    const float pixelsPerSecond = viewState.pixelsPerSecond;
    const float timelineScrollX = viewState.scrollX;
    const float width = labelWidth + timelineWidth;
    ImGui::SetNextItemAllowOverlap(); // the tempo-lane toggle sits on top of it
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
    const double beatsPerBar = std::max(1.0, timeline.getBeatsPerBar());
    const float barSpacingPixels = beatToPixels(timeline, beatsPerBar, bpm);
    int barLabelStep = 1;
    while(barSpacingPixels * barLabelStep < 38.0f) barLabelStep *= 2;
    // Only the lines that are on screen.
    const double visibleFromBeat = pixelsToBeat(timeline, timelineScrollX, bpm, endBeat);
    const double visibleToBeat = std::min(endBeat, pixelsToBeat(timeline, timelineScrollX + timelineWidth, bpm, endBeat) + grid);
    const int firstGridLine = std::max(0, static_cast<int>(std::floor(visibleFromBeat / grid)));
    const int gridLines = static_cast<int>(std::ceil(visibleToBeat / grid));
    for(int i = firstGridLine; i <= gridLines; ++i) {
        const double beat = i * grid;
        const float x = laneMin.x + beatToPixels(timeline, beat, bpm);
        const bool bar = std::fmod(beat, beatsPerBar) < 0.001;
        const bool quarter = std::fmod(beat, 1.0) < 0.001;
        if(bar || quarter || beatToPixels(timeline, beat + grid, bpm) - beatToPixels(timeline, beat, bpm) >= 4.0f)
            dl->AddLine(ImVec2(x, rulerDividerY), ImVec2(x, max.y), bar ? kBar : quarter ? IM_COL32(95, 95, 95, 165) : kGrid, bar ? 1.5f : 1.0f);
        const int barNumber = static_cast<int>(std::round(beat / beatsPerBar));
        if(bar && barNumber % barLabelStep == 0) {
            char beatLabel[16];
            std::snprintf(beatLabel, sizeof(beatLabel), "%d", barNumber + 1);
            dl->AddText(ImVec2(x + 5, rulerDividerY + 5), IM_COL32(235, 235, 235, 255), beatLabel);
        }
    }

    const int totalSeconds = static_cast<int>(std::floor(timeline.beatToSeconds(endBeat, bpm)));
    // Tick every second (fewer when zoomed far out); label every Nth tick so
    // labels stay about 38 px apart and don't shift while scrolling.
    const int secondsStep = std::max(1, static_cast<int>(std::ceil(3.0 / std::max(0.001f, pixelsPerSecond))));
    const int labelEvery = std::max(1, static_cast<int>(std::ceil(38.0 / std::max(0.001f, pixelsPerSecond * secondsStep))));
    const int firstSecond = std::max(0, static_cast<int>(std::floor(timelineScrollX / std::max(0.001f, pixelsPerSecond))) / secondsStep * secondsStep);
    const int lastSecond = std::min(totalSeconds, static_cast<int>(std::ceil((timelineScrollX + timelineWidth) / std::max(0.001f, pixelsPerSecond))) + secondsStep);
    for(int second = firstSecond; second <= lastSecond; second += secondsStep) {
        const float x = laneMin.x + second * pixelsPerSecond;
        dl->AddLine(ImVec2(x, min.y), ImVec2(x, rulerDividerY), IM_COL32(90, 90, 100, 145), 1.0f);
        if((second / secondsStep) % labelEvery == 0) {
            char secondsLabel[20];
            std::snprintf(secondsLabel, sizeof(secondsLabel), "%ds", second);
            dl->AddText(ImVec2(x + 4.0f, min.y + 5.0f), IM_COL32(175, 175, 185, 255), secondsLabel);
        }
    }

    if(timeline.isLoopEnabled()) {
        const float loopX1 = laneMin.x + beatToPixels(timeline, timeline.getLoopStartBeat(), bpm);
        const float loopX2 = laneMin.x + beatToPixels(timeline, timeline.getLoopEndBeat(), bpm);
        dl->AddRectFilled(ImVec2(loopX1, min.y), ImVec2(loopX2, max.y), IM_COL32(95, 105, 220, 48));
        dl->AddRectFilled(ImVec2(loopX1 - 3.0f, min.y), ImVec2(loopX1 + 3.0f, max.y), IM_COL32(150, 165, 255, 230));
        dl->AddRectFilled(ImVec2(loopX2 - 3.0f, min.y), ImVec2(loopX2 + 3.0f, max.y), IM_COL32(150, 165, 255, 230));
    }
    // Markers: a flag in the lower part of the seconds strip, with a line down
    // through the ruler. Click jumps there, drag moves it, double-click renames,
    // right-click for the menu.
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    std::string hoveredMarkerId;
    for(const auto& marker : timeline.getMarkers()) {
        const float x = laneMin.x + beatToPixels(timeline, marker.beat, bpm);
        if(x < zoneLeft - 200.0f || x > max.x) continue;
        const ImVec2 textSize = ImGui::CalcTextSize(marker.name.c_str());
        const ImVec2 flagMin(x, min.y + 15.0f), flagMax(x + textSize.x + 8.0f, rulerDividerY - 1.0f);
        const bool flagHovered = ImGui::IsItemHovered() && hoverRect(ImVec2(x - 3.0f, flagMin.y), flagMax) && mouse.x >= zoneLeft;
        if(flagHovered) hoveredMarkerId = marker.id;
        const bool active = flagHovered || markerDragId == marker.id;
        dl->AddLine(ImVec2(x, min.y + 15.0f), ImVec2(x, max.y), IM_COL32(255, 196, 64, active ? 255 : 200), 1.5f);
        dl->AddRectFilled(flagMin, flagMax, active ? IM_COL32(255, 206, 90, 255) : IM_COL32(230, 170, 50, 225), 2.0f);
        dl->AddText(ImVec2(x + 4.0f, flagMin.y + 0.5f), IM_COL32(30, 20, 0, 255), marker.name.c_str());
    }
    const float px = laneMin.x + beatToPixels(timeline, beatPosition, bpm);
    if(px >= zoneLeft && px <= max.x) dl->AddLine(ImVec2(px, min.y), ImVec2(px, max.y), kPlayhead, 2.5f);
    dl->PopClipRect();

    const bool hovered = ImGui::IsItemHovered() && mouse.x >= zoneLeft;
    const double rawMouseBeat = ofClamp(
        pixelsToBeat(timeline, mouse.x - laneMin.x, bpm, endBeat), 0.0, endBeat);
    const double mouseBeat = snapBeat(rawMouseBeat);
    if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        markerMenuId = hoveredMarkerId;
        markerMenuBeat = mouseBeat;
        ImGui::OpenPopup("##rulerMarkerMenu");
    }
    if(ImGui::BeginPopup("##rulerMarkerMenu")) {
        if(const auto* marker = markerMenuId.empty() ? nullptr : timeline.getMarker(markerMenuId)) {
            ImGui::TextDisabled("%s", marker->name.c_str());
            ImGui::Separator();
            if(ImGui::MenuItem("Jump here")) jumpToBeat(marker->beat);
            if(ImGui::MenuItem("Rename...")) {
                markerRenameId = marker->id;
                std::strncpy(markerRenameBuffer, marker->name.c_str(), sizeof(markerRenameBuffer) - 1);
                markerRenameBuffer[sizeof(markerRenameBuffer) - 1] = '\0';
                requestMarkerRename = true;
            }
            if(ImGui::MenuItem("Delete marker")) timeline.removeMarker(markerMenuId);
        } else {
            if(ImGui::MenuItem("Add marker here")) timeline.addMarker(markerMenuBeat);
            if(ImGui::MenuItem("Add marker at playhead", "M")) timeline.addMarker(snapBeat(beatPosition));
        }
        ImGui::Separator();
        ImGui::MenuItem("Wait for the next bar to jump", nullptr, &quantizeJumps);
        ImGui::EndPopup();
    }
    if(!hoveredMarkerId.empty() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if(const auto* marker = timeline.getMarker(hoveredMarkerId)) {
            markerRenameId = marker->id;
            std::strncpy(markerRenameBuffer, marker->name.c_str(), sizeof(markerRenameBuffer) - 1);
            markerRenameBuffer[sizeof(markerRenameBuffer) - 1] = '\0';
            requestMarkerRename = true;
        }
        markerDragId.clear();
    } else if(!hoveredMarkerId.empty() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        markerDragId = hoveredMarkerId;
        markerDragStartX = mouse.x;
        markerDragMoved = false;
    } else if(hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        const double loopLength = timeline.getBeatsPerBar();
        const double loopStart = snapBeat(std::max(0.0, mouseBeat - loopLength * 0.5));
        timeline.setLoopEnabled(true);
        timeline.setLoopRange(loopStart, loopStart + loopLength);
        loopDragMode = LoopDragMode::None;
    } else if(hovered && markerDragId.empty() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
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
            container->getTransport()->seekToBeat(mouseBeat);
        }
    }
    if(loopDragMode != LoopDragMode::None && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if(loopDragMode == LoopDragMode::Scrub) {
            if(container->getTransport() != nullptr) container->getTransport()->seekToBeat(mouseBeat);
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
    if(!markerDragId.empty()) {
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if(std::abs(mouse.x - markerDragStartX) >= std::max(3.0f, ImGui::GetIO().MouseDragThreshold)) markerDragMoved = true;
            if(markerDragMoved) {
                timeline.moveMarker(markerDragId, mouseBeat);
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
        } else {
            // A click (no drag) on a marker jumps there.
            if(!markerDragMoved) {
                if(const auto* marker = timeline.getMarker(markerDragId)) jumpToBeat(marker->beat);
            }
            markerDragId.clear();
        }
    }

    // Tempo (BPM) lane visibility: a checkbox in the ruler's label column, and the
    // same toggle on right-click anywhere in that column.
    {
        bool tempoVisible = timeline.isBpmLaneVisible();
        ImGui::SetCursorScreenPos(ImVec2(min.x + labelWidth - 104.0f, min.y + 5.0f));
        if(ImGui::Checkbox("Tempo lane##rulerTempoLane", &tempoVisible)) timeline.setBpmLaneVisible(tempoVisible);
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Show or hide the tempo (BPM) automation row.\nHidden, tempo automation still plays.");
        if(hoverRect(min, ImVec2(zoneLeft, max.y)) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("##rulerLabelMenu");
        if(ImGui::BeginPopup("##rulerLabelMenu")) {
            if(ImGui::MenuItem("Show tempo (BPM) lane", nullptr, timeline.isBpmLaneVisible()))
                timeline.setBpmLaneVisible(!timeline.isBpmLaneVisible());
            ImGui::EndPopup();
        }
    }
}

void ofxOceanodeTimelineController::drawBpmLane(ofxOceanodeTimelineManager& timeline,
                                                float contentWidth, double endBeat,
                                                double beatPosition, float fallbackBpm) {
    const float timelineScrollX = timeline.getViewState().scrollX;
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
            if(bpmDragPointIndex >= 0 && ImGui::GetIO().KeyShift) {
                // Matches the regular curve lane editor's shift-click-to-delete
                // gesture. Tensions are keyed by segment index, so removing a
                // point shifts every tension after it -- reset them the same
                // way the curve editor does rather than leave stale shaping on
                // the wrong (now different) pair of neighbours.
                sourcePoints.erase(sourcePoints.begin() + bpmDragPointIndex);
                sourceTensions.assign(sourcePoints.size() > 0 ? sourcePoints.size() - 1 : 0,
                                      ofxOceanodeTimelineCurveTension{});
                bpmDragPointIndex = -1;
                bpmValuePointIndex = -1;
            } else {
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

void ofxOceanodeTimelineController::drawWaveClipProperties(ofxOceanodeTimelineManager& timeline,
                                                            const ofxOceanodeTimelineTrack& track,
                                                            ofxOceanodeTimelineClip& clip) {
    if(!track.isWaveTrack) return;

    ImGui::SeparatorText("Audio clip properties");
    ImGui::Text("Name: %s", clip.name.c_str());
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Double-click to rename this clip");
        if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) requestClipRename(track.id, clip);
    }
    ImGui::Text("Channels: %d   Duration: %.3fs", clip.waveNumChannels,
                clip.waveFileDurationMs / 1000.0);
    const std::string sampleName = clip.waveFilePath.empty()
        ? "No file loaded" : ofFilePath::getFileName(clip.waveFilePath);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 4.0f);
    ImGui::TextWrapped("Sample: %s", sampleName.c_str());
    ImGui::PopTextWrapPos();
    if(!clip.waveFilePath.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", clip.waveFilePath.c_str());
    if(ImGui::Button("Replace sample", ImVec2(-1.0f, 0.0f))) {
        waveFileRequest = WaveFileRequest::ReplaceClip;
        waveFileRequestTrackId = track.id;
        waveFileRequestClipId = clip.id;
    }
    if(ImGui::Button("Reload waveform", ImVec2(-1.0f, 0.0f))) timeline.reloadWaveform(track.id, clip.id);

    ImGui::TextUnformatted("Playback rate");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::DragFloat("##wavePlaybackRate", &clip.wavePlaybackRate, 0.01f, 0.1f, 4.0f, "%.2fx");
    ImGui::Checkbox("Reverse", &clip.waveReverse);
    ImGui::TextUnformatted("Clip gain");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::DragFloat("##waveGain", &clip.waveGain, 0.01f, 0.0f, 4.0f, "%.2f");
    ImGui::TextDisabled("Volume automation is shown on the right and applies to the whole track.");
    ImGui::Separator();
}

void ofxOceanodeTimelineController::drawWaveTrackVolumeAutomation(
    ofxOceanodeTimelineManager& timeline, ofxOceanodeTimelineTrack& track,
    float width, float height, double endBeat, double beatPosition,
    float timelineOriginX) {
    ImGui::TextUnformatted("Track volume");
    ImGui::SameLine();
    // Both of these are read by evaluateWaveTrackVolume and persisted, and
    // neither had any way to be set: the flag was forced on here every frame
    // and the shape was stuck at whatever a preset happened to hold.
    ImGui::Checkbox("Automate", &track.waveVolumeAutomationEnabled);
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("Off plays the track at its gain alone; the envelope stays as you left it");
    const float controlWidth = std::min(96.0f, std::max(58.0f, width * 0.22f));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(controlWidth);
    ImGui::DragFloat("Gain", &track.waveVolume, 0.01f, 0.0f, 4.0f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::BeginCombo("Shape", track.waveVolumeInterpolation.c_str())) {
        for(const char* option : {"Step", "Linear", "Log / Exp", "Sigmoid"}) {
            if(ImGui::Selectable(option, track.waveVolumeInterpolation == option))
                track.waveVolumeInterpolation = option;
        }
        ImGui::EndCombo();
    }
    // Through the manager's accessor rather than the member, so every path
    // that touches these points goes through one door.
    auto& points = timeline.getWaveTrackVolumePoints(track.id);
    if(points.empty()) {
        const double automationEnd = std::max(1.0, endBeat);
        points = {{0.0, track.waveVolume}, {automationEnd, track.waveVolume}};
        track.waveVolumeTensions.assign(1, ofxOceanodeTimelineCurveTension{});
    }

    const float graphHeight = std::max(80.0f, height - 58.0f);
    ImGui::InvisibleButton("##waveTrackVolumeCanvas",
                           ImVec2(std::max(40.0f, width - 8.0f), graphHeight));
    const ImVec2 graphMin = ImGui::GetItemRectMin();
    const ImVec2 graphMax = ImGui::GetItemRectMax();
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(graphMin, graphMax, IM_COL32(22, 24, 30, 220), 3.0f);
    dl->AddRect(graphMin, graphMax, IM_COL32(track.color.r, track.color.g, track.color.b, 180), 3.0f);

    const float graphBpm = container != nullptr ? container->getTransportState().bpm : 120.0f;
    auto xForBeat = [&](double beat) {
        return timelineOriginX + beatToPixels(timeline, beat, graphBpm);
    };
    auto yForValue = [&](float value) {
        return graphMax.y - 5.0f - ofClamp(value, 0.0f, 4.0f) / 4.0f * (graphMax.y - graphMin.y - 10.0f);
    };
    auto beatForX = [&](float x) {
        return pixelsToBeat(timeline, x - timelineOriginX, graphBpm, endBeat);
    };

    // Match the timeline's vertical beat/bar grid in the automation panel so
    // envelope points can be read against the same musical positions as the
    // clips above.
    const double grid = displayGridBeats();
    const int gridLines = static_cast<int>(std::ceil(endBeat / grid));
    for(int i = 0; i <= gridLines; ++i) {
        const double beat = i * grid;
        const float x = xForBeat(beat);
        if(x < graphMin.x || x > graphMax.x) continue;
        const bool bar = std::fmod(beat, timeline.getBeatsPerBar()) < 0.001;
        const bool quarter = std::fmod(beat, 1.0) < 0.001;
        dl->AddLine(ImVec2(x, graphMin.y), ImVec2(x, graphMax.y),
                    bar ? IM_COL32(150, 155, 190, 125)
                        : quarter ? IM_COL32(105, 110, 135, 100)
                                  : IM_COL32(75, 80, 100, 75),
                    bar ? 1.5f : 1.0f);
    }

    for(int level = 0; level <= 4; ++level) {
        const float y = yForValue(static_cast<float>(level));
        dl->AddLine(ImVec2(graphMin.x, y), ImVec2(graphMax.x, y),
                    level == 1 ? IM_COL32(190, 190, 205, 120) : IM_COL32(90, 95, 110, 90));
        if(level < 4)
            dl->AddText(ImVec2(graphMin.x + 5.0f, y - 14.0f), IM_COL32(150, 155, 170, 190),
                        ofToString(level).c_str());
    }
    const float playheadX = xForBeat(beatPosition);
    if(playheadX >= graphMin.x && playheadX <= graphMax.x)
        dl->AddLine(ImVec2(playheadX, graphMin.y), ImVec2(playheadX, graphMax.y), kPlayhead, 1.5f);

    std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) { return a.beat < b.beat; });
    for(size_t i = 1; i < points.size(); ++i) {
        dl->AddLine(ImVec2(xForBeat(points[i - 1].beat), yForValue(points[i - 1].value)),
                    ImVec2(xForBeat(points[i].beat), yForValue(points[i].value)),
                    IM_COL32(255, 220, 90, 235), 2.0f);
    }
    for(const auto& point : points) {
        const float x = xForBeat(point.beat);
        if(x < graphMin.x - 6.0f || x > graphMax.x + 6.0f) continue;
        dl->AddCircleFilled(ImVec2(x, yForValue(point.value)), 4.0f, IM_COL32(255, 220, 90, 255));
    }

    if(hovered) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        int nearest = -1;
        float nearestDistance = 9.0f;
        for(size_t i = 0; i < points.size(); ++i) {
            const float distance = std::abs(mouse.x - xForBeat(points[i].beat));
            if(distance < nearestDistance) {
                nearest = static_cast<int>(i);
                nearestDistance = distance;
            }
        }
        if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            const double beat = snapBeat(ofClamp(beatForX(mouse.x), 0.0, endBeat));
            const float value = ofClamp(4.0f * (graphMax.y - 5.0f - mouse.y) /
                                        std::max(1.0f, graphMax.y - graphMin.y - 10.0f), 0.0f, 4.0f);
            timeline.addWaveTrackVolumePoint(track.id, beat, value);
        } else if(ImGui::IsMouseClicked(ImGuiMouseButton_Right) && nearest >= 0) {
            // Right-click: set an exact value or delete (was: delete immediately).
            waveVolumeValuePointIndex = nearest;
            waveVolumeNumericValue = points[nearest].value;
            ImGui::OpenPopup("Volume point value");
        } else if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && nearest >= 0) {
            waveVolumeDragPointIndex = nearest;
        }
    }
    if(waveVolumeDragPointIndex >= 0 && waveVolumeDragPointIndex < static_cast<int>(points.size())) {
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            points[waveVolumeDragPointIndex].beat = snapBeat(ofClamp(beatForX(mouse.x), 0.0, endBeat));
            points[waveVolumeDragPointIndex].value = ofClamp(4.0f * (graphMax.y - 5.0f - mouse.y) /
                                                               std::max(1.0f, graphMax.y - graphMin.y - 10.0f),
                                                               0.0f, 4.0f);
        }
        if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) { return a.beat < b.beat; });
            track.waveVolumeTensions.resize(points.size() > 1 ? points.size() - 1 : 0);
            waveVolumeDragPointIndex = -1;
        }
    }
    if(ImGui::BeginPopup("Volume point value")) {
        if(waveVolumeValuePointIndex >= 0 && waveVolumeValuePointIndex < static_cast<int>(points.size())) {
            ImGui::SetNextItemWidth(110.0f);
            if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            if(ImGui::InputFloat("Value", &waveVolumeNumericValue, 0.0f, 0.0f, "%.4g"))
                points[waveVolumeValuePointIndex].value = ofClamp(waveVolumeNumericValue, 0.0f, 4.0f);
            if(ImGui::IsItemDeactivatedAfterEdit()) waveVolumeNumericValue = ofClamp(waveVolumeNumericValue, 0.0f, 4.0f);
            ImGui::Separator();
            // Decide once: deleting changes the point count before EndDisabled.
            const bool deleteDisabled = points.size() <= 2;
            if(deleteDisabled) ImGui::BeginDisabled();
            if(ImGui::MenuItem("Delete point")) {
                timeline.removeWaveTrackVolumePoint(track.id, points[waveVolumeValuePointIndex].beat);
                waveVolumeValuePointIndex = -1;
            }
            if(deleteDisabled) ImGui::EndDisabled();
        } else {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ofxOceanodeTimelineController::drawWaveTrackEditor(
    ofxOceanodeTimelineManager& timeline, const ofxOceanodeTimelineTrack& track,
    float contentWidth, double endBeat, double beatPosition) {
    const float timelineScrollX = timeline.getViewState().scrollX;
    auto* editTrack = timeline.getTrack(track.id);
    auto* clip = timeline.getClip(editorTrackId, editorClipId);
    if(editTrack == nullptr || clip == nullptr) {
        clipEditorOpen = false;
        return;
    }

    const float editorHeight = 250.0f;
    ImGui::Dummy(ImVec2(contentWidth, editorHeight));
    const ImVec2 editorMin = ImGui::GetItemRectMin();
    const float gap = 8.0f;
    const float propertiesWidth = std::min(kLabelWidth - 10.0f, contentWidth * 0.38f);
    const float automationWidth = std::max(40.0f, contentWidth - propertiesWidth - gap);

    ImGui::SetCursorScreenPos(editorMin);
    // Let the properties column scroll vertically if a narrow panel needs it;
    // the sample label and controls must never paint outside the column.
    ImGui::BeginChild("##waveClipPropertiesPanel", ImVec2(propertiesWidth, editorHeight), true);
    drawWaveClipProperties(timeline, track, *clip);
    ImGui::EndChild();

    ImGui::SetCursorScreenPos(ImVec2(editorMin.x + propertiesWidth + gap, editorMin.y));
    ImGui::BeginChild("##waveTrackVolumePanel", ImVec2(automationWidth, editorHeight), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawWaveTrackVolumeAutomation(timeline, *editTrack, automationWidth, editorHeight,
                                   endBeat, beatPosition,
                                   editorMin.x + kLabelWidth - timelineScrollX);
    ImGui::EndChild();
    finishAbsoluteLayout(ImVec2(editorMin.x, editorMin.y + editorHeight));
}

void ofxOceanodeTimelineController::drawLfoEditor(ofxOceanodeTimelineManager& timeline,
                                                  const ofxOceanodeTimelineTrack& track,
                                                  ofxOceanodeTimelineClip& clip,
                                                  float contentWidth, double endBeat,
                                                  double beatPosition) {
    const float timelineScrollX = timeline.getViewState().scrollX;
    // Laid out like drawLaneEditor's rows rather than as one fixed block: a
    // row per section, each reserving its own height from laneEditorHeights,
    // foldable through collapsedLaneIds and resizable by the same bottom
    // strip. The result view is a row like any other -- it just has no lane
    // behind it, so it borrows a key that cannot collide with a lane id.
    const float clipIndent = 20.0f;
    const std::string resultKey = "lfoResult:" + clip.id;
    constexpr float kDefaultResultHeight = 230.0f;
    constexpr float kDefaultLaneHeight = 120.0f;

    ImGui::Dummy(ImVec2(contentWidth, 22.0f));
    const ImVec2 headerMin = ImGui::GetItemRectMin();
    const ImVec2 headerMax = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(headerMin.x + clipIndent, headerMin.y),
                      ImVec2(headerMin.x + kLabelWidth, headerMax.y),
                      mutedTrackColor(track.color, 0.28f, 0.46f));
    dl->AddLine(ImVec2(headerMin.x + kLabelWidth - 1.0f, headerMin.y),
                ImVec2(headerMin.x + kLabelWidth - 1.0f, headerMax.y),
                IM_COL32(track.color.r, track.color.g, track.color.b, 210));
    ImGui::SetCursorScreenPos(ImVec2(headerMin.x + 5.0f + clipIndent, headerMin.y + 2.0f));
    const bool editorFolded = foldedClipEditors.count(clip.id) > 0;
    if(ImGui::SmallButton(editorFolded ? ">##foldLfoEditor" : "v##foldLfoEditor")) {
        if(editorFolded) foldedClipEditors.erase(clip.id); else foldedClipEditors.insert(clip.id);
    }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip(editorFolded ? "Unfold the clip editor" : "Fold the clip editor to this header");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(track.color.r / 255.0f, track.color.g / 255.0f,
                              track.color.b / 255.0f, 1.0f),
                       "%s  [LFO]", clip.name.c_str());
    if(ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Double-click to rename this clip");
        if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) requestClipRename(track.id, clip);
    }
    ImGui::SameLine(kLabelWidth - 50.0f);
    if(ImGui::SmallButton("x##closeLfoEditor")) clipEditorOpen = false;
    finishAbsoluteLayout(ImVec2(headerMin.x, headerMax.y));
    if(editorFolded) return; // folded: only the header row is shown

    // Every row is a full-width block starting at the same x, so one origin
    // serves them all -- and it is kLabelWidth, the same gutter the clip rows
    // and every other lane editor use.
    const float rowOriginX = headerMin.x;
    const float graphOriginX = rowOriginX + kLabelWidth - timelineScrollX;
    const float graphWidth = std::max(40.0f, contentWidth - kLabelWidth);
    const float fallbackBpm = container->getTransportState().bpm;
    const double contentDuration = std::max(1.0 / kPPQ, clip.contentDurationBeats);
    const ImU32 lfoColor = IM_COL32(track.color.r, track.color.g, track.color.b, 235);
    const int samples = std::max(64, static_cast<int>(graphWidth / 2.0f));
    const double resultSourceBeat = timelineToSourceBeat(clip, beatPosition);

    auto xForSource = [&](double sourceBeat) {
        return graphOriginX + beatToPixels(timeline, sourceBeat, fallbackBpm);
    };
    auto sourceForX = [&](float x) {
        return pixelsToBeat(timeline, x - graphOriginX, fallbackBpm, contentDuration);
    };
    auto drawGrid = [&](const ImVec2& graphMin, const ImVec2& graphMax) {
        const double grid = std::max(1.0 / kPPQ, displayGridBeats());
        const int gridLines = static_cast<int>(std::ceil(contentDuration / grid));
        for(int i = 0; i <= gridLines; ++i) {
            const double beat = std::min(contentDuration, i * grid);
            const float x = xForSource(beat);
            if(x < graphMin.x || x > graphMax.x) continue;
            const bool bar = std::fmod(beat, timeline.getBeatsPerBar()) < 0.001;
            dl->AddLine(ImVec2(x, graphMin.y), ImVec2(x, graphMax.y),
                        bar ? kBar : kGrid, bar ? 1.5f : 1.0f);
        }
    };
    auto drawPlayhead = [&](const ImVec2& graphMin, const ImVec2& graphMax) {
        const float x = xForSource(resultSourceBeat);
        if(x >= graphMin.x && x <= graphMax.x)
            dl->AddLine(ImVec2(x, graphMin.y), ImVec2(x, graphMax.y), kPlayhead, 1.5f);
    };

    // One folded row, shared by the result view and the lanes. Returns true
    // when its label was clicked, which is how a lane takes focus.
    auto drawCollapsedRow = [&](const std::string& key, const std::string& label, bool focused) {
        ImGui::Dummy(ImVec2(contentWidth, 22.0f));
        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        ImDrawList* rowDl = ImGui::GetWindowDrawList();
        rowDl->AddRectFilled(ImVec2(rowMin.x + clipIndent, rowMin.y),
                             ImVec2(rowMin.x + kLabelWidth, rowMax.y),
                             mutedTrackColor(track.color, 0.18f, 0.30f));
        ImGui::SetCursorScreenPos(ImVec2(rowMin.x + 5.0f + clipIndent, rowMin.y + 2.0f));
        if(ImGui::SmallButton((">##lfoExpand" + key).c_str())) collapsedLaneIds.erase(key);
        ImGui::SameLine();
        ImGui::Selectable((label + "##lfoHeaderCollapsed" + key).c_str(), focused,
                          ImGuiSelectableFlags_None, ImVec2(ImGui::GetContentRegionAvail().x - 4.0f, 0));
        const bool clicked = ImGui::IsItemClicked();
        finishAbsoluteLayout(ImVec2(rowMin.x, rowMax.y));
        return clicked;
    };

    struct LfoRow {
        ImVec2 rowMin, rowMax, graphMin, graphMax;
        float height = 0.0f;
        bool canvasHovered = false;
    };
    // Reserves one row, paints its background, runs the bottom resize strip
    // and hands back the graph rectangle -- the same sequence, in the same
    // order, as an ordinary lane editor row.
    auto beginRow = [&](const std::string& key, float defaultHeight, bool focused) {
        LfoRow row;
        const auto heightIt = laneEditorHeights.find(key);
        row.height = heightIt != laneEditorHeights.end() ? heightIt->second : defaultHeight;
        ImGui::Dummy(ImVec2(contentWidth, row.height));
        row.rowMin = ImGui::GetItemRectMin();
        row.rowMax = ImGui::GetItemRectMax();
        const float zoneLeft = row.rowMin.x + kLabelWidth;
        // The canvas stops short of the resize strip so the two can never
        // fight over the same click.
        ImGui::SetCursorScreenPos(ImVec2(zoneLeft, row.rowMin.y));
        ImGui::InvisibleButton(("##lfoRowCanvas" + editorTrackId + editorClipId + key).c_str(),
                               ImVec2(std::max(1.0f, contentWidth - kLabelWidth),
                                      std::max(1.0f, row.height - kLaneResizeHandleHeight)));
        row.canvasHovered = ImGui::IsItemHovered() && ImGui::GetIO().MousePos.x >= zoneLeft;

        dl->AddRectFilled(ImVec2(row.rowMin.x + clipIndent, row.rowMin.y), row.rowMax,
                          IM_COL32(27, 29, 35, 235));
        dl->AddRectFilled(ImVec2(row.rowMin.x + clipIndent, row.rowMin.y),
                          ImVec2(zoneLeft, row.rowMax.y),
                          mutedTrackColor(track.color, focused ? 0.30f : 0.21f, 0.42f));
        dl->AddLine(ImVec2(zoneLeft - 1.0f, row.rowMin.y), ImVec2(zoneLeft - 1.0f, row.rowMax.y),
                    IM_COL32(track.color.r, track.color.g, track.color.b, focused ? 220 : 150));

        const ImVec2 handleMin(row.rowMin.x + clipIndent, row.rowMax.y - kLaneResizeHandleHeight);
        const ImVec2 handleMax(row.rowMax.x, row.rowMax.y);
        const bool resizeHovered = hoverRect(handleMin, handleMax);
        if(resizeHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            resizingLaneId = key;
            laneResizeStartMouseY = ImGui::GetIO().MousePos.y;
            laneResizeStartHeight = row.height;
        }
        const bool resizeActive = resizingLaneId == key && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if(resizeHovered || resizeActive) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if(resizeActive) {
            laneEditorHeights[key] = ofClamp(laneResizeStartHeight + ImGui::GetIO().MousePos.y - laneResizeStartMouseY,
                                             kLaneEditorMinHeight, kLaneEditorMaxHeight);
        }
        if(resizingLaneId == key && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) resizingLaneId.clear();
        const float separatorY = row.rowMax.y - kLaneResizeHandleHeight * 0.5f;
        dl->AddLine(ImVec2(handleMin.x, separatorY), ImVec2(handleMax.x, separatorY),
                    resizeActive ? IM_COL32(205, 205, 215, 235)
                                 : resizeHovered ? IM_COL32(160, 160, 170, 205)
                                                 : IM_COL32(80, 80, 86, 110),
                    resizeActive ? 2.0f : 1.0f);

        row.graphMin = ImVec2(zoneLeft, row.rowMin.y + 5.0f);
        row.graphMax = ImVec2(row.rowMax.x, row.rowMax.y - 5.0f - kLaneResizeHandleHeight);
        return row;
    };
    // Opens the row's left-hand properties child with its fold button; the
    // caller fills it and closes it with EndChild, hence the "begin" name.
    auto beginRowProperties = [&](const LfoRow& row, const std::string& key) {
        ImGui::SetCursorScreenPos(ImVec2(row.rowMin.x + 5.0f + clipIndent, row.rowMin.y + 4.0f));
        ImGui::BeginChild(("##lfoRowProperties" + editorTrackId + editorClipId + key).c_str(),
                          ImVec2(kLabelWidth - 10.0f - clipIndent,
                                 std::max(1.0f, row.height - 8.0f - kLaneResizeHandleHeight)), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        if(ImGui::SmallButton(("v##lfoCollapse" + key).c_str())) collapsedLaneIds.insert(key);
        ImGui::SameLine();
    };

    // --- result row ------------------------------------------------------
    // Sampled from the same evaluator playback uses, so editing any control
    // lane below previews exactly what the target binding receives.
    if(collapsedLaneIds.count(resultKey) > 0) {
        drawCollapsedRow(resultKey, "LFO result", false);
    } else {
        const auto row = beginRow(resultKey, kDefaultResultHeight, false);
        beginRowProperties(row, resultKey);
        ImGui::TextUnformatted("LFO result");
        const auto* outputBinding = clip.lfoOutputBindingId.empty()
            ? nullptr : timeline.getBinding(track.id, clip.lfoOutputBindingId);
        const std::string outputPreview = outputBinding == nullptr
            ? "No output target" : compactParameterName(outputBinding->parameterPath);
        ImGui::SetNextItemWidth(-1.0f);
        if(ImGui::BeginCombo("Output", outputPreview.c_str())) {
            for(const auto& binding : track.bindings) {
                const bool selected = binding.id == clip.lfoOutputBindingId;
                if(ImGui::Selectable(compactParameterName(binding.parameterPath).c_str(), selected)) {
                    clip.lfoOutputBindingId = binding.id;
                    if(binding.valueType == typeid(float).name()) {
                        if(auto* parameter = container->findTimelineParameter(binding.parameterPath)) {
                            clip.lfoOutputMin = parameter->cast<float>().getParameter().getMin();
                            clip.lfoOutputMax = parameter->cast<float>().getParameter().getMax();
                        }
                    } else if(binding.valueType == typeid(int).name()) {
                        if(auto* parameter = container->findTimelineParameter(binding.parameterPath)) {
                            clip.lfoOutputMin = static_cast<float>(parameter->cast<int>().getParameter().getMin());
                            clip.lfoOutputMax = static_cast<float>(parameter->cast<int>().getParameter().getMax());
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("The LFO result is mapped to this range.");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("Min", &clip.lfoOutputMin, 0.01f, -99999.0f, 99999.0f, "%.4g");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("Max", &clip.lfoOutputMax, 0.01f, -99999.0f, 99999.0f, "%.4g");
        if(clip.lfoOutputMax < clip.lfoOutputMin) std::swap(clip.lfoOutputMax, clip.lfoOutputMin);
        ImGui::EndChild();

        drawGrid(row.graphMin, row.graphMax);
        for(int level = 0; level <= 4; ++level) {
            const float y = row.graphMax.y - 8.0f - level / 4.0f *
                (row.graphMax.y - row.graphMin.y - 16.0f);
            dl->AddLine(ImVec2(row.graphMin.x, y), ImVec2(row.graphMax.x, y),
                        level == 0 || level == 4 ? IM_COL32(130, 135, 150, 125) : IM_COL32(75, 80, 95, 90));
        }
        dl->PushClipRect(row.graphMin, row.graphMax, true);
        for(int i = 0; i < samples; ++i) {
            const double source1 = contentDuration * i / static_cast<double>(samples);
            const double source2 = contentDuration * (i + 1) / static_cast<double>(samples);
            const float value1 = ofxOceanodeTimelineLfo::evaluate(clip, source1);
            const float value2 = ofxOceanodeTimelineLfo::evaluate(clip, source2);
            const ImVec2 a(xForSource(source1), row.graphMax.y - 8.0f - value1 *
                           (row.graphMax.y - row.graphMin.y - 16.0f));
            const ImVec2 b(xForSource(source2), row.graphMax.y - 8.0f - value2 *
                           (row.graphMax.y - row.graphMin.y - 16.0f));
            dl->AddLine(a, b, lfoColor, 2.0f);
        }
        dl->PopClipRect();
        drawPlayhead(row.graphMin, row.graphMax);
        finishAbsoluteLayout(ImVec2(row.rowMin.x, row.rowMax.y));
    }

    // --- one row per oscillator control ----------------------------------
    bool anyLfoCanvasHovered = false;
    for(size_t laneIndex = 0; laneIndex < clip.lanes.size(); ++laneIndex) {
        auto& lane = clip.lanes[laneIndex];
        const bool focused = editorLaneId == lane.id;
        const float current = ofxOceanodeTimelineLfo::evaluateParameter(clip, lane.lfoParameter,
                                                                        resultSourceBeat, 0.0f);
        if(collapsedLaneIds.count(lane.id) > 0) {
            char collapsedLabel[96];
            std::snprintf(collapsedLabel, sizeof(collapsedLabel), "%s   %.3g", lane.name.c_str(), current);
            if(drawCollapsedRow(lane.id, collapsedLabel, focused)) editorLaneId = lane.id;
            continue;
        }

        const auto row = beginRow(lane.id, kDefaultLaneHeight, focused);
        anyLfoCanvasHovered = anyLfoCanvasHovered || row.canvasHovered;
        beginRowProperties(row, lane.id);
        if(ImGui::Selectable((lane.name + "##lfoLaneHeader" + lane.id).c_str(), focused,
                             ImGuiSelectableFlags_None, ImVec2(ImGui::GetContentRegionAvail().x - 4.0f, 0))) {
            editorLaneId = lane.id;
        }
        ImGui::TextDisabled("%.3g", current);
        const float laneSpan = lane.valueMax - lane.valueMin;
        {
            int interpolationMode = lane.curveInterpolation == "Value" ? 0
                : 1 + static_cast<int>(curveInterpolationMode(lane.curveInterpolation));
            const int previousMode = interpolationMode;
            const float fieldWidth = std::max(40.0f, std::min(105.0f, ImGui::GetContentRegionAvail().x - 4.0f));
            ImGui::SetNextItemWidth(fieldWidth);
            if(ImGui::Combo(("##lfoInterpolation" + lane.id).c_str(), &interpolationMode, kLfoInterpolationNames, 5) &&
               interpolationMode != previousMode) {
                if(interpolationMode == 0) {
                    // Entering Value mode: start from what the lane outputs at the playhead.
                    lane.lfoValue = ofxOceanodeTimelineLfo::laneNormalizedFromValue(lane, current);
                } else {
                    if(previousMode != 0) resetCurveTensions(lane);
                    // Leaving Value mode keeps the stored points; a lane without any
                    // starts from the constant it held.
                    if(lane.curvePoints.empty()) {
                        lane.curvePoints.push_back({0.0, lane.lfoValue});
                        resetCurveTensions(lane);
                    }
                }
                lane.curveInterpolation = kLfoInterpolationNames[interpolationMode];
                lfoTensionSegment = -1;
                if(lfoSelectedLaneId == lane.id) lfoSelectedPointIndex = -1;
            }
            if(lane.curveInterpolation == "Value") {
                // Drag, or double-click / Ctrl+click to type a value.
                float realValue = ofxOceanodeTimelineLfo::laneValueFromNormalized(lane, lane.lfoValue);
                const float lo = std::min(lane.valueMin, lane.valueMax);
                const float hi = std::max(lane.valueMin, lane.valueMax);
                ImGui::SetNextItemWidth(fieldWidth);
                if(ImGui::DragFloat(("##lfoValue" + lane.id).c_str(), &realValue,
                                    lane.lfoParameter == "frequency"
                                        ? std::max(1e-3f, std::abs(realValue) * 0.01f) // proportional: fine at short cycles
                                        : std::max(1e-4f, std::abs(laneSpan) / 300.0f),
                                    lo, hi, "%.4g")) {
                    realValue = ofClamp(realValue, lo, hi);
                    lane.lfoValue = ofxOceanodeTimelineLfo::laneNormalizedFromValue(lane, realValue);
                }
            } else {
                const auto modeShape = curveInterpolationMode(lane.curveInterpolation);
                if(modeShape == CurveInterpolationMode::LogExp) ImGui::TextDisabled("Alt-drag vertically");
                else if(modeShape == CurveInterpolationMode::Sigmoid) ImGui::TextDisabled("Alt-drag freely");
            }
            if(lane.lfoParameter == "frequency") {
                ImGui::Checkbox(("Snap##lfoSnap" + lane.id).c_str(), &lane.lfoSnap);
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("Snap edits to the grid lengths shown in the lane");
                ImGui::SameLine();
                const char* snapModes[] = {"Straight", "Triplet", "Dotted"};
                int snapMode = std::max(0, std::min(2, lane.lfoSnapMode));
                ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - 4.0f));
                // Only new edits use the new grid; existing points keep their values.
                if(ImGui::Combo(("##lfoSnapMode" + lane.id).c_str(), &snapMode, snapModes, 3))
                    lane.lfoSnapMode = snapMode;
            }
        }
        ImGui::EndChild();
        lane.curveTensions.resize(lane.curvePoints.empty() ? 0 : lane.curvePoints.size() - 1);
        const auto laneInterpolation = curveInterpolationMode(lane.curveInterpolation);
        const bool valueMode = lane.curveInterpolation == "Value";
        const bool laneSelected = !valueMode && lfoSelectedClipId == clip.id && lfoSelectedLaneId == lane.id &&
            lfoSelectedPointIndex >= 0 && lfoSelectedPointIndex < static_cast<int>(lane.curvePoints.size());

        drawGrid(row.graphMin, row.graphMax);
        dl->AddLine(ImVec2(row.graphMin.x, row.graphMax.y - 1.0f),
                    ImVec2(row.graphMax.x, row.graphMax.y - 1.0f), IM_COL32(95, 100, 115, 110));
        const float valueSpan = std::max(1.0f, row.graphMax.y - row.graphMin.y - 12.0f);
        auto yForValue = [&](float value) { return row.graphMax.y - 6.0f - value * valueSpan; };
        const bool frequencyLane = lane.lfoParameter == "frequency";
        // Normalized value under the mouse, snapped to the frequency grid when enabled.
        auto normalizedAtMouseY = [&](float mouseY) {
            float normalized = ofClamp((row.graphMax.y - 6.0f - mouseY) / valueSpan, 0.0f, 1.0f);
            if(frequencyLane && lane.lfoSnap) {
                normalized = ofxOceanodeTimelineLfo::laneNormalizedFromValue(lane,
                    ofxOceanodeTimelineLfo::snapFrequencyValue(ofxOceanodeTimelineLfo::laneValueFromNormalized(lane, normalized), lane.lfoSnapMode));
            }
            return normalized;
        };
        dl->PushClipRect(row.graphMin, row.graphMax, true);
        if(frequencyLane) {
            // Reference lines at musical cycle lengths (log-spaced, so evenly spread):
            // the selected grid (straight / triplet / dotted) labelled, and the
            // straight lengths kept faint underneath for orientation.
            const int gridMode = std::max(0, std::min(2, lane.lfoSnapMode));
            if(gridMode != 0) {
                for(float gridValue : ofxOceanodeTimelineLfo::frequencyGridValues(0)) {
                    const float y = yForValue(ofxOceanodeTimelineLfo::laneNormalizedFromValue(lane, gridValue));
                    dl->AddLine(ImVec2(row.graphMin.x, y), ImVec2(row.graphMax.x, y), IM_COL32(80, 84, 98, 55));
                }
            }
            for(float gridValue : ofxOceanodeTimelineLfo::frequencyGridValues(gridMode)) {
                const float y = yForValue(ofxOceanodeTimelineLfo::laneNormalizedFromValue(lane, gridValue));
                const bool oneBeat = gridMode == 0 && gridValue == 1.0f;
                dl->AddLine(ImVec2(row.graphMin.x, y), ImVec2(row.graphMax.x, y),
                            oneBeat ? IM_COL32(150, 150, 165, 120) : IM_COL32(95, 100, 115, 85));
                const std::string label = ofxOceanodeTimelineLfo::frequencyGridLabel(gridValue, gridMode);
                dl->AddText(ImVec2(row.graphMin.x + 3.0f, y - ImGui::GetTextLineHeight() + 1.0f),
                            IM_COL32(160, 165, 180, 150), label.c_str());
            }
        }
        if(valueMode) {
            // Value mode: one constant across the whole lane; points are hidden (kept for later).
            const float y = yForValue(lane.lfoValue);
            dl->AddLine(ImVec2(row.graphMin.x, y), ImVec2(row.graphMax.x, y), lfoColor, focused ? 2.0f : 1.5f);
        }
        for(int i = 0; i < (valueMode ? 0 : samples); ++i) {
            const double source1 = contentDuration * i / static_cast<double>(samples);
            const double source2 = contentDuration * (i + 1) / static_cast<double>(samples);
            dl->AddLine(ImVec2(xForSource(source1), yForValue(valueAtBeat(lane.curvePoints, lane.curveTensions, lane.curveInterpolation, source1, 0.0f))),
                        ImVec2(xForSource(source2), yForValue(valueAtBeat(lane.curvePoints, lane.curveTensions, lane.curveInterpolation, source2, 0.0f))),
                        lfoColor, focused ? 2.0f : 1.5f);
        }
        for(size_t pointIndex = 0; pointIndex < (valueMode ? 0 : lane.curvePoints.size()); ++pointIndex) {
            const auto& point = lane.curvePoints[pointIndex];
            const float x = xForSource(point.beat);
            if(x >= row.graphMin.x - 6.0f && x <= row.graphMax.x + 6.0f) {
                dl->AddCircleFilled(ImVec2(x, yForValue(point.value)), focused ? 4.0f : 3.0f,
                                    focused ? IM_COL32(245, 235, 150, 255) : lfoColor);
                if(laneSelected && static_cast<int>(pointIndex) == lfoSelectedPointIndex)
                    dl->AddCircle(ImVec2(x, yForValue(point.value)), 7.0f, IM_COL32(255, 255, 255, 240), 0, 2.0f);
            }
        }
        dl->PopClipRect();
        drawPlayhead(row.graphMin, row.graphMax);

        if(valueMode) {
            // Clicking or dragging in the graph also sets the constant.
            if(row.canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                editorLaneId = lane.id;
                lfoDragLaneId = lane.id;
                lfoDragPointIndex = -1;
                lfoTensionSegment = -1;
                lfoValueDragActive = true;
            }
            if(lfoValueDragActive && lfoDragLaneId == lane.id && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                float normalized = normalizedAtMouseY(ImGui::GetIO().MousePos.y);
                if(!frequencyLane && curveSnapLevels(lane) >= 2)
                    normalized = std::round(normalized * lane.valueQuantizeSteps) / static_cast<float>(lane.valueQuantizeSteps);
                lane.lfoValue = normalized;
            }
            finishAbsoluteLayout(ImVec2(row.rowMin.x, row.rowMax.y));
            continue;
        }

        auto hitLfoPoint = [&](const ImVec2& mouse) {
            int hit = -1;
            float nearest = 9.0f;
            for(int i = static_cast<int>(lane.curvePoints.size()) - 1; i >= 0; --i) {
                const float px = xForSource(lane.curvePoints[i].beat);
                const float py = yForValue(lane.curvePoints[i].value);
                const float distance = std::hypot(mouse.x - px, mouse.y - py);
                if(distance < nearest) { nearest = distance; hit = i; }
            }
            return hit;
        };
        auto selectLfoPoint = [&](int index) {
            lfoSelectedClipId = clip.id;
            lfoSelectedLaneId = lane.id;
            lfoSelectedPointIndex = index;
        };
        auto deleteLfoPoint = [&](int index) {
            // Keep at least one point so the lane still defines its control.
            if(index < 0 || index >= static_cast<int>(lane.curvePoints.size()) || lane.curvePoints.size() <= 1) return;
            eraseCurvePointKeepingTensions(lane, static_cast<size_t>(index));
            if(lfoSelectedLaneId == lane.id) lfoSelectedPointIndex = -1;
            lfoDragPointIndex = -1;
            lfoTensionSegment = -1;
        };
        const std::string pointPopupId = "LFO point##" + lane.id;

        if(row.canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            const int hit = hitLfoPoint(ImGui::GetIO().MousePos);
            if(hit >= 0) {
                editorLaneId = lane.id;
                selectLfoPoint(hit);
                lfoPointNumericValue = ofxOceanodeTimelineLfo::laneValueFromNormalized(lane, lane.curvePoints[hit].value);
                ImGui::OpenPopup(pointPopupId.c_str());
            }
        }
        if(row.canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            editorLaneId = lane.id;
            lfoDragLaneId = lane.id;
            lfoDragPointIndex = -1;
            lfoTensionSegment = -1;
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const double beat = snapBeat(ofClamp(sourceForX(mouse.x), 0.0, contentDuration));
            const float value = normalizedAtMouseY(mouse.y);
            lfoDragPointIndex = hitLfoPoint(mouse);
            const bool alt = ImGui::GetIO().KeyAlt;
            if(lfoDragPointIndex >= 0 && ImGui::GetIO().KeyShift) {
                deleteLfoPoint(lfoDragPointIndex);
            } else if(lfoDragPointIndex >= 0) {
                selectLfoPoint(lfoDragPointIndex);
            } else if(alt && (laneInterpolation == CurveInterpolationMode::LogExp ||
                              laneInterpolation == CurveInterpolationMode::Sigmoid) &&
                      lane.curvePoints.size() > 1) {
                // Alt-click near a segment: grab its tension (same gesture as the Curve editor).
                const double sourceBeat = sourceForX(mouse.x);
                for(size_t i = 1; i < lane.curvePoints.size(); ++i) {
                    const auto& a = lane.curvePoints[i - 1];
                    const auto& b = lane.curvePoints[i];
                    if(sourceBeat < a.beat || sourceBeat > b.beat) continue;
                    const float t = static_cast<float>((sourceBeat - a.beat) / std::max(1e-9, b.beat - a.beat));
                    const auto tension = lane.curveTensions[i - 1];
                    const float segmentY = yForValue(ofLerp(a.value, b.value, curveSegmentShape(t, laneInterpolation, tension)));
                    if(std::abs(mouse.y - segmentY) <= 12.0f) {
                        lfoTensionSegment = static_cast<int>(i - 1);
                        lfoTensionDragStartX = mouse.x;
                        lfoTensionDragStartY = mouse.y;
                        lfoTensionStartInflection = tension.inflection;
                        lfoTensionStartSteepness = tension.steepness;
                    }
                    break;
                }
            } else if(!alt) {
                lfoDragPointIndex = insertLinearCurvePoint(lane, {beat, value});
                selectLfoPoint(lfoDragPointIndex);
            }
        }
        if(lfoDragLaneId == lane.id && lfoTensionSegment >= 0 &&
           lfoTensionSegment < static_cast<int>(lane.curveTensions.size()) &&
           ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            auto& tension = lane.curveTensions[lfoTensionSegment];
            if(laneInterpolation == CurveInterpolationMode::Sigmoid) {
                tension.inflection = ofClamp(lfoTensionStartInflection +
                    (mouse.x - lfoTensionDragStartX) / std::max(1.0f, row.graphMax.x - row.graphMin.x), 0.01f, 0.99f);
            } else {
                tension.inflection = 0.5f;
            }
            const float steepnessDelta = -(mouse.y - lfoTensionDragStartY) / std::max(1.0f, valueSpan / 3.0f);
            tension.steepness = ofClamp(lfoTensionStartSteepness * std::exp(steepnessDelta * 0.5f), 0.1f, 10.0f);
        }
        // Delete / Backspace removes the selected point (Timeline focused, nothing being edited).
        if(laneSelected && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
           !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
           (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))) {
            deleteLfoPoint(lfoSelectedPointIndex);
        }
        if(ImGui::BeginPopup(pointPopupId.c_str())) {
            const bool valid = lfoSelectedLaneId == lane.id && lfoSelectedPointIndex >= 0 &&
                lfoSelectedPointIndex < static_cast<int>(lane.curvePoints.size());
            if(!valid) {
                ImGui::CloseCurrentPopup();
            } else {
                // Set value: type or drag an exact value (real units, e.g. beats for Frequency).
                const float lo = std::min(lane.valueMin, lane.valueMax);
                const float hi = std::max(lane.valueMin, lane.valueMax);
                ImGui::SetNextItemWidth(110.0f);
                if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
                if(ImGui::InputFloat("Value", &lfoPointNumericValue, 0.0f, 0.0f, "%.6g")) {
                    lane.curvePoints[lfoSelectedPointIndex].value =
                        ofxOceanodeTimelineLfo::laneNormalizedFromValue(lane, ofClamp(lfoPointNumericValue, lo, hi));
                }
                if(ImGui::IsItemDeactivatedAfterEdit()) lfoPointNumericValue = ofClamp(lfoPointNumericValue, lo, hi);
                ImGui::Separator();
                // Decide once: deleting changes the point count before EndDisabled.
                const bool deleteDisabled = lane.curvePoints.size() <= 1;
                if(deleteDisabled) ImGui::BeginDisabled();
                if(ImGui::MenuItem("Delete point")) deleteLfoPoint(lfoSelectedPointIndex);
                if(deleteDisabled) {
                    ImGui::EndDisabled();
                    ImGui::TextDisabled("A lane keeps at least one point");
                }
            }
            ImGui::EndPopup();
        }
        if(lfoDragLaneId == lane.id && lfoDragPointIndex >= 0 &&
           lfoDragPointIndex < static_cast<int>(lane.curvePoints.size()) &&
           ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const double beat = snapBeat(ofClamp(sourceForX(mouse.x), 0.0, contentDuration));
            const double minimum = lfoDragPointIndex > 0
                ? lane.curvePoints[lfoDragPointIndex - 1].beat + 1.0 / kPPQ : 0.0;
            const double maximum = lfoDragPointIndex + 1 < static_cast<int>(lane.curvePoints.size())
                ? lane.curvePoints[lfoDragPointIndex + 1].beat - 1.0 / kPPQ : contentDuration;
            lane.curvePoints[lfoDragPointIndex].beat = std::max(minimum, std::min(maximum, beat));
            lane.curvePoints[lfoDragPointIndex].value = normalizedAtMouseY(mouse.y);
        }
        finishAbsoluteLayout(ImVec2(row.rowMin.x, row.rowMax.y));
    }

    // A left click anywhere outside the LFO lanes (e.g. selecting a clip) drops the
    // point selection, so Delete goes back to acting on the timeline selection.
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !anyLfoCanvasHovered &&
       !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
        lfoSelectedPointIndex = -1;
    }
    if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if(!lfoDragLaneId.empty()) {
            if(auto* lane = timeline.getLane(track.id, clip.id, lfoDragLaneId))
                std::sort(lane->curvePoints.begin(), lane->curvePoints.end(),
                          [](const auto& a, const auto& b) { return a.beat < b.beat; });
        }
        lfoDragLaneId.clear();
        lfoDragPointIndex = -1;
        lfoTensionSegment = -1;
        lfoValueDragActive = false;
    }
    // Drop a stale selection (clip closed/changed, point removed elsewhere).
    if(lfoSelectedClipId != clip.id) lfoSelectedPointIndex = -1;
}

void ofxOceanodeTimelineController::drawLaneEditor(ofxOceanodeTimelineManager& timeline,
                                                   const ofxOceanodeTimelineTrack& track,
                                                   float contentWidth, double endBeat,
                                                   double beatPosition) {
    const float timelineScrollX = timeline.getViewState().scrollX;
    auto* clip = timeline.getClip(editorTrackId, editorClipId);
    if(clip == nullptr || (clip->lanes.empty() && !track.isWaveTrack)) {
        clipEditorOpen = false;
        return;
    }
    if(clip->isLfo) {
        drawLfoEditor(timeline, track, *clip, contentWidth, endBeat, beatPosition);
        return;
    }
    if(!clip->lanes.empty() && timeline.getLane(editorTrackId, editorClipId, editorLaneId) == nullptr) {
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
        const bool folded = foldedClipEditors.count(clip->id) > 0;
        if(ImGui::SmallButton(folded ? ">##foldClipEditor" : "v##foldClipEditor")) {
            if(folded) foldedClipEditors.erase(clip->id); else foldedClipEditors.insert(clip->id);
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip(folded ? "Unfold the clip editor" : "Fold the clip editor to this header");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(track.color.r / 255.0f, track.color.g / 255.0f, track.color.b / 255.0f, 1.0f),
                           "%s", clip->name.c_str());
        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Double-click to rename this clip");
            if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) requestClipRename(track.id, *clip);
        }
        ImGui::SameLine(kLabelWidth - 50.0f);
        if(ImGui::SmallButton("+##addLane")) ImGui::OpenPopup("##addLaneTypePopup");
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Add another lane to this clip");
        ImGui::SameLine();
        if(ImGui::SmallButton("x")) clipEditorOpen = false;
        if(ImGui::BeginPopup("##addLaneTypePopup")) {
            if(track.isWaveTrack) {
                if(ImGui::MenuItem("Edit track volume automation")) {
                    timeline.createWaveVolumeLane(track.id, clip->id);
                    editorLaneId.clear();
                }
            } else {
                for(int optionIndex = 0; optionIndex < kLaneTypeOptionCount; ++optionIndex) {
                    if(ImGui::MenuItem(kLaneTypeOptions[optionIndex])) { requestAddLane = true; pendingAddLaneType = optionIndex; }
                }
            }
            ImGui::EndPopup();
        }
        finishAbsoluteLayout(ImVec2(clipHeaderMin.x, clipHeaderMax.y));
    }
    if(foldedClipEditors.count(clip->id) > 0) return; // folded: only the header row is shown

    if(track.isWaveTrack && clip != nullptr)
        drawWaveClipProperties(timeline, track, *clip);

    if(clip->lanes.empty()) return;

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
        const bool resizeHovered = hoverRect(handleMin, handleMax);
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

    // Same narrow-column concern as the Start/Len row above: a labelled
    // widget's on-screen width is its own SetNextItemWidth PLUS the trailing
    // label ImGui draws after it, and several of the Editor tab's widgets
    // (particularly the piano role combos) were sized for the wider,
    // non-piano-roll case. Clamp each one to what's actually left on its row
    // so the label can't run past the available width and disappear behind
    // the pinned piano keyboard strip.
    auto labeledWidgetWidth = [](float desired, const char* label) {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float labelWidth = ImGui::CalcTextSize(label).x;
        const float budget = avail - labelWidth - ImGui::GetStyle().ItemInnerSpacing.x - 2.0f;
        return std::max(24.0f, std::min(desired, budget));
    };

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
            // A piano roll lane's properties column is narrower than other
            // lane types' (it shares its width with the pinned keyboard
            // strip to its right), so fixed pixel offsets tuned for the
            // wider case can push the Len field past the available width --
            // where it's clipped away right where the keyboard begins.
            // Size both fields from the row's actual available width instead.
            const float startLabelWidth = ImGui::CalcTextSize("Start").x + 6.0f;
            const float lenLabelWidth = ImGui::CalcTextSize("Len").x + 6.0f;
            const float startLenSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float startLenFieldWidth = std::max(28.0f,
                (ImGui::GetContentRegionAvail().x - startLabelWidth - lenLabelWidth - startLenSpacing * 2.0f) * 0.5f);
            ImGui::TextUnformatted("Start"); ImGui::SameLine(startLabelWidth);
            ImGui::SetNextItemWidth(startLenFieldWidth);
            if(ImGui::DragFloat("##clipStart", &start, static_cast<float>(editIncrement()), 0.0f, 9999.0f, "%.3g"))
                timeline.setClipTiming(track.id, clip->id, snapBeat(start), clip->durationBeats);
            ImGui::SameLine(); ImGui::TextUnformatted("Len"); ImGui::SameLine();
            ImGui::SetNextItemWidth(startLenFieldWidth);
            if(ImGui::DragFloat("##clipLength", &duration, static_cast<float>(editIncrement()), static_cast<float>(1.0 / kPPQ), 9999.0f, "%.3g")) {
                const double newLength = std::max(1.0 / kPPQ, snapBeat(duration));
                const double clipStretch = stretch(*clip);
                timeline.setClipTiming(track.id, clip->id, clip->startBeat, newLength);
                timeline.setClipContentDuration(track.id, clip->id, newLength / clipStretch, false);
            }

            if(lane->type != ofxOceanodeTimelineLaneType::PianoRoll &&
               lane->type != ofxOceanodeTimelineLaneType::MultiGate &&
               lane->type != ofxOceanodeTimelineLaneType::Wave) {
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
                ImGui::SetNextItemWidth(labeledWidgetWidth(94.0f, "Grid"));
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
                ImGui::SetNextItemWidth(labeledWidgetWidth(94.0f, "Steps"));
                if(ImGui::DragInt("Steps", &steps, 0.2f, 1, 128)) {
                    lane->stepCount = steps;
                }
                drawDivision();
                int behavior = lane->behavior == "Always" ? 1 : lane->behavior == "Mute" ? 2 : 0;
                const char* behaviors[] = {"Probability", "Always", "Mute"};
                ImGui::SetNextItemWidth(labeledWidgetWidth(94.0f, "Mode"));
                if(ImGui::Combo("Mode", &behavior, behaviors, 3)) lane->behavior = behaviors[behavior];
                ImGui::Checkbox("Probability", &lane->probabilityEnabled);
                drawDivision();
                ImGui::SetNextItemWidth(labeledWidgetWidth(94.0f, "Seed"));
                ImGui::DragInt("Seed", &lane->probabilitySeed, 1.0f, 0, 99999);
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("Which steps a probability below 100%% lets through.\nChange it for another pattern.");
                ImGui::SameLine();
                if(ImGui::SmallButton("New##stepSeed")) lane->probabilitySeed = static_cast<int>(ofRandom(1.0f, 100000.0f));
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
                ImGui::Checkbox("Clamp to range", &lane->curveClamp);
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("Clamp curve values to the configured Range min..max");
                if(ImGui::Checkbox("Snap##curveValueSnap", &lane->valueSnap) && lane->valueSnap &&
                   lane->valueQuantizeSteps < 2) {
                    lane->valueQuantizeSteps = 8; // sensible first grid
                }
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("Snap point values to evenly spaced levels across the range");
                ImGui::SameLine();
                int curveSteps = std::max(2, lane->valueQuantizeSteps);
                ImGui::SetNextItemWidth(labeledWidgetWidth(60.0f, "Steps"));
                if(!lane->valueSnap) ImGui::BeginDisabled();
                if(ImGui::DragInt("Steps##curveValueSteps", &curveSteps, 0.1f, 2, 128))
                    lane->valueQuantizeSteps = std::max(2, std::min(128, curveSteps));
                if(!lane->valueSnap) ImGui::EndDisabled();
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("Number of intervals: %d steps = %d levels (min and max included)",
                                      std::max(2, lane->valueQuantizeSteps), std::max(2, lane->valueQuantizeSteps) + 1);
            } else if(lane->type == ofxOceanodeTimelineLaneType::MultiValue ||
                      lane->type == ofxOceanodeTimelineLaneType::MultiGate) {
                int rows = std::max(1, lane->multiRowCount);
                ImGui::SetNextItemWidth(labeledWidgetWidth(94.0f, "Rows"));
                if(ImGui::DragInt("Rows", &rows, 0.1f, 1, 16))
                    timeline.setLaneMultiRowCount(track.id, clip->id, lane->id, rows);
                drawDivision();
                if(lane->type == ofxOceanodeTimelineLaneType::MultiValue) {
                    ImGui::TextDisabled("Dbl-click a block to edit");
                    if(ImGui::Checkbox("Integer", &lane->multiValueInteger) && lane->multiValueInteger) {
                        for(auto& row : lane->multiValueRows)
                            for(auto& region : row)
                                region.value = std::round(region.value);
                    }
                } else ImGui::TextDisabled("Drag to draw gates");
            } else if(lane->type == ofxOceanodeTimelineLaneType::MultiSlider) {
                int steps = std::max(1, lane->stepCount);
                ImGui::SetNextItemWidth(labeledWidgetWidth(94.0f, "Steps"));
                if(ImGui::DragInt("Steps", &steps, 0.2f, 1, 128)) {
                    lane->stepCount = steps;
                    lane->multiSliderValues.resize(static_cast<size_t>(steps), 0.0f);
                }
                drawDivision();
                int sliderSnap = std::max(0, lane->valueQuantizeSteps);
                ImGui::SetNextItemWidth(labeledWidgetWidth(94.0f, "Value Snap"));
                if(ImGui::DragInt("Value Snap", &sliderSnap, 0.1f, 0, 64))
                    lane->valueQuantizeSteps = std::max(0, sliderSnap);
                if(lane->valueQuantizeSteps >= 2) ImGui::TextDisabled("%d levels", lane->valueQuantizeSteps);
            } else {
                auto drawPianoRole = [&](const char* label, std::string& roleId) {
                    const auto* currentBinding = roleId.empty() ? nullptr : timeline.getBinding(track.id, roleId);
                    const std::string preview = currentBinding == nullptr ? "None" : compactParameterName(currentBinding->parameterPath);
                    ImGui::SetNextItemWidth(labeledWidgetWidth(112.0f, label));
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
                const ofxOceanodeTimelineNoteGroup* laneGroup = nullptr;
                for(const auto& id : lane->bindingIds)
                    if(laneGroup == nullptr) laneGroup = ofxOceanodeTimelineManager::findNoteGroupForBinding(track, id);
                if(laneGroup != nullptr) {
                    // Grouped piano rolls follow their group's targets.
                    const std::string buttonLabel = "Targets: " + laneGroup->name + "...";
                    if(ImGui::Button(buttonLabel.c_str()))
                        openNoteGroupSetup(timeline, nullptr, track.id, laneGroup->id);
                    if(ImGui::IsItemHovered()) {
                        std::string tip;
                        const char* roleNames[3] = {"Pitch", "Gate", "Velocity"};
                        const std::string* roles[3] = {&laneGroup->pitchBindingId, &laneGroup->gateBindingId, &laneGroup->velocityBindingId};
                        for(int role = 0; role < 3; ++role) {
                            const auto* member = roles[role]->empty() ? nullptr : timeline.getBinding(track.id, *roles[role]);
                            tip += std::string(role > 0 ? "\n" : "") + roleNames[role] + ": " +
                                (member == nullptr ? std::string("none") : member->parameterPath);
                        }
                        ImGui::SetTooltip("%s", tip.c_str());
                    }
                } else {
                    drawPianoRole("Pitch", lane->pianoPitchBindingId);
                    drawPianoRole("Gate", lane->pianoGateBindingId);
                    drawPianoRole("Velocity", lane->pianoVelocityBindingId);
                }
                drawDivision();
                ImGui::SetNextItemWidth(labeledWidgetWidth(94.0f, "Low"));
                ImGui::DragInt("Low", &lane->pianoLowPitch, 0.25f, 0, 127);
                ImGui::SetNextItemWidth(labeledWidgetWidth(94.0f, "High"));
                ImGui::DragInt("High", &lane->pianoHighPitch, 0.25f, 0, 127);
                lane->pianoHighPitch = std::max(lane->pianoLowPitch, lane->pianoHighPitch);
                ImGui::SetNextItemWidth(labeledWidgetWidth(94.0f, "Vel"));
                ImGui::SliderFloat("Vel", &lane->pianoDefaultVelocity, 0.0f, 1.0f, "%.2f");
                ImGui::Checkbox("Snap", &lane->pianoSnapToGrid);
                ImGui::SameLine(); ImGui::Checkbox("Mono", &lane->pianoMonophonic);
                drawDivision();
                ImGui::SetNextItemWidth(labeledWidgetWidth(94.0f, "Seed"));
                ImGui::DragInt("Seed", &lane->probabilitySeed, 1.0f, 0, 99999);
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("Which notes a probability below 100%% lets through.\nChange it for another pattern.");
                ImGui::SameLine();
                if(ImGui::SmallButton("New##pianoSeed")) lane->probabilitySeed = static_cast<int>(ofRandom(1.0f, 100000.0f));
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
            const bool keyHovered = hoverRect(ImVec2(keyboardLeft, keyTop), ImVec2(keyboardRight, keyBottom));
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
                if(ImGui::IsItemClicked() && !hoverRect(pianoScrollThumbMin, pianoScrollThumbMax)) {
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
        const bool hovered = hoverRect(ImVec2(left, rollTop), ImVec2(right, rollBottom));
        const bool velocityHovered = hoverRect(ImVec2(left, velocityTop), ImVec2(right, velocityBottom));
        const bool probabilityHovered = hoverRect(ImVec2(left, probabilityTop), ImVec2(right, probabilityBottom));
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
           !ImGui::GetIO().WantTextInput &&
           (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))) {
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
        const float curveValueSpan = lane->valueMax - lane->valueMin;
        const float curveRangeMin = std::min(lane->valueMin, lane->valueMax);
        const float curveRangeMax = std::max(lane->valueMin, lane->valueMax);
        const auto curveValueToY = [&](float normalizedValue) {
            if(std::abs(curveValueSpan) < 1e-9f) return curveBottom;
            const float actualValue = lane->valueMin + normalizedValue * curveValueSpan;
            const float visibleValue = ofClamp((actualValue - curveRangeMin) /
                                               std::max(1e-9f, curveRangeMax - curveRangeMin),
                                               0.0f, 1.0f);
            return curveBottom - visibleValue * (curveBottom - curveTop);
        };
        if(curveSnapLevels(*lane) >= 2) {
            const int levels = curveSnapLevels(*lane);
            for(int row = 0; row <= levels; ++row) {
                const float y = curveTop + (curveBottom - curveTop) * row / static_cast<float>(levels);
                dl->AddLine(ImVec2(timelineMin.x, y), ImVec2(editorMax.x, y), IM_COL32(150, 140, 80, 150));
            }
        } else {
            for(int row = 0; row <= 4; ++row) {
                const float y = curveTop + (curveBottom - curveTop) * row / 4.0f;
                dl->AddLine(ImVec2(timelineMin.x, y), ImVec2(editorMax.x, y), IM_COL32(75, 75, 75, 120));
            }
        }
        char curveMaxLabel[32];
        char curveMinLabel[32];
        std::snprintf(curveMaxLabel, sizeof(curveMaxLabel), "%.4g", lane->valueMax);
        std::snprintf(curveMinLabel, sizeof(curveMinLabel), "%.4g", lane->valueMin);
        dl->AddText(ImVec2(left + 5.0f, curveTop + 2.0f), IM_COL32(205, 205, 210, 220), curveMaxLabel);
        dl->AddText(ImVec2(left + 5.0f, curveBottom - ImGui::GetTextLineHeight() - 2.0f),
                    IM_COL32(205, 205, 210, 220), curveMinLabel);
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
                    const float y1 = curveValueToY(points[i - 1].value);
                    const float y2 = curveValueToY(points[i].value);
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
                                   curveValueToY(value1));
                    const ImVec2 b(timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, source2, cycle)),
                                   curveValueToY(value2));
                    dl->AddLine(a, b, IM_COL32(track.color.r, track.color.g, track.color.b, cycle == 0 ? 240 : 130), 2.0f);
                }
            }
            for(const auto& point : points) {
                const ImVec2 p(timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, point.beat, cycle)), curveValueToY(point.value));
                dl->AddCircleFilled(p, cycle == 0 ? 4.0f : 3.0f, IM_COL32(245, 245, 245, cycle == 0 ? 230 : 115));
            }
        }
        dl->PopClipRect();
        // Require the editor canvas item itself to be hoverable, not merely
        // geometric overlap. This prevents clicks in the numeric popup from
        // passing through and creating a point underneath it.
        if(isFocused) {
        const bool curveHovered = editorCanvasHovered && !ImGui::IsPopupOpen("Curve point value") && right > left &&
            hoverRect(ImVec2(left, curveTop), ImVec2(right, curveBottom));
        const ImVec2 curveMouse = ImGui::GetIO().MousePos;
        const double curveTimelineBeat = beatAtOffset(curveMouse.x - timelineMin.x);
        const double curveSourceBeat = timelineToSourceBeat(*clip, curveTimelineBeat);
        const int hoveredCycle = clip->repeatContent
            ? std::max(0, static_cast<int>(std::floor(std::max(0.0, curveTimelineBeat - clip->startBeat) / repeatCycleDuration))) : 0;
        float curveValue = (curveBottom - curveMouse.y) / (curveBottom - curveTop);
        if(lane->curveClamp && std::abs(curveValueSpan) > 1e-9f) {
            const float actualValue = ofClamp(lane->valueMin + curveValue * curveValueSpan,
                                              curveRangeMin, curveRangeMax);
            curveValue = (actualValue - lane->valueMin) / curveValueSpan;
        }
        if(curveSnapLevels(*lane) >= 2) {
            const float levels = static_cast<float>(curveSnapLevels(*lane));
            curveValue = std::round(curveValue * levels) / levels;
        }
        auto hitPoint = [&]() {
            for(int i = static_cast<int>(lane->curvePoints.size()) - 1; i >= 0; --i) {
                const auto& point = lane->curvePoints[i];
                const float pointX = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, point.beat, hoveredCycle));
                const float pointY = curveValueToY(point.value);
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
                    const float segmentY = curveValueToY(segmentValue);
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
                    float actualValue = curveNumericValue;
                    if(lane->curveClamp)
                        actualValue = ofClamp(actualValue,
                                              std::min(lane->valueMin, lane->valueMax),
                                              std::max(lane->valueMin, lane->valueMax));
                    lane->curvePoints[curveValuePointIndex].value = std::abs(span) < 1e-9f ? 0.0f
                        : (actualValue - lane->valueMin) / span;
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
        dl->PopClipRect();
        finishAbsoluteLayout(ImVec2(editorMin.x, editorMax.y));
        continue;
    }

    if(lane->type == ofxOceanodeTimelineLaneType::MultiSlider) {
        if(static_cast<int>(lane->multiSliderValues.size()) != std::max(1, lane->stepCount))
            lane->multiSliderValues.resize(static_cast<size_t>(std::max(1, lane->stepCount)), 0.0f);
        const int stepCount = std::max(1, lane->stepCount);
        const double beatsPerStep = std::max(1.0 / kPPQ, lane->beatsPerStep);
        const double patternLength = stepCount * beatsPerStep;
        const float cellTop = editorMin.y + 10.0f;
        const float cellBottom = editorMax.y - 10.0f;
        const float span = lane->valueMax - lane->valueMin;
        if(right > left) {
            dl->AddRectFilled(ImVec2(left, editorMin.y + 3), ImVec2(right, editorMax.y - kLaneResizeHandleHeight),
                              IM_COL32(track.color.r, track.color.g, track.color.b, 50), 2);
            if(lane->valueQuantizeSteps >= 2) {
                for(int row = 0; row <= lane->valueQuantizeSteps; ++row) {
                    const float y = cellTop + (cellBottom - cellTop) * row / static_cast<float>(lane->valueQuantizeSteps);
                    dl->AddLine(ImVec2(left, y), ImVec2(right, y), IM_COL32(150, 140, 80, 150));
                }
            }
            const double content = sourceDuration(*clip);
            const int clipCycles = clip->repeatContent ? std::max(1, static_cast<int>(std::ceil(clip->durationBeats / cycleDuration(*clip)))) : 1;
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
                        const float value = index < static_cast<int>(lane->multiSliderValues.size()) ? lane->multiSliderValues[index] : 0.0f;
                        const float normalized = std::abs(span) < 1e-9f ? 0.0f : ofClamp((value - lane->valueMin) / span, 0.0f, 1.0f);
                        const float fillTop = cellBottom - normalized * (cellBottom - cellTop);
                        dl->AddRectFilled(ImVec2(std::max(x1, left) + 1, fillTop),
                                          ImVec2(std::min(x2, right) - 1, cellBottom),
                                          IM_COL32(track.color.r, track.color.g, track.color.b, firstPattern ? 210 : 120), 2);
                        dl->AddRect(ImVec2(std::max(x1, left) + 1, cellTop),
                                    ImVec2(std::min(x2, right) - 1, cellBottom),
                                    IM_COL32(130, 130, 130, firstPattern ? 110 : 65), 1.0f);
                        if(firstPattern && x2 - x1 > 28.0f) {
                            char valueLabel[16];
                            std::snprintf(valueLabel, sizeof(valueLabel), "%.2g", value);
                            dl->AddText(ImVec2(std::max(x1, left) + 3, cellTop + 3), IM_COL32(240, 240, 240, 220), valueLabel);
                        }
                    }
                }
            }
            dl->PopClipRect();
            if(isFocused) {
                const ImVec2 mouse = ImGui::GetIO().MousePos;
                const bool inSliderEditor = hoverRect(ImVec2(left, cellTop), ImVec2(right, cellBottom));
                if(inSliderEditor && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
                    const double timelineBeat = beatAtOffset(mouse.x - timelineMin.x);
                    const double sourceBeat = timelineToSourceBeat(*clip, timelineBeat);
                    const double patternBeat = std::fmod(sourceBeat, patternLength);
                    const int index = ofClamp(static_cast<int>(std::floor(patternBeat / beatsPerStep)), 0, stepCount - 1);
                    if(index >= 0 && index < static_cast<int>(lane->multiSliderValues.size())) {
                        float normalized = ofClamp((cellBottom - mouse.y) / (cellBottom - cellTop), 0.0f, 1.0f);
                        if(lane->valueQuantizeSteps >= 2)
                            normalized = std::round(normalized * lane->valueQuantizeSteps) / static_cast<float>(lane->valueQuantizeSteps);
                        lane->multiSliderValues[index] = lane->valueMin + normalized * span;
                    }
                }
                if(inSliderEditor && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    const double timelineBeat = beatAtOffset(mouse.x - timelineMin.x);
                    const double sourceBeat = timelineToSourceBeat(*clip, timelineBeat);
                    const double patternBeat = std::fmod(sourceBeat, patternLength);
                    const int index = ofClamp(static_cast<int>(std::floor(patternBeat / beatsPerStep)), 0, stepCount - 1);
                    if(index >= 0 && index < static_cast<int>(lane->multiSliderValues.size())) lane->multiSliderValues[index] = 0.0f;
                }
            } // isFocused (multi-slider interaction)
        }
        const float playheadX = timelineMin.x + beatOffset(beatPosition);
        if(playheadX >= zoneLeft && playheadX <= editorMax.x) dl->AddLine(ImVec2(playheadX, editorMin.y), ImVec2(playheadX, editorMax.y), kPlayhead, 2.0f);
        dl->PopClipRect();
        finishAbsoluteLayout(ImVec2(editorMin.x, editorMax.y));
        continue;
    }

    if(lane->type == ofxOceanodeTimelineLaneType::MultiValue || lane->type == ofxOceanodeTimelineLaneType::MultiGate) {
        const bool isValueRow = lane->type == ofxOceanodeTimelineLaneType::MultiValue;
        const int rowCount = std::max(1, lane->multiRowCount);
        if(isValueRow) {
            if(static_cast<int>(lane->multiValueRows.size()) != rowCount) lane->multiValueRows.resize(static_cast<size_t>(rowCount));
        } else {
            if(static_cast<int>(lane->multiGateRows.size()) != rowCount) lane->multiGateRows.resize(static_cast<size_t>(rowCount));
        }
        const float rowsTop = editorMin.y + 6.0f;
        const float rowsBottom = editorMax.y - 6.0f;
        const float rowGap = 3.0f;
        const float rowHeight = std::max(6.0f, (rowsBottom - rowsTop - rowGap * std::max(0, rowCount - 1)) / rowCount);
        auto rowTopY = [&](int r) { return rowsTop + r * (rowHeight + rowGap); };
        auto rowBottomY = [&](int r) { return rowTopY(r) + rowHeight; };
        const int clipCycles = clip->repeatContent ? std::max(1, static_cast<int>(std::ceil(clip->durationBeats / cycleDuration(*clip)))) : 1;
        const float span = lane->valueMax - lane->valueMin;
        {
            const double multiGrid = displayGridBeats();
            const int multiGridLines = static_cast<int>(std::ceil(endBeat / multiGrid));
            for(int gridIndex = 0; gridIndex <= multiGridLines; ++gridIndex) {
                const double beat = gridIndex * multiGrid;
                const float x = timelineMin.x + beatOffset(beat);
                const bool bar = std::fmod(beat, timeline.getBeatsPerBar()) < 0.001;
                if(bar || beatOffset(beat + multiGrid) - beatOffset(beat) >= 4.0f)
                    dl->AddLine(ImVec2(x, rowsTop), ImVec2(x, rowsBottom), bar ? kBar : kGrid, bar ? 1.5f : 1.0f);
            }
        }
        dl->PushClipRect(ImVec2(left, rowsTop), ImVec2(right, rowsBottom), true);
        for(int r = 0; r < rowCount; ++r) {
            const float rTop = rowTopY(r), rBottom = rowBottomY(r);
            dl->AddRectFilled(ImVec2(left, rTop), ImVec2(right, rBottom), IM_COL32(255, 255, 255, r % 2 ? 8 : 14));
            for(int cycle = 0; cycle < clipCycles; ++cycle) {
                if(isValueRow) {
                    for(const auto& region : lane->multiValueRows[r]) {
                        const float x1 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, region.startBeat, cycle));
                        const float x2 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, region.end(), cycle));
                        if(x2 < left || x1 > right) continue;
                        const float normalized = std::abs(span) < 1e-9f ? 0.0f : ofClamp((region.value - lane->valueMin) / span, 0.0f, 1.0f);
                        const float fillTop = rBottom - normalized * (rBottom - rTop);
                        dl->AddRectFilled(ImVec2(std::max(x1, left) + 1, fillTop), ImVec2(std::min(x2, right) - 1, rBottom),
                                          IM_COL32(track.color.r, track.color.g, track.color.b, cycle == 0 ? 220 : 110), 2);
                        dl->AddRect(ImVec2(std::max(x1, left) + 1, rTop), ImVec2(std::min(x2, right) - 1, rBottom),
                                    IM_COL32(200, 200, 200, cycle == 0 ? 160 : 70), 1.0f);
                        if(cycle == 0 && x2 - x1 > 26.0f) {
                            char valueLabel[16];
                            std::snprintf(valueLabel, sizeof(valueLabel), lane->multiValueInteger ? "%.0f" : "%.3g", region.value);
                            dl->AddText(ImVec2(std::max(x1, left) + 3, rTop + 2), IM_COL32(240, 240, 240, 225), valueLabel);
                        }
                    }
                } else {
                    for(const auto& region : lane->multiGateRows[r]) {
                        const float x1 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, region.startBeat, cycle));
                        const float x2 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, region.end(), cycle));
                        if(x2 < left || x1 > right) continue;
                        dl->AddRectFilled(ImVec2(std::max(x1, left) + 1, rTop + 1), ImVec2(std::min(x2, right) - 1, rBottom - 1),
                                          IM_COL32(track.color.r, track.color.g, track.color.b, cycle == 0 ? 225 : 115), 2);
                    }
                }
            }
        }
        dl->PopClipRect();
        if(isFocused) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const double timelineBeat = beatAtOffset(mouse.x - timelineMin.x);
            const double sourceBeat = timelineToSourceBeat(*clip, timelineBeat);
            int hoveredRow = -1;
            for(int r = 0; r < rowCount; ++r) {
                if(mouse.y >= rowTopY(r) && mouse.y <= rowBottomY(r)) { hoveredRow = r; break; }
            }
            const bool inRegionEditor = editorCanvasHovered && mouse.x >= left && mouse.x <= right &&
                mouse.y >= rowsTop && mouse.y <= rowsBottom;

            auto hitValueRegion = [&](int r, double beat) -> int {
                if(!isValueRow || r < 0 || r >= static_cast<int>(lane->multiValueRows.size())) return -1;
                const auto& regionsInRow = lane->multiValueRows[r];
                for(int i = static_cast<int>(regionsInRow.size()) - 1; i >= 0; --i)
                    if(beat >= regionsInRow[i].startBeat && beat <= regionsInRow[i].end()) return i;
                return -1;
            };
            auto hitGateRegion = [&](int r, double beat) -> int {
                if(isValueRow || r < 0 || r >= static_cast<int>(lane->multiGateRows.size())) return -1;
                const auto& regionsInRow = lane->multiGateRows[r];
                for(int i = static_cast<int>(regionsInRow.size()) - 1; i >= 0; --i)
                    if(beat >= regionsInRow[i].startBeat && beat <= regionsInRow[i].end()) return i;
                return -1;
            };
            auto hitRegion = [&](int r, double beat) { return isValueRow ? hitValueRegion(r, beat) : hitGateRegion(r, beat); };

            if(inRegionEditor && hoveredRow >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                const int hit = hitRegion(hoveredRow, sourceBeat);
                if(hit >= 0) {
                    if(isValueRow) lane->multiValueRows[hoveredRow].erase(lane->multiValueRows[hoveredRow].begin() + hit);
                    else lane->multiGateRows[hoveredRow].erase(lane->multiGateRows[hoveredRow].begin() + hit);
                }
            }
            if(isValueRow && inRegionEditor && hoveredRow >= 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                const int hit = hitRegion(hoveredRow, sourceBeat);
                if(hit >= 0) {
                    multiValueEditRow = hoveredRow;
                    multiValueEditIndex = hit;
                    multiValueEditNumeric = lane->multiValueInteger
                        ? std::round(lane->multiValueRows[hoveredRow][hit].value)
                        : lane->multiValueRows[hoveredRow][hit].value;
                    ImGui::OpenPopup("Multi value block");
                }
            }
            if(inRegionEditor && hoveredRow >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
               regionDragMode == RegionDragMode::None) {
                const int hit = hitRegion(hoveredRow, sourceBeat);
                if(hit >= 0) {
                    const double regionStart = isValueRow ? lane->multiValueRows[hoveredRow][hit].startBeat : lane->multiGateRows[hoveredRow][hit].startBeat;
                    const double regionDuration = isValueRow ? lane->multiValueRows[hoveredRow][hit].durationBeats : lane->multiGateRows[hoveredRow][hit].durationBeats;
                    const int hoveredCycle = clip->repeatContent
                        ? std::max(0, static_cast<int>(std::floor(std::max(0.0, timelineBeat - clip->startBeat) / cycleDuration(*clip)))) : 0;
                    const float x2 = timelineMin.x + beatOffset(sourceToTimelineBeat(*clip, regionStart + regionDuration, hoveredCycle));
                    const bool nearRightEdge = std::abs(mouse.x - x2) <= 6.0f;
                    regionDragMode = nearRightEdge ? RegionDragMode::ResizeRight : RegionDragMode::Move;
                    regionDragRow = hoveredRow;
                    regionDragIndex = hit;
                    regionDragAnchorBeat = sourceBeat;
                    regionDragOriginalStart = regionStart;
                    regionDragOriginalDuration = regionDuration;
                } else {
                    const double gridStep = std::max(1.0 / kPPQ, lane->beatsPerStep);
                    const double startBeat = std::max(0.0, std::floor(sourceBeat / gridStep) * gridStep);
                    if(isValueRow) {
                        ofxOceanodeTimelineValueRegion newRegion;
                        newRegion.startBeat = startBeat;
                        newRegion.durationBeats = gridStep;
                        newRegion.value = lane->valueMin + ofClamp((rowBottomY(hoveredRow) - mouse.y) / std::max(1.0f, rowHeight), 0.0f, 1.0f) * span;
                        if(lane->multiValueInteger) newRegion.value = std::round(newRegion.value);
                        lane->multiValueRows[hoveredRow].push_back(newRegion);
                        regionDragIndex = static_cast<int>(lane->multiValueRows[hoveredRow].size()) - 1;
                    } else {
                        ofxOceanodeTimelineGateRegion newRegion;
                        newRegion.startBeat = startBeat;
                        newRegion.durationBeats = gridStep;
                        lane->multiGateRows[hoveredRow].push_back(newRegion);
                        regionDragIndex = static_cast<int>(lane->multiGateRows[hoveredRow].size()) - 1;
                    }
                    regionDragMode = RegionDragMode::Create;
                    regionDragRow = hoveredRow;
                    regionDragAnchorBeat = startBeat;
                    regionDragOriginalStart = startBeat;
                    regionDragOriginalDuration = gridStep;
                }
            }
            const int regionRowCount = isValueRow ? static_cast<int>(lane->multiValueRows.size()) : static_cast<int>(lane->multiGateRows.size());
            if(regionDragMode != RegionDragMode::None && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
               regionDragRow >= 0 && regionDragRow < regionRowCount) {
                const int regionCount = isValueRow ? static_cast<int>(lane->multiValueRows[regionDragRow].size())
                                                    : static_cast<int>(lane->multiGateRows[regionDragRow].size());
                if(regionDragIndex >= 0 && regionDragIndex < regionCount) {
                    double newStart = regionDragOriginalStart;
                    double newDuration = regionDragOriginalDuration;
                    const double gridStep = std::max(1.0 / kPPQ, lane->beatsPerStep);
                    if(regionDragMode == RegionDragMode::Move) {
                        newStart = std::max(0.0, regionDragOriginalStart + (sourceBeat - regionDragAnchorBeat));
                        newStart = std::max(0.0, std::round(newStart / gridStep) * gridStep);
                    } else {
                        newDuration = std::max(1.0 / kPPQ, sourceBeat - newStart);
                        newDuration = std::max(gridStep, std::round(newDuration / gridStep) * gridStep);
                    }
                    if(isValueRow) {
                        auto& region = lane->multiValueRows[regionDragRow][regionDragIndex];
                        region.startBeat = newStart;
                        region.durationBeats = newDuration;
                        const float rTop = rowTopY(regionDragRow), rBottom = rowBottomY(regionDragRow);
                        region.value = lane->valueMin + ofClamp((rBottom - mouse.y) / std::max(1.0f, rBottom - rTop), 0.0f, 1.0f) * span;
                        if(lane->multiValueInteger) region.value = std::round(region.value);
                    } else {
                        auto& region = lane->multiGateRows[regionDragRow][regionDragIndex];
                        region.startBeat = newStart;
                        region.durationBeats = newDuration;
                    }
                }
            }
            if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                regionDragMode = RegionDragMode::None;
                regionDragRow = -1;
                regionDragIndex = -1;
            }
            if(isValueRow && ImGui::BeginPopup("Multi value block")) {
                if(multiValueEditRow >= 0 && multiValueEditRow < static_cast<int>(lane->multiValueRows.size()) &&
                   multiValueEditIndex >= 0 && multiValueEditIndex < static_cast<int>(lane->multiValueRows[multiValueEditRow].size())) {
                    ImGui::SetNextItemWidth(110.0f);
                    if(ImGui::InputFloat("Value", &multiValueEditNumeric, 0.001f, 0.01f, lane->multiValueInteger ? "%.0f" : "%.6g")) {
                        multiValueEditNumeric = lane->multiValueInteger ? std::round(multiValueEditNumeric) : multiValueEditNumeric;
                        lane->multiValueRows[multiValueEditRow][multiValueEditIndex].value = multiValueEditNumeric;
                    }
                    if(ImGui::Button("Delete block")) {
                        lane->multiValueRows[multiValueEditRow].erase(lane->multiValueRows[multiValueEditRow].begin() + multiValueEditIndex);
                        multiValueEditRow = -1;
                        multiValueEditIndex = -1;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndPopup();
            }
        } // isFocused (multi-value / multi-gate interaction)
        const float playheadX = timelineMin.x + beatOffset(beatPosition);
        if(playheadX >= zoneLeft && playheadX <= editorMax.x) dl->AddLine(ImVec2(playheadX, editorMin.y), ImVec2(playheadX, editorMax.y), kPlayhead, 2.0f);
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
        const bool inStepEditor = hoverRect(ImVec2(left, cellTop), ImVec2(right, cellBottom));
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
    dl->PopClipRect();
    finishAbsoluteLayout(ImVec2(editorMin.x, editorMax.y));
    } // end for(laneIndex : clip->lanes)
}

namespace {
// Which piano-roll role a parameter name suggests: 0 pitch, 1 gate, 2 velocity, -1 none.
int guessNoteRole(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    auto has = [&](const char* word) { return lower.find(word) != std::string::npos; };
    if(has("gate") || has("trig") || has("noteon") || has("note on") || lower == "on") return 1;
    if(has("vel") || has("amp") || has("dynamic")) return 2;
    if(has("pitch") || has("note") || has("midi") || has("key")) return 0;
    return -1;
}

bool isNoteRoleType(const std::string& valueType) {
    return valueType == typeid(float).name() || valueType == typeid(int).name() || valueType == typeid(bool).name();
}

bool containsCaseInsensitive(const std::string& text, const char* filter) {
    if(filter == nullptr || filter[0] == '\0') return true;
    std::string a = text, b = filter;
    std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return std::tolower(c); });
    std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return std::tolower(c); });
    return a.find(b) != std::string::npos;
}
}

void ofxOceanodeTimelineController::openNoteGroupSetup(ofxOceanodeTimelineManager& timeline,
                                                      ofxOceanodeAbstractParameter* source,
                                                      const std::string& trackId, const std::string& editGroupId,
                                                      double clipStart, double clipDuration) {
    noteSetupCandidates.clear();
    for(auto& path : noteSetupRolePaths) path.clear();
    noteSetupFilter[0] = '\0';
    noteSetupTrackId = trackId;
    noteSetupTrackFixed = !trackId.empty();
    noteSetupEditGroupId = editGroupId;
    const auto transportState = container->getTransportState();
    noteSetupClipStart = clipStart >= 0.0 ? clipStart : snapBeat(std::max(0.0, transportState.beatPosition));
    noteSetupClipDuration = clipDuration > 0.0 ? clipDuration : timeline.getBeatsPerBar();

    // Parameters the roles can use: the source node's first, then every node in the patch.
    ofxOceanodeNodeModel* sourceModel = source == nullptr ? nullptr : source->getNodeModel();
    auto addFromGroup = [&](ofParameterGroup& group, bool sameNode) {
        for(auto& entry : group) {
            auto parameter = std::dynamic_pointer_cast<ofxOceanodeAbstractParameter>(entry);
            if(parameter == nullptr || !isNoteRoleType(parameter->valueType())) continue;
            if(parameter->getFlags() & ofxOceanodeParameterFlags_ReadOnly) continue;
            if(std::any_of(noteSetupCandidates.begin(), noteSetupCandidates.end(),
                           [&](const auto& c) { return c.parameter == parameter.get(); })) continue;
            NoteRoleCandidate candidate;
            candidate.parameter = parameter.get();
            candidate.path = container->getTimelineParameterPath(*parameter);
            candidate.label = group.getName() + " / " + parameter->getName();
            candidate.sameNode = sameNode;
            noteSetupCandidates.push_back(candidate);
        }
    };
    if(sourceModel != nullptr) addFromGroup(sourceModel->getParameterGroup(), true);
    // "Every node" means the nodes beside the source (it may sit in a macro).
    ofxOceanodeContainer* candidateContainer = container.get();
    if(sourceModel != nullptr && sourceModel->getHostContainer() != nullptr)
        candidateContainer = sourceModel->getHostContainer();
    for(auto* node : candidateContainer->getAllModules()) {
        if(node == nullptr) continue;
        addFromGroup(node->getParameters(), false);
    }

    if(!editGroupId.empty()) {
        // Retargeting: start from the group's current roles.
        if(const auto* group = timeline.getNoteGroup(trackId, editGroupId)) {
            const std::string* roles[3] = {&group->pitchBindingId, &group->gateBindingId, &group->velocityBindingId};
            for(int role = 0; role < 3; ++role) {
                if(const auto* binding = roles[role]->empty() ? nullptr : timeline.getBinding(trackId, *roles[role]))
                    noteSetupRolePaths[role] = binding->parameterPath;
            }
            std::strncpy(noteSetupName, group->name.c_str(), sizeof(noteSetupName) - 1);
            noteSetupName[sizeof(noteSetupName) - 1] = '\0';
        }
    } else {
        // New group: the clicked parameter takes the role its name suggests
        // (pitch if it suggests none); its node's other parameters fill the rest.
        if(source != nullptr) {
            const std::string sourcePath = container->getTimelineParameterPath(*source);
            const int sourceRole = guessNoteRole(source->getName());
            noteSetupRolePaths[sourceRole < 0 ? 0 : sourceRole] = sourcePath;
            for(const auto& candidate : noteSetupCandidates) {
                if(!candidate.sameNode || candidate.path == sourcePath) continue;
                const int role = guessNoteRole(candidate.parameter->getName());
                if(role >= 0 && noteSetupRolePaths[role].empty()) noteSetupRolePaths[role] = candidate.path;
            }
        }
        const std::string name = sourceModel != nullptr ? sourceModel->getParameterGroup().getName() : std::string("Notes");
        std::strncpy(noteSetupName, name.c_str(), sizeof(noteSetupName) - 1);
        noteSetupName[sizeof(noteSetupName) - 1] = '\0';
        // Default to a track where the clicked parameter already is, else a new track.
        if(noteSetupTrackId.empty() && source != nullptr) {
            const std::string sourcePath = container->getTimelineParameterPath(*source);
            for(const auto& track : timeline.getTracks()) {
                if(track.isWaveTrack) continue;
                if(std::any_of(track.bindings.begin(), track.bindings.end(),
                               [&](const auto& b) { return b.parameterPath == sourcePath; })) {
                    noteSetupTrackId = track.id;
                    break;
                }
            }
        }
    }
    requestNoteSetupPopup = true;
}

void ofxOceanodeTimelineController::drawNoteGroupSetupPopup(ofxOceanodeTimelineManager& timeline) {
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(!ImGui::BeginPopupModal("Piano Roll##noteGroupSetup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    const bool editing = !noteSetupEditGroupId.empty();
    ImGui::TextDisabled(editing ? "Parameters this piano roll plays"
                                : "One timeline row that plays pitch, gate and velocity");
    ImGui::Spacing();
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputText("Name", noteSetupName, sizeof(noteSetupName));

    if(!editing && !noteSetupTrackFixed) {
        const auto* currentTrack = noteSetupTrackId.empty() ? nullptr : timeline.getTrack(noteSetupTrackId);
        ImGui::SetNextItemWidth(260.0f);
        if(ImGui::BeginCombo("Track", currentTrack == nullptr ? "New track" : currentTrack->name.c_str())) {
            if(ImGui::Selectable("New track", currentTrack == nullptr)) noteSetupTrackId.clear();
            for(const auto& track : timeline.getTracks()) {
                if(track.isWaveTrack) continue;
                if(ImGui::Selectable((track.name + "##" + track.id).c_str(), track.id == noteSetupTrackId))
                    noteSetupTrackId = track.id;
            }
            ImGui::EndCombo();
        }
    }
    ImGui::Separator();

    const char* roleLabels[3] = {"Pitch", "Gate", "Velocity"};
    for(int role = 0; role < 3; ++role) {
        std::string preview = "None";
        for(const auto& candidate : noteSetupCandidates)
            if(candidate.path == noteSetupRolePaths[role]) preview = candidate.label;
        if(preview == "None" && !noteSetupRolePaths[role].empty()) preview = noteSetupRolePaths[role];
        ImGui::SetNextItemWidth(260.0f);
        if(ImGui::BeginCombo(roleLabels[role], preview.c_str(), ImGuiComboFlags_HeightLarge)) {
            if(ImGui::IsWindowAppearing()) {
                noteSetupFilter[0] = '\0';
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##filter", "Search parameters", noteSetupFilter, sizeof(noteSetupFilter));
            if(ImGui::Selectable("None", noteSetupRolePaths[role].empty())) noteSetupRolePaths[role].clear();
            bool separatorDrawn = false;
            for(const auto& candidate : noteSetupCandidates) {
                if(!containsCaseInsensitive(candidate.label, noteSetupFilter)) continue;
                if(!candidate.sameNode && !separatorDrawn) {
                    ImGui::Separator();
                    separatorDrawn = true;
                }
                if(ImGui::Selectable((candidate.label + "##" + candidate.path).c_str(), candidate.path == noteSetupRolePaths[role])) {
                    // A parameter plays one role only.
                    for(auto& other : noteSetupRolePaths) if(other == candidate.path) other.clear();
                    noteSetupRolePaths[role] = candidate.path;
                }
            }
            ImGui::EndCombo();
        }
    }
    const bool valid = !noteSetupRolePaths[0].empty() || !noteSetupRolePaths[1].empty();
    if(!valid) ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "Choose at least a pitch or a gate parameter");
    ImGui::Spacing();

    ImGui::BeginDisabled(!valid);
    const bool confirmed = ImGui::Button(editing ? "Save" : "Create");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();

    if(confirmed && valid) {
        const std::string name = noteSetupName[0] == '\0' ? std::string("Notes") : std::string(noteSetupName);
        std::string trackId = noteSetupTrackId;
        if(trackId.empty() || timeline.getTrack(trackId) == nullptr) trackId = timeline.createTrack(name);
        // The binding for each role: the track's existing row for that
        // parameter, or a new one.
        std::string roleIds[3];
        for(int role = 0; role < 3; ++role) {
            const std::string& path = noteSetupRolePaths[role];
            if(path.empty()) continue;
            if(const auto* track = timeline.getTrack(trackId)) {
                for(const auto& binding : track->bindings)
                    if(binding.parameterPath == path) roleIds[role] = binding.id;
            }
            if(!roleIds[role].empty()) continue;
            ofxOceanodeAbstractParameter* parameter = nullptr;
            for(const auto& candidate : noteSetupCandidates)
                if(candidate.path == path) parameter = candidate.parameter;
            if(parameter == nullptr) parameter = container->findTimelineParameter(path);
            if(parameter != nullptr) roleIds[role] = timeline.addBinding(trackId, *parameter);
        }
        if(editing) {
            timeline.setNoteGroupRoles(trackId, noteSetupEditGroupId, roleIds[0], roleIds[1], roleIds[2]);
            timeline.renameNoteGroup(trackId, noteSetupEditGroupId, name);
        } else {
            const std::string groupId = timeline.createNoteGroup(trackId, name, roleIds[0], roleIds[1], roleIds[2]);
            if(!groupId.empty()) {
                std::string laneId;
                const std::string clipId = timeline.createNoteGroupClip(trackId, groupId, name, noteSetupClipStart,
                                                                        noteSetupClipDuration, &laneId);
                if(auto* track = timeline.getTrack(trackId)) track->collapsed = false;
                if(!clipId.empty()) {
                    // Open the new clip's editor right under the group row.
                    const auto* group = timeline.getNoteGroup(trackId, groupId);
                    const auto members = group == nullptr ? std::vector<std::string>() : group->members();
                    editorAnchorBindingId = members.empty() ? std::string() : members.front();
                    editorTrackId = trackId;
                    editorClipId = clipId;
                    editorLaneId = laneId;
                    clipEditorOpen = true;
                    foldedClipEditors.erase(clipId);
                }
            }
        }
        noteSetupCandidates.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void ofxOceanodeTimelineController::requestClipRename(const std::string& trackId,
                                                       const ofxOceanodeTimelineClip& clip) {
    clipRenameTrackId = trackId;
    clipRenameClipId = clip.id;
    std::strncpy(pendingClipRenameName, clip.name.c_str(), sizeof(pendingClipRenameName) - 1);
    pendingClipRenameName[sizeof(pendingClipRenameName) - 1] = '\0';
    requestClipRenamePopup = true;
}

void ofxOceanodeTimelineController::drawClipRenamePopup(ofxOceanodeTimelineManager& timeline) {
    if(!ImGui::BeginPopupModal("Rename Timeline Clip", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool nameEntered = ImGui::InputText(
        "Name", pendingClipRenameName, sizeof(pendingClipRenameName),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    const bool savePressed = ImGui::Button("Save");
    ImGui::SameLine();
    const bool cancelPressed = ImGui::Button("Cancel");
    if((nameEntered || savePressed) && pendingClipRenameName[0] != '\0') {
        timeline.renameClip(clipRenameTrackId, clipRenameClipId, pendingClipRenameName);
        ImGui::CloseCurrentPopup();
    } else if(cancelPressed) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void ofxOceanodeTimelineController::drawRenamePopup(ofxOceanodeTimelineManager& timeline) {
    const char* popupTitle = pendingNewTrackDialog ? "New Timeline Track" : "Rename Timeline Track";
    // This can be requested from a node's right-click menu on the main
    // canvas while this controller's own window is elsewhere (a background
    // docked tab, or a separate OS-level window under multi-viewport
    // docking) -- without an explicit position/viewport, ImGui would anchor
    // the popup to wherever it happened to be called from, which is exactly
    // the "shows up in the Timeline window instead of on the canvas" bug
    // this fixes. Force it onto the main viewport, centered, every time.
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(!ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool entered = ImGui::InputText("Name", pendingTrackName, sizeof(pendingTrackName),
                                          ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    const bool nameValid = pendingTrackName[0] != '\0';
    ImGui::BeginDisabled(!nameValid);
    const bool confirmed = ImGui::Button(pendingNewTrackDialog ? "Create" : "Save");
    ImGui::EndDisabled();
    if((confirmed || entered) && nameValid) {
        timeline.renameTrack(pendingTrackId, pendingTrackName);
        pendingNewTrackDialog = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void ofxOceanodeTimelineController::drawClipPopup(ofxOceanodeTimelineManager& timeline) {
    if(!ImGui::BeginPopupModal("New Timeline Clip", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::InputText("Name", pendingClipName, sizeof(pendingClipName));
    // One list, LFO included. This used to be two combos -- an "Automation
    // clip / LFO clip" question here and an "Automation type" one below --
    // which asked twice for a single decision and made the LFO look like a
    // different category of thing rather than another kind of clip.
    ImGui::Combo("Clip type", &pendingClipLaneType, kLaneTypeOptions, kClipCreationOptionCount);
    const bool isLfoClip = pendingClipLaneType == kLfoClipOptionIndex;
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
                    // Picking a parameter pre-selects the type it is bound
                    // with -- but never over an explicit LFO choice, which
                    // is not a lane type and must not be silently undone.
                    if(!isLfoClip) pendingClipLaneType = optionIndexForLaneType(binding.laneType);
                }
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", binding.parameterPath.c_str());
            }
            ImGui::EndCombo();
        }
    }
    ImGui::InputDouble("Start beat", &pendingStartBeat, editIncrement(), 1.0, "%.4f");
    ImGui::InputDouble("Duration", &pendingDurationBeats, editIncrement(), 1.0, "%.4f");
    const bool pianoRollClip = !isLfoClip &&
        laneTypeFromOptionIndex(pendingClipLaneType) == ofxOceanodeTimelineLaneType::PianoRoll;
    if(pianoRollClip) ImGui::TextDisabled("Next: choose the pitch, gate and velocity parameters");
    if(ImGui::Button(pianoRollClip ? "Next..." : "Create Clip")) {
        const double startBeat = snapBeat(pendingStartBeat);
        const double durationBeats = std::max(1.0 / kPPQ, snapBeat(pendingDurationBeats));
        const auto* pianoTrack = timeline.getTrack(pendingTrackId);
        const auto* pianoGroup = pianoTrack == nullptr ? nullptr
            : ofxOceanodeTimelineManager::findNoteGroupForBinding(*pianoTrack, pendingClipBindingId);
        if(pianoRollClip && pianoGroup != nullptr) {
            // The row already plays in a note group: a new clip for that group.
            std::string laneId;
            const std::string groupId = pianoGroup->id;
            const std::string clipId = timeline.createNoteGroupClip(pendingTrackId, groupId, pendingClipName,
                                                                    startBeat, durationBeats, &laneId);
            if(!clipId.empty()) {
                editorAnchorBindingId = pendingClipBindingId;
                editorTrackId = pendingTrackId;
                editorClipId = clipId;
                editorLaneId = laneId;
                clipEditorOpen = true;
                foldedClipEditors.erase(clipId);
            }
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        if(pianoRollClip) {
            // A piano roll plays several parameters: pick them (this row's
            // parameter is the starting point), then the group row is made.
            const auto* binding = timeline.getBinding(pendingTrackId, pendingClipBindingId);
            auto* source = binding == nullptr ? nullptr : container->findTimelineParameter(binding->parameterPath);
            openNoteGroupSetup(timeline, source, pendingTrackId, std::string(), startBeat, durationBeats);
            if(source == nullptr && binding != nullptr) noteSetupRolePaths[0] = binding->parameterPath;
            if(pendingClipName[0] != '\0') {
                std::strncpy(noteSetupName, pendingClipName, sizeof(noteSetupName) - 1);
                noteSetupName[sizeof(noteSetupName) - 1] = '\0';
            }
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        const auto clipId = isLfoClip
            ? timeline.createLfoClip(pendingTrackId, pendingClipBindingId,
                                     pendingClipName[0] == '\0' ? "LFO" : pendingClipName,
                                     startBeat, durationBeats)
            : timeline.createClip(pendingTrackId, pendingClipName[0] == '\0' ? "Clip" : pendingClipName,
                                  startBeat, durationBeats);
        if(!isLfoClip) {
            if(const auto* binding = timeline.getBinding(pendingTrackId, pendingClipBindingId)) {
            const auto laneType = laneTypeFromOptionIndex(pendingClipLaneType);
            const auto laneId = timeline.createLane(pendingTrackId, clipId, binding->parameterPath, laneType);
            if(!laneId.empty()) {
                timeline.addBindingToLane(pendingTrackId, clipId, laneId, binding->id);
                if(auto* lane = timeline.getLane(pendingTrackId, clipId, laneId)) {
                    if(laneType != ofxOceanodeTimelineLaneType::PianoRoll &&
                       laneType != ofxOceanodeTimelineLaneType::MultiGate &&
                       laneType != ofxOceanodeTimelineLaneType::Wave) {
                        if(auto* parameter = container->findTimelineParameter(binding->parameterPath)) {
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
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
