#ifndef ofxOceanodeTimelineController_h
#define ofxOceanodeTimelineController_h

#include "ofxOceanodeBaseController.h"
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class ofxOceanodeContainer;
class ofxOceanodeTimelineManager;
struct ofxOceanodeTimelineTrack;
struct ofxOceanodeTimelineClip;
struct ofxOceanodeTimelineParameterBinding;

class ofxOceanodeTimelineController : public ofxOceanodeBaseController {
public:
    explicit ofxOceanodeTimelineController(std::shared_ptr<ofxOceanodeContainer> container);

    void draw() override;
    // Handles a track name/creation request queued by anything outside this
    // controller (currently: a node parameter's right-click "Add to Timeline
    // Track -> New Timeline Track"). Call this every frame from
    // ofxOceanodeControls::draw() UNCONDITIONALLY -- not gated behind this
    // controller's own window being visible/focused the way draw() is.
    // Requesting a new track can happen while the Timeline window is closed
    // or just isn't the active docked tab; if consuming the request were
    // left inside draw() (as it used to be), it would silently wait until
    // the user happened to bring that window/tab forward, and would then
    // pop up anchored to wherever that window/tab's viewport is instead of
    // the main canvas the user was actually looking at.
    void drawPendingTrackPopup();

private:
    void drawRuler(ofxOceanodeTimelineManager& timeline, float labelWidth, float timelineWidth,
                   double endBeat, double beatPosition, float bpm);
    void drawBpmLane(ofxOceanodeTimelineManager& timeline, float contentWidth,
                     double endBeat, double beatPosition, float fallbackBpm);
    void drawLaneEditor(ofxOceanodeTimelineManager& timeline, const ofxOceanodeTimelineTrack& track,
                        float contentWidth, double endBeat, double beatPosition);
    void drawLfoEditor(ofxOceanodeTimelineManager& timeline, const ofxOceanodeTimelineTrack& track,
                       ofxOceanodeTimelineClip& clip, float contentWidth, double endBeat,
                       double beatPosition);
    void drawWaveClipProperties(ofxOceanodeTimelineManager& timeline,
                                const ofxOceanodeTimelineTrack& track,
                                ofxOceanodeTimelineClip& clip);
    void drawWaveTrackEditor(ofxOceanodeTimelineManager& timeline,
                             const ofxOceanodeTimelineTrack& track,
                             float contentWidth, double endBeat,
                             double beatPosition);
    void drawWaveTrackVolumeAutomation(ofxOceanodeTimelineManager& timeline,
                                       ofxOceanodeTimelineTrack& track,
                                       float width, float height,
                                       double endBeat, double beatPosition);
    void drawBlendModeOptions(ofxOceanodeTimelineManager& timeline, const std::string& trackId,
                             const ofxOceanodeTimelineParameterBinding& binding);
    void drawRenamePopup(ofxOceanodeTimelineManager& timeline);
    void drawClipPopup(ofxOceanodeTimelineManager& timeline);
    bool createWaveClipFromFile(ofxOceanodeTimelineManager& timeline,
                                const std::string& trackId,
                                const std::string& filePath,
                                double startBeat,
                                double durationBeats);
    bool replaceWaveClipFromFile(ofxOceanodeTimelineManager& timeline,
                                 const std::string& trackId,
                                 const std::string& clipId,
                                 const std::string& filePath);
    bool splitWaveClipAtPlayhead(ofxOceanodeTimelineManager& timeline,
                                 const std::string& trackId,
                                 const std::string& clipId,
                                 double playheadBeat);
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
    // Derived from the selected snap and current zoom. The selected value is
    // kept separately so zooming back in restores the user's finer grid.
    double effectiveRulerSnapBeats = 0.25;
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
    // The resize grip is handled from its screen rectangle instead of as an
    // overlapping ImGui item. This keeps the editor canvas/child window from
    // stealing the active id after the drag has started.
    std::string resizingLaneId;
    float laneResizeStartMouseY = 0.0f;
    float laneResizeStartHeight = 0.0f;
    std::string waveTrackResizeId;
    float waveTrackResizeStartMouseY = 0.0f;
    float waveTrackResizeStartHeight = 0.0f;
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
    char pendingClipName[128] = {};
    std::string pendingClipBindingId;
    int pendingClipLaneType = 0;
    double pendingStartBeat = 0.0;
    double pendingDurationBeats = 1.0;
    // Beat used by a Wave Track's context-menu "Add new wave clip" action.
    // Header clicks use the playhead; empty-row clicks use the clicked beat.
    double pendingWaveClipBeat = 0.0;
    int waveVolumeDragPointIndex = -1;

    enum class ClipDragMode { None, Move, Resize, Stretch, Repeat };
    ClipDragMode clipDragMode = ClipDragMode::None;
    std::string draggingTrackId;
    std::string draggingClipId;
    double dragOffsetBeats = 0.0;
    double dragInitialContentDuration = 4.0;
    double dragInitialContentStretch = 1.0;
    float dragTimelineOriginX = 0.0f;
    // Ephemeral multi-clip selection built with Shift+click on a clip (see
    // handleClip in the .cpp). This is the input to the "Group" button/menu
    // item and to a plain drag that keeps multiple selected clips moving
    // together even before they're formally grouped -- it is not persisted
    // and is a separate concept from a *group* (ofxOceanodeTimelineClipGroup),
    // which persists once created. Keyed by (trackId, clipId).
    std::set<std::pair<std::string, std::string>> selectedClips;
    // Set once per frame (across every track, not just the one currently
    // being drawn) whenever a clip consumes a mouse click -- lets the
    // "clicked empty timeline space" handler know a click already had a
    // more specific target, without it having to know which track that was.
    bool anyClipInteractionClaimedThisFrame = false;
    // Snapshot of every clip that should move/stretch together with the one
    // being dragged (its persistent group's members, or the current
    // Shift+click selection if the dragged clip is part of one bigger than
    // itself), taken once when the drag starts so the whole set scales from
    // one shared reference each frame instead of drifting from repeatedly
    // re-deriving deltas off values already mutated this frame. Empty means
    // the current drag (if any) is an ordinary single-clip drag.
    struct GroupDragSnapshotEntry {
        std::string trackId, clipId;
        double startBeat, durationBeats, contentDurationBeats, contentStretch;
    };
    std::vector<GroupDragSnapshotEntry> groupDragSnapshot;
    double groupDragAnchorBeat = 0.0;
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
    bool requestWaveSplit = false;
    std::string waveSplitTrackId;
    std::string waveSplitClipId;
    std::vector<std::pair<std::string, std::string>> keyboardClipDeletionRequests;
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
    bool clipEditorOpen = false;
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
    struct PianoValueDragSnapshotEntry { int index; float value; };
    std::vector<PianoValueDragSnapshotEntry> pianoValueDragSnapshot;
    float pianoValueDragAnchorValue = 0.0f;
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
    std::string lfoDragLaneId;
    int lfoDragPointIndex = -1;
    enum class LoopDragMode { None, Scrub, Start, End, Move };
    LoopDragMode loopDragMode = LoopDragMode::None;
    double loopDragAnchorBeat = 0.0;
    double loopDragStartBeat = 0.0;
    double loopDragEndBeat = 4.0;

    // MultiValue / MultiGate region drag (create/move/resize), shared by
    // both lane types since only one lane is ever focused (interactive) at
    // a time -- see the .cpp for why that makes sharing this state safe.
    // Keyed by row index (into the lane's multiValueRows/multiGateRows) +
    // index into that row's region vector.
    enum class RegionDragMode { None, Create, Move, ResizeRight };
    RegionDragMode regionDragMode = RegionDragMode::None;
    int regionDragRow = -1;
    int regionDragIndex = -1;
    double regionDragAnchorBeat = 0.0;
    double regionDragOriginalStart = 0.0;
    double regionDragOriginalDuration = 0.0;
    // MultiValue's double-click "edit value" popup (mirrors the Curve
    // lane's "Curve point value" popup).
    int multiValueEditRow = -1;
    int multiValueEditIndex = -1;
    float multiValueEditNumeric = 0.0f;
};

#endif
