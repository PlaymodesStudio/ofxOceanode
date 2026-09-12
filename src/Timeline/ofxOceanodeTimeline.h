#ifndef ofxOceanodeTimeline_h
#define ofxOceanodeTimeline_h

#include "ofMain.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class ofxOceanodeAbstractParameter;
class ofxOceanodeContainer;

enum class ofxOceanodeTimelineAutomationMode {
    Replace
};

enum class ofxOceanodeTimelineLaneType {
    Step,
    PianoRoll,
    Curve
};

struct ofxOceanodeTimelineStep {
    double startBeat = 0.0;
    // A non-positive duration means "hold until the next step". This keeps
    // the old stepValueTrack behaviour while allowing explicit variable sizes.
    double durationBeats = 0.0;
    std::string value;
    // Probability that this step fires. A value of 1 is an ordinary step and
    // 0 is a muted step. Keeping it on the step makes the editor genuinely
    // probabilistic instead of treating the whole lane as a single gate.
    float probability = 1.0f;
};

class ofxOceanodeTimelineStepLane {
public:
    double lengthBeats = 4.0;
    bool loop = true;
    std::string fallbackValue;
    std::vector<ofxOceanodeTimelineStep> steps;

    void sortSteps();
    void setStep(double startBeat, const std::string& value, double durationBeats = 0.0,
                float probability = 1.0f);
    bool removeStep(double startBeat, double epsilon = 1e-6);
    void clear();

    // Evaluates in local lane space. Returns false when no step or fallback is
    // active, allowing the manager to apply its gap policy later.
    bool evaluate(double localBeat, std::string& value, bool useProbability = true) const;

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

struct ofxOceanodeTimelinePianoNote {
    double startBeat = 0.0;
    double durationBeats = 0.25;
    int pitch = 60;
    float velocity = 1.0f;
    float probability = 1.0f;
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
    std::string beatDivision = "16th";
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
};

struct ofxOceanodeTimelineClip {
    std::string id;
    std::string name;
    double startBeat = 0.0;
    double durationBeats = 4.0;
    // The source length is kept separately so a clip can be stretched without
    // destroying its original content length.
    double contentDurationBeats = 4.0;
    bool repeatContent = true;
    std::vector<ofxOceanodeTimelineLane> lanes;
};

struct ofxOceanodeTimelineTrack {
    std::string id;
    std::string name;
    ofColor color = ofColor(65, 165, 245, 255);
    bool collapsed = false;
    std::vector<ofxOceanodeTimelineParameterBinding> bindings;
    std::vector<ofxOceanodeTimelineClip> clips;
};

class ofxOceanodeTimelineManager {
public:
    explicit ofxOceanodeTimelineManager(ofxOceanodeContainer* container = nullptr);

    void setContainer(ofxOceanodeContainer* container);
    void update();
    void clear();

    std::string createTrack(const std::string& requestedName = "Timeline Track");
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
    ofxOceanodeTimelineParameterBinding* getBinding(const std::string& trackId, const std::string& bindingId);
    const ofxOceanodeTimelineParameterBinding* getBinding(const std::string& trackId, const std::string& bindingId) const;

    std::string createClip(const std::string& trackId,
                           const std::string& requestedName = "Clip",
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
    bool setClipStep(const std::string& trackId, const std::string& clipId, const std::string& laneId,
                     double startBeat, const std::string& value, double durationBeats = 0.0);
    bool removeClipStep(const std::string& trackId, const std::string& clipId, const std::string& laneId,
                        double startBeat);

    bool setStep(const std::string& trackId, const std::string& bindingId,
                 double startBeat, const std::string& value, double durationBeats = 0.0);
    bool removeStep(const std::string& trackId, const std::string& bindingId, double startBeat);
    bool clearSteps(const std::string& trackId, const std::string& bindingId);

    bool isBpmAutomationEnabled() const { return bpmAutomationEnabled; }
    void setBpmAutomationEnabled(bool enabled);
    bool isBpmLaneCollapsed() const { return bpmLaneCollapsed; }
    void setBpmLaneCollapsed(bool collapsed) { bpmLaneCollapsed = collapsed; }
    float getBpmMinimum() const { return bpmMinimum; }
    float getBpmMaximum() const { return bpmMaximum; }
    void setBpmRange(float minimum, float maximum);
    std::vector<ofxOceanodeTimelineCurvePoint>& getBpmAutomationPoints() { return bpmAutomationPoints; }
    const std::vector<ofxOceanodeTimelineCurvePoint>& getBpmAutomationPoints() const { return bpmAutomationPoints; }
    float evaluateBpm(double beat, float fallbackBpm) const;
    double beatToSeconds(double beat, float fallbackBpm) const;

    bool isLoopEnabled() const { return loopEnabled; }
    void setLoopEnabled(bool enabled) { loopEnabled = enabled; }
    double getLoopStartBeat() const { return loopStartBeat; }
    double getLoopEndBeat() const { return loopEndBeat; }
    void setLoopRange(double startBeat, double endBeat);

    ofJson toJson() const;
    void fromJson(const ofJson& json);
    bool savePreset(const std::string& presetFolderPath) const;
    bool loadPreset(const std::string& presetFolderPath);

private:
    static std::string modeToString(ofxOceanodeTimelineAutomationMode mode);
    static ofxOceanodeTimelineAutomationMode modeFromString(const std::string& mode);
    static std::string laneTypeToString(ofxOceanodeTimelineLaneType laneType);
    static ofxOceanodeTimelineLaneType laneTypeFromString(const std::string& laneType);
    static std::string makeId(const char* prefix, uint64_t number);
    std::string makeUniqueTrackName(const std::string& requestedName) const;
    std::string makeUniqueClipName(const ofxOceanodeTimelineTrack& track, const std::string& requestedName) const;
    std::string makeUniqueBindingId() const;
    std::string makeUniqueTrackId() const;
    std::string makeUniqueClipId() const;
    std::string makeUniqueLaneId() const;
    void clearTimelineFlag(const ofxOceanodeTimelineTrack& track);

    ofxOceanodeContainer* container = nullptr;
    std::vector<ofxOceanodeTimelineTrack> tracks;
    uint64_t nextTrackNumber = 1;
    uint64_t nextBindingNumber = 1;
    uint64_t nextClipNumber = 1;
    uint64_t nextLaneNumber = 1;
    std::string pendingTrackRenameId;
    bool pendingTrackRenameIsNew = false;
    bool bpmAutomationEnabled = false;
    bool bpmLaneCollapsed = true;
    float bpmMinimum = 20.0f;
    float bpmMaximum = 300.0f;
    std::vector<ofxOceanodeTimelineCurvePoint> bpmAutomationPoints;
    bool loopEnabled = false;
    double loopStartBeat = 0.0;
    double loopEndBeat = 4.0;
};

#endif
