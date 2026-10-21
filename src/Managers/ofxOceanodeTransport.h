#ifndef ofxOceanodeTransport_h
#define ofxOceanodeTransport_h

#define OFX_OCEANODE_HAS_GLOBAL_TRANSPORT 1

#include <cstdint>
#include <mutex>

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
    bool loopActive() const { return loopEnabled && loopEndBeat > loopStartBeat + 1e-9; }
};

struct ofxOceanodeFrameTransportState {
    ofxOceanodeTransportState previous;
    ofxOceanodeTransportState current;
};

class ofxOceanodeTransport {
public:
    ofxOceanodeTransport();

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
    void notifyReset();
    void latchFrameState();

    // ---- External clock (e.g. MIDI clock) ----
    // A source claims the transport; while claimed, ofxOceanodeTime runs it in
    // TransportDriverMode::External and the user-facing controls above (play,
    // stop, seek, BPM) are ignored so they cannot fight the master clock.
    // The source reports "beat B at steady time T, tempo, playing"; between
    // reports the position is extrapolated at that tempo, never more than
    // maxLeadBeats past B, and never backwards unless the report is a jump.
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

private:
    static uint64_t getSteadyNowUs();
    void advanceStateToNowLocked(uint64_t nowUs);
    // Moves the position forward, wrapping at the loop end when crossing it.
    void advanceBeatsLocked(double deltaBeats, double deltaSeconds);
    void setPositionLocked(double beat); // a discontinuity: seconds follow the beat

    mutable std::mutex mutex;
    ofxOceanodeTransportState state;
    ofxOceanodeFrameTransportState frameState;

    const void* externalOwner = nullptr;
    double externalAnchorBeat = 0.0;
    uint64_t externalAnchorUs = 0;
    double externalMaxLeadBeats = 0.0;
    bool externalPlaying = false;
};

#endif
