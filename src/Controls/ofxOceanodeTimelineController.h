#ifndef ofxOceanodeTimelineController_h
#define ofxOceanodeTimelineController_h

#include "ofxOceanodeBaseController.h"
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class ofxOceanodeContainer;
class ofxOceanodeTimelineManager;
struct ofxOceanodeTimelineTrack;
struct ofxOceanodeTimelineParameterBinding;

class ofxOceanodeTimelineController : public ofxOceanodeBaseController {
public:
    explicit ofxOceanodeTimelineController(std::shared_ptr<ofxOceanodeContainer> container);

    void draw() override;

private:
    void drawRuler(ofxOceanodeTimelineManager& timeline, float labelWidth, float timelineWidth,
                   double endBeat, double beatPosition, float bpm);
    void drawBpmLane(ofxOceanodeTimelineManager& timeline, float contentWidth,
                     double endBeat, double beatPosition, float fallbackBpm);
    void drawLaneEditor(ofxOceanodeTimelineManager& timeline, const ofxOceanodeTimelineTrack& track,
                        float contentWidth, double endBeat, double beatPosition);
    void drawBlendModeOptions(ofxOceanodeTimelineManager& timeline, const std::string& trackId,
                             const ofxOceanodeTimelineParameterBinding& binding);
    void drawRenamePopup(ofxOceanodeTimelineManager& timeline);
    void drawClipPopup(ofxOceanodeTimelineManager& timeline);
    double getContentEndBeat(const ofxOceanodeTimelineManager& timeline) const;
    double snapBeat(double beat) const;
    double editIncrement() const;
    double displayGridBeats() const;
    float beatToPixels(const ofxOceanodeTimelineManager& timeline, double beat, float fallbackBpm) const;
    double pixelsToBeat(const ofxOceanodeTimelineManager& timeline, float pixels,
                        float fallbackBpm, double endBeatHint) const;

    std::shared_ptr<ofxOceanodeContainer> container;
    float pixelsPerSecond = 140.0f;
    int visibleBars = 8;
    double rulerSnapBeats = 0.25;
    // Horizontal scroll is fully manual (not native ImGui ScrollX) so the
    // left-hand label/properties column can stay pinned in place while only
    // the beat-based content to its right slides underneath it.
    float timelineScrollX = 0.0f;

    std::string pendingTrackId;
    std::string pendingClipId;
    std::string pendingLaneId;
    std::string editorTrackId;
    std::string editorClipId;
    std::string editorLaneId;
    // Independent expand/collapse state for each lane in a multi-lane
    // clip's editor (keyed by lane id, globally unique). Absence from
    // this set means expanded -- most lanes are expanded most of the
    // time, so "collapsed" is the state worth tracking explicitly.
    std::set<std::string> collapsedLaneIds;
    // Per-lane editor height override (keyed by lane id), set by dragging
    // the resize handle at the bottom of a lane's editor row. Absence means
    // "use the type's default height" -- UI-only, not persisted with the
    // preset.
    std::unordered_map<std::string, float> laneEditorHeights;
    // Piano-roll keyboard-strip "audition" state: while a key is held down,
    // the lane's Gate/Pitch bindings get a live override (see
    // ofxOceanodeTimelineManager::setLiveOverride) so the bound parameters
    // sound immediately, independent of the playhead or any clip data.
    bool pianoKeyboardPreviewActive = false;
    std::string pianoKeyboardPreviewTrackId;
    std::string pianoKeyboardPreviewGateBindingId;
    std::string pianoKeyboardPreviewPitchBindingId;
    int pianoKeyboardPreviewPitch = -1;
    char pendingTrackName[128] = {};
    int pendingNewTrackLaneType = 0;
    char pendingClipName[128] = {};
    std::string pendingClipBindingId;
    int pendingClipLaneType = 0;
    double pendingStartBeat = 0.0;
    double pendingDurationBeats = 1.0;

    enum class ClipDragMode { None, Move, Resize, Stretch, Repeat };
    ClipDragMode clipDragMode = ClipDragMode::None;
    std::string draggingTrackId;
    std::string draggingClipId;
    double dragOffsetBeats = 0.0;
    double dragInitialContentDuration = 4.0;
    double dragInitialContentStretch = 1.0;
    float dragTimelineOriginX = 0.0f;
    bool requestRenamePopup = false;
    bool pendingNewTrackDialog = false;
    bool requestClipPopup = false;
    // Track removal is deferred until the track iteration has finished;
    // erasing from timeline.getTracks() inside its context menu would
    // invalidate the current track and every clip/lane reference below it.
    bool requestTrackDeletion = false;
    std::string trackDeletionId;
    bool requestClipDeletion = false;
    std::string clipDeletionTrackId;
    std::string clipDeletionClipId;
    // "Remove from Timeline" (on a binding's row, or on a collapsed track's
    // header menu) unbinds a parameter entirely. Deferred for the same
    // reason as clip deletion above: both popups that can request this are
    // opened mid-iteration over track.bindings, and code later in that same
    // iteration still reads the binding being removed.
    bool requestRemoveBinding = false;
    std::string removeBindingTrackId;
    std::string removeBindingId;
    // A clip can hold more than one lane (e.g. a curve and a step pattern
    // combined into one clip); these back the clip editor's "Add lane" /
    // "Remove this lane" controls. The actual mutation is deferred to a
    // fixed point in draw(), like clip deletion above, since createLane/
    // removeLane can reallocate the clip's lane vector and the editor keeps
    // pointers into it for the rest of the frame it was requested on.
    bool requestAddLane = false;
    int pendingAddLaneType = 0;
    bool requestRemoveLane = false;
    std::string pendingRemoveLaneId;
    bool stepEditorOpen = false;
    enum class PianoDragMode { None, Move, Resize, Velocity, Probability };
    PianoDragMode pianoDragMode = PianoDragMode::None;
    int pianoDragNoteIndex = -1;
    double pianoDragBeatOffset = 0.0;
    // Multi-note selection: which notes (by index into the lane's
    // pianoNotes) are selected, plus the state needed to move the whole
    // group together by a common delta instead of snapping each note
    // independently to the mouse.
    std::string pianoSelectionLaneId;
    std::set<int> pianoSelectedNoteIndices;
    struct PianoDragSnapshotEntry { int index; double startBeat; int pitch; };
    std::vector<PianoDragSnapshotEntry> pianoDragSnapshot;
    double pianoDragAnchorStartBeat = 0.0;
    int pianoDragAnchorPitch = 0;
    bool pianoMarqueeActive = false;
    float pianoMarqueeStartX = 0.0f;
    float pianoMarqueeStartY = 0.0f;
    enum class PianoNumericField { None, Velocity, Probability };
    PianoNumericField pianoNumericField = PianoNumericField::None;
    int pianoNumericNoteIndex = -1;
    float pianoNumericValue = 0.0f;
    int bpmDragPointIndex = -1;
    int bpmValuePointIndex = -1;
    float bpmNumericValue = 120.0f;
    int bpmTensionSegment = -1;
    float bpmTensionDragStartX = 0.0f;
    float bpmTensionDragStartY = 0.0f;
    float bpmTensionStartInflection = 0.5f;
    float bpmTensionStartSteepness = 1.0f;
    int curveDragPointIndex = -1;
    int curveValuePointIndex = -1;
    float curveNumericValue = 0.0f;
    int curveTensionSegment = -1;
    float curveTensionDragStartX = 0.0f;
    float curveTensionDragStartY = 0.0f;
    float curveTensionStartInflection = 0.5f;
    float curveTensionStartSteepness = 1.0f;
    enum class LoopDragMode { None, Scrub, Start, End, Move };
    LoopDragMode loopDragMode = LoopDragMode::None;
    double loopDragAnchorBeat = 0.0;
    double loopDragStartBeat = 0.0;
    double loopDragEndBeat = 4.0;
};

#endif
