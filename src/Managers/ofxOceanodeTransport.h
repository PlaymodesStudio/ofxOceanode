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
    bool isPlaying = true;
    float bpm = 120.0f;
    double beatPosition = 0.0;
    uint64_t steadyTimeUs = 0;
    uint64_t generation = 0;
    TransportDriverMode driverMode = TransportDriverMode::RealTime;
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
    void notifyReset();
    void latchFrameState();

    ofxOceanodeTransportState getState() const;
    ofxOceanodeFrameTransportState getFrameState() const;
    double getTimeInSeconds() const;

private:
    static uint64_t getSteadyNowUs();
    void advanceStateToNowLocked(uint64_t nowUs);

    mutable std::mutex mutex;
    ofxOceanodeTransportState state;
    ofxOceanodeFrameTransportState frameState;
};

#endif
