//
//  ofxOceanodeMidiClockCore.h
//  ofxOceanode
//
//  MIDI-agnostic MIDI clock follower: interprets clock ticks (24 per quarter
//  note), Start / Continue / Stop and Song Position Pointer, estimates tempo from
//  tick timing, and keeps the state needed to drive the transport. The MIDI port
//  handling lives in ofxOceanodeMidiClock (OFXOCEANODE_USE_MIDI builds); this
//  part has no dependencies so it can be tested on its own.
//
//  Thread-safety: every method locks an internal mutex, so events can arrive on
//  the MIDI thread while the main thread reads the status.
//

#ifndef ofxOceanodeMidiClockCore_h
#define ofxOceanodeMidiClockCore_h

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>

struct ofxOceanodeMidiClockStatus {
    bool playing = false;
    bool receiving = false;       // a tick arrived recently
    int64_t tickPosition = 0;     // song position of the NEXT tick, in 24 PPQ ticks
    double beat = 0.0;            // beat of the last tick (stepped, 1/24 resolution)
    double smoothBeat = 0.0;      // beat extrapolated to "now" (continuous)
    float bpm = 120.0f;
    uint32_t startCount = 0, stopCount = 0, continueCount = 0, jumpCount = 0, tickCount = 0;
};

// What the core asks the transport to do (maps onto ofxOceanodeTransport::ExternalClockUpdate).
struct ofxOceanodeMidiClockEvent {
    double beat = 0.0;
    uint64_t steadyTimeUs = 0;
    float bpm = 0.0f;
    bool playing = false;
    bool jump = false;
    double maxLeadBeats = 0.0;
};

class ofxOceanodeMidiClockCore {
public:
    static constexpr int kTicksPerBeat = 24;
    static constexpr uint64_t kReceivingTimeoutUs = 500000;   // "no clock" after 0.5 s
    static constexpr uint64_t kClockOnlyStopTimeoutUs = 300000;
    static constexpr uint64_t kReaperStartWindowUs = 250000; // SPP this recent before Start = start there

    // Called for every transport-relevant change (from the thread that fed the event).
    std::function<void(const ofxOceanodeMidiClockEvent&)> onEvent;

    // ---- settings ----
    void setReaperMode(bool on) { std::lock_guard<std::mutex> l(m); reaperMode = on; }
    bool getReaperMode() const { std::lock_guard<std::mutex> l(m); return reaperMode; }
    // For devices that send ticks but no Start/Stop: ticks mean "playing".
    void setClockOnly(bool on) { std::lock_guard<std::mutex> l(m); clockOnly = on; }
    bool getClockOnly() const { std::lock_guard<std::mutex> l(m); return clockOnly; }
    // Positive = run ahead of the incoming clock (compensates output latency).
    void setOffsetMs(float ms) { std::lock_guard<std::mutex> l(m); offsetUs = static_cast<int64_t>(ms * 1000.0f); }
    float getOffsetMs() const { std::lock_guard<std::mutex> l(m); return offsetUs / 1000.0f; }
    // Ticks averaged for the tempo estimate (24 = one beat).
    void setTempoWindow(int ticks) { std::lock_guard<std::mutex> l(m); tempoWindow = std::max(2, std::min(96, ticks)); }
    int getTempoWindow() const { std::lock_guard<std::mutex> l(m); return tempoWindow; }
    // What happens when ticks stop arriving while playing (cable pulled, master
    // crashed): stop the transport (default), or hold where it is.
    void setStopOnClockLoss(bool on) { std::lock_guard<std::mutex> l(m); stopOnClockLoss = on; }
    bool getStopOnClockLoss() const { std::lock_guard<std::mutex> l(m); return stopOnClockLoss; }

    void reset() {
        std::lock_guard<std::mutex> l(m);
        playing = false; position = 0; lastTickUs = 0; lastTickBeat = 0.0;
        tickTimes.clear(); bpm = 120.0; lastSppUs = 0; haveTick = false;
        ticksSinceReset = 0; lastActivityUs = 0;
    }

    // ---- MIDI events (usually on the MIDI thread) ----
    void tick(uint64_t nowUs) {
        std::lock_guard<std::mutex> l(m);
        status_.tickCount++;
        ticksSinceReset++;
        // A pause in the clock (longer than three expected ticks) must not be
        // averaged into the tempo: restart the estimate from this tick.
        if(!tickTimes.empty() && nowUs > tickTimes.back()) {
            const double expectedUs = 60000000.0 / (std::max(20.0, bpm) * kTicksPerBeat);
            if(static_cast<double>(nowUs - tickTimes.back()) > 3.0 * expectedUs) {
                tickTimes.clear();
                ticksSinceReset = 1;
            }
        }
        tickTimes.push_back(nowUs);
        while(tickTimes.size() > 128) tickTimes.pop_front();
        estimateTempoLocked();
        haveTick = true;
        lastTickUs = nowUs;
        lastActivityUs = nowUs;
        if(clockOnly && !playing) {
            playing = true;
            status_.startCount++;
        }
        if(!playing) return; // ticks while stopped only keep the tempo estimate alive
        // The tick marks `position`; the next one will be one tick later.
        lastTickBeat = static_cast<double>(position) / kTicksPerBeat;
        position++;
        emitLocked(lastTickBeat, nowUs, false, 1.0 / kTicksPerBeat);
    }

    void start(uint64_t nowUs) {
        std::lock_guard<std::mutex> l(m);
        // Standard: Start = from the top. REAPER mode: a Song Position received just
        // before Start is honoured (start from there).
        const bool keepSpp = reaperMode && lastSppUs != 0 && nowUs >= lastSppUs &&
                             nowUs - lastSppUs <= kReaperStartWindowUs;
        if(!keepSpp) position = 0;
        playing = true;
        lastActivityUs = nowUs; // ticks may start a moment after Start
        status_.startCount++;
        status_.jumpCount++;
        lastTickBeat = static_cast<double>(position) / kTicksPerBeat;
        emitLocked(lastTickBeat, nowUs, true, 0.0); // hold until the first tick
    }

    void resume(uint64_t nowUs) { // MIDI Continue
        std::lock_guard<std::mutex> l(m);
        playing = true;
        lastActivityUs = nowUs;
        status_.continueCount++;
        status_.jumpCount++;
        lastTickBeat = static_cast<double>(position) / kTicksPerBeat;
        emitLocked(lastTickBeat, nowUs, true, 0.0);
    }

    void stop(uint64_t nowUs) {
        std::lock_guard<std::mutex> l(m);
        if(!playing) return;
        playing = false;
        status_.stopCount++;
        emitLocked(lastTickBeat, nowUs, false, 0.0);
    }

    // Song Position Pointer: `sixteenths` = MIDI beats (16th notes) from the song start.
    void songPosition(int sixteenths, uint64_t nowUs) {
        std::lock_guard<std::mutex> l(m);
        position = static_cast<int64_t>(std::max(0, sixteenths)) * (kTicksPerBeat / 4);
        lastSppUs = nowUs;
        status_.jumpCount++;
        lastTickBeat = static_cast<double>(position) / kTicksPerBeat;
        // Jumps move our playhead, playing or stopped.
        emitLocked(lastTickBeat, nowUs, true, 0.0);
    }

    // Main thread, once per frame: stop when ticks stop arriving (clock-only
    // devices always; otherwise per the clock-loss setting).
    void poll(uint64_t nowUs) {
        std::lock_guard<std::mutex> l(m);
        if(!playing || lastActivityUs == 0 || nowUs <= lastActivityUs) return;
        const uint64_t silentUs = nowUs - lastActivityUs;
        const bool lost = clockOnly ? (haveTick && silentUs > kClockOnlyStopTimeoutUs)
                                    : (stopOnClockLoss && silentUs > kReceivingTimeoutUs);
        if(lost) {
            playing = false;
            status_.stopCount++;
            emitLocked(lastTickBeat, nowUs, false, 0.0);
        }
    }

    ofxOceanodeMidiClockStatus status(uint64_t nowUs) const {
        std::lock_guard<std::mutex> l(m);
        ofxOceanodeMidiClockStatus s = status_;
        s.playing = playing;
        s.receiving = haveTick && nowUs >= lastTickUs && nowUs - lastTickUs < kReceivingTimeoutUs;
        s.tickPosition = position;
        s.beat = lastTickBeat;
        s.bpm = static_cast<float>(bpm);
        s.smoothBeat = lastTickBeat;
        if(playing && haveTick && nowUs > lastTickUs) {
            const double elapsed = static_cast<double>(nowUs - lastTickUs) / 1000000.0;
            s.smoothBeat = lastTickBeat + std::min(elapsed * bpm / 60.0, 1.0 / kTicksPerBeat);
        }
        return s;
    }

    // Standard SPP message: [0xF2, LSB, MSB] (7-bit data bytes). Returns sixteenths.
    static int decodeSongPosition(uint8_t lsb, uint8_t msb) {
        return (static_cast<int>(msb & 0x7F) << 7) | static_cast<int>(lsb & 0x7F);
    }

private:
    void estimateTempoLocked() {
        const size_t n = tickTimes.size();
        const size_t use = std::min(static_cast<size_t>(tempoWindow), n);
        if(use < 2) return;
        const uint64_t newest = tickTimes.back();
        const uint64_t oldest = tickTimes[n - use];
        if(newest <= oldest) return;
        const double usPerTick = static_cast<double>(newest - oldest) / static_cast<double>(use - 1);
        const double instant = 60000000.0 / (usPerTick * kTicksPerBeat);
        if(!std::isfinite(instant) || instant < 20.0 || instant > 400.0) return;
        // A large change (tempo jump, or the first estimate) is taken at once;
        // small wobble (USB jitter) is smoothed.
        if(std::abs(instant - bpm) > 8.0 || ticksSinceReset < 4) bpm = instant;
        else bpm = bpm * 0.8 + instant * 0.2;
    }

    void emitLocked(double beat, uint64_t atUs, bool jump, double maxLead) {
        if(!onEvent) return;
        ofxOceanodeMidiClockEvent e;
        // Offset: shift the reported beat (ahead when positive, behind when
        // negative) and keep the anchor at the real arrival time. Shifting the
        // anchor time instead put it in the future for negative offsets larger
        // than a tick, and the transport never extrapolated past it (frozen).
        const double offsetBeats = (static_cast<double>(offsetUs) / 1000000.0) * bpm / 60.0;
        e.beat = beat + offsetBeats;
        e.steadyTimeUs = atUs;
        e.bpm = static_cast<float>(bpm);
        e.playing = playing;
        e.jump = jump;
        e.maxLeadBeats = maxLead;
        onEvent(e);
    }

    mutable std::mutex m;
    bool reaperMode = true;
    bool clockOnly = false;
    int64_t offsetUs = 0;
    int tempoWindow = 24;
    bool stopOnClockLoss = true;
    uint32_t ticksSinceReset = 0;
    uint64_t lastActivityUs = 0; // last tick, Start or Continue

    bool playing = false;
    int64_t position = 0;      // ticks; position of the next tick
    double lastTickBeat = 0.0;
    uint64_t lastTickUs = 0;
    bool haveTick = false;
    uint64_t lastSppUs = 0;
    std::deque<uint64_t> tickTimes;
    double bpm = 120.0;
    ofxOceanodeMidiClockStatus status_;
};

#endif /* ofxOceanodeMidiClockCore_h */
