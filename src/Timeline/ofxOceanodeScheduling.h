#ifndef ofxOceanodeScheduling_h
#define ofxOceanodeScheduling_h

#include <cstdint>
#include <functional>
#include <string>

class ofxOceanodeAbstractParameter;

// ---------------------------------------------------------------------------
// Timestamped (scheduled) parameter events.
//
// The GUI runs at frame rate, but a musical event must not. A backend that can
// execute a value at an exact instant (SuperCollider: an OSC bundle carries an
// NTP timetag and scsynth applies it sample-accurately) registers a handler for
// the parameters it owns. The timeline scheduler looks a short window ahead,
// converts each upcoming change of a discrete lane to an absolute instant with
// the tempo map, and hands it to that handler BEFORE the playhead reaches it.
// The parameter itself is still set on its normal frame, so the GUI, the node
// graph and every non-scheduling consumer behave exactly as before -- only the
// backend send moves off the frame clock.
//
// Times are std::chrono::steady_clock microseconds, the same domain as
// ofxOceanodeTransportState::steadyTimeUs.
// ---------------------------------------------------------------------------

struct ofxOceanodeScheduledParameterEvent {
    // Text encoding of the value, identical to what the timeline applies to
    // the parameter itself (comma separated for vector parameters).
    std::string value;
    uint64_t dueSteadyTimeUs = 0;
    // Transport generation this event was computed for.
    uint64_t generation = 0;
    // A correction is sent after a stop/seek/edit invalidated events already
    // handed to the backend. It must land after everything queued before it,
    // even if its own due time is now in the past.
    bool isCorrection = false;
};

namespace ofxOceanodeScheduling {

using Handler = std::function<bool(const ofxOceanodeScheduledParameterEvent&)>;

uint64_t steadyNowUs();

// owner is an opaque tag (usually the node model) so every parameter of a
// destroyed node can be unregistered in one call.
void registerParameterTarget(const ofxOceanodeAbstractParameter* parameter,
                             const void* owner, Handler handler);
void unregisterParameterTarget(const ofxOceanodeAbstractParameter* parameter);
void unregisterOwner(const void* owner);
bool hasParameterTarget(const ofxOceanodeAbstractParameter* parameter);
// Returns false when there is no handler, or the handler could not schedule
// (for example the synth does not exist yet); the caller then falls back to
// the ordinary frame path.
bool dispatch(const ofxOceanodeAbstractParameter* parameter,
              const ofxOceanodeScheduledParameterEvent& event);

// While a suppression scope is active, a backend that already received a value
// as a scheduled event must not send it again from its parameter listener;
// otherwise every scheduled change would also be sent untimed on the frame the
// playhead reaches it (and a trigger parameter would fire twice).
bool isBackendSendSuppressed();

class ScopedBackendSuppression {
public:
    explicit ScopedBackendSuppression(bool active = true);
    ~ScopedBackendSuppression();
    ScopedBackendSuppression(const ScopedBackendSuppression&) = delete;
    ScopedBackendSuppression& operator=(const ScopedBackendSuppression&) = delete;
private:
    bool active;
    bool previous;
};

}

#endif /* ofxOceanodeScheduling_h */
