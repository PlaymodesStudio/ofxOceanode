#include "Timeline/ofxOceanodeTimeline.h"

#include "Managers/ofxOceanodeContainer.h"
#include "ofxOceanodeParameter.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>

namespace ofxOceanodeTimelineCurve {

CurveInterpolationMode curveInterpolationMode(const std::string& name) {
    if(name == "Step") return CurveInterpolationMode::Step;
    if(name == "Log / Exp") return CurveInterpolationMode::LogExp;
    if(name == "Sigmoid") return CurveInterpolationMode::Sigmoid;
    return CurveInterpolationMode::Linear;
}

float sigmoidFlex(float x, float inflection, float steepness) {
    x = ofClamp(x, 0.0f, 1.0f);
    inflection = ofClamp(inflection, 0.01f, 0.99f);
    steepness = ofClamp(steepness, 0.1f, 10.0f);
    // The neutral sigmoid must be an exact identity. Besides avoiding tiny
    // floating-point bends, this makes newly-created curve segments truly
    // linear in both evaluation and drawing.
    if(std::abs(inflection - 0.5f) <= 1e-6f && std::abs(steepness - 1.0f) <= 1e-6f) return x;
    constexpr float epsilon = 0.0001f;
    if(x < epsilon) return 0.0f;
    if(x > 1.0f - epsilon) return 1.0f;
    const float xSafe = ofClamp(x, epsilon, 1.0f - epsilon);
    const float pSafe = ofClamp(inflection, epsilon, 1.0f - epsilon);
    const float a = std::pow(xSafe / pSafe, steepness);
    const float b = std::pow((1.0f - xSafe) / (1.0f - pSafe), steepness);
    const float denominator = a + b;
    return denominator < epsilon ? 0.5f : a / denominator;
}

float curveSegmentShape(float x, CurveInterpolationMode interpolation,
                        const ofxOceanodeTimelineCurveTension& tension) {
    x = ofClamp(x, 0.0f, 1.0f);
    switch(interpolation) {
        case CurveInterpolationMode::Step: return x >= 1.0f ? 1.0f : 0.0f;
        case CurveInterpolationMode::Linear: return x;
        case CurveInterpolationMode::LogExp:
            // steepness is clamped symmetrically around 1 (0.1 <-> 10) so the
            // "logarithmic" (steepness < 1) and "exponential" (steepness > 1)
            // ends are true reciprocals of each other and bend by comparable
            // amounts. A lower bound closer to 0 makes pow(x, steepness)
            // degenerate into a near-vertical rise right at x=0 followed by a
            // flat plateau, which reads as "broken" rather than "logarithmic".
            return std::pow(x, ofClamp(tension.steepness, 0.1f, 10.0f));
        case CurveInterpolationMode::Sigmoid:
            return sigmoidFlex(x, tension.inflection, tension.steepness);
    }
    return x;
}

} // namespace ofxOceanodeTimelineCurve

namespace {
using ofxOceanodeTimelineCurve::CurveInterpolationMode;
using ofxOceanodeTimelineCurve::curveInterpolationMode;
using ofxOceanodeTimelineCurve::sigmoidFlex;
using ofxOceanodeTimelineCurve::curveSegmentShape;

constexpr double kEpsilon = 1e-9;

double positiveModulo(double value, double length) {
    if(length <= kEpsilon) return 0.0;
    double wrapped = std::fmod(value, length);
    if(wrapped < 0.0) wrapped += length;
    return wrapped;
}

std::string combineAutomationValues(const std::vector<std::pair<ofxOceanodeTimelineAutomationMode, std::string>>& values,
                                    const std::string& valueType) {
    if(values.empty()) return std::string();
    const bool isFloat = valueType == typeid(float).name();
    const bool isInt = valueType == typeid(int).name();
    if(!isFloat && !isInt) {
        // Non-scalar values don't have a defined numeric combine yet, so
        // whichever contributor evaluated last simply wins (old behaviour).
        return values.back().second;
    }
    // A Replace contributor is the "base" signal -- everything else
    // (Add/Multiply/Min/Max) is a modulator applied on top of it. That base
    // must win by MEANING, not by vector position: which lane got added to
    // the clip first is an authoring accident the user has no reason to
    // track, so e.g. a Curve (left at the default Replace mode) driving
    // Levels together with a piano-roll Gate set to Multiply must multiply
    // correctly whichever lane happens to be earlier in the list. Find the
    // last Replace contributor (if several plain/overwrite lanes share the
    // parameter, the most recently evaluated one is the base, matching the
    // old strictly-positional behaviour in that case) and fold every other
    // contributor onto it, in their original relative order, using each
    // one's own mode.
    int baseIndex = -1;
    for(size_t i = 0; i < values.size(); ++i) {
        if(values[i].first == ofxOceanodeTimelineAutomationMode::Replace) baseIndex = static_cast<int>(i);
    }
    // No explicit Replace contributor at all (every lane driving this
    // parameter has its own blend mode) -- fall back to the first
    // contributor as the seed. Its own mode is irrelevant here since there
    // is nothing before it to combine with (Add against 0, Multiply against
    // 1, Min/Max against +-infinity all simplify to just its value).
    if(baseIndex < 0) baseIndex = 0;
    double result = ofToDouble(values[static_cast<size_t>(baseIndex)].second);
    for(size_t i = 0; i < values.size(); ++i) {
        if(static_cast<int>(i) == baseIndex) continue;
        // Any OTHER Replace contributor is a superseded plain/overwrite
        // signal -- the one at baseIndex already won that contest above, so
        // an earlier Replace entry must not clobber the base or the
        // modulators folded onto it while iterating past it.
        if(values[i].first == ofxOceanodeTimelineAutomationMode::Replace) continue;
        const double contribution = ofToDouble(values[i].second);
        switch(values[i].first) {
            case ofxOceanodeTimelineAutomationMode::Add: result += contribution; break;
            case ofxOceanodeTimelineAutomationMode::Multiply: result *= contribution; break;
            case ofxOceanodeTimelineAutomationMode::Min: result = std::min(result, contribution); break;
            case ofxOceanodeTimelineAutomationMode::Max: result = std::max(result, contribution); break;
            default: break;
        }
    }
    return isInt ? ofToString(static_cast<int>(std::lround(result))) : ofToString(static_cast<float>(result));
}

bool clipSourceBeat(const ofxOceanodeTimelineClip& clip, double globalBeat, double& sourceBeat) {
    const double duration = std::max(1.0 / 24.0, clip.durationBeats);
    if(globalBeat < clip.startBeat - kEpsilon || globalBeat >= clip.startBeat + duration - kEpsilon) return false;

    const double localBeat = std::max(0.0, globalBeat - clip.startBeat);
    const double contentDuration = std::max(1.0 / 24.0, clip.contentDurationBeats);
    if(clip.repeatContent) {
        sourceBeat = positiveModulo(localBeat, contentDuration);
    } else {
        sourceBeat = localBeat * contentDuration / duration;
    }
    return true;
}

bool evaluateCurve(const ofxOceanodeTimelineLane& lane, double beat, std::string& value) {
    if(lane.curvePoints.empty()) return false;
    if(lane.curvePoints.size() == 1) {
        value = ofToString(lane.curvePoints.front().value);
        return true;
    }
    const auto& points = lane.curvePoints;
    if(beat <= points.front().beat) value = ofToString(points.front().value);
    else if(beat >= points.back().beat) value = ofToString(points.back().value);
    else {
        for(size_t i = 1; i < points.size(); ++i) {
            if(beat > points[i].beat) continue;
            const double span = std::max(kEpsilon, points[i].beat - points[i - 1].beat);
            const float t = static_cast<float>(ofClamp((beat - points[i - 1].beat) / span, 0.0, 1.0));
            const auto tension = i - 1 < lane.curveTensions.size()
                ? lane.curveTensions[i - 1] : ofxOceanodeTimelineCurveTension{};
            value = ofToString(ofLerp(points[i - 1].value, points[i].value,
                                      curveSegmentShape(t, curveInterpolationMode(lane.curveInterpolation), tension)));
            break;
        }
    }
    return true;
}

std::string mapNormalizedLaneValue(const ofxOceanodeTimelineLane& lane,
                                   const ofxOceanodeTimelineParameterBinding& binding,
                                   const std::string& normalizedValue) {
    const float normalized = ofToFloat(normalizedValue);
    const float mapped = lane.valueMin + normalized * (lane.valueMax - lane.valueMin);
    if(binding.valueType == typeid(float).name()) return ofToString(mapped);
    if(binding.valueType == typeid(int).name()) return ofToString(static_cast<int>(std::lround(mapped)));
    if(binding.valueType == typeid(bool).name()) return normalized >= 0.5f ? "1" : "0";
    return normalizedValue;
}

bool applyAutomationValue(ofxOceanodeAbstractParameter& parameter,
                          const std::string& value,
                          const std::string& valueType) {
    if(valueType == typeid(float).name()) {
        parameter.cast<float>().getParameter().set(ofToFloat(value));
        return true;
    }
    if(valueType == typeid(int).name()) {
        parameter.cast<int>().getParameter().set(ofToInt(value));
        return true;
    }
    if(valueType == typeid(bool).name()) {
        parameter.cast<bool>().getParameter().set(ofToBool(value));
        return true;
    }
    if(valueType == typeid(std::string).name()) {
        parameter.cast<std::string>().getParameter().set(value);
        return true;
    }
    const auto tokens = ofSplitString(value, ",", true, true);
    if(valueType == typeid(std::vector<float>).name()) {
        std::vector<float> result;
        for(const auto& token : tokens) result.push_back(ofToFloat(token));
        parameter.cast<std::vector<float>>().getParameter().set(result);
        return true;
    }
    if(valueType == typeid(std::vector<int>).name()) {
        std::vector<int> result;
        for(const auto& token : tokens) result.push_back(ofToInt(token));
        parameter.cast<std::vector<int>>().getParameter().set(result);
        return true;
    }
    if(valueType == typeid(std::vector<bool>).name()) {
        std::vector<bool> result;
        for(const auto& token : tokens) result.push_back(ofToBool(token));
        parameter.cast<std::vector<bool>>().getParameter().set(result);
        return true;
    }
    if(valueType == typeid(std::vector<std::string>).name()) {
        parameter.cast<std::vector<std::string>>().getParameter().set(tokens);
        return true;
    }
    parameter.fromString(value);
    return true;
}

void applyPianoPitchRange(ofxOceanodeAbstractParameter& parameter,
                          const std::string& valueType,
                          int lowPitch, int highPitch) {
    lowPitch = ofClamp(lowPitch, 0, 127);
    highPitch = ofClamp(highPitch, lowPitch, 127);
    if(valueType == typeid(float).name()) {
        auto& target = parameter.cast<float>().getParameter();
        const float minimum = static_cast<float>(lowPitch);
        const float maximum = static_cast<float>(highPitch);
        if(target.getMin() != minimum) target.setMin(minimum);
        if(target.getMax() != maximum) target.setMax(maximum);
    } else if(valueType == typeid(int).name()) {
        auto& target = parameter.cast<int>().getParameter();
        if(target.getMin() != lowPitch) target.setMin(lowPitch);
        if(target.getMax() != highPitch) target.setMax(highPitch);
    } else if(valueType == typeid(std::vector<float>).name()) {
        auto& target = parameter.cast<std::vector<float>>().getParameter();
        size_t size = std::max(target.get().size(), target.getMin().size());
        size = std::max(size, target.getMax().size());
        size = std::max<size_t>(size, 1);
        const std::vector<float> minimum(size, static_cast<float>(lowPitch));
        const std::vector<float> maximum(size, static_cast<float>(highPitch));
        if(target.getMin() != minimum) target.setMin(minimum);
        if(target.getMax() != maximum) target.setMax(maximum);
    } else if(valueType == typeid(std::vector<int>).name()) {
        auto& target = parameter.cast<std::vector<int>>().getParameter();
        size_t size = std::max(target.get().size(), target.getMin().size());
        size = std::max(size, target.getMax().size());
        size = std::max<size_t>(size, 1);
        const std::vector<int> minimum(size, lowPitch);
        const std::vector<int> maximum(size, highPitch);
        if(target.getMin() != minimum) target.setMin(minimum);
        if(target.getMax() != maximum) target.setMax(maximum);
    }
}

std::string joinAutomationValues(const std::vector<std::string>& values) {
    std::string result;
    for(size_t i = 0; i < values.size(); ++i) {
        if(i > 0) result += ", ";
        result += values[i];
    }
    return result;
}

bool stepProbabilityPasses(const ofxOceanodeTimelineStep& step, double cycle) {
    const float probability = ofClamp(step.probability, 0.0f, 1.0f);
    if(probability <= 0.0f) return false;
    if(probability >= 1.0f) return true;

    // The result must remain stable while the playhead is inside a step. A
    // per-frame ofRandom() roll would make automation flicker at audio/frame
    // rate. This small integer hash gives one repeatable roll per step/cycle.
    const int64_t start = static_cast<int64_t>(std::llround(step.startBeat * 960.0));
    const int64_t cycleIndex = static_cast<int64_t>(std::llround(cycle));
    uint64_t hash = static_cast<uint64_t>(start) ^ (static_cast<uint64_t>(cycleIndex) * 0x9e3779b97f4a7c15ULL);
    hash ^= hash >> 30;
    hash *= 0xbf58476d1ce4e5b9ULL;
    hash ^= hash >> 27;
    hash *= 0x94d049bb133111ebULL;
    hash ^= hash >> 31;
    const float roll = static_cast<float>(hash & 0xffffULL) / 65535.0f;
    return roll < probability;
}

bool pianoProbabilityPasses(const ofxOceanodeTimelinePianoNote& note, double cycle) {
    const float probability = ofClamp(note.probability, 0.0f, 1.0f);
    if(probability <= 0.0f) return false;
    if(probability >= 1.0f) return true;
    const int64_t start = static_cast<int64_t>(std::llround(note.startBeat * 960.0));
    const int64_t pitch = static_cast<int64_t>(note.pitch);
    const int64_t cycleIndex = static_cast<int64_t>(std::llround(cycle));
    uint64_t hash = static_cast<uint64_t>(start) ^ (static_cast<uint64_t>(pitch) * 0x9e3779b97f4a7c15ULL);
    hash ^= static_cast<uint64_t>(cycleIndex) * 0xbf58476d1ce4e5b9ULL;
    hash ^= hash >> 30;
    hash *= 0xbf58476d1ce4e5b9ULL;
    hash ^= hash >> 27;
    hash *= 0x94d049bb133111ebULL;
    hash ^= hash >> 31;
    return static_cast<float>(hash & 0xffffULL) / 65535.0f < probability;
}

bool evaluateStepSequencer(const ofxOceanodeTimelineLane& lane,
                           double localBeat,
                           std::string& value) {
    if(lane.behavior == "Mute") return false;
    const double cellLength = std::max(1.0 / 24.0, lane.beatsPerStep);
    const double patternLength = std::max(cellLength, lane.stepCount * cellLength);
    const double cycle = std::floor(std::max(0.0, localBeat) / patternLength);
    const double patternBeat = positiveModulo(localBeat, patternLength);

    for(const auto& step : lane.step.steps) {
        const double start = std::max(0.0, step.startBeat);
        const double duration = step.durationBeats > kEpsilon ? step.durationBeats : cellLength;
        if(patternBeat + kEpsilon < start || patternBeat >= start + duration - kEpsilon) continue;
        const bool useProbability = lane.probabilityEnabled && lane.behavior == "Probability";
        if(useProbability && !stepProbabilityPasses(step, cycle)) return false;
        value = step.value.empty() ? "1" : step.value;
        return true;
    }
    return false;
}
}

void ofxOceanodeTimelineStepLane::sortSteps() {
    std::sort(steps.begin(), steps.end(), [](const auto& a, const auto& b) {
        return a.startBeat < b.startBeat;
    });
}

void ofxOceanodeTimelineStepLane::setStep(double startBeat, const std::string& value,
                                          double durationBeats, float probability) {
    startBeat = std::max(0.0, startBeat);
    durationBeats = std::max(0.0, durationBeats);
    probability = ofClamp(probability, 0.0f, 1.0f);
    for(auto& step : steps) {
        if(std::abs(step.startBeat - startBeat) <= kEpsilon) {
            step.value = value;
            step.durationBeats = durationBeats;
            step.probability = probability;
            sortSteps();
            return;
        }
    }
    steps.push_back({startBeat, durationBeats, value, probability});
    sortSteps();
}

bool ofxOceanodeTimelineStepLane::removeStep(double startBeat, double epsilon) {
    const auto oldSize = steps.size();
    steps.erase(std::remove_if(steps.begin(), steps.end(), [&](const auto& step) {
        return std::abs(step.startBeat - startBeat) <= epsilon;
    }), steps.end());
    return steps.size() != oldSize;
}

void ofxOceanodeTimelineStepLane::clear() {
    steps.clear();
}

bool ofxOceanodeTimelineStepLane::evaluate(double localBeat, std::string& value, bool useProbability) const {
    double cycle = 0.0;
    if(lengthBeats > kEpsilon && loop) {
        cycle = std::floor(std::max(0.0, localBeat) / lengthBeats);
        localBeat = positiveModulo(localBeat, lengthBeats);
    }
    if(localBeat < -kEpsilon) return false;
    if(lengthBeats > kEpsilon && localBeat >= lengthBeats - kEpsilon) return false;

    for(size_t i = 0; i < steps.size(); ++i) {
        const auto& step = steps[i];
        const double start = std::max(0.0, step.startBeat);
        if(localBeat + kEpsilon < start) break;

        double end = lengthBeats > kEpsilon ? lengthBeats : std::numeric_limits<double>::infinity();
        if(i + 1 < steps.size()) end = std::min(end, std::max(start, steps[i + 1].startBeat));
        if(step.durationBeats > kEpsilon) end = std::min(end, start + step.durationBeats);
        if(localBeat < end - kEpsilon || (end == std::numeric_limits<double>::infinity() && localBeat >= start)) {
            if(useProbability && !stepProbabilityPasses(step, cycle)) return false;
            value = step.value;
            return true;
        }
    }

    if(!fallbackValue.empty()) {
        value = fallbackValue;
        return true;
    }
    return false;
}

ofJson ofxOceanodeTimelineStepLane::toJson() const {
    ofJson json;
    json["lengthBeats"] = lengthBeats;
    json["loop"] = loop;
    json["fallbackValue"] = fallbackValue;
    json["steps"] = ofJson::array();
    for(const auto& step : steps) {
        json["steps"].push_back({
            {"startBeat", step.startBeat},
            {"durationBeats", step.durationBeats},
            {"value", step.value},
            {"probability", step.probability}
        });
    }
    return json;
}

void ofxOceanodeTimelineStepLane::fromJson(const ofJson& json) {
    lengthBeats = std::max(0.0, json.value("lengthBeats", 4.0));
    loop = json.value("loop", true);
    fallbackValue = json.value("fallbackValue", std::string());
    steps.clear();
    if(json.contains("steps") && json["steps"].is_array()) {
        for(const auto& stepJson : json["steps"]) {
            if(!stepJson.is_object() || !stepJson.contains("startBeat") || !stepJson.contains("value")) continue;
            ofxOceanodeTimelineStep step;
            step.startBeat = std::max(0.0, stepJson.value("startBeat", 0.0));
            step.durationBeats = std::max(0.0, stepJson.value("durationBeats", 0.0));
            step.value = stepJson.value("value", std::string());
            step.probability = ofClamp(stepJson.value("probability", 1.0f), 0.0f, 1.0f);
            steps.push_back(std::move(step));
        }
    }
    sortSteps();
}

ofxOceanodeTimelineManager::ofxOceanodeTimelineManager(ofxOceanodeContainer* owner) : container(owner) {}

void ofxOceanodeTimelineManager::setContainer(ofxOceanodeContainer* owner) {
    container = owner;
}

std::string ofxOceanodeTimelineManager::makeId(const char* prefix, uint64_t number) {
    return std::string(prefix) + "_" + ofToString(number);
}

std::string ofxOceanodeTimelineManager::makeUniqueTrackName(const std::string& requestedName) const {
    const std::string base = requestedName.empty() ? "Timeline Track" : requestedName;
    auto exists = [&](const std::string& candidate) {
        return std::any_of(tracks.begin(), tracks.end(), [&](const auto& track) { return track.name == candidate; });
    };
    if(!exists(base)) return base;
    for(int suffix = 2; ; ++suffix) {
        const std::string candidate = base + " " + ofToString(suffix);
        if(!exists(candidate)) return candidate;
    }
}

std::string ofxOceanodeTimelineManager::makeUniqueClipName(const ofxOceanodeTimelineTrack& track,
                                                           const std::string& requestedName) const {
    const std::string base = requestedName.empty() ? "Clip" : requestedName;
    auto exists = [&](const std::string& candidate) {
        return std::any_of(track.clips.begin(), track.clips.end(), [&](const auto& clip) {
            return clip.name == candidate;
        });
    };
    if(!exists(base)) return base;
    for(int suffix = 2; ; ++suffix) {
        const std::string candidate = base + " " + ofToString(suffix);
        if(!exists(candidate)) return candidate;
    }
}

std::string ofxOceanodeTimelineManager::makeUniqueTrackId() const {
    std::string id;
    uint64_t number = nextTrackNumber;
    do {
        id = makeId("timeline_track", number++);
    } while(getTrack(id) != nullptr);
    return id;
}

std::string ofxOceanodeTimelineManager::makeUniqueBindingId() const {
    std::string id;
    uint64_t number = nextBindingNumber;
    do {
        id = makeId("timeline_binding", number++);
        bool used = false;
        for(const auto& track : tracks) {
            used = std::any_of(track.bindings.begin(), track.bindings.end(), [&](const auto& binding) { return binding.id == id; });
            if(used) break;
        }
        if(!used) return id;
    } while(true);
}

std::string ofxOceanodeTimelineManager::makeUniqueClipId() const {
    std::string id;
    uint64_t number = nextClipNumber;
    do {
        id = makeId("timeline_clip", number++);
        bool used = false;
        for(const auto& track : tracks) {
            used = std::any_of(track.clips.begin(), track.clips.end(), [&](const auto& clip) {
                return clip.id == id;
            });
            if(used) break;
        }
        if(!used) return id;
    } while(true);
}

std::string ofxOceanodeTimelineManager::makeUniqueLaneId() const {
    std::string id;
    uint64_t number = nextLaneNumber;
    do {
        id = makeId("timeline_lane", number++);
        bool used = false;
        for(const auto& track : tracks) {
            for(const auto& clip : track.clips) {
                used = std::any_of(clip.lanes.begin(), clip.lanes.end(), [&](const auto& lane) {
                    return lane.id == id;
                });
                if(used) break;
            }
            if(used) break;
        }
        if(!used) return id;
    } while(true);
}

std::string ofxOceanodeTimelineManager::createTrack(const std::string& requestedName) {
    ofxOceanodeTimelineTrack track;
    track.id = makeUniqueTrackId();
    track.name = makeUniqueTrackName(requestedName);
    static const ofColor defaultColors[] = {
        ofColor(65, 165, 245, 255),
        ofColor(75, 190, 125, 255),
        ofColor(235, 165, 65, 255),
        ofColor(190, 95, 220, 255),
        ofColor(235, 90, 95, 255)
    };
    track.color = defaultColors[tracks.size() % (sizeof(defaultColors) / sizeof(defaultColors[0]))];
    ++nextTrackNumber;
    tracks.push_back(std::move(track));
    return tracks.back().id;
}

bool ofxOceanodeTimelineManager::renameTrack(const std::string& trackId, const std::string& requestedName) {
    auto* track = getTrack(trackId);
    if(track == nullptr || requestedName.empty()) return false;
    if(track->name == requestedName) return true;
    track->name = makeUniqueTrackName(requestedName);
    return true;
}

void ofxOceanodeTimelineManager::requestTrackRename(const std::string& trackId, bool isNewTrack) {
    if(getTrack(trackId) != nullptr) {
        pendingTrackRenameId = trackId;
        pendingTrackRenameIsNew = isNewTrack;
    }
}

bool ofxOceanodeTimelineManager::consumePendingTrackRename(std::string& trackId, bool* isNewTrack) {
    if(pendingTrackRenameId.empty()) return false;
    trackId = pendingTrackRenameId;
    if(isNewTrack != nullptr) *isNewTrack = pendingTrackRenameIsNew;
    pendingTrackRenameId.clear();
    pendingTrackRenameIsNew = false;
    return true;
}

bool ofxOceanodeTimelineManager::removeTrack(const std::string& trackId) {
    auto it = std::find_if(tracks.begin(), tracks.end(), [&](const auto& track) { return track.id == trackId; });
    if(it == tracks.end()) return false;
    clearTimelineFlag(*it);
    tracks.erase(it);
    return true;
}

ofxOceanodeTimelineTrack* ofxOceanodeTimelineManager::getTrack(const std::string& trackId) {
    auto it = std::find_if(tracks.begin(), tracks.end(), [&](auto& track) { return track.id == trackId; });
    return it == tracks.end() ? nullptr : &*it;
}

const ofxOceanodeTimelineTrack* ofxOceanodeTimelineManager::getTrack(const std::string& trackId) const {
    auto it = std::find_if(tracks.begin(), tracks.end(), [&](const auto& track) { return track.id == trackId; });
    return it == tracks.end() ? nullptr : &*it;
}

std::string ofxOceanodeTimelineManager::addBinding(const std::string& trackId,
                                                   ofxOceanodeAbstractParameter& parameter,
                                                   ofxOceanodeTimelineAutomationMode mode) {
    auto* track = getTrack(trackId);
    if(track == nullptr || container == nullptr || !isStepLaneCompatible(parameter)) return std::string();
    const bool createInitialClip = track->bindings.empty() && track->clips.empty();

    const std::string path = container->getCustomGuiParameterPath(parameter);
    for(const auto& existingTrack : tracks) {
        for(const auto& existing : existingTrack.bindings) {
            if(existing.parameterPath == path) return std::string();
        }
    }

    ofxOceanodeTimelineParameterBinding binding;
    binding.id = makeUniqueBindingId();
    binding.parameterPath = path;
    binding.valueType = parameter.valueType();
    binding.defaultValue = parameter.toString();
    binding.mode = mode;
    track->bindings.push_back(binding);

    // The first binding in a new track gets a starter clip. Publishing more
    // parameters to an existing track only adds them to the track's pool;
    // it must not silently add lanes to any existing clip.
    std::vector<std::string> clipIds;
    if(createInitialClip) {
        const auto clipId = createClip(trackId, "Clip", 0.0, 4.0);
        if(!clipId.empty()) clipIds.push_back(clipId);
    }
    for(const auto& clipId : clipIds) {
        const std::string laneId = createLane(trackId, clipId, parameter.getName(), binding.laneType);
        if(!laneId.empty()) {
            addBindingToLane(trackId, clipId, laneId, binding.id);
            if(auto* lane = getLane(trackId, clipId, laneId)) {
                if(binding.valueType == typeid(float).name()) {
                    lane->valueMin = parameter.cast<float>().getParameter().getMin();
                    lane->valueMax = parameter.cast<float>().getParameter().getMax();
                } else if(binding.valueType == typeid(int).name()) {
                    lane->valueMin = static_cast<float>(parameter.cast<int>().getParameter().getMin());
                    lane->valueMax = static_cast<float>(parameter.cast<int>().getParameter().getMax());
                } else if(binding.valueType == typeid(bool).name()) {
                    lane->valueMin = 0.0f;
                    lane->valueMax = 1.0f;
                }
            }
        }
    }
    parameter.setTimelined(true);
    ++nextBindingNumber;
    return binding.id;
}

bool ofxOceanodeTimelineManager::isParameterBound(const ofxOceanodeAbstractParameter& parameter) const {
    if(container == nullptr) return false;
    const std::string path = container->getCustomGuiParameterPath(const_cast<ofxOceanodeAbstractParameter&>(parameter));
    for(const auto& track : tracks) {
        if(std::any_of(track.bindings.begin(), track.bindings.end(), [&](const auto& binding){
            return binding.parameterPath == path;
        })) return true;
    }
    return false;
}

bool ofxOceanodeTimelineManager::getParameterTrackColor(const ofxOceanodeAbstractParameter& parameter, ofColor& color) const {
    if(container == nullptr) return false;
    const std::string path = container->getCustomGuiParameterPath(const_cast<ofxOceanodeAbstractParameter&>(parameter));
    for(const auto& track : tracks) {
        if(std::any_of(track.bindings.begin(), track.bindings.end(), [&](const auto& binding){
            return binding.parameterPath == path;
        })) {
            color = track.color;
            return true;
        }
    }
    return false;
}

bool ofxOceanodeTimelineManager::isStepLaneCompatible(const ofxOceanodeAbstractParameter& parameter) const {
    const std::string type = parameter.valueType();
    return type == typeid(float).name() ||
           type == typeid(int).name() ||
           type == typeid(bool).name() ||
           type == typeid(std::string).name() ||
           type == typeid(std::vector<float>).name() ||
           type == typeid(std::vector<int>).name() ||
           type == typeid(std::vector<bool>).name() ||
           type == typeid(std::vector<std::string>).name();
}

bool ofxOceanodeTimelineManager::removeBinding(const std::string& trackId, const std::string& bindingId) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return false;
    auto it = std::find_if(track->bindings.begin(), track->bindings.end(), [&](const auto& binding) { return binding.id == bindingId; });
    if(it == track->bindings.end()) return false;
    if(container != nullptr) {
        if(auto* parameter = container->findCustomGuiParameter(it->parameterPath)) parameter->setTimelined(false);
    }
    for(auto& clip : track->clips) {
        for(auto& lane : clip.lanes) {
            lane.bindingIds.erase(std::remove(lane.bindingIds.begin(), lane.bindingIds.end(), bindingId), lane.bindingIds.end());
            if(lane.pianoPitchBindingId == bindingId) lane.pianoPitchBindingId.clear();
            if(lane.pianoGateBindingId == bindingId) lane.pianoGateBindingId.clear();
            if(lane.pianoVelocityBindingId == bindingId) lane.pianoVelocityBindingId.clear();
        }
    }
    track->bindings.erase(it);
    return true;
}

bool ofxOceanodeTimelineManager::setLaneType(const std::string& trackId,
                                             const std::string& bindingId,
                                             ofxOceanodeTimelineLaneType laneType) {
    auto* binding = getBinding(trackId, bindingId);
    if(binding == nullptr) return false;
    binding->laneType = laneType;
    if(auto* track = getTrack(trackId)) {
        for(auto& clip : track->clips) {
            for(auto& lane : clip.lanes) {
                if(std::find(lane.bindingIds.begin(), lane.bindingIds.end(), bindingId) != lane.bindingIds.end()) {
                    lane.type = laneType;
                    if(laneType == ofxOceanodeTimelineLaneType::PianoRoll) {
                        lane.valueMin = 0.0f;
                        lane.valueMax = 1.0f;
                        if(lane.pianoPitchBindingId.empty()) lane.pianoPitchBindingId = bindingId;
                        else if(lane.pianoGateBindingId.empty() && lane.pianoPitchBindingId != bindingId) lane.pianoGateBindingId = bindingId;
                        else if(lane.pianoVelocityBindingId.empty() && lane.pianoPitchBindingId != bindingId && lane.pianoGateBindingId != bindingId) lane.pianoVelocityBindingId = bindingId;
                    }
                }
            }
        }
    }
    return true;
}

bool ofxOceanodeTimelineManager::setBindingMode(const std::string& trackId,
                                                const std::string& bindingId,
                                                ofxOceanodeTimelineAutomationMode mode) {
    auto* binding = getBinding(trackId, bindingId);
    if(binding == nullptr) return false;
    binding->mode = mode;
    return true;
}

std::string ofxOceanodeTimelineManager::createClip(const std::string& trackId,
                                                   const std::string& requestedName,
                                                   double startBeat,
                                                   double durationBeats) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return std::string();

    ofxOceanodeTimelineClip clip;
    clip.id = makeUniqueClipId();
    clip.name = makeUniqueClipName(*track, requestedName);
    clip.startBeat = std::max(0.0, startBeat);
    clip.durationBeats = std::max(1.0 / 24.0, durationBeats);
    clip.contentDurationBeats = clip.durationBeats;
    ++nextClipNumber;
    track->clips.push_back(std::move(clip));
    return track->clips.back().id;
}

bool ofxOceanodeTimelineManager::renameClip(const std::string& trackId, const std::string& clipId,
                                            const std::string& requestedName) {
    auto* track = getTrack(trackId);
    auto* clip = getClip(trackId, clipId);
    if(track == nullptr || clip == nullptr || requestedName.empty()) return false;
    clip->name = makeUniqueClipName(*track, requestedName);
    return true;
}

bool ofxOceanodeTimelineManager::removeClip(const std::string& trackId, const std::string& clipId) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return false;
    const auto oldSize = track->clips.size();
    track->clips.erase(std::remove_if(track->clips.begin(), track->clips.end(), [&](const auto& clip) {
        return clip.id == clipId;
    }), track->clips.end());
    return track->clips.size() != oldSize;
}

ofxOceanodeTimelineClip* ofxOceanodeTimelineManager::getClip(const std::string& trackId, const std::string& clipId) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return nullptr;
    auto it = std::find_if(track->clips.begin(), track->clips.end(), [&](auto& clip) { return clip.id == clipId; });
    return it == track->clips.end() ? nullptr : &*it;
}

const ofxOceanodeTimelineClip* ofxOceanodeTimelineManager::getClip(const std::string& trackId, const std::string& clipId) const {
    const auto* track = getTrack(trackId);
    if(track == nullptr) return nullptr;
    auto it = std::find_if(track->clips.begin(), track->clips.end(), [&](const auto& clip) { return clip.id == clipId; });
    return it == track->clips.end() ? nullptr : &*it;
}

std::string ofxOceanodeTimelineManager::createLane(const std::string& trackId, const std::string& clipId,
                                                   const std::string& requestedName,
                                                   ofxOceanodeTimelineLaneType type) {
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return std::string();
    ofxOceanodeTimelineLane lane;
    lane.id = makeUniqueLaneId();
    lane.name = requestedName.empty() ? "Lane" : requestedName;
    lane.type = type;
    lane.step.lengthBeats = clip->contentDurationBeats;
    lane.beatsPerStep = 0.25;
    lane.stepCount = std::max(1, static_cast<int>(std::llround(clip->contentDurationBeats / lane.beatsPerStep)));
    if(type == ofxOceanodeTimelineLaneType::Curve) {
        lane.curvePoints = {{0.0, 0.0f}, {clip->contentDurationBeats, 1.0f}};
        lane.curveTensions.push_back({0.5f, 1.0f});
    }
    ++nextLaneNumber;
    clip->lanes.push_back(std::move(lane));
    return clip->lanes.back().id;
}

bool ofxOceanodeTimelineManager::removeLane(const std::string& trackId, const std::string& clipId,
                                            const std::string& laneId) {
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return false;
    const auto oldSize = clip->lanes.size();
    clip->lanes.erase(std::remove_if(clip->lanes.begin(), clip->lanes.end(), [&](const auto& lane) {
        return lane.id == laneId;
    }), clip->lanes.end());
    return clip->lanes.size() != oldSize;
}

bool ofxOceanodeTimelineManager::addBindingToLane(const std::string& trackId, const std::string& clipId,
                                                  const std::string& laneId, const std::string& bindingId) {
    auto* track = getTrack(trackId);
    auto* lane = getLane(trackId, clipId, laneId);
    if(track == nullptr || lane == nullptr || getBinding(trackId, bindingId) == nullptr) return false;
    if(std::find(lane->bindingIds.begin(), lane->bindingIds.end(), bindingId) != lane->bindingIds.end()) return true;
    lane->bindingIds.push_back(bindingId);
    if(lane->type == ofxOceanodeTimelineLaneType::PianoRoll) {
        if(lane->pianoPitchBindingId.empty()) lane->pianoPitchBindingId = bindingId;
        else if(lane->pianoGateBindingId.empty()) lane->pianoGateBindingId = bindingId;
        else if(lane->pianoVelocityBindingId.empty()) lane->pianoVelocityBindingId = bindingId;
    }
    return true;
}

bool ofxOceanodeTimelineManager::removeBindingFromLane(const std::string& trackId, const std::string& clipId,
                                                       const std::string& laneId, const std::string& bindingId) {
    auto* lane = getLane(trackId, clipId, laneId);
    if(lane == nullptr) return false;
    const auto oldSize = lane->bindingIds.size();
    lane->bindingIds.erase(std::remove(lane->bindingIds.begin(), lane->bindingIds.end(), bindingId), lane->bindingIds.end());
    if(lane->pianoPitchBindingId == bindingId) lane->pianoPitchBindingId.clear();
    if(lane->pianoGateBindingId == bindingId) lane->pianoGateBindingId.clear();
    if(lane->pianoVelocityBindingId == bindingId) lane->pianoVelocityBindingId.clear();
    return lane->bindingIds.size() != oldSize;
}

ofxOceanodeTimelineLane* ofxOceanodeTimelineManager::getLane(const std::string& trackId, const std::string& clipId,
                                                             const std::string& laneId) {
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return nullptr;
    auto it = std::find_if(clip->lanes.begin(), clip->lanes.end(), [&](auto& lane) { return lane.id == laneId; });
    return it == clip->lanes.end() ? nullptr : &*it;
}

const ofxOceanodeTimelineLane* ofxOceanodeTimelineManager::getLane(const std::string& trackId, const std::string& clipId,
                                                                    const std::string& laneId) const {
    const auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return nullptr;
    auto it = std::find_if(clip->lanes.begin(), clip->lanes.end(), [&](const auto& lane) { return lane.id == laneId; });
    return it == clip->lanes.end() ? nullptr : &*it;
}

bool ofxOceanodeTimelineManager::setClipLaneType(const std::string& trackId, const std::string& clipId,
                                                 const std::string& laneId, ofxOceanodeTimelineLaneType type) {
    auto* lane = getLane(trackId, clipId, laneId);
    auto* clip = getClip(trackId, clipId);
    if(lane == nullptr) return false;
    lane->type = type;
    if(type == ofxOceanodeTimelineLaneType::PianoRoll) {
        lane->valueMin = 0.0f;
        lane->valueMax = 1.0f;
        if(lane->pianoPitchBindingId.empty() && !lane->bindingIds.empty()) lane->pianoPitchBindingId = lane->bindingIds[0];
        if(lane->pianoGateBindingId.empty() && lane->bindingIds.size() > 1) lane->pianoGateBindingId = lane->bindingIds[1];
        if(lane->pianoVelocityBindingId.empty() && lane->bindingIds.size() > 2) lane->pianoVelocityBindingId = lane->bindingIds[2];
    } else if(type == ofxOceanodeTimelineLaneType::Curve) {
        lane->curveInterpolation = "Linear";
        if(lane->curvePoints.empty()) {
            const double contentDuration = clip == nullptr ? 4.0 : clip->contentDurationBeats;
            lane->curvePoints = {{0.0, 0.0f}, {contentDuration, 1.0f}};
            lane->curveTensions = {{0.5f, 1.0f}};
        }
    }
    return true;
}

bool ofxOceanodeTimelineManager::setClipTiming(const std::string& trackId, const std::string& clipId,
                                               double startBeat, double durationBeats) {
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return false;
    clip->startBeat = std::max(0.0, startBeat);
    clip->durationBeats = std::max(1.0 / 24.0, durationBeats);
    return true;
}

bool ofxOceanodeTimelineManager::setClipContentDuration(const std::string& trackId, const std::string& clipId,
                                                         double contentDurationBeats, bool repeatContent) {
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return false;
    clip->contentDurationBeats = std::max(1.0 / 24.0, contentDurationBeats);
    clip->repeatContent = repeatContent;
    return true;
}

bool ofxOceanodeTimelineManager::setClipStep(const std::string& trackId, const std::string& clipId,
                                             const std::string& laneId, double startBeat,
                                             const std::string& value, double durationBeats) {
    auto* lane = getLane(trackId, clipId, laneId);
    if(lane == nullptr || lane->type != ofxOceanodeTimelineLaneType::Step) return false;
    lane->step.setStep(startBeat, value, durationBeats);
    return true;
}

bool ofxOceanodeTimelineManager::removeClipStep(const std::string& trackId, const std::string& clipId,
                                                const std::string& laneId, double startBeat) {
    auto* lane = getLane(trackId, clipId, laneId);
    return lane != nullptr && lane->step.removeStep(startBeat);
}

ofxOceanodeTimelineParameterBinding* ofxOceanodeTimelineManager::getBinding(const std::string& trackId, const std::string& bindingId) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return nullptr;
    auto it = std::find_if(track->bindings.begin(), track->bindings.end(), [&](auto& binding) { return binding.id == bindingId; });
    return it == track->bindings.end() ? nullptr : &*it;
}

const ofxOceanodeTimelineParameterBinding* ofxOceanodeTimelineManager::getBinding(const std::string& trackId, const std::string& bindingId) const {
    const auto* track = getTrack(trackId);
    if(track == nullptr) return nullptr;
    auto it = std::find_if(track->bindings.begin(), track->bindings.end(), [&](const auto& binding) { return binding.id == bindingId; });
    return it == track->bindings.end() ? nullptr : &*it;
}

bool ofxOceanodeTimelineManager::setStep(const std::string& trackId, const std::string& bindingId,
                                         double startBeat, const std::string& value, double durationBeats) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return false;
    for(auto& clip : track->clips) {
        for(auto& lane : clip.lanes) {
            if(lane.type == ofxOceanodeTimelineLaneType::Step &&
               std::find(lane.bindingIds.begin(), lane.bindingIds.end(), bindingId) != lane.bindingIds.end()) {
                lane.step.setStep(startBeat, value, durationBeats);
                return true;
            }
        }
    }
    return false;
}

bool ofxOceanodeTimelineManager::removeStep(const std::string& trackId, const std::string& bindingId, double startBeat) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return false;
    for(auto& clip : track->clips) {
        for(auto& lane : clip.lanes) {
            if(std::find(lane.bindingIds.begin(), lane.bindingIds.end(), bindingId) != lane.bindingIds.end()) return lane.step.removeStep(startBeat);
        }
    }
    return false;
}

bool ofxOceanodeTimelineManager::clearSteps(const std::string& trackId, const std::string& bindingId) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return false;
    bool changed = false;
    for(auto& clip : track->clips) {
        for(auto& lane : clip.lanes) {
            if(std::find(lane.bindingIds.begin(), lane.bindingIds.end(), bindingId) != lane.bindingIds.end()) {
                lane.step.clear();
                changed = true;
            }
        }
    }
    return changed;
}

void ofxOceanodeTimelineManager::setBpmAutomationEnabled(bool enabled) {
    bpmAutomationEnabled = enabled;
    if(enabled && bpmAutomationPoints.empty()) {
        const float currentBpm = container == nullptr ? 120.0f : container->getTransportState().bpm;
        bpmAutomationPoints.push_back({0.0, ofClamp(currentBpm, bpmMinimum, bpmMaximum)});
    }
}

void ofxOceanodeTimelineManager::setBpmRange(float minimum, float maximum) {
    bpmMinimum = std::max(1.0f, std::min(minimum, maximum));
    bpmMaximum = std::max(bpmMinimum + 1.0f, std::max(minimum, maximum));
    for(auto& point : bpmAutomationPoints) point.value = ofClamp(point.value, bpmMinimum, bpmMaximum);
}

float ofxOceanodeTimelineManager::evaluateBpm(double beat, float fallbackBpm) const {
    if(!bpmAutomationEnabled || bpmAutomationPoints.empty()) return std::max(1.0f, fallbackBpm);
    const auto& points = bpmAutomationPoints;
    beat = std::max(0.0, beat);
    if(beat <= points.front().beat) return ofClamp(points.front().value, bpmMinimum, bpmMaximum);
    if(beat >= points.back().beat) return ofClamp(points.back().value, bpmMinimum, bpmMaximum);
    const auto mode = curveInterpolationMode(bpmInterpolation);
    for(size_t i = 1; i < points.size(); ++i) {
        if(beat > points[i].beat) continue;
        const double span = std::max(kEpsilon, points[i].beat - points[i - 1].beat);
        const float t = static_cast<float>(ofClamp((beat - points[i - 1].beat) / span, 0.0, 1.0));
        const auto tension = i - 1 < bpmCurveTensions.size() ? bpmCurveTensions[i - 1] : ofxOceanodeTimelineCurveTension{};
        return ofClamp(ofLerp(points[i - 1].value, points[i].value, curveSegmentShape(t, mode, tension)), bpmMinimum, bpmMaximum);
    }
    return std::max(1.0f, fallbackBpm);
}

void ofxOceanodeTimelineManager::setBpmInterpolation(const std::string& interpolation) {
    bpmInterpolation = interpolation;
    // Topology/shape changes should not retain hidden shaping state from a
    // different mode, mirroring resetCurveTensions() for a lane's curve.
    bpmCurveTensions.assign(bpmAutomationPoints.empty() ? 0 : bpmAutomationPoints.size() - 1,
                            ofxOceanodeTimelineCurveTension{});
}

double ofxOceanodeTimelineManager::beatToSeconds(double beat, float fallbackBpm) const {
    beat = std::max(0.0, beat);
    if(!bpmAutomationEnabled || bpmAutomationPoints.empty())
        return beat * 60.0 / std::max(1.0f, fallbackBpm);

    const auto& points = bpmAutomationPoints;
    const auto mode = curveInterpolationMode(bpmInterpolation);
    auto clampedBpm = [&](float value) {
        return static_cast<double>(ofClamp(value, bpmMinimum, bpmMaximum));
    };
    auto integrateLinearBpm = [](double duration, double startBpm, double endBpm) {
        if(duration <= kEpsilon) return 0.0;
        startBpm = std::max(1.0, startBpm);
        endBpm = std::max(1.0, endBpm);
        const double slope = (endBpm - startBpm) / duration;
        return std::abs(slope) <= kEpsilon
            ? duration * 60.0 / startBpm
            : 60.0 * std::log(endBpm / startBpm) / slope;
    };
    // Elapsed time under a segment is the integral of 60/bpm(x) over its
    // length. That has a closed form only when bpm(x) is linear in x
    // (integrateLinearBpm above); Step holds at the start value the whole
    // way and jumps at the very boundary (an instant, so it contributes
    // nothing extra); Log/Exp and Sigmoid have no closed form for this, so
    // they're integrated numerically with a fixed Simpson's rule -- the
    // shapes are smooth and bounded, so this is far more precise than the
    // pixel/ruler math this feeds needs. `upToFraction` lets the same
    // helper answer both "the whole segment" (1.0) and "partway into it"
    // (< 1.0, for the segment beat actually falls in).
    auto integrateSegmentSeconds = [&](double segmentDuration, double startBpm, double endBpm,
                                       const ofxOceanodeTimelineCurveTension& tension, double upToFraction) {
        upToFraction = ofClamp(upToFraction, 0.0, 1.0);
        if(segmentDuration <= kEpsilon || upToFraction <= 0.0) return 0.0;
        startBpm = std::max(1.0, startBpm);
        endBpm = std::max(1.0, endBpm);
        if(mode == CurveInterpolationMode::Step)
            return segmentDuration * upToFraction * 60.0 / startBpm;
        if(mode == CurveInterpolationMode::Linear) {
            if(upToFraction >= 1.0 - 1e-9) return integrateLinearBpm(segmentDuration, startBpm, endBpm);
            const double partialEndBpm = startBpm + (endBpm - startBpm) * upToFraction;
            return integrateLinearBpm(segmentDuration * upToFraction, startBpm, partialEndBpm);
        }
        constexpr int steps = 64;
        const double h = upToFraction / steps;
        auto integrand = [&](double x) {
            const float shaped = curveSegmentShape(static_cast<float>(x), mode, tension);
            const double bpm = std::max(1.0, startBpm + (endBpm - startBpm) * static_cast<double>(shaped));
            return 60.0 / bpm;
        };
        double sum = integrand(0.0) + integrand(upToFraction);
        for(int i = 1; i < steps; ++i) sum += integrand(i * h) * (i % 2 == 0 ? 2.0 : 4.0);
        return segmentDuration * (h / 3.0) * sum;
    };

    const double firstBeat = std::max(0.0, points.front().beat);
    const double firstBpm = clampedBpm(points.front().value);
    if(beat <= firstBeat) return beat * 60.0 / firstBpm;
    double seconds = 0.0;
    seconds += firstBeat * 60.0 / firstBpm;
    for(size_t i = 1; i < points.size(); ++i) {
        const double segmentStart = std::max(0.0, points[i - 1].beat);
        const double segmentEnd = std::max(segmentStart, points[i].beat);
        const double segmentDuration = segmentEnd - segmentStart;
        if(segmentDuration <= kEpsilon) continue;
        const double startBpm = clampedBpm(points[i - 1].value);
        const double endBpm = clampedBpm(points[i].value);
        const auto tension = i - 1 < bpmCurveTensions.size() ? bpmCurveTensions[i - 1] : ofxOceanodeTimelineCurveTension{};
        if(beat >= segmentEnd) {
            seconds += integrateSegmentSeconds(segmentDuration, startBpm, endBpm, tension, 1.0);
            continue;
        }
        if(beat > segmentStart) {
            const double fraction = (beat - segmentStart) / segmentDuration;
            seconds += integrateSegmentSeconds(segmentDuration, startBpm, endBpm, tension, fraction);
        }
        return seconds;
    }
    const double lastBeat = std::max(0.0, points.back().beat);
    if(beat > lastBeat) seconds += (beat - lastBeat) * 60.0 / clampedBpm(points.back().value);
    return seconds;
}

void ofxOceanodeTimelineManager::setLoopRange(double startBeat, double endBeat) {
    const double minimumLength = 1.0 / 24.0;
    loopStartBeat = std::max(0.0, std::min(startBeat, endBeat - minimumLength));
    loopEndBeat = std::max(loopStartBeat + minimumLength, endBeat);
}

void ofxOceanodeTimelineManager::update() {
    evaluateAutomation();
    applyAutomation();
}

void ofxOceanodeTimelineManager::evaluateAutomation() {
    if(container == nullptr) return;
    auto transport = container->getTransportState();

    if(loopEnabled && transport.isPlaying && loopEndBeat > loopStartBeat + kEpsilon &&
       transport.beatPosition >= loopEndBeat) {
        const double loopLength = loopEndBeat - loopStartBeat;
        const double wrappedBeat = loopStartBeat + positiveModulo(transport.beatPosition - loopStartBeat, loopLength);
        if(auto ownerTransport = container->getTransport()) ownerTransport->seekToBeat(wrappedBeat);
        transport = container->getTransportState();
    }

    if(bpmAutomationEnabled) {
        const float automatedBpm = evaluateBpm(transport.beatPosition, transport.bpm);
        if(std::abs(automatedBpm - transport.bpm) > 0.001f) {
            container->setBpm(automatedBpm);
            transport.bpm = automatedBpm;
        }
    }

    struct PianoPitchRange {
        std::string valueType;
        int low = 127;
        int high = 0;
    };
    auto& activeValues = activeAutomationValues;
    auto& zeroWhenInactivePaths = zeroWhenInactiveAutomationPaths;
    activeValues.clear();
    zeroWhenInactivePaths.clear();
    std::map<std::string, PianoPitchRange> pianoPitchRanges;
    for(const auto& track : tracks) {
        for(const auto& clip : track.clips) {
            for(const auto& lane : clip.lanes) {
                if(lane.type != ofxOceanodeTimelineLaneType::PianoRoll) continue;
                const std::string pitchId = !lane.pianoPitchBindingId.empty()
                    ? lane.pianoPitchBindingId : (!lane.bindingIds.empty() ? lane.bindingIds[0] : std::string());
                const std::string gateId = !lane.pianoGateBindingId.empty()
                    ? lane.pianoGateBindingId : (lane.bindingIds.size() > 1 ? lane.bindingIds[1] : std::string());
                const std::string velocityId = !lane.pianoVelocityBindingId.empty()
                    ? lane.pianoVelocityBindingId : (lane.bindingIds.size() > 2 ? lane.bindingIds[2] : std::string());
                if(const auto* binding = getBinding(track.id, pitchId)) {
                    auto& range = pianoPitchRanges[binding->parameterPath];
                    range.valueType = binding->valueType;
                    const int laneLowPitch = static_cast<int>(ofClamp(lane.pianoLowPitch, 0, 127));
                    const int laneHighPitch = static_cast<int>(ofClamp(lane.pianoHighPitch, 0, 127));
                    if(laneLowPitch < range.low) range.low = laneLowPitch;
                    if(laneHighPitch > range.high) range.high = laneHighPitch;
                }
                if(const auto* binding = getBinding(track.id, gateId)) zeroWhenInactivePaths.insert(binding->parameterPath);
                if(const auto* binding = getBinding(track.id, velocityId)) zeroWhenInactivePaths.insert(binding->parameterPath);
            }
        }
    }
    for(const auto& entry : pianoPitchRanges) {
        if(auto* parameter = container->findCustomGuiParameter(entry.first)) {
            applyPianoPitchRange(*parameter, entry.second.valueType, entry.second.low, entry.second.high);
        }
    }
    for(auto& track : tracks) {
        for(const auto& clip : track.clips) {
            double sourceBeat = 0.0;
            if(!clipSourceBeat(clip, transport.beatPosition, sourceBeat)) continue;
            for(const auto& lane : clip.lanes) {
                if(lane.type == ofxOceanodeTimelineLaneType::Step || lane.type == ofxOceanodeTimelineLaneType::Curve) {
                    std::string value;
                    const double stepBeat = (lane.type == ofxOceanodeTimelineLaneType::Step && clip.repeatContent)
                        ? std::max(0.0, transport.beatPosition - clip.startBeat)
                        : sourceBeat;
                    const bool hasValue = lane.type == ofxOceanodeTimelineLaneType::Step
                        ? evaluateStepSequencer(lane, stepBeat, value)
                        : evaluateCurve(lane, sourceBeat, value);
                    if(!hasValue && lane.type != ofxOceanodeTimelineLaneType::Step) continue;
                    if(!hasValue) value = "0";
                    for(const auto& bindingId : lane.bindingIds) {
                        if(const auto* binding = getBinding(track.id, bindingId)) {
                            // A bypassed binding shouldn't contribute to a
                            // shared parameter's combined value at all.
                            if(binding->bypass) continue;
                            activeValues[binding->parameterPath].emplace_back(
                                binding->mode, mapNormalizedLaneValue(lane, *binding, value));
                        }
                    }
                } else {
                    std::vector<const ofxOceanodeTimelinePianoNote*> activeNotes;
                    const double rawLocalBeat = std::max(0.0, transport.beatPosition - clip.startBeat);
                    const double pianoCycle = clip.repeatContent
                        ? std::floor(rawLocalBeat / std::max(1.0 / 24.0, clip.contentDurationBeats)) : 0.0;
                    for(const auto& note : lane.pianoNotes) {
                        if(transport.isPlaying && sourceBeat >= note.startBeat &&
                           sourceBeat < note.startBeat + std::max(1.0 / 24.0, note.durationBeats) &&
                           pianoProbabilityPasses(note, pianoCycle)) {
                            activeNotes.push_back(&note);
                        }
                    }
                    const std::string roleBindingIds[] = {
                        !lane.pianoPitchBindingId.empty() ? lane.pianoPitchBindingId : (lane.bindingIds.size() > 0 ? lane.bindingIds[0] : std::string()),
                        !lane.pianoGateBindingId.empty() ? lane.pianoGateBindingId : (lane.bindingIds.size() > 1 ? lane.bindingIds[1] : std::string()),
                        !lane.pianoVelocityBindingId.empty() ? lane.pianoVelocityBindingId : (lane.bindingIds.size() > 2 ? lane.bindingIds[2] : std::string())
                    };
                    if(activeNotes.empty()) {
                        // No note is sounding right now. Pitch holds its
                        // last value (skip it, same as before a note ever
                        // played), but Gate and Velocity are conceptually
                        // "off" -- they must still contribute an explicit 0
                        // rather than nothing at all, otherwise a shared
                        // parameter driven by e.g. a Curve lane AND this
                        // lane's Gate (Multiply) never actually gets
                        // silenced: combineAutomationValues only zeroes a
                        // parameter when it has zero contributors, and the
                        // Curve lane would still be contributing every beat.
                        for(size_t bindingIndex = 1; bindingIndex < 3; ++bindingIndex) {
                            if(const auto* binding = getBinding(track.id, roleBindingIds[bindingIndex])) {
                                if(binding->bypass) continue;
                                activeValues[binding->parameterPath].emplace_back(binding->mode, std::string("0"));
                            }
                        }
                        continue;
                    }
                    std::sort(activeNotes.begin(), activeNotes.end(), [](const auto* a, const auto* b) {
                        return a->pitch < b->pitch;
                    });
                    if(lane.pianoMonophonic && activeNotes.size() > 1) activeNotes.erase(activeNotes.begin(), activeNotes.end() - 1);
                    for(size_t bindingIndex = 0; bindingIndex < 3; ++bindingIndex) {
                        if(const auto* binding = getBinding(track.id, roleBindingIds[bindingIndex])) {
                            if(binding->bypass) continue;
                            std::vector<std::string> noteValues;
                            for(const auto* activeNote : activeNotes) {
                                if(bindingIndex == 0) noteValues.push_back(ofToString(activeNote->pitch));
                                else if(bindingIndex == 1) noteValues.push_back("1");
                                else if(bindingIndex == 2) noteValues.push_back(ofToString(activeNote->velocity));
                                else noteValues.push_back(ofToString(activeNote->pitch));
                            }
                            const bool vectorTarget = binding->valueType == typeid(std::vector<float>).name() ||
                                binding->valueType == typeid(std::vector<int>).name() ||
                                binding->valueType == typeid(std::vector<bool>).name() ||
                                binding->valueType == typeid(std::vector<std::string>).name();
                            std::string value = vectorTarget ? joinAutomationValues(noteValues) : noteValues.back();
                            if(bindingIndex == 2 && !vectorTarget)
                                value = mapNormalizedLaneValue(lane, *binding, value);
                            activeValues[binding->parameterPath].emplace_back(binding->mode, value);
                        }
                    }
                }
            }
        }
    }
}

void ofxOceanodeTimelineManager::applyAutomation() {
    if(container == nullptr) return;
    const auto& activeValues = activeAutomationValues;
    const auto& zeroWhenInactivePaths = zeroWhenInactiveAutomationPaths;
    for(auto& track : tracks) {
        for(auto& binding : track.bindings) {
            if(binding.bypass) {
                // A bypassed binding stops driving its parameter, but it was
                // marked isTimelined() while active; clear that so the node
                // GUI's automation badge doesn't stay on for a binding that
                // no longer applies.
                if(auto* parameter = container->findCustomGuiParameter(binding.parameterPath))
                    parameter->setTimelined(false);
                continue;
            }
            auto* parameter = container->findCustomGuiParameter(binding.parameterPath);
            if(parameter == nullptr) {
                binding.missingTarget = true;
                continue;
            }
            binding.missingTarget = false;
            parameter->setTimelined(true);
            // Every binding pointing at this parameter path shares the same
            // already-combined result (combineAutomationValues folded each
            // contributor's own mode in already), so there is nothing left
            // to gate on binding.mode here -- it only mattered while
            // building the combined value above.

            const auto valuesIt = activeValues.find(binding.parameterPath);
            const std::string value = valuesIt == activeValues.end()
                ? (zeroWhenInactivePaths.count(binding.parameterPath) > 0 ? std::string("0") : binding.defaultValue)
                : combineAutomationValues(valuesIt->second, binding.valueType);
            if(!value.empty() && parameter->toString() != value)
                applyAutomationValue(*parameter, value, binding.valueType);
        }
    }
}

void ofxOceanodeTimelineManager::clearTimelineFlag(const ofxOceanodeTimelineTrack& track) {
    if(container == nullptr) return;
    for(const auto& binding : track.bindings) {
        if(auto* parameter = container->findCustomGuiParameter(binding.parameterPath)) parameter->setTimelined(false);
    }
}

void ofxOceanodeTimelineManager::clear() {
    for(const auto& track : tracks) clearTimelineFlag(track);
    tracks.clear();
    nextTrackNumber = 1;
    nextBindingNumber = 1;
    nextClipNumber = 1;
    nextLaneNumber = 1;
    pendingTrackRenameId.clear();
    pendingTrackRenameIsNew = false;
    bpmAutomationEnabled = false;
    bpmLaneCollapsed = true;
    bpmMinimum = 20.0f;
    bpmMaximum = 300.0f;
    bpmAutomationPoints.clear();
    bpmCurveTensions.clear();
    bpmInterpolation = "Linear";
    loopEnabled = false;
    loopStartBeat = 0.0;
    loopEndBeat = 4.0;
    timeSignatureNumerator = 4;
    timeSignatureDenominator = 4;
}

void ofxOceanodeTimelineManager::setTimeSignature(int numerator, int denominator) {
    timeSignatureNumerator = std::max(1, numerator);
    timeSignatureDenominator = std::max(1, denominator);
}

std::string ofxOceanodeTimelineManager::modeToString(ofxOceanodeTimelineAutomationMode mode) {
    switch(mode) {
        case ofxOceanodeTimelineAutomationMode::Add: return "Add";
        case ofxOceanodeTimelineAutomationMode::Multiply: return "Multiply";
        case ofxOceanodeTimelineAutomationMode::Min: return "Min";
        case ofxOceanodeTimelineAutomationMode::Max: return "Max";
        case ofxOceanodeTimelineAutomationMode::Replace:
        default: return "Replace";
    }
}

ofxOceanodeTimelineAutomationMode ofxOceanodeTimelineManager::modeFromString(const std::string& mode) {
    if(mode == "Add") return ofxOceanodeTimelineAutomationMode::Add;
    if(mode == "Multiply") return ofxOceanodeTimelineAutomationMode::Multiply;
    if(mode == "Min") return ofxOceanodeTimelineAutomationMode::Min;
    if(mode == "Max") return ofxOceanodeTimelineAutomationMode::Max;
    return ofxOceanodeTimelineAutomationMode::Replace;
}

std::string ofxOceanodeTimelineManager::laneTypeToString(ofxOceanodeTimelineLaneType laneType) {
    switch(laneType) {
        case ofxOceanodeTimelineLaneType::PianoRoll: return "PianoRoll";
        case ofxOceanodeTimelineLaneType::Curve: return "Curve";
        case ofxOceanodeTimelineLaneType::Step:
        default: return "Step";
    }
}

ofxOceanodeTimelineLaneType ofxOceanodeTimelineManager::laneTypeFromString(const std::string& laneType) {
    if(laneType == "PianoRoll") return ofxOceanodeTimelineLaneType::PianoRoll;
    if(laneType == "Curve") return ofxOceanodeTimelineLaneType::Curve;
    return ofxOceanodeTimelineLaneType::Step;
}

ofJson ofxOceanodeTimelineManager::toJson() const {
    ofJson json;
    json["version"] = 4;
    json["tempo"] = {
        {"enabled", bpmAutomationEnabled},
        {"collapsed", bpmLaneCollapsed},
        {"minimum", bpmMinimum},
        {"maximum", bpmMaximum},
        {"interpolation", bpmInterpolation},
        {"points", ofJson::array()},
        {"tensions", ofJson::array()}
    };
    for(const auto& point : bpmAutomationPoints)
        json["tempo"]["points"].push_back({{"beat", point.beat}, {"bpm", point.value}});
    for(const auto& tension : bpmCurveTensions)
        json["tempo"]["tensions"].push_back({{"inflection", tension.inflection}, {"steepness", tension.steepness}});
    json["loop"] = {
        {"enabled", loopEnabled},
        {"startBeat", loopStartBeat},
        {"endBeat", loopEndBeat}
    };
    json["timeSignature"] = {
        {"numerator", timeSignatureNumerator},
        {"denominator", timeSignatureDenominator}
    };
    json["tracks"] = ofJson::array();
    for(const auto& track : tracks) {
        ofJson trackJson;
        trackJson["id"] = track.id;
        trackJson["name"] = track.name;
        trackJson["collapsed"] = track.collapsed;
        trackJson["color"] = {
            {"r", track.color.r}, {"g", track.color.g},
            {"b", track.color.b}, {"a", track.color.a}
        };
        trackJson["bindings"] = ofJson::array();
        for(const auto& binding : track.bindings) {
            trackJson["bindings"].push_back({
                {"id", binding.id},
                {"parameterPath", binding.parameterPath},
                {"valueType", binding.valueType},
                {"defaultValue", binding.defaultValue},
                {"mode", modeToString(binding.mode)},
                {"laneType", laneTypeToString(binding.laneType)},
                {"bypass", binding.bypass}
            });
        }
        trackJson["clips"] = ofJson::array();
        for(const auto& clip : track.clips) {
            ofJson clipJson;
            clipJson["id"] = clip.id;
            clipJson["name"] = clip.name;
            clipJson["startBeat"] = clip.startBeat;
            clipJson["durationBeats"] = clip.durationBeats;
            clipJson["contentDurationBeats"] = clip.contentDurationBeats;
            clipJson["repeatContent"] = clip.repeatContent;
            clipJson["lanes"] = ofJson::array();
            for(const auto& lane : clip.lanes) {
                ofJson laneJson;
                laneJson["id"] = lane.id;
                laneJson["name"] = lane.name;
                laneJson["laneType"] = laneTypeToString(lane.type);
                laneJson["bindingIds"] = lane.bindingIds;
                laneJson["stepCount"] = lane.stepCount;
                laneJson["beatsPerStep"] = lane.beatsPerStep;
                laneJson["beatDivision"] = lane.beatDivision;
                laneJson["valueMin"] = lane.valueMin;
                laneJson["valueMax"] = lane.valueMax;
                laneJson["probabilityEnabled"] = lane.probabilityEnabled;
                laneJson["behavior"] = lane.behavior;
                laneJson["pianoLowPitch"] = lane.pianoLowPitch;
                laneJson["pianoHighPitch"] = lane.pianoHighPitch;
                laneJson["pianoSnapToGrid"] = lane.pianoSnapToGrid;
                laneJson["pianoPitchBindingId"] = lane.pianoPitchBindingId;
                laneJson["pianoGateBindingId"] = lane.pianoGateBindingId;
                laneJson["pianoVelocityBindingId"] = lane.pianoVelocityBindingId;
                laneJson["pianoDefaultVelocity"] = lane.pianoDefaultVelocity;
                laneJson["pianoMonophonic"] = lane.pianoMonophonic;
                laneJson["curveClamp"] = lane.curveClamp;
                laneJson["curveInterpolation"] = lane.curveInterpolation;
                laneJson["step"] = lane.step.toJson();
                laneJson["curvePoints"] = ofJson::array();
                for(const auto& point : lane.curvePoints) laneJson["curvePoints"].push_back({{"beat", point.beat}, {"value", point.value}});
                laneJson["curveTensions"] = ofJson::array();
                for(const auto& tension : lane.curveTensions) laneJson["curveTensions"].push_back({
                    {"inflection", tension.inflection}, {"steepness", tension.steepness}
                });
                laneJson["pianoNotes"] = ofJson::array();
                for(const auto& note : lane.pianoNotes) laneJson["pianoNotes"].push_back({
                    {"startBeat", note.startBeat}, {"durationBeats", note.durationBeats},
                    {"pitch", note.pitch}, {"velocity", note.velocity},
                    {"probability", note.probability}
                });
                clipJson["lanes"].push_back(std::move(laneJson));
            }
            trackJson["clips"].push_back(std::move(clipJson));
        }

        json["tracks"].push_back(std::move(trackJson));
    }
    return json;
}

void ofxOceanodeTimelineManager::fromJson(const ofJson& json) {
    clear();
    if(!json.is_object() || !json.contains("tracks") || !json["tracks"].is_array()) return;

    if(json.contains("tempo") && json["tempo"].is_object()) {
        const auto& tempo = json["tempo"];
        bpmAutomationEnabled = tempo.value("enabled", false);
        bpmLaneCollapsed = tempo.value("collapsed", true);
        setBpmRange(tempo.value("minimum", 20.0f), tempo.value("maximum", 300.0f));
        bpmInterpolation = tempo.value("interpolation", std::string("Linear"));
        if(bpmInterpolation != "Step" && bpmInterpolation != "Linear" &&
           bpmInterpolation != "Log / Exp" && bpmInterpolation != "Sigmoid") {
            bpmInterpolation = "Linear";
        }
        if(tempo.contains("points") && tempo["points"].is_array()) {
            for(const auto& point : tempo["points"]) {
                if(!point.is_object()) continue;
                bpmAutomationPoints.push_back({
                    std::max(0.0, point.value("beat", 0.0)),
                    ofClamp(point.value("bpm", 120.0f), bpmMinimum, bpmMaximum)
                });
            }
        }
        std::sort(bpmAutomationPoints.begin(), bpmAutomationPoints.end(), [](const auto& a, const auto& b) {
            return a.beat < b.beat;
        });
        if(tempo.contains("tensions") && tempo["tensions"].is_array()) {
            for(const auto& tensionJson : tempo["tensions"]) {
                if(!tensionJson.is_object()) continue;
                bpmCurveTensions.push_back({
                    ofClamp(tensionJson.value("inflection", 0.5f), 0.01f, 0.99f),
                    ofClamp(tensionJson.value("steepness", 1.0f), 0.1f, 10.0f)
                });
            }
        }
        bpmCurveTensions.resize(bpmAutomationPoints.empty() ? 0 : bpmAutomationPoints.size() - 1);
    }
    if(json.contains("loop") && json["loop"].is_object()) {
        const auto& loop = json["loop"];
        loopEnabled = loop.value("enabled", false);
        setLoopRange(loop.value("startBeat", 0.0), loop.value("endBeat", 4.0));
    }
    if(json.contains("timeSignature") && json["timeSignature"].is_object()) {
        const auto& signature = json["timeSignature"];
        setTimeSignature(signature.value("numerator", 4), signature.value("denominator", 4));
    }

    for(const auto& trackJson : json["tracks"]) {
        if(!trackJson.is_object()) continue;
        ofxOceanodeTimelineTrack track;
        track.id = trackJson.value("id", std::string());
        if(track.id.empty() || getTrack(track.id) != nullptr) track.id = makeUniqueTrackId();
        track.name = makeUniqueTrackName(trackJson.value("name", std::string("Timeline Track")));
        track.collapsed = trackJson.value("collapsed", false);
        if(trackJson.contains("color") && trackJson["color"].is_object()) {
            const auto& colorJson = trackJson["color"];
            track.color = ofColor(
                static_cast<unsigned char>(ofClamp(colorJson.value("r", 65), 0, 255)),
                static_cast<unsigned char>(ofClamp(colorJson.value("g", 165), 0, 255)),
                static_cast<unsigned char>(ofClamp(colorJson.value("b", 245), 0, 255)),
                static_cast<unsigned char>(ofClamp(colorJson.value("a", 255), 0, 255))
            );
        }

        if(trackJson.contains("bindings") && trackJson["bindings"].is_array()) {
            for(const auto& bindingJson : trackJson["bindings"]) {
                if(!bindingJson.is_object()) continue;
                ofxOceanodeTimelineParameterBinding binding;
                binding.id = bindingJson.value("id", std::string());
                const auto bindingIdExists = [&](const std::string& id) {
                    for(const auto& existingTrack : tracks) {
                        if(std::any_of(existingTrack.bindings.begin(), existingTrack.bindings.end(), [&](const auto& existing){
                            return existing.id == id;
                        })) return true;
                    }
                    return std::any_of(track.bindings.begin(), track.bindings.end(), [&](const auto& existing){
                        return existing.id == id;
                    });
                };
                if(binding.id.empty() || bindingIdExists(binding.id)) {
                    do {
                        binding.id = makeId("timeline_binding", nextBindingNumber++);
                    } while(bindingIdExists(binding.id));
                }
                binding.parameterPath = bindingJson.value("parameterPath", std::string());
                if(binding.id.empty() || binding.parameterPath.empty()) continue;
                binding.valueType = bindingJson.value("valueType", std::string());
                binding.defaultValue = bindingJson.value("defaultValue", std::string());
                binding.mode = modeFromString(bindingJson.value("mode", std::string("Replace")));
                binding.laneType = laneTypeFromString(bindingJson.value("laneType", std::string("Step")));
                binding.bypass = bindingJson.value("bypass", false);
                track.bindings.push_back(std::move(binding));
            }
        }

        if(trackJson.contains("clips") && trackJson["clips"].is_array()) {
            for(const auto& clipJson : trackJson["clips"]) {
                if(!clipJson.is_object()) continue;
                ofxOceanodeTimelineClip clip;
                clip.id = clipJson.value("id", std::string());
                if(clip.id.empty()) clip.id = makeUniqueClipId();
                clip.name = clipJson.value("name", std::string("Clip"));
                clip.startBeat = std::max(0.0, clipJson.value("startBeat", 0.0));
                clip.durationBeats = std::max(1.0 / 24.0, clipJson.value("durationBeats", 4.0));
                clip.contentDurationBeats = std::max(1.0 / 24.0, clipJson.value("contentDurationBeats", clip.durationBeats));
                clip.repeatContent = clipJson.value("repeatContent", true);
                if(clipJson.contains("lanes") && clipJson["lanes"].is_array()) {
                    for(const auto& laneJson : clipJson["lanes"]) {
                        if(!laneJson.is_object()) continue;
                        ofxOceanodeTimelineLane lane;
                        lane.id = laneJson.value("id", std::string());
                        if(lane.id.empty()) lane.id = makeUniqueLaneId();
                        lane.name = laneJson.value("name", std::string("Lane"));
                        lane.type = laneTypeFromString(laneJson.value("laneType", std::string("Step")));
                        lane.stepCount = std::max(1, laneJson.value("stepCount", 16));
                        lane.beatsPerStep = std::max(1.0 / 24.0, laneJson.value("beatsPerStep", 0.25));
                        lane.beatDivision = laneJson.value("beatDivision", std::string("16th"));
                        lane.valueMin = laneJson.value("valueMin", 0.0f);
                        lane.valueMax = laneJson.value("valueMax", 1.0f);
                        lane.probabilityEnabled = laneJson.value("probabilityEnabled", true);
                        lane.behavior = laneJson.value("behavior", std::string("Probability"));
                        lane.pianoLowPitch = ofClamp(laneJson.value("pianoLowPitch", 36), 0, 127);
                        lane.pianoHighPitch = ofClamp(laneJson.value("pianoHighPitch", 84), lane.pianoLowPitch, 127);
                        lane.pianoSnapToGrid = laneJson.value("pianoSnapToGrid", true);
                        lane.pianoPitchBindingId = laneJson.value("pianoPitchBindingId", std::string());
                        lane.pianoGateBindingId = laneJson.value("pianoGateBindingId", std::string());
                        lane.pianoVelocityBindingId = laneJson.value("pianoVelocityBindingId", std::string());
                        lane.pianoDefaultVelocity = ofClamp(laneJson.value("pianoDefaultVelocity", 0.8f), 0.0f, 1.0f);
                        lane.pianoMonophonic = laneJson.value("pianoMonophonic", false);
                        lane.curveClamp = laneJson.value("curveClamp", true);
                        lane.curveInterpolation = laneJson.value("curveInterpolation", std::string("Linear"));
                        if(lane.curveInterpolation != "Step" && lane.curveInterpolation != "Linear" &&
                           lane.curveInterpolation != "Log / Exp" && lane.curveInterpolation != "Sigmoid") {
                            lane.curveInterpolation = "Linear";
                        }
                        if(laneJson.contains("bindingIds") && laneJson["bindingIds"].is_array()) {
                            for(const auto& bindingId : laneJson["bindingIds"]) {
                                if(bindingId.is_string()) lane.bindingIds.push_back(bindingId.get<std::string>());
                            }
                        }
                        if(laneJson.contains("step") && laneJson["step"].is_object()) lane.step.fromJson(laneJson["step"]);
                        lane.step.lengthBeats = lane.stepCount * lane.beatsPerStep;
                        if(laneJson.contains("curvePoints") && laneJson["curvePoints"].is_array()) {
                            for(const auto& pointJson : laneJson["curvePoints"]) {
                                if(!pointJson.is_object()) continue;
                                lane.curvePoints.push_back({
                                    pointJson.value("beat", 0.0),
                                    pointJson.value("value", 0.0f)
                                });
                            }
                        }
                        std::sort(lane.curvePoints.begin(), lane.curvePoints.end(), [](const auto& a, const auto& b) {
                            return a.beat < b.beat;
                        });
                        if(laneJson.contains("curveTensions") && laneJson["curveTensions"].is_array()) {
                            for(const auto& tensionJson : laneJson["curveTensions"]) {
                                if(!tensionJson.is_object()) continue;
                                lane.curveTensions.push_back({
                                    ofClamp(tensionJson.value("inflection", 0.5f), 0.01f, 0.99f),
                                    ofClamp(tensionJson.value("steepness", 1.0f), 0.1f, 10.0f)
                                });
                            }
                        }
                        lane.curveTensions.resize(lane.curvePoints.empty() ? 0 : lane.curvePoints.size() - 1);
                        if(laneJson.contains("pianoNotes") && laneJson["pianoNotes"].is_array()) {
                            for(const auto& noteJson : laneJson["pianoNotes"]) {
                                if(!noteJson.is_object()) continue;
                                lane.pianoNotes.push_back({
                                    noteJson.value("startBeat", 0.0),
                                    noteJson.value("durationBeats", 0.25),
                                    noteJson.value("pitch", 60),
                                    noteJson.value("velocity", 1.0f),
                                    noteJson.value("probability", 1.0f)
                                });
                            }
                        }
                        clip.lanes.push_back(std::move(lane));
                    }
                }
                track.clips.push_back(std::move(clip));
            }
        }

        tracks.push_back(std::move(track));
    }

    nextTrackNumber = tracks.size() + 1;
    size_t bindingCount = 0;
    size_t clipCount = 0;
    size_t laneCount = 0;
    for(const auto& track : tracks) {
        bindingCount += track.bindings.size();
        clipCount += track.clips.size();
        for(const auto& clip : track.clips) laneCount += clip.lanes.size();
    }
    nextBindingNumber = bindingCount + 1;
    nextClipNumber = clipCount + 1;
    nextLaneNumber = laneCount + 1;
}

bool ofxOceanodeTimelineManager::savePreset(const std::string& presetFolderPath) const {
    return ofSavePrettyJson(presetFolderPath + "/timeline.json", toJson());
}

bool ofxOceanodeTimelineManager::loadPreset(const std::string& presetFolderPath) {
    const std::string path = presetFolderPath + "/timeline.json";
    if(!ofFile::doesFileExist(path)) {
        clear();
        return false;
    }
    try {
        fromJson(ofLoadJson(path));
        return true;
    } catch(const std::exception& e) {
        ofLogError("ofxOceanodeTimeline") << "Could not load timeline: " << e.what();
        clear();
        return false;
    }
}
