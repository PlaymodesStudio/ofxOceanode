#ifndef ofxOceanodeTransport_h
#define ofxOceanodeTransport_h

#define OFX_OCEANODE_HAS_GLOBAL_TRANSPORT 1
// Derived transports, timestamped actions and selectable time sources.
#define OFX_OCEANODE_HAS_DERIVED_TRANSPORTS 1

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

enum class TransportDriverMode {
    RealTime,
    FrameStep,
    External
};

struct ofxOceanodeTransportState {
    bool isPlaying = false;
    float bpm = 120.0f;
    double beatPosition = 0.0;
    uint64_t steadyTimeUs = 0;
    uint64_t generation = 0;
    TransportDriverMode driverMode = TransportDriverMode::RealTime;
    // Seconds of play: follows the beat on seeks, stops, jumps and loop wraps
    // (beat / tempo) and accumulates real elapsed time in between, so a tempo
    // change never makes it jump or run backwards.
    double seconds = 0.0;
    // Loop: the transport wraps at loopEndBeat (keeping the overshoot, so the
    // loop never drifts). A wrap bumps loopCount, not generation: it is
    // continuous playback, not a seek.
    bool loopEnabled = false;
    double loopStartBeat = 0.0;
    double loopEndBeat = 0.0;
    uint64_t loopCount = 0;
    // The shared clock, in seconds: it runs whenever the clock runs (real
    // time, each frame step, or while an external clock plays), whether or
    // not this transport plays. Every transport of one root reports the
    // root's clock, so clock instants compare across them.
    double clockSeconds = 0.0;
    // Meter of the timeline this transport plays (set by its owner).
    double beatsPerBar = 4.0;
    bool loopActive() const { return loopEnabled && loopEndBeat > loopStartBeat + 1e-9; }
};

struct ofxOceanodeFrameTransportState {
    ofxOceanodeTransportState previous;
    ofxOceanodeTransportState current;
};

// One root transport owns the clock. Derived transports (one per
// independently playing timeline) follow that clock with their own position,
// play state, tempo, loop and generation: their beat is always computed from
// the clock instant they were anchored at, never accumulated frame by frame,
// so transports at equal or integer-ratio tempos stay phase-locked.
class ofxOceanodeTransport : public std::enable_shared_from_this<ofxOceanodeTransport> {
public:
    ofxOceanodeTransport();
    // A transport following `root`'s clock. Use makeDerived, which also
    // registers it with the root so it is advanced and latched with it.
    explicit ofxOceanodeTransport(std::shared_ptr<ofxOceanodeTransport> root);
    static std::shared_ptr<ofxOceanodeTransport> makeDerived(const std::shared_ptr<ofxOceanodeTransport>& root);

    void setBpm(float bpm);
    void setIsPlaying(bool isPlaying);
    void togglePlay();
    void play();
    void pause();
    void stop();

    void setDriverMode(TransportDriverMode mode);
    void syncRealTime();
    void advanceFrameStep(double deltaSeconds);

    void seekToBeat(double beatPosition);
    // Loop range the transport wraps in (ignored while an external clock drives it).
    void setLoop(bool enabled, double startBeat, double endBeat);
    void setBeatsPerBar(double beatsPerBar);
    void notifyReset();
    void latchFrameState();

    // ---- Timestamped actions ----
    // Take effect exactly when the clock reaches clockSeconds (an instant
    // already passed applies at the start of the next advance). A negative
    // beat keeps the current position. Ignored by a root an external clock
    // drives, like the immediate controls.
    void playAt(double clockSeconds, double beat = -1.0);
    void stopAt(double clockSeconds, bool rewind = false);
    void seekAt(double clockSeconds, double beat);
    void cancelPendingActions();
    // The earliest pending action (false when none).
    bool nextPendingAction(double& clockSeconds, bool& startsPlaying) const;
    // Clock time at a steady-clock instant and back (linear in real time).
    double clockSecondsAt(uint64_t steadyUs) const;
    uint64_t steadyUsAtClock(double clockSeconds) const;
    // The first instant on this transport's beat grid (gridBeats > 0) at
    // least minimumLeadSeconds from now, in clock seconds, at the current
    // tempo. While stopped: now plus the lead.
    double nextGridClockSeconds(double gridBeats, double minimumLeadSeconds = 0.0) const;

    // ---- External clock (e.g. MIDI clock) ----
    // A source claims the transport; while claimed, ofxOceanodeTime runs it in
    // TransportDriverMode::External and the user-facing controls above (play,
    // stop, seek, BPM) are ignored so they cannot fight the master clock.
    // The source reports "beat B at steady time T, tempo, playing"; between
    // reports the position is extrapolated at that tempo, never more than
    // maxLeadBeats past B, and never backwards unless the report is a jump.
    // Only a root takes an external clock; derived transports forward these.
    struct ExternalClockUpdate {
        double beat = 0.0;           // position at steadyTimeUs
        uint64_t steadyTimeUs = 0;   // same clock as getSteadyNowUs()
        float bpm = 0.0f;            // <= 0 keeps the current tempo
        bool playing = false;
        bool jump = false;           // discontinuity (start, song position): bumps generation
        double maxLeadBeats = 0.0;   // how far extrapolation may run past `beat`
    };
    void claimExternalClock(const void* owner);
    void releaseExternalClock(const void* owner);
    bool hasExternalClock() const;
    const void* getExternalClockOwner() const;
    void updateExternalClock(const void* owner, const ExternalClockUpdate& update);
    static uint64_t steadyNowUs() { return getSteadyNowUs(); }

    ofxOceanodeTransportState getState() const;
    ofxOceanodeFrameTransportState getFrameState() const;
    double getTimeInSeconds() const;

    // ---- Derived transports and time sources ----
    bool isDerived() const { return root != nullptr; }
    // The transport owning the clock (this one for a root).
    ofxOceanodeTransport* getRoot() { return root != nullptr ? root.get() : this; }
    const ofxOceanodeTransport* getRoot() const { return root != nullptr ? root.get() : this; }
    // A derived transport that follows the root's tempo and plays whenever
    // the clock runs: the "Clock" time source (created on first use; root
    // only, so a root created with make_shared).
    std::shared_ptr<ofxOceanodeTransport> getClockTransport();

    // Transports a node can choose to follow ("time sources"), kept by the
    // root. The core knows nothing about what owns them (the timeline addon
    // registers its players). id is stable across sessions; group is the
    // canvas path shown as a heading; a non-selectable entry is listed
    // greyed out with `note` as the reason. Registering an existing id
    // replaces it.
    struct TimeSourceInfo {
        std::string id;
        std::string label;
        std::string group;
        bool selectable = true;
        std::string note;
    };
    void registerTimeSource(const TimeSourceInfo& info, std::shared_ptr<ofxOceanodeTransport> transport);
    void unregisterTimeSource(const std::string& id);
    std::vector<TimeSourceInfo> getTimeSources() const;
    // nullptr when unknown, expired or not selectable.
    std::shared_ptr<ofxOceanodeTransport> findTimeSource(const std::string& id) const;
    // Bumped whenever the list of time sources changes.
    uint64_t getTimeSourcesRevision() const;

private:
    static uint64_t getSteadyNowUs();
    void advanceStateToNowLocked(uint64_t nowUs);
    // Moves the position forward, wrapping at the loop end when crossing it.
    void advanceBeatsLocked(double deltaBeats, double deltaSeconds);
    void setPositionLocked(double beat); // a discontinuity: seconds follow the beat

    // ---- Timestamped actions (root and derived) ----
    enum class ActionType { Play, Stop, Seek };
    struct PendingAction {
        double clockSeconds = 0.0;
        ActionType type = ActionType::Play;
        double beat = -1.0;
        bool rewind = false;
    };
    void addPendingLocked(const PendingAction& action);
    void applyActionLocked(const PendingAction& action);
    // Root: moves the clock by deltaSeconds and, when advancePosition, the
    // position too, applying the pending actions due in that span at their
    // instant.
    void advanceRootClockLocked(double deltaSeconds, bool advancePosition);

    // ---- Derived ----
    // Follows the root's state for one advance (called by the root, outside its lock).
    void followRoot(const ofxOceanodeTransportState& rootState);
    void advanceDerivedLocked(double toClock);
    void advanceDerivedLinearLocked(double toClock);
    void reanchorLocked();
    void advanceChildren();
    void latchChildren();
    std::vector<std::shared_ptr<ofxOceanodeTransport>> liveChildren();

    mutable std::mutex mutex;
    ofxOceanodeTransportState state;
    ofxOceanodeFrameTransportState frameState;
    std::vector<PendingAction> pending; // sorted by clockSeconds

    const void* externalOwner = nullptr;
    double externalAnchorBeat = 0.0;
    uint64_t externalAnchorUs = 0;
    double externalMaxLeadBeats = 0.0;
    bool externalPlaying = false;

    // Derived: the root it follows, and the clock instant its beat is measured from.
    std::shared_ptr<ofxOceanodeTransport> root;
    double anchorClock = 0.0;
    double anchorBeat = 0.0;
    double lastClock = 0.0;
    bool followRootTempo = false; // the Clock time source

    // Root: derived transports to advance, and the selectable time sources.
    mutable std::mutex childrenMutex;
    std::vector<std::weak_ptr<ofxOceanodeTransport>> children;
    struct TimeSourceEntry {
        TimeSourceInfo info;
        std::weak_ptr<ofxOceanodeTransport> transport;
    };
    std::vector<TimeSourceEntry> timeSources;
    uint64_t timeSourcesRevision = 0;
    std::shared_ptr<ofxOceanodeTransport> clockTransport;
};

#endif
