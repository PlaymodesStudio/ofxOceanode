#ifndef ofxOceanodeTimelineController_h
#define ofxOceanodeTimelineController_h

#include "ofxOceanodeBaseController.h"
#include <memory>
#include <string>

class ofxOceanodeContainer;
class ofxOceanodeTimelineManager;
struct ofxOceanodeTimelineTrack;

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

    std::string pendingTrackId;
    std::string pendingClipId;
    std::string pendingLaneId;
    std::string editorTrackId;
    std::string editorClipId;
    std::string editorLaneId;
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
    float dragTimelineOriginX = 0.0f;
    bool requestRenamePopup = false;
    bool pendingNewTrackDialog = false;
    bool requestClipPopup = false;
    bool requestClipDeletion = false;
    std::string clipDeletionTrackId;
    std::string clipDeletionClipId;
    bool stepEditorOpen = false;
    enum class PianoDragMode { None, Move, Resize, Velocity, Probability };
    PianoDragMode pianoDragMode = PianoDragMode::None;
    int pianoDragNoteIndex = -1;
    double pianoDragBeatOffset = 0.0;
    enum class PianoNumericField { None, Velocity, Probability };
    PianoNumericField pianoNumericField = PianoNumericField::None;
    int pianoNumericNoteIndex = -1;
    float pianoNumericValue = 0.0f;
    int bpmDragPointIndex = -1;
    int bpmValuePointIndex = -1;
    float bpmNumericValue = 120.0f;
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
