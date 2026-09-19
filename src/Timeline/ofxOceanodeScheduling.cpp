#include "ofxOceanodeScheduling.h"

#include <chrono>
#include <map>
#include <mutex>

namespace {
struct TargetEntry {
    const void* owner = nullptr;
    ofxOceanodeScheduling::Handler handler;
};

std::mutex& registryMutex() {
    static std::mutex mutex;
    return mutex;
}

std::map<const ofxOceanodeAbstractParameter*, TargetEntry>& registry() {
    static std::map<const ofxOceanodeAbstractParameter*, TargetEntry> targets;
    return targets;
}

// Per-thread: the timeline applies automation on the main thread, but a
// backend could send from its own.
thread_local bool backendSendSuppressed = false;
}

namespace ofxOceanodeScheduling {

uint64_t steadyNowUs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

void registerParameterTarget(const ofxOceanodeAbstractParameter* parameter,
                             const void* owner, Handler handler) {
    if(parameter == nullptr || !handler) return;
    std::lock_guard<std::mutex> lock(registryMutex());
    registry()[parameter] = TargetEntry{owner, std::move(handler)};
}

void unregisterParameterTarget(const ofxOceanodeAbstractParameter* parameter) {
    if(parameter == nullptr) return;
    std::lock_guard<std::mutex> lock(registryMutex());
    registry().erase(parameter);
}

void unregisterOwner(const void* owner) {
    if(owner == nullptr) return;
    std::lock_guard<std::mutex> lock(registryMutex());
    auto& targets = registry();
    for(auto it = targets.begin(); it != targets.end();) {
        if(it->second.owner == owner) it = targets.erase(it);
        else ++it;
    }
}

bool hasParameterTarget(const ofxOceanodeAbstractParameter* parameter) {
    if(parameter == nullptr) return false;
    std::lock_guard<std::mutex> lock(registryMutex());
    return registry().count(parameter) > 0;
}

bool dispatch(const ofxOceanodeAbstractParameter* parameter,
              const ofxOceanodeScheduledParameterEvent& event) {
    if(parameter == nullptr) return false;
    Handler handler;
    {
        std::lock_guard<std::mutex> lock(registryMutex());
        auto it = registry().find(parameter);
        if(it == registry().end()) return false;
        handler = it->second.handler;
    }
    // Called outside the lock: a handler may register/unregister targets.
    return handler ? handler(event) : false;
}

bool isBackendSendSuppressed() {
    return backendSendSuppressed;
}

ScopedBackendSuppression::ScopedBackendSuppression(bool _active)
: active(_active), previous(backendSendSuppressed) {
    if(active) backendSendSuppressed = true;
}

ScopedBackendSuppression::~ScopedBackendSuppression() {
    if(active) backendSendSuppressed = previous;
}

}
