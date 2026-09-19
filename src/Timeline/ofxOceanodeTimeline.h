#ifndef ofxOceanodeTimeline_h
#define ofxOceanodeTimeline_h

#include "ofMain.h"
#include "ofxOceanodeTransport.h"
#include "ofxOceanodeScheduling.h"
#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class ofxOceanodeAbstractParameter;
class ofxOceanodeContainer;

// How a binding's per-frame value should combine with whatever other
// bindings already write to the same parameter. Replace is the classic
// single-source case; the others let several lanes/tracks drive one
// parameter together (e.g. a piano-roll gate multiplying a level curve).
// The base contributor's own mode is irrelevant (nothing precedes it to
// combine with) -- each later contributor's mode decides how it blends
// onto the running result, the same way layer blend modes work.
enum class ofxOceanodeTimelineAutomationMode {
    Replace,
    Add,
    Multiply,
    Min,
    Max
};

enum class ofxOceanodeTimelineLaneType {
    Step,
    PianoRoll,
    Curve,
    // The four below mirror four older, standalone ofxOceanodeNodeModel
    // "track" nodes (ofxSantiNodes' valueTrack/stepValueTrack/gateTrack,
    // ofxOceanodeSuperCollider's waveTrack) that each had their own
    // ppqTimeline/transportTrack-based timeline, predating this one. They
    // live here as lane types instead so they share this timeline's clip
    // (position/stretch/repeat), track, blend-mode and grouping machinery
    // rather than duplicating it a fourth and fifth time.
    MultiValue,
    MultiSlider,
    MultiGate,
    Wave
};

struct ofxOceanodeTimelineStep {
    double startBeat = 0.0;
    // A non-positive duration uses the lane's current grid-cell length.
    double durationBeats = 0.0;
    std::string value;
    // Probability that this step fires. A value of 1 is an ordinary step and
    // 0 is a muted step. Keeping it on the step makes the editor genuinely
    // probabilistic instead of treating the whole lane as a single gate.
    float probability = 1.0f;
};

class ofxOceanodeTimelineStepLane {
public:
    std::vector<ofxOceanodeTimelineStep> steps;

    void sortSteps();
    void setStep(double startBeat, const std::string& value, double durationBeats = 0.0,
                float probability = 1.0f);
    bool removeStep(double startBeat, double epsilon = 1e-6);

    ofJson toJson() const;
    void fromJson(const ofJson& json);
};

struct ofxOceanodeTimelineCurvePoint {
    double beat = 0.0;
    float value = 0.0f;
};

// One tension per segment, matching the sigmoidCurve node: inflection moves
// the bend horizontally and steepness controls how abrupt it is.
struct ofxOceanodeTimelineCurveTension {
    float inflection = 0.5f;
    float steepness = 1.0f;
};

// Shared by both automation evaluation (ofxOceanodeTimeline.cpp) and the
// timeline editor (ofxOceanodeTimelineController.cpp) so a curve always
// renders exactly what gets applied to the parameter. The steepness range
// is kept symmetric under reciprocal (0.1 <-> 10) so the "logarithmic" and
// "exponential" ends of LogExp bend by comparable amounts instead of one
// side collapsing into a near-step curve before the other looks bent at all.
namespace ofxOceanodeTimelineCurve {
    enum class CurveInterpolationMode {
        Step,
        Linear,
        LogExp,
        Sigmoid
    };

    CurveInterpolationMode curveInterpolationMode(const std::string& name);
    float sigmoidFlex(float x, float inflection, float steepness);
    float curveSegmentShape(float x, CurveInterpolationMode interpolation,
                            const ofxOceanodeTimelineCurveTension& tension);
}

struct ofxOceanodeTimelinePianoNote {
    double startBeat = 0.0;
    double durationBeats = 0.25;
    int pitch = 60;
    float velocity = 1.0f;
    float probability = 1.0f;
};

// One block on a MultiValue lane's row -- see ofxOceanodeTimelineLane::
// multiValueRows. Modeled on ValueRegion from the old standalone "Multi
// Value Track" node (ofxSantiNodes/valueTrack.h): an arbitrary (not
// normalized) float value held for [startBeat, startBeat+durationBeats).
// Both beats live in the clip's SOURCE-beat space, same as
// ofxOceanodeTimelineCurvePoint::beat and ofxOceanodeTimelineStep::
// startBeat -- clip stretch/repeat is handled once, generically, by
// ofxOceanodeTimelineClipTime, not re-implemented per lane type.
struct ofxOceanodeTimelineValueRegion {
    double startBeat = 0.0;
    double durationBeats = 1.0;
    float value = 0.0f;
    double end() const { return startBeat + durationBeats; }
};

// One block on a MultiGate lane's row -- see ofxOceanodeTimelineLane::
// multiGateRows. Modeled on GateRegion from the old standalone "Multi Gate
// Track" node (ofxSantiNodes/gateTrack.h): on for
// [startBeat, startBeat+durationBeats), off elsewhere. Same source-beat-
// space note as ofxOceanodeTimelineValueRegion above.
struct ofxOceanodeTimelineGateRegion {
    double startBeat = 0.0;
    double durationBeats = 1.0;
    double end() const { return startBeat + durationBeats; }
};

struct ofxOceanodeTimelineParameterBinding {
    std::string id;
    std::string parameterPath;
    std::string valueType;
    std::string defaultValue;
    ofxOceanodeTimelineAutomationMode mode = ofxOceanodeTimelineAutomationMode::Replace;
    ofxOceanodeTimelineLaneType laneType = ofxOceanodeTimelineLaneType::Step;
    bool bypass = false;
    bool missingTarget = false;
    // A live override takes priority over whatever clip automation computes
    // for this binding's parameter, for as long as it's set. Used by the
    // piano roll's keyboard strip so clicking a key sounds a note
    // immediately regardless of playhead/clip content, without touching any
    // clip data. Never serialized -- it's momentary UI interaction state.
    bool hasLiveOverride = false;
    std::string liveOverrideValue;
    // When several bindings (different clips/lanes/tracks, combined via
    // Add/Multiply/Min/Max) drive the same parameter, their combined result
    // can land outside the parameter's own min/max -- nothing about Add or
    // Multiply keeps the sum/product inside range the way a single Replace
    // binding naturally does. This clamps the final combined value back to
    // the parameter's min/max before it's applied. Only the binding that
    // ends up "authoritative" for its parameter (see defaultBinding in
    // applyAutomation -- the first non-bypassed binding sharing that
    // parameterPath, the same one whose valueType/defaultValue already act
    // as the shared source of truth today) is consulted; the flag still
    // lives per-binding, matching valueType/defaultValue, rather than
    // needing a separate per-parameter table.
    bool clampToParameterRange = true;
};

// A lane is an automation editor inside a clip. It may drive more than one
// parameter, which is useful for piano-roll style editors (pitch/gate/velocity)
// and for reusing one curve as the source of several parameters.
struct ofxOceanodeTimelineLane {
    std::string id;
    std::string name;
    ofxOceanodeTimelineLaneType type = ofxOceanodeTimelineLaneType::Step;
    std::vector<std::string> bindingIds;
    ofxOceanodeTimelineStepLane step;
    int stepCount = 16;
    double beatsPerStep = 0.25;
    float valueMin = 0.0f;
    float valueMax = 1.0f;
    bool probabilityEnabled = true;
    // Probability: roll the gate, Always: ignore probability, Mute: no output.
    std::string behavior = "Probability";
    int pianoLowPitch = 36;
    int pianoHighPitch = 84;
    bool pianoSnapToGrid = true;
    std::string pianoPitchBindingId;
    std::string pianoGateBindingId;
    std::string pianoVelocityBindingId;
    float pianoDefaultVelocity = 0.8f;
    bool pianoMonophonic = false;
    bool curveClamp = true;
    std::string curveInterpolation = "Linear";
    std::vector<ofxOceanodeTimelineCurvePoint> curvePoints;
    std::vector<ofxOceanodeTimelineCurveTension> curveTensions;
    std::vector<ofxOceanodeTimelinePianoNote> pianoNotes;

    // MultiValue / MultiGate: N independent rows, one component of a bound
    // vector-typed parameter per row (row i -> component i of whatever
    // vector<float>/vector<int>/vector<bool> parameter this lane's
    // bindingIds point at). See ofxOceanodeTimelineValueRegion/GateRegion
    // above. A lane only ever populates whichever of these two vectors
    // matches its own type; multiRowCount is shared by both so switching a
    // lane between MultiValue and MultiGate keeps the row count (not the
    // block data itself, which is type-specific).
    int multiRowCount = 4;
    std::vector<std::vector<ofxOceanodeTimelineValueRegion>> multiValueRows;
    std::vector<std::vector<ofxOceanodeTimelineGateRegion>> multiGateRows;
    // MultiValue: when true, a block's value is rounded to the nearest
    // integer everywhere it's entered or displayed (drag, the numeric edit
    // popup, and the label drawn on the block) -- purely a display/editing
    // convenience toggle on the lane, independent of whatever type the
    // bound vector parameter itself is (a vector<float> parameter still
    // happily accepts "3.0, 5.0" strings).
    bool multiValueInteger = false;
    // MultiSlider ("step value"): a DENSE per-cell value grid, one entry
    // per step (sized to stepCount, reusing this lane's own stepCount/
    // beatsPerStep grid rather than duplicating them) -- unlike the Step
    // lane's sparse `step.steps` (only has an entry where the user placed
    // one, whose height is a fire *probability*), every cell here always
    // holds a value (default 0) and that value IS the output, mapped
    // through valueMin/valueMax. Painted with a continuous vertical-drag
    // stroke, matching the old standalone "Step Value Track" node
    // (ofxSantiNodes/stepValueTrack.h).
    std::vector<float> multiSliderValues;
    // Curve / MultiSlider: optional value-axis "snap to grid" -- when >= 2,
    // the lane's value range (0..1 for Curve, valueMin..valueMax for
    // MultiSlider) is divided into this many evenly-spaced discrete levels,
    // drawn as horizontal guide lines, and any value entered by dragging or
    // painting magnetizes to the nearest one. 0 (the default) means free
    // (no value snapping) -- separate from beatsPerStep, which is the
    // existing *time*-axis grid every lane type already has.
    int valueQuantizeSteps = 0;
    // Kept for backwards-compatible loading of the first Wave-lane format.
    // New timelines store audio on the clip itself, because Wave is a track
    // type, not an automation/lane type.
    std::string waveFilePath;
    float waveGain = 1.0f;
    int waveNumChannels = 0;
    double waveFileDurationMs = 0.0;
    std::vector<float> waveformPeaks; // not persisted -- see waveNumChannels for its layout
    // A Curve lane with no parameter binding can be used as the audio
    // volume envelope for a clip on a Wave track. Keeping this marker
    // explicit avoids overloading an ordinary unbound Curve lane and keeps
    // the envelope in the clip's own source-beat space.
    bool isWaveVolume = false;
    // LFO clips use ordinary Curve data for their parameter automation, but
    // keep the semantic role here so the evaluator and editor do not have to
    // infer it from the user-facing lane name.
    std::string lfoParameter;
};

struct ofxOceanodeTimelineClip {
    std::string id;
    std::string name;
    double startBeat = 0.0;
    double durationBeats = 4.0;
    // The source length is kept separately so a clip can be stretched without
    // destroying its original content length.
    double contentDurationBeats = 4.0;
    // Timeline beats occupied by one source beat. Keeping this independently
    // from the total clip duration lets a stretched cycle retain its scale
    // when the visible clip edge is later extended to add repetitions.
    double contentStretch = 1.0;
    bool repeatContent = true;
    // An LFO clip is a self-contained continuous modulator. Its Curve lanes
    // describe the oscillator controls and this binding receives the final
    // normalized result mapped through lfoOutputMin/lfoOutputMax.
    bool isLfo = false;
    std::string lfoOutputBindingId;
    float lfoOutputMin = 0.0f;
    float lfoOutputMax = 1.0f;
    // Audio clips belong to a Wave track. These fields intentionally live on
    // the clip rather than on a lane: a Wave track can contain several
    // independently placed files. Per-clip volume automation is represented
    // by an optional isWaveVolume Curve lane below.
    std::string waveFilePath;
    float waveGain = 1.0f;
    // Base playback multiplier for this audio clip. Stretching the clip
    // changes the effective rate sent to the audio provider, while this
    // value remains the user-facing clip property.
    float wavePlaybackRate = 1.0f;
    // Offset into the full source file, expressed in source beats. A newly
    // loaded clip starts at zero; splitting a clip advances this for the
    // right-hand slice while keeping both clips pointed at the same file.
    double waveSourceStartBeat = 0.0;
    // Full-file duration in the same source-beat domain as
    // waveSourceStartBeat. Keeping this fixed when BPM changes makes slice
    // boundaries stable instead of recomputing the file length from the
    // current transport tempo every frame.
    double waveFileDurationBeats = 0.0;
    bool waveReverse = false;
    int waveNumChannels = 0;
    double waveFileDurationMs = 0.0;
    std::vector<float> waveformPeaks;
    std::vector<ofxOceanodeTimelineLane> lanes;
};

// Canonical conversion between source beats (the data stored in a clip) and
// timeline beats (where the clip is displayed and played). Keeping this in
// the model prevents the controller and evaluator from developing subtly
// different stretch/repeat behaviour.
namespace ofxOceanodeTimelineClipTime {
    double sourceDuration(const ofxOceanodeTimelineClip& clip);
    double stretch(const ofxOceanodeTimelineClip& clip);
    double cycleDuration(const ofxOceanodeTimelineClip& clip);
    double sourceToTimelineBeat(const ofxOceanodeTimelineClip& clip,
                                double sourceBeat, int64_t cycle = 0);
    double timelineToSourceBeat(const ofxOceanodeTimelineClip& clip,
                                double timelineBeat);
    int64_t cycleIndex(const ofxOceanodeTimelineClip& clip,
                       double timelineBeat);
}

namespace ofxOceanodeTimelineLfo {
    // Evaluates the complete LFO result in normalized 0..1 space. The shape
    // is intentionally shared by playback and the editor's result view.
    float evaluate(const ofxOceanodeTimelineClip& clip, double sourceBeat);
    float evaluateParameter(const ofxOceanodeTimelineClip& clip,
                            const std::string& parameter,
                            double sourceBeat, float fallback);
}

struct ofxOceanodeTimelineTrack {
    std::string id;
    std::string name;
    ofColor color = ofColor(65, 165, 245, 255);
    bool collapsed = false;
    std::vector<ofxOceanodeTimelineParameterBinding> bindings;
    std::vector<ofxOceanodeTimelineClip> clips;
    // A Wave Track is created via ofxOceanodeTimelineManager::createWaveTrack
    // and never gets any bindings -- it isn't automation for a parameter at
    // all, just audio clips (each holding exactly one Wave lane) arranged on
    // the timeline. Kept as a plain flag on an ordinary track, rather than a
    // parallel data structure, so it still gets every other track feature
    // (color, collapse, clip grouping, JSON persistence) for free; the
    // controller uses this flag to render it as a single always-visible row
    // (there's nothing to expand into per-binding rows) and to skip the
    // usual parameter-picking step when creating a clip on it.
    bool isWaveTrack = false;
    // Height of the single Wave Track row in the timeline. This is persisted
    // because a taller waveform is part of the timeline layout, not a
    // transient editor preference.
    float waveTrackHeight = 96.0f;
    // Track-wide volume controls. The automation points live in global
    // timeline-beat space and affect every clip on this Wave Track.
    float waveVolume = 1.0f;
    bool waveVolumeAutomationEnabled = true;
    std::string waveVolumeInterpolation = "Linear";
    std::vector<ofxOceanodeTimelineCurvePoint> waveVolumePoints;
    std::vector<ofxOceanodeTimelineCurveTension> waveVolumeTensions;
};

// A lightweight cross-track grouping: it holds no automation data of its
// own, just references to existing clips by (trackId, clipId). Each member
// clip still belongs to its own track and serializes exactly as before --
// the group is purely coordination metadata layered on top, so the
// controller can fan a move/stretch drag on any one member out to the rest.
// (This is "option A" from the multi-lane clip design notes: a first-class
// track-independent clip would be the "real" version of a combined clip,
// but is a much bigger restructure than what grouping already-existing
// clips needs.)
struct ofxOceanodeTimelineClipGroup {
    std::string id;
    // (trackId, clipId) pairs, in the order they were grouped -- not
    // necessarily timeline order.
    std::vector<std::pair<std::string, std::string>> members;
};

// Optional, addon-agnostic hook that lets a project which also includes
// ofxOceanodeSuperCollider drive REAL audio playback for Wave tracks,
// without ofxOceanodeTimeline itself ever depending on SuperCollider types
// -- this header and its .cpp include nothing SC-specific, and never will.
// The conditional-compilation boundary the user asked for (Wave audio only
// available when ofxOceanodeSuperCollider is in the project) lives entirely
// on the implementer's side: a small bridge class inside
// ofxOceanodeSuperCollider, gated by its own build flag, implements this
// interface and registers itself via setWaveAudioProvider below. With no
// provider registered, Wave lanes are visual-only -- the waveform displays
// and the clip can be positioned/stretched like any other clip, but
// nothing actually plays.
class ofxOceanodeTimelineWaveAudioProvider {
public:
    virtual ~ofxOceanodeTimelineWaveAudioProvider() = default;
    // Called once per frame for every clip on every Wave track. inactive is
    // sent too, which lets a backend stop voices as the playhead leaves a
    // clip without needing a separate begin/end-frame callback.
    virtual void updateWaveClip(const std::string& trackId, const std::string& clipId,
                                const std::string& filePath, float gain, float trackVolume,
                                int numChannels, double clipContentStartBeat, double contentDurationBeats,
                                double sourceStartBeat, double sourceFileDurationBeats,
                                float playbackRate, bool reverse, float bpm, bool isPlaying, bool active,
                                bool forceTransportSync = false) = 0;
    virtual void releaseWaveClip(const std::string& trackId, const std::string& clipId) = 0;
};

class ofxOceanodeTimelineManager {
public:
    explicit ofxOceanodeTimelineManager(ofxOceanodeContainer* container = nullptr);

    void setContainer(ofxOceanodeContainer* container);
    // Applies automation for the current frame. Equivalent to calling
    // evaluateAutomation() followed by applyAutomation().
    void update();
    // Recomputes this frame's active lane values, resolves the loop/BPM
    // automation, and applies live piano-roll pitch ranges. Call once per
    // frame, before node updates.
    void evaluateAutomation();
    // Re-applies the values computed by the last evaluateAutomation() call,
    // without recomputing them. Intended to be called again after node
    // updates, so a node that writes back to its own parameter during
    // update() doesn't silently override automation for that frame -
    // without paying for a second full evaluation pass.
    void applyAutomation();
    // See ofxOceanodeTimelineParameterBinding::hasLiveOverride. setLiveOverride
    // takes effect starting with the next evaluateAutomation()/applyAutomation()
    // pass; the caller is responsible for calling clearLiveOverride once the
    // interaction ends (e.g. on mouse release) -- there's no timeout.
    void setLiveOverride(const std::string& trackId, const std::string& bindingId, const std::string& value);
    void clearLiveOverride(const std::string& trackId, const std::string& bindingId);
    void clear();

    std::string createTrack(const std::string& requestedName = "Timeline Track");
    // A track with no bindings, dedicated to Wave clips (see
    // ofxOceanodeTimelineTrack::isWaveTrack). Reuses createTrack for the
    // usual id/uniqueness/list bookkeeping and just marks the result.
    std::string createWaveTrack(const std::string& requestedName = "Wave Track");
    bool removeTrack(const std::string& trackId);
    ofxOceanodeTimelineTrack* getTrack(const std::string& trackId);
    const ofxOceanodeTimelineTrack* getTrack(const std::string& trackId) const;
    const std::vector<ofxOceanodeTimelineTrack>& getTracks() const { return tracks; }

    std::string addBinding(const std::string& trackId,
                          ofxOceanodeAbstractParameter& parameter,
                          ofxOceanodeTimelineAutomationMode mode = ofxOceanodeTimelineAutomationMode::Replace);
    bool renameTrack(const std::string& trackId, const std::string& requestedName);
    void requestTrackRename(const std::string& trackId, bool isNewTrack = false);
    bool consumePendingTrackRename(std::string& trackId, bool* isNewTrack = nullptr);
    bool isParameterBound(const ofxOceanodeAbstractParameter& parameter) const;
    bool getParameterTrackColor(const ofxOceanodeAbstractParameter& parameter, ofColor& color) const;
    bool isStepLaneCompatible(const ofxOceanodeAbstractParameter& parameter) const;
    bool removeBinding(const std::string& trackId, const std::string& bindingId);
    bool setLaneType(const std::string& trackId, const std::string& bindingId, ofxOceanodeTimelineLaneType laneType);
    bool setBindingMode(const std::string& trackId, const std::string& bindingId, ofxOceanodeTimelineAutomationMode mode);
    bool setBindingClamp(const std::string& trackId, const std::string& bindingId, bool clampToParameterRange);
    ofxOceanodeTimelineParameterBinding* getBinding(const std::string& trackId, const std::string& bindingId);
    const ofxOceanodeTimelineParameterBinding* getBinding(const std::string& trackId, const std::string& bindingId) const;

    std::string createClip(const std::string& trackId,
                           const std::string& requestedName = "Clip",
                           double startBeat = 0.0,
                           double durationBeats = 4.0);
    std::string createLfoClip(const std::string& trackId,
                              const std::string& outputBindingId = std::string(),
                              const std::string& requestedName = "LFO",
                              double startBeat = 0.0,
                              double durationBeats = 4.0);
    bool renameClip(const std::string& trackId, const std::string& clipId, const std::string& requestedName);
    bool removeClip(const std::string& trackId, const std::string& clipId);
    ofxOceanodeTimelineClip* getClip(const std::string& trackId, const std::string& clipId);
    const ofxOceanodeTimelineClip* getClip(const std::string& trackId, const std::string& clipId) const;
    std::string createLane(const std::string& trackId, const std::string& clipId,
                           const std::string& requestedName = "Lane",
                           ofxOceanodeTimelineLaneType type = ofxOceanodeTimelineLaneType::Step);
    bool removeLane(const std::string& trackId, const std::string& clipId, const std::string& laneId);
    bool addBindingToLane(const std::string& trackId, const std::string& clipId,
                          const std::string& laneId, const std::string& bindingId);
    bool removeBindingFromLane(const std::string& trackId, const std::string& clipId,
                               const std::string& laneId, const std::string& bindingId);
    ofxOceanodeTimelineLane* getLane(const std::string& trackId, const std::string& clipId, const std::string& laneId);
    const ofxOceanodeTimelineLane* getLane(const std::string& trackId, const std::string& clipId, const std::string& laneId) const;
    bool setClipLaneType(const std::string& trackId, const std::string& clipId,
                         const std::string& laneId, ofxOceanodeTimelineLaneType type);
    bool setClipTiming(const std::string& trackId, const std::string& clipId,
                       double startBeat, double durationBeats);
    bool setClipContentDuration(const std::string& trackId, const std::string& clipId,
                                 double contentDurationBeats, bool repeatContent);
    bool consolidateClipContent(const std::string& trackId, const std::string& clipId);
    bool setClipStep(const std::string& trackId, const std::string& clipId, const std::string& laneId,
                     double startBeat, const std::string& value, double durationBeats = 0.0);
    bool removeClipStep(const std::string& trackId, const std::string& clipId, const std::string& laneId,
                        double startBeat);
    // MultiValue / MultiGate row count (see ofxOceanodeTimelineLane::
    // multiRowCount). Preserves existing row data when growing; drops the
    // trailing rows' data when shrinking, the same "data is lost" tradeoff
    // the old standalone Multi Value/Gate Track nodes made when their own
    // Num Lanes shrank.
    bool setLaneMultiRowCount(const std::string& trackId, const std::string& clipId,
                              const std::string& laneId, int rowCount);
    // Recomputes waveNumChannels/waveFileDurationMs/waveformPeaks from
    // whatever waveFilePath currently points at (WAV files only, matching
    // the old standalone Wave Track node's own reader). Pure file IO, no
    // SuperCollider dependency -- safe to call unconditionally regardless
    // of whether a wave audio provider is registered. Called automatically
    // by fromJson()/loadPreset() for every Wave lane, and should also be
    // called by the controller whenever the user edits waveFilePath.
    bool reloadWaveform(const std::string& trackId, const std::string& clipId, const std::string& laneId);
    bool reloadWaveform(const std::string& trackId, const std::string& clipId);

    // Evaluates the optional per-clip Wave volume lane. A clip without one
    // uses unity; the legacy track-level volume fields remain readable for
    // old presets but are no longer the authoring model.
    float evaluateWaveClipVolume(const std::string& trackId, const std::string& clipId,
                                 double beat) const;
    // Compatibility entry point: enables the track-wide volume editor for a
    // Wave Track. It no longer creates a lane inside the supplied clip.
    std::string createWaveVolumeLane(const std::string& trackId, const std::string& clipId);

    float evaluateWaveTrackVolume(const std::string& trackId, double beat) const;
    std::vector<ofxOceanodeTimelineCurvePoint>& getWaveTrackVolumePoints(const std::string& trackId);
    const std::vector<ofxOceanodeTimelineCurvePoint>& getWaveTrackVolumePoints(const std::string& trackId) const;
    bool addWaveTrackVolumePoint(const std::string& trackId, double beat, float value);
    bool removeWaveTrackVolumePoint(const std::string& trackId, double beat, double epsilon = 1e-6);

    // Registers/clears the optional audio backend for Wave lanes (see
    // ofxOceanodeTimelineWaveAudioProvider above). The manager does not own
    // the provider; the caller (typically a bridge object living inside
    // ofxOceanodeSuperCollider) is responsible for its lifetime and must
    // clear it (pass nullptr) before destroying it.
    void setWaveAudioProvider(ofxOceanodeTimelineWaveAudioProvider* provider) { waveAudioProvider = provider; }

    // Cross-track clip grouping (see ofxOceanodeTimelineClipGroup above).
    // groupClips folds in the membership of any group its arguments
    // already belong to, so grouping a mix of already-grouped and
    // ungrouped clips merges everything into one group instead of leaving
    // overlapping ones -- returns the new group's id, or an empty string if
    // fewer than two distinct valid clips ended up in it. ungroupClip
    // dissolves the whole group a clip belongs to (groups are flat, not
    // nested, so there's no partial-ungroup of just one member).
    std::string groupClips(const std::vector<std::pair<std::string, std::string>>& members);
    bool ungroupClip(const std::string& trackId, const std::string& clipId);
    const ofxOceanodeTimelineClipGroup* getGroupForClip(const std::string& trackId, const std::string& clipId) const;
    const std::vector<ofxOceanodeTimelineClipGroup>& getClipGroups() const { return clipGroups; }

    bool isBpmAutomationEnabled() const { return bpmAutomationEnabled; }
    void setBpmAutomationEnabled(bool enabled);
    bool isBpmLaneCollapsed() const { return bpmLaneCollapsed; }
    void setBpmLaneCollapsed(bool collapsed) { bpmLaneCollapsed = collapsed; }
    float getBpmMinimum() const { return bpmMinimum; }
    float getBpmMaximum() const { return bpmMaximum; }
    void setBpmRange(float minimum, float maximum);
    std::vector<ofxOceanodeTimelineCurvePoint>& getBpmAutomationPoints() { return bpmAutomationPoints; }
    const std::vector<ofxOceanodeTimelineCurvePoint>& getBpmAutomationPoints() const { return bpmAutomationPoints; }
    std::vector<ofxOceanodeTimelineCurveTension>& getBpmCurveTensions() { return bpmCurveTensions; }
    const std::vector<ofxOceanodeTimelineCurveTension>& getBpmCurveTensions() const { return bpmCurveTensions; }
    const std::string& getBpmInterpolation() const { return bpmInterpolation; }
    void setBpmInterpolation(const std::string& interpolation);
    float evaluateBpm(double beat, float fallbackBpm) const;
    double beatToSeconds(double beat, float fallbackBpm) const;

    bool isLoopEnabled() const { return loopEnabled; }
    bool didLoopWrapThisFrame() const { return loopWrappedThisFrame; }
    void setLoopEnabled(bool enabled) {
        if(loopEnabled != enabled) hasEvaluatedTransportBeat = false;
        loopEnabled = enabled;
    }
    double getLoopStartBeat() const { return loopStartBeat; }
    double getLoopEndBeat() const { return loopEndBeat; }
    void setLoopRange(double startBeat, double endBeat);

    // The timeline isn't always 4/4: numerator/denominator are stored and
    // persisted like any other project setting, and getBeatsPerBar() is
    // what every bar-line/grid computation in the timeline UI should use
    // instead of assuming 4 beats (quarter notes) per bar.
    int getTimeSignatureNumerator() const { return timeSignatureNumerator; }
    int getTimeSignatureDenominator() const { return timeSignatureDenominator; }
    double getBeatsPerBar() const { return timeSignatureNumerator * (4.0 / timeSignatureDenominator); }
    void setTimeSignature(int numerator, int denominator);

    // --- Timestamped scheduling -------------------------------------------
    // A parameter whose backend registered a handler through
    // ofxOceanodeScheduling receives its discrete events (steps, notes, gates,
    // multi-value blocks) ahead of time, with the exact instant they are due,
    // instead of on the GUI frame that happens to cross them. Continuous lanes
    // (curves) and any parameter without such a backend keep the frame path.
    void setSchedulingEnabled(bool enabled);
    bool isSchedulingEnabled() const { return schedulingEnabled; }
    // How far ahead events are handed to the backend. It must exceed the worst
    // frame interval that still has to sound on time; 120 ms covers a GUI
    // stall of several frames at 60 FPS.
    void setSchedulingLookaheadMs(double milliseconds);
    double getSchedulingLookaheadMs() const { return schedulingLookaheadMs; }
    // Editors may call this after changing clip/lane/loop data; the scheduler
    // also repairs itself when it notices the playhead's value is not the one
    // the backend was given.
    void invalidateSchedule();

    ofJson toJson() const;
    void fromJson(const ofJson& json);
    bool savePreset(const std::string& presetFolderPath) const;
    bool loadPreset(const std::string& presetFolderPath);

    // Public because the controller UI displays a binding's blend mode
    // (e.g. next to the parameter name) and needs to turn it into a label.
    static std::string modeToString(ofxOceanodeTimelineAutomationMode mode);
    static ofxOceanodeTimelineAutomationMode modeFromString(const std::string& mode);

private:
    static std::string laneTypeToString(ofxOceanodeTimelineLaneType laneType);
    static ofxOceanodeTimelineLaneType laneTypeFromString(const std::string& laneType);
    static std::string makeId(const char* prefix, uint64_t number);
    std::string makeUniqueTrackName(const std::string& requestedName) const;
    std::string makeUniqueClipName(const ofxOceanodeTimelineTrack& track, const std::string& requestedName) const;
    std::string makeUniqueBindingId() const;
    std::string makeUniqueTrackId() const;
    std::string makeUniqueClipId() const;
    std::string makeUniqueLaneId() const;
    std::string makeUniqueGroupId() const;
    // Keeps clipGroups' invariants (every member clip still exists, every
    // group has at least two members) after a clip is removed -- called
    // from removeClip/removeTrack rather than left to every caller.
    void removeClipFromGroups(const std::string& trackId, const std::string& clipId);
    void clearTimelineFlag(const ofxOceanodeTimelineTrack& track);
    void refreshTimelineFlag(const std::string& parameterPath);
    void releaseWaveClipIfNeeded(const std::string& trackId, const std::string& clipId);

    using ActiveValueMap = std::map<std::string,
        std::vector<std::pair<ofxOceanodeTimelineAutomationMode, std::string>>>;
    // Pure evaluation of every binding at one beat. evaluateAutomation() uses
    // it for the current playhead; the scheduler uses it for beats that have
    // not been reached yet, which is what makes lookahead possible without a
    // second implementation of the lane semantics.
    void collectActiveValues(double beat, bool isPlaying, bool applyPianoRanges,
                             ActiveValueMap& activeValues) const;
    bool computeParameterValue(const std::vector<const ofxOceanodeTimelineParameterBinding*>& bindings,
                               const ActiveValueMap& activeValues,
                               ofxOceanodeAbstractParameter* parameter,
                               std::string& outValue) const;
    // Beats in (fromBeat, toBeat] at which any discrete lane can change value:
    // clip edges, repeat cycles, step grid cells, step/region/note boundaries.
    void collectChangeBeats(double fromBeat, double toBeat, bool inclusiveStart,
                            std::vector<double>& outBeats) const;
    double beatAfterSeconds(double startBeat, double seconds, float fallbackBpm) const;
    void runScheduler(const ofxOceanodeTransportState& transport, bool loopWrappedThisFrame);
    void sendScheduleCorrections(uint64_t nowUs);

    struct ScheduledPathState {
        bool initialized = false;
        // Value the backend is playing right now (the last event whose due
        // time has passed).
        std::string valueInEffect;
        // Value of the last event handed to the backend, with its due time.
        std::string lastScheduledValue;
        uint64_t lastScheduledDueUs = 0;
        std::deque<std::pair<uint64_t, std::string>> pending;
    };

    ofxOceanodeContainer* container = nullptr;
    std::vector<ofxOceanodeTimelineTrack> tracks;
    std::vector<ofxOceanodeTimelineClipGroup> clipGroups;
    uint64_t nextTrackNumber = 1;
    uint64_t nextBindingNumber = 1;
    uint64_t nextClipNumber = 1;
    uint64_t nextLaneNumber = 1;
    uint64_t nextGroupNumber = 1;
    std::string pendingTrackRenameId;
    bool pendingTrackRenameIsNew = false;
    std::map<std::string, std::vector<std::pair<ofxOceanodeTimelineAutomationMode, std::string>>> activeAutomationValues;
    bool bpmAutomationEnabled = false;
    bool bpmLaneCollapsed = true;
    float bpmMinimum = 20.0f;
    float bpmMaximum = 300.0f;
    std::vector<ofxOceanodeTimelineCurvePoint> bpmAutomationPoints;
    std::vector<ofxOceanodeTimelineCurveTension> bpmCurveTensions;
    std::string bpmInterpolation = "Linear";
    bool loopEnabled = false;
    double loopStartBeat = 0.0;
    double loopEndBeat = 4.0;
    bool loopWrappedThisFrame = false;
    // Loop wrapping is an edge-triggered transport operation. Remembering
    // the previous evaluated beat prevents a wrapped transport from being
    // wrapped again (or from being interpreted as an ordinary forward jump)
    // while the audio provider is being resynchronised.
    bool hasEvaluatedTransportBeat = false;
    double lastEvaluatedTransportBeat = 0.0;
    int timeSignatureNumerator = 4;
    int timeSignatureDenominator = 4;
    bool schedulingEnabled = true;
    double schedulingLookaheadMs = 120.0;
    bool hasScheduleCursor = false;
    double scheduleCursorBeat = 0.0;
    uint64_t lastSeenTransportGeneration = 0;
    bool hasSeenTransportGeneration = false;
    std::map<std::string, ScheduledPathState> scheduledPaths;
    // Paths whose backend is driven by the scheduler: their ordinary
    // frame-rate send is suppressed (the value still reaches the parameter,
    // the GUI and the node graph as usual).
    std::set<std::string> scheduledBackendPaths;
    // Not owned -- see setWaveAudioProvider above.
    ofxOceanodeTimelineWaveAudioProvider* waveAudioProvider = nullptr;
};

#endif
