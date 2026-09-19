#include "Timeline/ofxOceanodeTimeline.h"

#include "Managers/ofxOceanodeContainer.h"
#include "ofxOceanodeParameter.h"
#include "Nodes/Default_Nodes/Base/baseOscillator.h"

#include <cmath>
#include <cstring>
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
using ofxOceanodeTimelineCurve::curveSegmentShape;

constexpr double kEpsilon = 1e-9;

double positiveModulo(double value, double length) {
    if(length <= kEpsilon) return 0.0;
    double wrapped = std::fmod(value, length);
    if(wrapped < 0.0) wrapped += length;
    return wrapped;
}

std::string resolveTimelineAudioPath(const std::string& input) {
    if(input.empty()) return {};
    if(ofFile::doesFileExist(input)) return input;
    const std::string dataPath = ofToDataPath(input, true);
    if(ofFile::doesFileExist(dataPath)) return dataPath;
    const std::string samplePath = ofToDataPath("Supercollider/Samples/" + input, true);
    if(ofFile::doesFileExist(samplePath)) return samplePath;
    return input;
}

std::string parameterValueForAutomation(ofxOceanodeAbstractParameter& parameter) {
    const std::string type = parameter.valueType();
    if(type == typeid(float).name()) return ofToString(parameter.cast<float>().getParameter().get());
    if(type == typeid(int).name()) return ofToString(parameter.cast<int>().getParameter().get());
    if(type == typeid(bool).name()) return parameter.cast<bool>().getParameter().get() ? "1" : "0";
    if(type == typeid(std::string).name()) return parameter.cast<std::string>().getParameter().get();

    auto joinValues = [](const auto& values) {
        std::string result;
        for(size_t i = 0; i < values.size(); ++i) {
            if(i > 0) result += ",";
            result += ofToString(values[i]);
        }
        return result;
    };
    if(type == typeid(std::vector<float>).name()) return joinValues(parameter.cast<std::vector<float>>().getParameter().get());
    if(type == typeid(std::vector<int>).name()) return joinValues(parameter.cast<std::vector<int>>().getParameter().get());
    if(type == typeid(std::vector<bool>).name()) return joinValues(parameter.cast<std::vector<bool>>().getParameter().get());
    if(type == typeid(std::vector<std::string>).name()) return joinValues(parameter.cast<std::vector<std::string>>().getParameter().get());

    return parameter.isSerializable() ? parameter.toString() : std::string();
}

std::string combineAutomationValues(const std::vector<std::pair<ofxOceanodeTimelineAutomationMode, std::string>>& values,
                                    const std::string& valueType) {
    if(values.empty()) return std::string();
    const bool isScalarFloat = valueType == typeid(float).name();
    const bool isScalarInt = valueType == typeid(int).name();
    const bool isScalarBool = valueType == typeid(bool).name();
    const bool isVectorFloat = valueType == typeid(std::vector<float>).name();
    const bool isVectorInt = valueType == typeid(std::vector<int>).name();
    const bool isVectorBool = valueType == typeid(std::vector<bool>).name();
    const bool isNumeric = isScalarFloat || isScalarInt || isScalarBool ||
        isVectorFloat || isVectorInt || isVectorBool;
    if(!isNumeric) {
        // Text has no meaningful arithmetic blend. Preserve layer ordering.
        return values.back().second;
    }

    auto parseValue = [](const std::string& value) {
        std::vector<double> result;
        const auto tokens = ofSplitString(value, ",", true, true);
        if(tokens.empty()) result.push_back(ofToDouble(value));
        else for(const auto& token : tokens) result.push_back(ofToDouble(token));
        if(result.empty()) result.push_back(0.0);
        return result;
    };

    std::vector<std::vector<double>> numericValues;
    numericValues.reserve(values.size());
    size_t outputSize = 1;
    for(const auto& value : values) {
        numericValues.push_back(parseValue(value.second));
        if(!isScalarFloat && !isScalarInt && !isScalarBool)
            outputSize = std::max(outputSize, numericValues.back().size());
    }
    auto componentAt = [](const std::vector<double>& value, size_t index) {
        // A scalar broadcasts to every channel. Different non-scalar sizes
        // wrap, matching Oceanode's usual vector-broadcasting behaviour.
        return value[index % value.size()];
    };
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
    std::vector<double> result(outputSize);
    for(size_t component = 0; component < outputSize; ++component)
        result[component] = componentAt(numericValues[static_cast<size_t>(baseIndex)], component);
    for(size_t i = 0; i < values.size(); ++i) {
        if(static_cast<int>(i) == baseIndex) continue;
        // Any OTHER Replace contributor is a superseded plain/overwrite
        // signal -- the one at baseIndex already won that contest above, so
        // an earlier Replace entry must not clobber the base or the
        // modulators folded onto it while iterating past it.
        if(values[i].first == ofxOceanodeTimelineAutomationMode::Replace) continue;
        for(size_t component = 0; component < outputSize; ++component) {
            const double contribution = componentAt(numericValues[i], component);
            switch(values[i].first) {
                case ofxOceanodeTimelineAutomationMode::Add: result[component] += contribution; break;
                case ofxOceanodeTimelineAutomationMode::Multiply: result[component] *= contribution; break;
                case ofxOceanodeTimelineAutomationMode::Min: result[component] = std::min(result[component], contribution); break;
                case ofxOceanodeTimelineAutomationMode::Max: result[component] = std::max(result[component], contribution); break;
                default: break;
            }
        }
    }

    auto componentToString = [&](double value) {
        if(isScalarBool || isVectorBool) return value >= 0.5 ? std::string("1") : std::string("0");
        if(isScalarInt || isVectorInt) return ofToString(static_cast<int>(std::lround(value)));
        return ofToString(static_cast<float>(value));
    };
    if(isScalarFloat || isScalarInt || isScalarBool) return componentToString(result.front());
    std::string combined;
    for(size_t component = 0; component < result.size(); ++component) {
        if(component > 0) combined += ",";
        combined += componentToString(result[component]);
    }
    return combined;
}

std::string sumAutomationValues(const std::vector<std::string>& values,
                                const std::string& valueType) {
    std::vector<std::pair<ofxOceanodeTimelineAutomationMode, std::string>> additions;
    additions.reserve(values.size());
    for(const auto& value : values)
        additions.emplace_back(ofxOceanodeTimelineAutomationMode::Add, value);
    return combineAutomationValues(additions, valueType);
}

// Wave clips are complete, independent audio slices: their whole source range
// [waveSourceStartBeat, waveSourceStartBeat + contentDurationBeats] is always
// mapped linearly over the visible clip. The generic repeat/crop model does not
// apply to them, and contentStretch is only a cached copy of that ratio.
void normalizeWaveClipMapping(ofxOceanodeTimelineClip& clip) {
    clip.repeatContent = false;
    clip.durationBeats = std::max(1.0 / 24.0, clip.durationBeats);
    clip.waveSourceStartBeat = std::max(0.0, clip.waveSourceStartBeat);
    if(clip.waveFileDurationBeats > 0.0) {
        // A slice can never extend past the end of its file; audio would
        // hold the last frame there while the waveform showed a rescaled
        // range. Allow a tiny tolerance for the rounding of split points.
        const double available = std::max(1.0 / 24.0,
            clip.waveFileDurationBeats - clip.waveSourceStartBeat);
        if(clip.contentDurationBeats > available + 1e-6) clip.contentDurationBeats = available;
    }
    clip.contentDurationBeats = std::max(1.0 / 24.0, clip.contentDurationBeats);
    clip.contentStretch = std::max(1.0 / 1024.0, clip.durationBeats / clip.contentDurationBeats);
}

bool clipSourceBeat(const ofxOceanodeTimelineClip& clip, double globalBeat, double& sourceBeat) {
    const double duration = std::max(1.0 / 24.0, clip.durationBeats);
    if(globalBeat < clip.startBeat - kEpsilon || globalBeat >= clip.startBeat + duration - kEpsilon) return false;

    sourceBeat = ofxOceanodeTimelineClipTime::timelineToSourceBeat(clip, globalBeat);
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

float evaluateCurvePoints(const std::vector<ofxOceanodeTimelineCurvePoint>& points,
                          const std::vector<ofxOceanodeTimelineCurveTension>& tensions,
                          const std::string& interpolation, double beat, float fallback) {
    if(points.empty()) return fallback;
    if(points.size() == 1) return points.front().value;
    if(beat <= points.front().beat) return points.front().value;
    if(beat >= points.back().beat) return points.back().value;
    for(size_t i = 1; i < points.size(); ++i) {
        if(beat > points[i].beat) continue;
        const double span = std::max(kEpsilon, points[i].beat - points[i - 1].beat);
        const float t = static_cast<float>(ofClamp((beat - points[i - 1].beat) / span, 0.0, 1.0));
        const auto tension = i - 1 < tensions.size() ? tensions[i - 1] : ofxOceanodeTimelineCurveTension{};
        return ofLerp(points[i - 1].value, points[i].value,
                      curveSegmentShape(t, curveInterpolationMode(interpolation), tension));
    }
    return points.back().value;
}

std::string mapNormalizedLaneValue(const ofxOceanodeTimelineLane& lane,
                                   const ofxOceanodeTimelineParameterBinding& binding,
                                   const std::string& normalizedValue) {
    const bool vectorFloat = binding.valueType == typeid(std::vector<float>).name();
    const bool vectorInt = binding.valueType == typeid(std::vector<int>).name();
    if(lane.type == ofxOceanodeTimelineLaneType::Curve && (vectorFloat || vectorInt)) {
        const auto tokens = ofSplitString(normalizedValue, ",", true, true);
        std::string result;
        for(size_t i = 0; i < tokens.size(); ++i) {
            const float normalized = ofToFloat(tokens[i]);
            float mapped = lane.valueMin + normalized * (lane.valueMax - lane.valueMin);
            if(lane.curveClamp) {
                mapped = ofClamp(mapped, std::min(lane.valueMin, lane.valueMax),
                                 std::max(lane.valueMin, lane.valueMax));
            }
            if(i > 0) result += ",";
            result += vectorInt ? ofToString(static_cast<int>(std::lround(mapped)))
                                : ofToString(mapped);
        }
        return result.empty() ? normalizedValue : result;
    }

    const float normalized = ofToFloat(normalizedValue);
    float mapped = lane.valueMin + normalized * (lane.valueMax - lane.valueMin);
    if(lane.type == ofxOceanodeTimelineLaneType::Curve && lane.curveClamp) {
        mapped = ofClamp(mapped, std::min(lane.valueMin, lane.valueMax),
                         std::max(lane.valueMin, lane.valueMax));
    }
    if(binding.valueType == typeid(float).name()) return ofToString(mapped);
    if(binding.valueType == typeid(int).name()) return ofToString(static_cast<int>(std::lround(mapped)));
    if(binding.valueType == typeid(bool).name()) return normalized >= 0.5f ? "1" : "0";
    return normalizedValue;
}

struct LfoParameterDefinition {
    const char* id;
    float minimum;
    float maximum;
    float defaultValue;
};

constexpr LfoParameterDefinition kLfoParameters[] = {
    {"frequency", 0.125f, 64.0f, 4.0f},
    {"roundness", 0.0f, 1.0f, 0.5f},
    {"skew", -1.0f, 1.0f, 0.0f},
    {"pw", 0.0f, 1.0f, 0.5f},
    {"pow", -1.0f, 1.0f, 0.0f},
    {"bipow", -1.0f, 1.0f, 0.0f},
    {"phaseOffset", 0.0f, 1.0f, 0.0f},
    {"scale", 0.0f, 2.0f, 1.0f},
    {"yOffset", -1.0f, 1.0f, 0.0f}
};

const LfoParameterDefinition* lfoParameterDefinition(const std::string& id) {
    for(const auto& definition : kLfoParameters)
        if(id == definition.id) return &definition;
    return nullptr;
}

float normalizedLfoLaneValue(const ofxOceanodeTimelineLane& lane, double beat,
                             float fallback) {
    return evaluateCurvePoints(lane.curvePoints, lane.curveTensions,
                               lane.curveInterpolation, beat, fallback);
}

} // namespace

namespace ofxOceanodeTimelineLfo {

float evaluateParameter(const ofxOceanodeTimelineClip& clip,
                        const std::string& parameter, double sourceBeat,
                        float fallback) {
    const auto* definition = lfoParameterDefinition(parameter);
    if(definition == nullptr) return fallback;
    for(const auto& lane : clip.lanes) {
        if(lane.lfoParameter != parameter) continue;
        const float defaultNormalized = (definition->defaultValue - definition->minimum) /
            (definition->maximum - definition->minimum);
        const float normalized = normalizedLfoLaneValue(lane, sourceBeat, defaultNormalized);
        return definition->minimum + ofClamp(normalized, 0.0f, 1.0f) *
            (definition->maximum - definition->minimum);
    }
    return fallback;
}

float evaluate(const ofxOceanodeTimelineClip& clip, double sourceBeat) {
    const float frequency = std::max(1.0f / 1024.0f,
        evaluateParameter(clip, "frequency", sourceBeat, 4.0f));
    baseOscillator oscillator;
    oscillator.phaseOffset_Param = evaluateParameter(clip, "phaseOffset", sourceBeat, 0.0f);
    oscillator.pow_Param = evaluateParameter(clip, "pow", sourceBeat, 0.0f);
    oscillator.pulseWidth_Param = evaluateParameter(clip, "pw", sourceBeat, 0.5f);
    oscillator.quant_Param = 0;
    oscillator.scale_Param = evaluateParameter(clip, "scale", sourceBeat, 1.0f);
    oscillator.offset_Param = evaluateParameter(clip, "yOffset", sourceBeat, 0.0f);
    oscillator.randomAdd_Param = 0.0f;
    oscillator.biPow_Param = evaluateParameter(clip, "bipow", sourceBeat, 0.0f);
    oscillator.waveSelect_Param = 0;
    oscillator.amplitude_Param = 1.0f;
    oscillator.invert_Param = 0.0f;
    oscillator.skew_Param = evaluateParameter(clip, "skew", sourceBeat, 0.0f);
    oscillator.roundness_Param = evaluateParameter(clip, "roundness", sourceBeat, 0.5f);
    return oscillator.computeFunc(static_cast<float>(sourceBeat / frequency));
}

} // namespace ofxOceanodeTimelineLfo

namespace {

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

// Clamps a combined automation value back to the target parameter's own
// min/max. Needed because Add/Multiply/Min/Max blending has no natural
// ceiling/floor of its own the way a single Replace binding does -- several
// bindings driving one parameter can sum or multiply past its range even
// though every individual contributor stayed within its lane's own bounds.
// Text and bool values have no meaningful notion of "range" here and pass
// through unchanged.
std::string clampValueToParameterRange(ofxOceanodeAbstractParameter& parameter,
                                       const std::string& value,
                                       const std::string& valueType) {
    auto clampComponent = [](double v, double lo, double hi) {
        if(lo > hi) std::swap(lo, hi); // a misconfigured/inverted range shouldn't force everything to one value
        return ofClamp(v, lo, hi);
    };
    if(valueType == typeid(float).name()) {
        auto& target = parameter.cast<float>().getParameter();
        return ofToString(clampComponent(ofToFloat(value), target.getMin(), target.getMax()));
    }
    if(valueType == typeid(int).name()) {
        auto& target = parameter.cast<int>().getParameter();
        const double clamped = clampComponent(ofToInt(value), target.getMin(), target.getMax());
        return ofToString(static_cast<int>(std::lround(clamped)));
    }
    if(valueType == typeid(std::vector<float>).name() || valueType == typeid(std::vector<int>).name()) {
        const bool isInt = valueType == typeid(std::vector<int>).name();
        std::vector<double> minimum;
        std::vector<double> maximum;
        if(isInt) {
            auto& target = parameter.cast<std::vector<int>>().getParameter();
            for(auto v : target.getMin()) minimum.push_back(v);
            for(auto v : target.getMax()) maximum.push_back(v);
        } else {
            auto& target = parameter.cast<std::vector<float>>().getParameter();
            for(auto v : target.getMin()) minimum.push_back(v);
            for(auto v : target.getMax()) maximum.push_back(v);
        }
        if(minimum.empty() || maximum.empty()) return value;
        const auto tokens = ofSplitString(value, ",", true, true);
        std::string result;
        for(size_t i = 0; i < tokens.size(); ++i) {
            const double clamped = clampComponent(ofToDouble(tokens[i]),
                                                  minimum[i % minimum.size()], maximum[i % maximum.size()]);
            if(i > 0) result += ",";
            result += isInt ? ofToString(static_cast<int>(std::lround(clamped))) : ofToString(static_cast<float>(clamped));
        }
        return result;
    }
    return value;
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

// MultiSlider ("step value"): unlike evaluateStepSequencer above (sparse --
// only fires where a step was explicitly placed, and that step's height is
// a fire *probability*), every cell here always has a value -- the whole
// point is a dense, always-on per-step value grid, like a bar of tiny
// vertical sliders. Always returns a value; there is no "not playing" case
// short of the clip itself not being active (already handled by the
// caller's clipSourceBeat check before this is ever called).
bool evaluateMultiSlider(const ofxOceanodeTimelineLane& lane, double localBeat, std::string& value) {
    const double cellLength = std::max(1.0 / 24.0, lane.beatsPerStep);
    const int stepCount = std::max(1, lane.stepCount);
    const double patternLength = cellLength * stepCount;
    const double patternBeat = positiveModulo(localBeat, patternLength);
    int index = static_cast<int>(std::floor(patternBeat / cellLength));
    index = std::min(std::max(index, 0), stepCount - 1);
    const float raw = index < static_cast<int>(lane.multiSliderValues.size()) ? lane.multiSliderValues[index] : 0.0f;
    value = ofToString(ofClamp(raw, 0.0f, 1.0f));
    return true;
}

// MultiValue / MultiGate: find whichever region in this one row is active
// at localBeat (regions never overlap within a row -- both the old
// standalone nodes and this lane's own controller-side create/move
// interactions prevent that), matching valueTrack.h/gateTrack.h's own
// per-lane region scan.
bool evaluateMultiValueRow(const std::vector<ofxOceanodeTimelineValueRegion>& row, double localBeat, float& value) {
    for(const auto& region : row) {
        if(localBeat >= region.startBeat && localBeat < region.end()) {
            value = region.value;
            return true;
        }
    }
    return false;
}

bool evaluateMultiGateRow(const std::vector<ofxOceanodeTimelineGateRegion>& row, double localBeat) {
    for(const auto& region : row) {
        if(localBeat >= region.startBeat && localBeat < region.end()) return true;
    }
    return false;
}
}

namespace ofxOceanodeTimelineClipTime {

double sourceDuration(const ofxOceanodeTimelineClip& clip) {
    return std::max(1.0 / 24.0, clip.contentDurationBeats);
}

double stretch(const ofxOceanodeTimelineClip& clip) {
    return std::max(1.0 / 1024.0, clip.contentStretch);
}

double cycleDuration(const ofxOceanodeTimelineClip& clip) {
    return sourceDuration(clip) * stretch(clip);
}

double sourceToTimelineBeat(const ofxOceanodeTimelineClip& clip,
                            double sourceBeat, int64_t cycle) {
    const double content = sourceDuration(clip);
    if(clip.repeatContent)
        return clip.startBeat + (static_cast<double>(cycle) * content + sourceBeat) * stretch(clip);
    return clip.startBeat + sourceBeat * clip.durationBeats / content;
}

double timelineToSourceBeat(const ofxOceanodeTimelineClip& clip,
                            double timelineBeat) {
    const double localBeat = std::max(0.0, timelineBeat - clip.startBeat);
    if(clip.repeatContent)
        return positiveModulo(localBeat, cycleDuration(clip)) / stretch(clip);
    return localBeat * sourceDuration(clip) /
        std::max(1.0 / 24.0, clip.durationBeats);
}

int64_t cycleIndex(const ofxOceanodeTimelineClip& clip, double timelineBeat) {
    if(!clip.repeatContent) return 0;
    return static_cast<int64_t>(std::floor(
        std::max(0.0, timelineBeat - clip.startBeat) / cycleDuration(clip)));
}

} // namespace ofxOceanodeTimelineClipTime

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

ofJson ofxOceanodeTimelineStepLane::toJson() const {
    ofJson json;
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
    if(container == owner) return;
    for(const auto& track : tracks) clearTimelineFlag(track);
    container = owner;
    std::set<std::string> parameterPaths;
    for(const auto& track : tracks)
        for(const auto& binding : track.bindings)
            parameterPaths.insert(binding.parameterPath);
    for(const auto& path : parameterPaths) refreshTimelineFlag(path);
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

std::string ofxOceanodeTimelineManager::makeUniqueGroupId() const {
    std::string id;
    uint64_t number = nextGroupNumber;
    do {
        id = makeId("timeline_group", number++);
    } while(std::any_of(clipGroups.begin(), clipGroups.end(), [&](const auto& group) { return group.id == id; }));
    return id;
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

std::string ofxOceanodeTimelineManager::createWaveTrack(const std::string& requestedName) {
    const auto trackId = createTrack(requestedName);
    if(auto* track = getTrack(trackId)) {
        track->isWaveTrack = true;
        track->waveTrackHeight = 96.0f;
        track->waveVolumeAutomationEnabled = true;
        track->waveVolumePoints = {{0.0, track->waveVolume}, {4.0, track->waveVolume}};
        track->waveVolumeTensions.assign(1, ofxOceanodeTimelineCurveTension{});
    }
    return trackId;
}

std::string ofxOceanodeTimelineManager::createWaveVolumeLane(const std::string& trackId,
                                                             const std::string& clipId) {
    const auto* track = getTrack(trackId);
    auto* clip = getClip(trackId, clipId);
    if(track == nullptr || clip == nullptr || !track->isWaveTrack) return {};
    // Compatibility shim for older callers. Volume belongs to the whole
    // Wave Track now, so never create a lane inside this clip.
    auto* editableTrack = getTrack(trackId);
    editableTrack->waveVolumeAutomationEnabled = true;
    if(editableTrack->waveVolumePoints.empty()) {
        const double endBeat = std::max(1.0 / 24.0, clip->startBeat + clip->durationBeats);
        editableTrack->waveVolumePoints = {{0.0, editableTrack->waveVolume},
                                           {endBeat, editableTrack->waveVolume}};
        editableTrack->waveVolumeTensions.assign(1, ofxOceanodeTimelineCurveTension{});
    }
    return "track-volume:" + trackId;
}

float ofxOceanodeTimelineManager::evaluateWaveClipVolume(const std::string& trackId,
                                                         const std::string& clipId,
                                                         double beat) const {
    const auto* track = getTrack(trackId);
    const auto* clip = getClip(trackId, clipId);
    if(track == nullptr || clip == nullptr || !track->isWaveTrack) return 1.0f;
    return evaluateWaveTrackVolume(trackId, beat);
}

float ofxOceanodeTimelineManager::evaluateWaveTrackVolume(const std::string& trackId, double beat) const {
    const auto* track = getTrack(trackId);
    if(track == nullptr || !track->isWaveTrack) return 0.0f;
    if(track->waveVolumePoints.empty()) return ofClamp(track->waveVolume, 0.0f, 4.0f);
    return ofClamp(evaluateCurvePoints(track->waveVolumePoints, track->waveVolumeTensions,
                                       track->waveVolumeInterpolation, beat, track->waveVolume), 0.0f, 4.0f);
}

std::vector<ofxOceanodeTimelineCurvePoint>& ofxOceanodeTimelineManager::getWaveTrackVolumePoints(const std::string& trackId) {
    static std::vector<ofxOceanodeTimelineCurvePoint> empty;
    auto* track = getTrack(trackId);
    return track != nullptr ? track->waveVolumePoints : empty;
}

const std::vector<ofxOceanodeTimelineCurvePoint>& ofxOceanodeTimelineManager::getWaveTrackVolumePoints(const std::string& trackId) const {
    static const std::vector<ofxOceanodeTimelineCurvePoint> empty;
    const auto* track = getTrack(trackId);
    return track != nullptr ? track->waveVolumePoints : empty;
}

bool ofxOceanodeTimelineManager::addWaveTrackVolumePoint(const std::string& trackId, double beat, float value) {
    auto* track = getTrack(trackId);
    if(track == nullptr || !track->isWaveTrack) return false;
    track->waveVolumeAutomationEnabled = true;
    track->waveVolumePoints.push_back({std::max(0.0, beat), ofClamp(value, 0.0f, 4.0f)});
    std::sort(track->waveVolumePoints.begin(), track->waveVolumePoints.end(),
              [](const auto& a, const auto& b) { return a.beat < b.beat; });
    std::vector<ofxOceanodeTimelineCurveTension> tensions;
    if(track->waveVolumePoints.size() > 1)
        tensions.assign(track->waveVolumePoints.size() - 1, ofxOceanodeTimelineCurveTension{});
    track->waveVolumeTensions = std::move(tensions);
    return true;
}

bool ofxOceanodeTimelineManager::removeWaveTrackVolumePoint(const std::string& trackId, double beat, double epsilon) {
    auto* track = getTrack(trackId);
    if(track == nullptr || !track->isWaveTrack) return false;
    const auto oldSize = track->waveVolumePoints.size();
    track->waveVolumePoints.erase(std::remove_if(track->waveVolumePoints.begin(), track->waveVolumePoints.end(),
        [&](const auto& point) { return std::abs(point.beat - beat) <= epsilon; }), track->waveVolumePoints.end());
    track->waveVolumeTensions.resize(track->waveVolumePoints.size() > 1 ? track->waveVolumePoints.size() - 1 : 0);
    return oldSize != track->waveVolumePoints.size();
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
    std::set<std::string> affectedPaths;
    for(const auto& binding : it->bindings) affectedPaths.insert(binding.parameterPath);
    // Every clip on this track is about to disappear along with it -- drop
    // it from any group first so removeClipFromGroups' bookkeeping
    // (dissolving a group that drops below two members) runs the same way
    // it would from removeClip, instead of leaving a group half-full of
    // references to a track that no longer exists. Any Wave lane also
    // needs its audio backend released the same way, or a registered
    // provider would keep a synth/buffer alive for a lane that no longer
    // exists anywhere in this manager.
    for(const auto& clip : it->clips) {
        removeClipFromGroups(trackId, clip.id);
        releaseWaveClipIfNeeded(trackId, clip.id);
    }
    tracks.erase(it);
    for(const auto& path : affectedPaths) refreshTimelineFlag(path);
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
    if(track == nullptr || track->isWaveTrack || container == nullptr || !isStepLaneCompatible(parameter)) return std::string();

    const std::string path = container->getCustomGuiParameterPath(parameter);
    // A parameter may be published to several tracks. Each track owns an
    // independent binding (and blend mode), while duplicate bindings inside
    // one track would only create ambiguous rows and are therefore rejected.
    if(std::any_of(track->bindings.begin(), track->bindings.end(), [&](const auto& existing) {
        return existing.parameterPath == path;
    })) return std::string();

    ofxOceanodeTimelineParameterBinding binding;
    binding.id = makeUniqueBindingId();
    binding.parameterPath = path;
    binding.valueType = parameter.valueType();
    binding.defaultValue = parameterValueForAutomation(parameter);
    binding.mode = mode;
    track->bindings.push_back(binding);

    // No clip or lane is created here, whether this is the track's first
    // binding or its fifth -- a binding just joins the track's pool of
    // automatable parameters. The user creates a clip explicitly (right-click
    // a row -> "New clip here", or the clip-creation dialog) and only then
    // does it get a lane pointed at one of these bindings. Previously the
    // very first binding on a brand-new track got a starter clip/lane
    // automatically, which was inconsistent with every binding added after
    // it (and surprising: it meant the *order* parameters were bound in
    // silently decided whether a clip appeared).
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
    const std::string parameterPath = it->parameterPath;
    for(auto& clip : track->clips) {
        for(auto& lane : clip.lanes) {
            lane.bindingIds.erase(std::remove(lane.bindingIds.begin(), lane.bindingIds.end(), bindingId), lane.bindingIds.end());
            if(lane.pianoPitchBindingId == bindingId) lane.pianoPitchBindingId.clear();
            if(lane.pianoGateBindingId == bindingId) lane.pianoGateBindingId.clear();
            if(lane.pianoVelocityBindingId == bindingId) lane.pianoVelocityBindingId.clear();
        }
    }
    track->bindings.erase(it);
    refreshTimelineFlag(parameterPath);
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

bool ofxOceanodeTimelineManager::setBindingClamp(const std::string& trackId,
                                                 const std::string& bindingId,
                                                 bool clampToParameterRange) {
    auto* binding = getBinding(trackId, bindingId);
    if(binding == nullptr) return false;
    binding->clampToParameterRange = clampToParameterRange;
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

std::string ofxOceanodeTimelineManager::createLfoClip(const std::string& trackId,
                                                      const std::string& outputBindingId,
                                                      const std::string& requestedName,
                                                      double startBeat,
                                                      double durationBeats) {
    const auto clipId = createClip(trackId, requestedName, startBeat, durationBeats);
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return std::string();

    clip->isLfo = true;
    clip->lfoOutputBindingId = outputBindingId;
    if(const auto* binding = getBinding(trackId, outputBindingId)) {
        if(container != nullptr) {
            if(auto* parameter = container->findCustomGuiParameter(binding->parameterPath)) {
                if(binding->valueType == typeid(float).name()) {
                    clip->lfoOutputMin = parameter->cast<float>().getParameter().getMin();
                    clip->lfoOutputMax = parameter->cast<float>().getParameter().getMax();
                } else if(binding->valueType == typeid(int).name()) {
                    clip->lfoOutputMin = static_cast<float>(parameter->cast<int>().getParameter().getMin());
                    clip->lfoOutputMax = static_cast<float>(parameter->cast<int>().getParameter().getMax());
                }
            }
        }
    }

    struct Definition {
        const char* id;
        const char* name;
        float minimum;
        float maximum;
        float defaultValue;
    };
    constexpr Definition definitions[] = {
        {"frequency", "Frequency (beats)", 0.125f, 64.0f, 4.0f},
        {"roundness", "Roundness", 0.0f, 1.0f, 0.5f},
        {"skew", "Skew", -1.0f, 1.0f, 0.0f},
        {"pw", "Pulse width", 0.0f, 1.0f, 0.5f},
        {"pow", "Pow", -1.0f, 1.0f, 0.0f},
        {"bipow", "BiPow", -1.0f, 1.0f, 0.0f},
        {"phaseOffset", "Phase offset", 0.0f, 1.0f, 0.0f},
        {"scale", "Scale", 0.0f, 2.0f, 1.0f},
        {"yOffset", "Y offset", -1.0f, 1.0f, 0.0f}
    };
    for(const auto& definition : definitions) {
        const auto laneId = createLane(trackId, clipId, definition.name,
                                       ofxOceanodeTimelineLaneType::Curve);
        if(auto* lane = getLane(trackId, clipId, laneId)) {
            lane->lfoParameter = definition.id;
            lane->valueMin = definition.minimum;
            lane->valueMax = definition.maximum;
            const float normalized = (definition.defaultValue - definition.minimum) /
                (definition.maximum - definition.minimum);
            lane->curvePoints = {{0.0, normalized},
                                 {clip->contentDurationBeats, normalized}};
            lane->curveTensions.assign(1, ofxOceanodeTimelineCurveTension{});
        }
    }
    return clipId;
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
    if(const auto* clip = getClip(trackId, clipId)) {
        releaseWaveClipIfNeeded(trackId, clip->id);
    }
    const auto oldSize = track->clips.size();
    track->clips.erase(std::remove_if(track->clips.begin(), track->clips.end(), [&](const auto& clip) {
        return clip.id == clipId;
    }), track->clips.end());
    const bool removed = track->clips.size() != oldSize;
    if(removed) removeClipFromGroups(trackId, clipId);
    return removed;
}

void ofxOceanodeTimelineManager::removeClipFromGroups(const std::string& trackId, const std::string& clipId) {
    for(auto& group : clipGroups) {
        group.members.erase(std::remove_if(group.members.begin(), group.members.end(), [&](const auto& member) {
            return member.first == trackId && member.second == clipId;
        }), group.members.end());
    }
    // A "group" of zero or one member is meaningless -- drop it rather than
    // leave an inert record around that the UI's group outline/Ungroup
    // button would otherwise have to special-case.
    clipGroups.erase(std::remove_if(clipGroups.begin(), clipGroups.end(), [](const auto& group) {
        return group.members.size() < 2;
    }), clipGroups.end());
}

std::string ofxOceanodeTimelineManager::groupClips(const std::vector<std::pair<std::string, std::string>>& members) {
    std::vector<std::pair<std::string, std::string>> merged;
    auto addUnique = [&](const std::pair<std::string, std::string>& member) {
        if(std::find(merged.begin(), merged.end(), member) == merged.end()) merged.push_back(member);
    };
    // Re-grouping a mix of already-grouped and ungrouped clips folds
    // everything into one group instead of creating overlapping ones --
    // every group any of the requested clips already belongs to gets
    // dissolved and its members pulled into the new group.
    std::set<std::string> groupIdsToDissolve;
    for(const auto& member : members) {
        if(getClip(member.first, member.second) == nullptr) continue; // stale reference -- skip rather than fail the whole call
        addUnique(member);
        if(const auto* existingGroup = getGroupForClip(member.first, member.second)) {
            groupIdsToDissolve.insert(existingGroup->id);
        }
    }
    for(const auto& group : clipGroups) {
        if(groupIdsToDissolve.count(group.id) == 0) continue;
        for(const auto& member : group.members) {
            // Groups are persisted as references. Do not carry a stale
            // reference into a newly merged group if a clip was deleted by
            // another path before the group metadata was refreshed.
            if(getClip(member.first, member.second) != nullptr) addUnique(member);
        }
    }
    // Do not dissolve an existing group when the requested selection did not
    // produce at least two live clips. In particular, a stale selection must
    // never make a valid group lose its only visible member.
    if(merged.size() < 2) {
        if(groupIdsToDissolve.size() == 1) return *groupIdsToDissolve.begin();
        return std::string();
    }
    clipGroups.erase(std::remove_if(clipGroups.begin(), clipGroups.end(), [&](const auto& group) {
        return groupIdsToDissolve.count(group.id) > 0;
    }), clipGroups.end());

    ofxOceanodeTimelineClipGroup newGroup;
    newGroup.id = makeUniqueGroupId();
    newGroup.members = std::move(merged);
    ++nextGroupNumber;
    clipGroups.push_back(std::move(newGroup));
    return clipGroups.back().id;
}

bool ofxOceanodeTimelineManager::ungroupClip(const std::string& trackId, const std::string& clipId) {
    const auto it = std::find_if(clipGroups.begin(), clipGroups.end(), [&](const auto& group) {
        return std::any_of(group.members.begin(), group.members.end(), [&](const auto& member) {
            return member.first == trackId && member.second == clipId;
        });
    });
    if(it == clipGroups.end()) return false;
    clipGroups.erase(it);
    return true;
}

const ofxOceanodeTimelineClipGroup* ofxOceanodeTimelineManager::getGroupForClip(const std::string& trackId, const std::string& clipId) const {
    for(const auto& group : clipGroups) {
        for(const auto& member : group.members) {
            if(member.first == trackId && member.second == clipId) return &group;
        }
    }
    return nullptr;
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
    if(type == ofxOceanodeTimelineLaneType::Wave) return std::string();
    ofxOceanodeTimelineLane lane;
    lane.id = makeUniqueLaneId();
    lane.name = requestedName.empty() ? "Lane" : requestedName;
    lane.type = type;
    lane.beatsPerStep = 0.25;
    lane.stepCount = std::max(1, static_cast<int>(std::llround(clip->contentDurationBeats / lane.beatsPerStep)));
    if(type == ofxOceanodeTimelineLaneType::Curve) {
        lane.curvePoints = {{0.0, 0.0f}, {clip->contentDurationBeats, 1.0f}};
        lane.curveTensions.push_back({0.5f, 1.0f});
    } else if(type == ofxOceanodeTimelineLaneType::MultiValue) {
        lane.multiValueRows.assign(std::max(1, lane.multiRowCount), {});
    } else if(type == ofxOceanodeTimelineLaneType::MultiGate) {
        lane.multiGateRows.assign(std::max(1, lane.multiRowCount), {});
    } else if(type == ofxOceanodeTimelineLaneType::MultiSlider) {
        lane.multiSliderValues.assign(lane.stepCount, 0.0f);
    }
    ++nextLaneNumber;
    clip->lanes.push_back(std::move(lane));
    return clip->lanes.back().id;
}

bool ofxOceanodeTimelineManager::removeLane(const std::string& trackId, const std::string& clipId,
                                            const std::string& laneId) {
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return false;
    if(const auto* lane = getLane(trackId, clipId, laneId)) {
    if(lane->type == ofxOceanodeTimelineLaneType::Wave) releaseWaveClipIfNeeded(trackId, clipId);
    }
    const auto oldSize = clip->lanes.size();
    clip->lanes.erase(std::remove_if(clip->lanes.begin(), clip->lanes.end(), [&](const auto& lane) {
        return lane.id == laneId;
    }), clip->lanes.end());
    return clip->lanes.size() != oldSize;
}

void ofxOceanodeTimelineManager::releaseWaveClipIfNeeded(const std::string& trackId, const std::string& clipId) {
    if(waveAudioProvider != nullptr) waveAudioProvider->releaseWaveClip(trackId, clipId);
}

bool ofxOceanodeTimelineManager::setLaneMultiRowCount(const std::string& trackId, const std::string& clipId,
                                                      const std::string& laneId, int rowCount) {
    auto* lane = getLane(trackId, clipId, laneId);
    if(lane == nullptr) return false;
    rowCount = ofClamp(rowCount, 1, 16);
    lane->multiRowCount = rowCount;
    // Grow/shrink in place so existing row data survives a resize -- only
    // the vector actually used by this lane's current type; the other one
    // is left alone (and picks up the new count next time the lane is
    // switched to that type, via setClipLaneType's "only if still empty"
    // initialization above).
    if(lane->type == ofxOceanodeTimelineLaneType::MultiValue) lane->multiValueRows.resize(rowCount);
    else if(lane->type == ofxOceanodeTimelineLaneType::MultiGate) lane->multiGateRows.resize(rowCount);
    return true;
}

bool ofxOceanodeTimelineManager::reloadWaveform(const std::string& trackId, const std::string& clipId,
                                                const std::string& laneId) {
    auto* lane = getLane(trackId, clipId, laneId);
    if(lane == nullptr) return false;
    lane->waveNumChannels = 0;
    lane->waveFileDurationMs = 0.0;
    lane->waveformPeaks.clear();
    if(lane->waveFilePath.empty()) return true;

    const std::string resolvedPath = resolveTimelineAudioPath(lane->waveFilePath);
    if(resolvedPath != lane->waveFilePath && ofFile::doesFileExist(resolvedPath))
        lane->waveFilePath = resolvedPath;

    ofFile file(lane->waveFilePath, ofFile::ReadOnly, true);
    if(!file.is_open()) {
        ofLogWarning("ofxOceanodeTimeline") << "Could not open wave file: " << lane->waveFilePath;
        return false;
    }

    // Minimal RIFF/WAVE reader -- same approach and same limitations
    // (PCM 16/24/32-bit only, first fmt/data chunks used) as the old
    // standalone Wave Track node's getFileInfo/loadWaveformCache
    // (ofxOceanodeSuperCollider/waveTrack.h), which this mirrors closely
    // enough to keep the two waveform caches visually identical.
    char riff[4], wave[4];
    uint32_t riffSize = 0;
    if(!file.read((char*)&riff, 4) || !file.read((char*)&riffSize, 4) || !file.read((char*)&wave, 4)) {
        ofLogWarning("ofxOceanodeTimeline") << "Invalid or truncated wave header: " << lane->waveFilePath;
        file.close();
        return false;
    }
    if(std::strncmp(riff, "RIFF", 4) != 0 || std::strncmp(wave, "WAVE", 4) != 0) {
        ofLogWarning("ofxOceanodeTimeline") << "Selected file is not a RIFF/WAVE file: " << lane->waveFilePath;
        file.close();
        return false;
    }

    int numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    uint16_t formatType = 0;
    uint32_t dataSize = 0;
    bool haveFmt = false, haveData = false;
    std::streamoff dataStart = 0;

    while(true) {
        char chunkId[4];
        uint32_t chunkSize = 0;
        if(!file.read((char*)&chunkId, 4)) break;
        if(!file.read((char*)&chunkSize, 4)) break;

        if(std::strncmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16) {
            uint16_t channels = 0, bits = 0;
            uint32_t srate = 0, byteRate = 0;
            uint16_t blockAlign = 0;
            file.read((char*)&formatType, 2);
            file.read((char*)&channels, 2);
            file.read((char*)&srate, 4);
            file.read((char*)&byteRate, 4);
            file.read((char*)&blockAlign, 2);
            file.read((char*)&bits, 2);
            numChannels = channels;
            sampleRate = srate;
            bitsPerSample = bits;
            haveFmt = true;
            if(chunkSize > 16) {
                const uint32_t extraBytes = chunkSize - 16;
                // WAVE_FORMAT_EXTENSIBLE wraps the real PCM/float subtype
                // in a 40-byte fmt chunk. Recover that subtype so a 32-bit
                // PCM file is not interpreted as IEEE float samples.
                if(formatType == 0xFFFE && extraBytes >= 24) {
                    uint16_t extensionSize = 0, validBits = 0, subtype = 0;
                    uint32_t channelMask = 0;
                    file.read((char*)&extensionSize, 2);
                    file.read((char*)&validBits, 2);
                    file.read((char*)&channelMask, 4);
                    file.read((char*)&subtype, 2);
                    formatType = subtype;
                    if(extraBytes > 10) file.seekg(extraBytes - 10, std::ios::cur);
                } else {
                    file.seekg(extraBytes, std::ios::cur);
                }
            }
        } else if(std::strncmp(chunkId, "data", 4) == 0) {
            dataSize = chunkSize;
            dataStart = file.tellg();
            haveData = true;
            break;
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
        if(chunkSize & 1u) file.seekg(1, std::ios::cur);
    }

    if(!haveFmt || !haveData || sampleRate == 0 || numChannels <= 0 || numChannels > 16 || bitsPerSample == 0) {
        ofLogWarning("ofxOceanodeTimeline") << "Wave file has no usable fmt/data chunks: " << lane->waveFilePath;
        file.close();
        return false;
    }

    const int bytesPerSample = bitsPerSample / 8;
    if((bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) ||
       bytesPerSample <= 0) {
        ofLogWarning("ofxOceanodeTimeline") << "Unsupported wave bit depth (" << bitsPerSample
            << ") in " << lane->waveFilePath;
        file.close();
        return false;
    }
    const int totalFrames = bytesPerSample > 0 ? static_cast<int>(dataSize / (bytesPerSample * numChannels)) : 0;
    lane->waveFileDurationMs = totalFrames > 0
        ? static_cast<double>(totalFrames) / sampleRate * 1000.0 : 0.0;
    lane->waveNumChannels = numChannels;

    constexpr int kPointsPerChannel = 2000;
    const int framesPerPoint = std::max(1, totalFrames / kPointsPerChannel);
    lane->waveformPeaks.assign(static_cast<size_t>(numChannels) * kPointsPerChannel, 0.0f);

    file.seekg(dataStart, std::ios::beg);
    std::vector<char> frameBuf(static_cast<size_t>(bytesPerSample) * numChannels);
    for(int pt = 0; pt < kPointsPerChannel; ++pt) {
        const int startFrame = pt * framesPerPoint;
        const int endFrame = std::min(startFrame + framesPerPoint, totalFrames);
        std::vector<float> minVals(numChannels, 1.0f);
        std::vector<float> maxVals(numChannels, -1.0f);
        for(int f = startFrame; f < endFrame; ++f) {
            if(!file.read(frameBuf.data(), frameBuf.size())) break;
            for(int ch = 0; ch < numChannels; ++ch) {
                float sample = 0.0f;
                const size_t offset = static_cast<size_t>(ch) * bytesPerSample;
                if(bitsPerSample == 8) {
                    const auto raw = static_cast<unsigned char>(frameBuf[offset]);
                    sample = (static_cast<float>(raw) - 128.0f) / 128.0f;
                } else if(bitsPerSample == 16) {
                    int16_t raw = 0;
                    std::memcpy(&raw, frameBuf.data() + offset, 2);
                    sample = raw / 32768.0f;
                } else if(bitsPerSample == 24) {
                    int32_t raw = 0;
                    std::memcpy(&raw, frameBuf.data() + offset, 3);
                    if(raw & 0x800000) raw |= (int32_t)0xFF000000;
                    sample = raw / 8388608.0f;
                } else if(bitsPerSample == 32) {
                    if(formatType == 1) {
                        int32_t raw = 0;
                        std::memcpy(&raw, frameBuf.data() + offset, 4);
                        sample = raw / 2147483648.0f;
                    } else {
                        std::memcpy(&sample, frameBuf.data() + offset, 4);
                    }
                }
                if(sample < minVals[ch]) minVals[ch] = sample;
                if(sample > maxVals[ch]) maxVals[ch] = sample;
            }
        }
        for(int ch = 0; ch < numChannels; ++ch) {
            const float peak = std::abs(maxVals[ch]) >= std::abs(minVals[ch]) ? maxVals[ch] : minVals[ch];
            lane->waveformPeaks[static_cast<size_t>(ch) * kPointsPerChannel + pt] = peak;
        }
    }
    file.close();
    return true;
}

bool ofxOceanodeTimelineManager::reloadWaveform(const std::string& trackId, const std::string& clipId) {
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return false;
    // Reuse the established RIFF reader/cache builder while keeping the new
    // track-level data model independent of the legacy Wave-lane fields.
    ofxOceanodeTimelineLane cacheLane;
    cacheLane.id = "__wave_clip_cache__";
    cacheLane.type = ofxOceanodeTimelineLaneType::Wave;
    cacheLane.waveFilePath = clip->waveFilePath;
    clip->lanes.push_back(cacheLane);
    const bool result = reloadWaveform(trackId, clipId, cacheLane.id);
    const auto& built = clip->lanes.back();
    if(built.waveFilePath != clip->waveFilePath && ofFile::doesFileExist(built.waveFilePath))
        clip->waveFilePath = built.waveFilePath;
    clip->waveNumChannels = built.waveNumChannels;
    clip->waveFileDurationMs = built.waveFileDurationMs;
    clip->waveformPeaks = built.waveformPeaks;
    clip->lanes.pop_back();
    return result;
}

bool ofxOceanodeTimelineManager::addBindingToLane(const std::string& trackId, const std::string& clipId,
                                                  const std::string& laneId, const std::string& bindingId) {
    auto* track = getTrack(trackId);
    auto* lane = getLane(trackId, clipId, laneId);
    if(track == nullptr || lane == nullptr || getBinding(trackId, bindingId) == nullptr) return false;
    if(std::find(lane->bindingIds.begin(), lane->bindingIds.end(), bindingId) != lane->bindingIds.end()) return true;
    lane->bindingIds.push_back(bindingId);
    if(lane->type == ofxOceanodeTimelineLaneType::PianoRoll) {
        // Only auto-assign the next open role slot when this binding hasn't
        // already been explicitly placed into one. The per-role UI combo
        // (Pitch/Gate/Velocity) sets its own role field itself BEFORE
        // calling this function, purely to make sure the binding also ends
        // up in the generic bindingIds list -- without this guard, e.g.
        // assigning "Gate" while Pitch was still empty would silently ALSO
        // assign that same binding to Pitch (the next empty slot found
        // below), corrupting the automation with a stray extra
        // contribution (the note's raw pitch number) any time a note
        // played. Only a fresh lane with no role set at all should get the
        // Pitch-then-Gate-then-Velocity auto-fill.
        const bool alreadyRoled = lane->pianoPitchBindingId == bindingId ||
            lane->pianoGateBindingId == bindingId || lane->pianoVelocityBindingId == bindingId;
        if(!alreadyRoled) {
            if(lane->pianoPitchBindingId.empty()) lane->pianoPitchBindingId = bindingId;
            else if(lane->pianoGateBindingId.empty()) lane->pianoGateBindingId = bindingId;
            else if(lane->pianoVelocityBindingId.empty()) lane->pianoVelocityBindingId = bindingId;
        }
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
    const auto previousType = lane->type;
    lane->type = type;
    if(previousType == ofxOceanodeTimelineLaneType::Wave && type != ofxOceanodeTimelineLaneType::Wave)
        releaseWaveClipIfNeeded(trackId, clipId);
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
    } else if(type == ofxOceanodeTimelineLaneType::MultiValue) {
        if(lane->multiValueRows.empty()) lane->multiValueRows.assign(std::max(1, lane->multiRowCount), {});
    } else if(type == ofxOceanodeTimelineLaneType::MultiGate) {
        if(lane->multiGateRows.empty()) lane->multiGateRows.assign(std::max(1, lane->multiRowCount), {});
    } else if(type == ofxOceanodeTimelineLaneType::MultiSlider) {
        if(lane->multiSliderValues.empty()) lane->multiSliderValues.assign(std::max(1, lane->stepCount), 0.0f);
    }
    return true;
}

bool ofxOceanodeTimelineManager::setClipTiming(const std::string& trackId, const std::string& clipId,
                                               double startBeat, double durationBeats) {
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return false;
    clip->startBeat = std::max(0.0, startBeat);
    clip->durationBeats = std::max(1.0 / 24.0, durationBeats);
    // Resizing a Wave clip stretches its complete source over the new length.
    const auto* track = getTrack(trackId);
    if(track != nullptr && track->isWaveTrack) normalizeWaveClipMapping(*clip);
    return true;
}

bool ofxOceanodeTimelineManager::setClipContentDuration(const std::string& trackId, const std::string& clipId,
                                                         double contentDurationBeats, bool repeatContent) {
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return false;
    clip->contentDurationBeats = std::max(1.0 / 24.0, contentDurationBeats);
    clip->repeatContent = repeatContent;
    const auto* track = getTrack(trackId);
    if(track != nullptr && track->isWaveTrack) {
        // Never allow a generic repeat/crop edit to turn a Wave slice back
        // into a repeating pattern clip.
        normalizeWaveClipMapping(*clip);
        return true;
    }
    // A non-repeating clip always maps its complete source duration over its
    // visible duration. Mirror that effective ratio here so switching it to
    // repetition later preserves the exact stretch the user was seeing.
    if(!repeatContent) {
        clip->contentStretch = std::max(1.0 / 1024.0,
            clip->durationBeats / clip->contentDurationBeats);
    }
    return true;
}

bool ofxOceanodeTimelineManager::consolidateClipContent(const std::string& trackId,
                                                        const std::string& clipId) {
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return false;

    const double oldSourceDuration = ofxOceanodeTimelineClipTime::sourceDuration(*clip);
    const bool croppedRepeat = clip->repeatContent &&
        clip->durationBeats < ofxOceanodeTimelineClipTime::cycleDuration(*clip) - kEpsilon;
    const double visibleSourceDuration = croppedRepeat
        ? clip->durationBeats / ofxOceanodeTimelineClipTime::stretch(*clip)
        : oldSourceDuration;
    const double contentEnd = std::max(1.0 / 24.0, visibleSourceDuration);
    bool changed = false;

    for(auto& lane : clip->lanes) {
        if(lane.type == ofxOceanodeTimelineLaneType::Step) {
            const auto oldSize = lane.step.steps.size();
            lane.step.steps.erase(std::remove_if(lane.step.steps.begin(), lane.step.steps.end(),
                [&](const auto& step) { return step.startBeat >= contentEnd - kEpsilon; }),
                lane.step.steps.end());
            changed |= lane.step.steps.size() != oldSize;
            for(auto& step : lane.step.steps) {
                if(step.durationBeats > kEpsilon && step.startBeat + step.durationBeats > contentEnd) {
                    step.durationBeats = std::max(0.0, contentEnd - step.startBeat);
                    changed = true;
                }
            }
            const double stepLength = std::max(1.0 / 24.0, lane.beatsPerStep);
            const int consolidatedSteps = std::max(1, static_cast<int>(std::ceil(contentEnd / stepLength)));
            if(lane.stepCount > consolidatedSteps) {
                lane.stepCount = consolidatedSteps;
                changed = true;
            }
        } else if(lane.type == ofxOceanodeTimelineLaneType::PianoRoll) {
            const auto oldSize = lane.pianoNotes.size();
            lane.pianoNotes.erase(std::remove_if(lane.pianoNotes.begin(), lane.pianoNotes.end(),
                [&](const auto& note) { return note.startBeat >= contentEnd - kEpsilon; }),
                lane.pianoNotes.end());
            changed |= lane.pianoNotes.size() != oldSize;
            for(auto& note : lane.pianoNotes) {
                const double maximumDuration = std::max(kEpsilon, contentEnd - note.startBeat);
                if(note.durationBeats > maximumDuration) {
                    note.durationBeats = maximumDuration;
                    changed = true;
                }
            }
        } else {
            std::sort(lane.curvePoints.begin(), lane.curvePoints.end(),
                [](const auto& a, const auto& b) { return a.beat < b.beat; });
            std::string boundaryValue;
            const bool hasBoundaryValue = evaluateCurve(lane, contentEnd, boundaryValue);
            const auto firstHidden = std::upper_bound(lane.curvePoints.begin(), lane.curvePoints.end(), contentEnd + kEpsilon,
                [](double beat, const auto& point) { return beat < point.beat; });
            const bool removedPoints = firstHidden != lane.curvePoints.end();
            if(removedPoints) {
                lane.curvePoints.erase(firstHidden, lane.curvePoints.end());
                changed = true;
            }
            if(removedPoints && hasBoundaryValue &&
               (lane.curvePoints.empty() || lane.curvePoints.back().beat < contentEnd - kEpsilon)) {
                if(lane.curvePoints.empty() && contentEnd > kEpsilon)
                    lane.curvePoints.push_back({0.0, ofToFloat(boundaryValue)});
                lane.curvePoints.push_back({contentEnd, ofToFloat(boundaryValue)});
            }
            lane.curveTensions.resize(lane.curvePoints.empty() ? 0 : lane.curvePoints.size() - 1);
        }
    }

    if(croppedRepeat || std::abs(clip->contentDurationBeats - contentEnd) > kEpsilon) {
        clip->contentDurationBeats = contentEnd;
        if(croppedRepeat) clip->repeatContent = false;
        clip->contentStretch = std::max(1.0 / 1024.0,
            clip->durationBeats / clip->contentDurationBeats);
        changed = true;
    }
    return changed;
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
    hasEvaluatedTransportBeat = false;
    // Events past the old boundary belong to a loop that no longer exists.
    invalidateSchedule();
}

void ofxOceanodeTimelineManager::update() {
    evaluateAutomation();
    applyAutomation();
}

// Evaluates every binding of every track at one beat, without touching any
// parameter. evaluateAutomation() calls it for the current playhead;
// runScheduler() calls it for beats that have not been reached yet, which is
// what lets discrete events be handed to a timestamping backend early without
// a second, divergent implementation of the lane semantics.
void ofxOceanodeTimelineManager::collectActiveValues(double beatPosition, bool isPlaying,
                                                     bool applyPianoRanges,
                                                     ActiveValueMap& activeValues) const {
    if(container == nullptr) return;
    struct PianoPitchRange {
        std::string valueType;
        int low = 127;
        int high = 0;
    };
    using BindingKey = std::pair<std::string, std::string>;
    activeValues.clear();
    using LaneContributions = std::vector<std::pair<ofxOceanodeTimelineAutomationMode, std::string>>;
    std::map<BindingKey, std::map<std::string, LaneContributions>> bindingClipValues;
    std::set<BindingKey> zeroWhenInactiveBindings;
    std::map<std::string, PianoPitchRange> pianoPitchRanges;
    for(const auto& track : tracks) {
        for(const auto& clip : track.clips) {
            for(const auto& lane : clip.lanes) {
                if(lane.type != ofxOceanodeTimelineLaneType::PianoRoll) continue;
                const std::string& pitchId = lane.pianoPitchBindingId;
                const std::string& gateId = lane.pianoGateBindingId;
                const std::string& velocityId = lane.pianoVelocityBindingId;
                if(const auto* binding = getBinding(track.id, pitchId)) {
                    auto& range = pianoPitchRanges[binding->parameterPath];
                    range.valueType = binding->valueType;
                    const int laneLowPitch = static_cast<int>(ofClamp(lane.pianoLowPitch, 0, 127));
                    const int laneHighPitch = static_cast<int>(ofClamp(lane.pianoHighPitch, 0, 127));
                    if(laneLowPitch < range.low) range.low = laneLowPitch;
                    if(laneHighPitch > range.high) range.high = laneHighPitch;
                }
                if(getBinding(track.id, gateId) != nullptr)
                    zeroWhenInactiveBindings.emplace(track.id, gateId);
                if(getBinding(track.id, velocityId) != nullptr)
                    zeroWhenInactiveBindings.emplace(track.id, velocityId);
            }
        }
    }
    if(applyPianoRanges) {
        for(const auto& entry : pianoPitchRanges) {
            if(auto* parameter = container->findCustomGuiParameter(entry.first)) {
                applyPianoPitchRange(*parameter, entry.second.valueType, entry.second.low, entry.second.high);
            }
        }
    }
    for(const auto& track : tracks) {
        for(const auto& clip : track.clips) {
            double sourceBeat = 0.0;
            if(!clipSourceBeat(clip, beatPosition, sourceBeat)) continue;
            if(clip.isLfo) {
                const auto* binding = getBinding(track.id, clip.lfoOutputBindingId);
                if(binding != nullptr && !binding->bypass) {
                    const float normalized = ofxOceanodeTimelineLfo::evaluate(clip, sourceBeat);
                    const float mapped = clip.lfoOutputMin + normalized *
                        (clip.lfoOutputMax - clip.lfoOutputMin);
                    std::string value = ofToString(mapped);
                    if(binding->valueType == typeid(int).name())
                        value = ofToString(static_cast<int>(std::lround(mapped)));
                    bindingClipValues[{track.id, binding->id}][clip.id].emplace_back(
                        binding->mode, std::move(value));
                }
                continue;
            }
            for(const auto& lane : clip.lanes) {
                if(lane.type == ofxOceanodeTimelineLaneType::Wave) {
                    // Real audio, not automation -- never touches a
                    // binding/parameter at all. Handled by the dedicated
                    // wave-audio-provider pass below instead.
                    continue;
                } else if(lane.type == ofxOceanodeTimelineLaneType::Step ||
                          lane.type == ofxOceanodeTimelineLaneType::Curve ||
                          lane.type == ofxOceanodeTimelineLaneType::MultiSlider) {
                    std::string value;
                    const bool hasValue = lane.type == ofxOceanodeTimelineLaneType::Step
                        ? evaluateStepSequencer(lane, sourceBeat, value)
                        : lane.type == ofxOceanodeTimelineLaneType::MultiSlider
                        ? evaluateMultiSlider(lane, sourceBeat, value)
                        : evaluateCurve(lane, sourceBeat, value);
                    if(!hasValue && lane.type != ofxOceanodeTimelineLaneType::Step) continue;
                    if(!hasValue) value = "0";
                    for(const auto& bindingId : lane.bindingIds) {
                        if(const auto* binding = getBinding(track.id, bindingId)) {
                            // A bypassed binding shouldn't contribute to a
                            // shared parameter's combined value at all.
                            if(binding->bypass) continue;
                            bindingClipValues[{track.id, binding->id}][clip.id].emplace_back(
                                binding->mode, mapNormalizedLaneValue(lane, *binding, value));
                        }
                    }
                } else if(lane.type == ofxOceanodeTimelineLaneType::MultiValue ||
                          lane.type == ofxOceanodeTimelineLaneType::MultiGate) {
                    // Unlike Step/Curve/PianoRoll (one lane -> one scalar
                    // stream, optionally broadcast to a vector target),
                    // these are inherently multi-component: row i always
                    // feeds vector component i of whatever this lane's
                    // bindings point at, joined into one comma-separated
                    // value exactly like the piano roll's own vector-target
                    // path below already does for pitch/gate/velocity.
                    const int rowCount = std::max(1, lane.multiRowCount);
                    std::vector<std::string> components;
                    components.reserve(rowCount);
                    for(int row = 0; row < rowCount; ++row) {
                        if(lane.type == ofxOceanodeTimelineLaneType::MultiValue) {
                            float rowValue = 0.0f;
                            const bool active = row < static_cast<int>(lane.multiValueRows.size()) &&
                                evaluateMultiValueRow(lane.multiValueRows[row], sourceBeat, rowValue);
                            components.push_back(active ? ofToString(rowValue) : "0");
                        } else {
                            const bool active = row < static_cast<int>(lane.multiGateRows.size()) &&
                                evaluateMultiGateRow(lane.multiGateRows[row], sourceBeat);
                            components.push_back(active ? "1" : "0");
                        }
                    }
                    const std::string value = joinAutomationValues(components);
                    for(const auto& bindingId : lane.bindingIds) {
                        if(const auto* binding = getBinding(track.id, bindingId)) {
                            if(binding->bypass) continue;
                            bindingClipValues[{track.id, binding->id}][clip.id].emplace_back(binding->mode, value);
                        }
                    }
                } else {
                    std::vector<const ofxOceanodeTimelinePianoNote*> activeNotes;
                    const double pianoCycle = static_cast<double>(
                        ofxOceanodeTimelineClipTime::cycleIndex(clip, beatPosition));
                    for(const auto& note : lane.pianoNotes) {
                        if(isPlaying && sourceBeat >= note.startBeat &&
                           sourceBeat < note.startBeat + std::max(1.0 / 24.0, note.durationBeats) &&
                           pianoProbabilityPasses(note, pianoCycle)) {
                            activeNotes.push_back(&note);
                        }
                    }
                    const std::string roleBindingIds[] = {
                        lane.pianoPitchBindingId,
                        lane.pianoGateBindingId,
                        lane.pianoVelocityBindingId
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
                                bindingClipValues[{track.id, binding->id}][clip.id].emplace_back(
                                    binding->mode, "0");
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
                            bindingClipValues[{track.id, binding->id}][clip.id].emplace_back(
                                binding->mode, std::move(value));
                        }
                    }
                }
            }
        }
    }

    // Lanes inside one clip use the binding's blend mode (for example, a
    // Multiply binding shared by a Curve and a piano Gate evaluates as
    // curve * gate). Independent overlapping clips are additive layers, so
    // their already-composited values are summed afterwards. Finally, expose
    // one contribution per binding for cross-track/cross-binding blending.
    for(const auto& track : tracks) {
        for(const auto& binding : track.bindings) {
            if(binding.bypass) continue;
            const BindingKey key{track.id, binding.id};
            const auto clipsIt = bindingClipValues.find(key);
            std::string value;
            if(binding.hasLiveOverride) value = binding.liveOverrideValue;
            else if(clipsIt != bindingClipValues.end()) {
                std::vector<std::string> clipValues;
                clipValues.reserve(clipsIt->second.size());
                for(const auto& clipEntry : clipsIt->second) {
                    const std::string clipValue = combineAutomationValues(
                        clipEntry.second, binding.valueType);
                    if(!clipValue.empty()) clipValues.push_back(clipValue);
                }
                value = sumAutomationValues(clipValues, binding.valueType);
            }
            else if(zeroWhenInactiveBindings.count(key) > 0) value = "0";
            else continue;
            activeValues[binding.parameterPath].emplace_back(binding.mode, std::move(value));
        }
    }

}

void ofxOceanodeTimelineManager::evaluateAutomation() {
    if(container == nullptr) return;
    loopWrappedThisFrame = false;
    auto transport = container->getTransportState();
    bool loopWrappedThisFrame = false;

    const bool crossedLoopEnd = !hasEvaluatedTransportBeat ||
        lastEvaluatedTransportBeat < loopEndBeat - kEpsilon;
    if(loopEnabled && transport.isPlaying && loopEndBeat > loopStartBeat + kEpsilon &&
       transport.beatPosition >= loopEndBeat && crossedLoopEnd) {
        // A loop boundary is a transport discontinuity, not a modulo
        // operation on the overshoot of the GUI frame.  Preserving that
        // overshoot makes the audio provider restart at a different source
        // position on every pass (and can turn a one-beat loop into the next
        // hit of a rhythm).  Seek to the exact loop start; the audio provider
        // receives forceTransportSync below and resets its voice there too.
        const double wrappedBeat = loopStartBeat;
        if(auto ownerTransport = container->getTransport()) ownerTransport->seekToBeat(wrappedBeat);
        transport = container->getTransportState();
        loopWrappedThisFrame = true;
    }
    lastEvaluatedTransportBeat = transport.beatPosition;
    hasEvaluatedTransportBeat = true;

    if(bpmAutomationEnabled) {
        const float automatedBpm = evaluateBpm(transport.beatPosition, transport.bpm);
        if(std::abs(automatedBpm - transport.bpm) > 0.001f) {
            container->setBpm(automatedBpm);
            transport.bpm = automatedBpm;
        }
    }

    collectActiveValues(transport.beatPosition, transport.isPlaying, true, activeAutomationValues);

    // Hand the discrete events of the coming window to any backend that can
    // execute them at an exact instant. Everything below still runs exactly
    // as before for this frame's own position.
    runScheduler(transport, loopWrappedThisFrame);

    // Wave tracks never touch the automation binding map. They are reported
    // through the optional audio backend as clip sources instead.
    if(waveAudioProvider != nullptr) {
        for(const auto& track : tracks) {
            if(!track.isWaveTrack) continue;
            for(const auto& clip : track.clips) {
                // Wave clips always map their complete source linearly over
                // the visible clip, independent of repeatContent and of the
                // cached contentStretch. Position and rate are derived from
                // the same ratio so the synth's audio-rate read head reaches
                // the slice end exactly when the playhead reaches the clip end.
                const double visibleDuration = std::max(1.0 / 24.0, clip.durationBeats);
                const double sourceDuration = std::max(1.0 / 24.0, clip.contentDurationBeats);
                const double sourcePerTimelineBeat = sourceDuration / visibleDuration;
                const double localBeat = transport.beatPosition - clip.startBeat;
                const bool active = localBeat >= -kEpsilon && localBeat < visibleDuration - kEpsilon;
                const double waveSourceBeat = active
                    ? ofClamp(localBeat * sourcePerTimelineBeat, 0.0, sourceDuration) : 0.0;
                const float volume = ofClamp(evaluateWaveClipVolume(track.id, clip.id,
                                                                     transport.beatPosition), 0.0f, 4.0f);
                // wavePlaybackRate is a user multiplier; the stretch ratio is
                // applied here once and nowhere else.
                const float clipPlaybackRate = ofClamp(
                    static_cast<float>(clip.wavePlaybackRate * sourcePerTimelineBeat),
                    0.01f, 16.0f);
                waveAudioProvider->updateWaveClip(track.id, clip.id, clip.waveFilePath, clip.waveGain,
                                                  volume, clip.waveNumChannels, waveSourceBeat,
                                                  clip.contentDurationBeats,
                                                  clip.waveSourceStartBeat,
                                                  std::max(1.0 / 24.0,
                                                           clip.waveFileDurationBeats > 0.0
                                                               ? clip.waveFileDurationBeats
                                                               : clip.waveFileDurationMs * transport.bpm / 60000.0),
                                                  clipPlaybackRate, clip.waveReverse, transport.bpm,
                                                  transport.isPlaying, active,
                                                  loopWrappedThisFrame);
            }
        }
    }
}

// Resolves the value one parameter should hold, given every binding that
// points at it and a set of evaluated lane contributions. Shared by
// applyAutomation() (this frame's value) and by the scheduler (the value at a
// beat that has not been reached yet) so the two can never disagree.
bool ofxOceanodeTimelineManager::computeParameterValue(
        const std::vector<const ofxOceanodeTimelineParameterBinding*>& bindings,
        const ActiveValueMap& activeValues,
        ofxOceanodeAbstractParameter* parameter,
        std::string& outValue) const {
    if(parameter == nullptr) return false;
    const ofxOceanodeTimelineParameterBinding* defaultBinding = nullptr;
    for(const auto* binding : bindings) {
        if(binding != nullptr && !binding->bypass) { defaultBinding = binding; break; }
    }
    if(defaultBinding == nullptr) return false;
    const auto valuesIt = activeValues.find(defaultBinding->parameterPath);
    std::string value = valuesIt == activeValues.end()
        ? defaultBinding->defaultValue
        : combineAutomationValues(valuesIt->second, defaultBinding->valueType);
    // Only actual combined automation can land outside range -- the binding's
    // own defaultValue (used when nothing is contributing) is never blended
    // against anything, so there is nothing to clamp there.
    if(valuesIt != activeValues.end() && defaultBinding->clampToParameterRange)
        value = clampValueToParameterRange(*parameter, value, defaultBinding->valueType);
    outValue = std::move(value);
    return true;
}

void ofxOceanodeTimelineManager::applyAutomation() {
    if(container == nullptr) return;
    const auto& activeValues = activeAutomationValues;
    std::map<std::string, std::vector<ofxOceanodeTimelineParameterBinding*>> bindingsByPath;
    for(auto& track : tracks) {
        for(auto& binding : track.bindings)
            bindingsByPath[binding.parameterPath].push_back(&binding);
    }
    for(auto& entry : bindingsByPath) {
        auto* parameter = container->findCustomGuiParameter(entry.first);
        ofxOceanodeTimelineParameterBinding* defaultBinding = nullptr;
        for(auto* binding : entry.second) {
            binding->missingTarget = parameter == nullptr;
            if(!binding->bypass && defaultBinding == nullptr) defaultBinding = binding;
        }
        if(parameter == nullptr) continue;
        parameter->setTimelined(defaultBinding != nullptr);
        if(defaultBinding == nullptr) continue;

        const std::vector<const ofxOceanodeTimelineParameterBinding*> bindings(
            entry.second.begin(), entry.second.end());
        std::string value;
        if(!computeParameterValue(bindings, activeValues, parameter, value)) continue;
        if(!value.empty() && parameterValueForAutomation(*parameter) != value) {
            // A scheduled path's backend already received this value with its
            // exact instant. The parameter, its GUI and the node graph are
            // still updated here, on the frame the playhead reaches it, but the
            // backend must not send it a second time -- an untimed duplicate
            // would arrive late and, for a trigger parameter, fire twice.
            ofxOceanodeScheduling::ScopedBackendSuppression suppression(
                scheduledBackendPaths.count(entry.first) > 0);
            applyAutomationValue(*parameter, value, defaultBinding->valueType);
        }
    }
}

// ---------------------------------------------------------------------------
// Timestamped scheduling
// ---------------------------------------------------------------------------

void ofxOceanodeTimelineManager::setSchedulingEnabled(bool enabled) {
    if(schedulingEnabled == enabled) return;
    schedulingEnabled = enabled;
    invalidateSchedule();
}

void ofxOceanodeTimelineManager::setSchedulingLookaheadMs(double milliseconds) {
    const double clamped = std::max(0.0, std::min(1000.0, milliseconds));
    if(std::abs(clamped - schedulingLookaheadMs) < 1e-6) return;
    schedulingLookaheadMs = clamped;
    invalidateSchedule();
}

void ofxOceanodeTimelineManager::invalidateSchedule() {
    if(!scheduledPaths.empty()) sendScheduleCorrections(ofxOceanodeScheduling::steadyNowUs());
    scheduledPaths.clear();
    scheduledBackendPaths.clear();
    hasScheduleCursor = false;
}

// Events already handed to a backend cannot be recalled (scsynth's /clearSched
// is global and would also drop whatever an unrelated part of the patch
// scheduled). Instead every path that has something queued is given the value
// it should really hold, stamped after everything queued for it: the stale
// events still execute, and this lands on top of them.
void ofxOceanodeTimelineManager::sendScheduleCorrections(uint64_t nowUs) {
    if(container == nullptr) {
        scheduledPaths.clear();
        return;
    }
    std::map<std::string, std::vector<const ofxOceanodeTimelineParameterBinding*>> bindingsByPath;
    for(const auto& track : tracks) {
        for(const auto& binding : track.bindings)
            bindingsByPath[binding.parameterPath].push_back(&binding);
    }
    for(auto& entry : scheduledPaths) {
        auto& state = entry.second;
        if(state.pending.empty() && state.lastScheduledDueUs <= nowUs) continue;
        auto* parameter = container->findCustomGuiParameter(entry.first);
        if(parameter == nullptr) continue;
        const auto bindingsIt = bindingsByPath.find(entry.first);
        if(bindingsIt == bindingsByPath.end()) continue;
        std::string value;
        if(!computeParameterValue(bindingsIt->second, activeAutomationValues, parameter, value)) continue;
        ofxOceanodeScheduledParameterEvent event;
        event.value = value;
        event.dueSteadyTimeUs = std::max(nowUs, state.lastScheduledDueUs + 1000);
        event.isCorrection = true;
        ofxOceanodeScheduling::dispatch(parameter, event);
    }
    scheduledPaths.clear();
}

// Inverts the tempo map over a short span: which beat is reached `seconds`
// after startBeat. beatToSeconds() is strictly increasing (bpm is clamped
// above zero), so a bisection is exact enough for a lookahead window and needs
// no closed form for the sigmoid segments.
double ofxOceanodeTimelineManager::beatAfterSeconds(double startBeat, double seconds,
                                                    float fallbackBpm) const {
    if(seconds <= 0.0) return startBeat;
    const double maximumBpm = bpmAutomationEnabled
        ? static_cast<double>(std::max(bpmMaximum, fallbackBpm))
        : static_cast<double>(std::max(1.0f, fallbackBpm));
    const double target = beatToSeconds(startBeat, fallbackBpm) + seconds;
    double low = startBeat;
    double high = startBeat + seconds * maximumBpm / 60.0 + 1.0;
    for(int i = 0; i < 48; ++i) {
        const double mid = (low + high) * 0.5;
        if(beatToSeconds(mid, fallbackBpm) < target) low = mid;
        else high = mid;
    }
    return (low + high) * 0.5;
}

void ofxOceanodeTimelineManager::collectChangeBeats(double fromBeat, double toBeat,
                                                    bool inclusiveStart,
                                                    std::vector<double>& outBeats) const {
    constexpr size_t kMaximumChangeBeats = 4096;
    if(toBeat < fromBeat) return;
    auto addTimelineBeat = [&](double beat) {
        if(outBeats.size() >= kMaximumChangeBeats) return;
        const bool afterStart = inclusiveStart ? beat >= fromBeat - kEpsilon : beat > fromBeat + kEpsilon;
        if(!afterStart || beat > toBeat + kEpsilon) return;
        outBeats.push_back(beat);
    };

    for(const auto& track : tracks) {
        if(track.isWaveTrack) continue; // audio, not automation
        for(const auto& clip : track.clips) {
            const double clipStart = clip.startBeat;
            const double clipDuration = std::max(1.0 / 24.0, clip.durationBeats);
            const double clipEnd = clipStart + clipDuration;
            // A clip edge is itself a change: automation stops (or starts)
            // contributing there.
            addTimelineBeat(clipStart);
            addTimelineBeat(clipEnd);
            if(clipEnd < fromBeat - kEpsilon || clipStart > toBeat + kEpsilon) continue;

            const double windowStart = std::max(fromBeat, clipStart);
            const double windowEnd = std::min(toBeat, clipEnd);
            if(windowEnd < windowStart) continue;
            const double sourceLength = ofxOceanodeTimelineClipTime::sourceDuration(clip);
            const double clipStretch = ofxOceanodeTimelineClipTime::stretch(clip);
            const double cycleLength = ofxOceanodeTimelineClipTime::cycleDuration(clip);

            // One pass per repeat cycle the window touches; each pass maps a
            // source-beat range back to timeline beats with the clip's own
            // conversion, so stretch/repeat live in exactly one place.
            const int64_t firstCycle = clip.repeatContent
                ? static_cast<int64_t>(std::floor((windowStart - clipStart) / std::max(kEpsilon, cycleLength))) : 0;
            const int64_t lastCycle = clip.repeatContent
                ? static_cast<int64_t>(std::floor((windowEnd - clipStart) / std::max(kEpsilon, cycleLength))) : 0;
            if(lastCycle - firstCycle > 64) continue; // pathological; frame path still covers it

            for(int64_t cycle = firstCycle; cycle <= lastCycle; ++cycle) {
                double sourceFrom = 0.0;
                double sourceTo = sourceLength;
                if(clip.repeatContent) {
                    sourceFrom = (windowStart - clipStart - static_cast<double>(cycle) * cycleLength) / clipStretch;
                    sourceTo = (windowEnd - clipStart - static_cast<double>(cycle) * cycleLength) / clipStretch;
                } else {
                    sourceFrom = (windowStart - clipStart) * sourceLength / clipDuration;
                    sourceTo = (windowEnd - clipStart) * sourceLength / clipDuration;
                }
                sourceFrom = std::max(0.0, sourceFrom);
                sourceTo = std::min(sourceLength, sourceTo);
                if(sourceTo < sourceFrom) continue;

                auto addSourceBeat = [&](double sourceBeat) {
                    if(sourceBeat < sourceFrom - kEpsilon || sourceBeat > sourceTo + kEpsilon) return;
                    addTimelineBeat(ofxOceanodeTimelineClipTime::sourceToTimelineBeat(clip, sourceBeat, cycle));
                };
                // The start of a repetition restarts the pattern (and re-rolls
                // per-cycle probabilities), so it is always a candidate.
                addSourceBeat(0.0);

                for(const auto& lane : clip.lanes) {
                    switch(lane.type) {
                        case ofxOceanodeTimelineLaneType::Curve:
                        case ofxOceanodeTimelineLaneType::Wave:
                            // Continuous, or not automation at all.
                            break;
                        case ofxOceanodeTimelineLaneType::Step: {
                            const double cellLength = std::max(1.0 / 24.0, lane.beatsPerStep);
                            const double patternLength = std::max(cellLength, lane.stepCount * cellLength);
                            const int64_t firstPattern = static_cast<int64_t>(std::floor(sourceFrom / patternLength));
                            const int64_t lastPattern = static_cast<int64_t>(std::floor(sourceTo / patternLength));
                            if(lastPattern - firstPattern > 64) break;
                            for(int64_t pattern = firstPattern; pattern <= lastPattern; ++pattern) {
                                const double base = static_cast<double>(pattern) * patternLength;
                                addSourceBeat(base);
                                for(const auto& step : lane.step.steps) {
                                    const double start = std::max(0.0, step.startBeat);
                                    const double duration = step.durationBeats > kEpsilon ? step.durationBeats : cellLength;
                                    addSourceBeat(base + start);
                                    addSourceBeat(base + start + duration);
                                }
                            }
                            break;
                        }
                        case ofxOceanodeTimelineLaneType::MultiSlider: {
                            // Dense grid: every cell boundary is a change.
                            const double cellLength = std::max(1.0 / 24.0, lane.beatsPerStep);
                            const int64_t firstCell = static_cast<int64_t>(std::floor(sourceFrom / cellLength));
                            const int64_t lastCell = static_cast<int64_t>(std::floor(sourceTo / cellLength)) + 1;
                            if(lastCell - firstCell > 512) break;
                            for(int64_t cell = firstCell; cell <= lastCell; ++cell)
                                addSourceBeat(static_cast<double>(cell) * cellLength);
                            break;
                        }
                        case ofxOceanodeTimelineLaneType::MultiValue: {
                            for(const auto& row : lane.multiValueRows)
                                for(const auto& region : row) {
                                    addSourceBeat(region.startBeat);
                                    addSourceBeat(region.end());
                                }
                            break;
                        }
                        case ofxOceanodeTimelineLaneType::MultiGate: {
                            for(const auto& row : lane.multiGateRows)
                                for(const auto& region : row) {
                                    addSourceBeat(region.startBeat);
                                    addSourceBeat(region.end());
                                }
                            break;
                        }
                        case ofxOceanodeTimelineLaneType::PianoRoll: {
                            for(const auto& note : lane.pianoNotes) {
                                addSourceBeat(note.startBeat);
                                addSourceBeat(note.startBeat + std::max(1.0 / 24.0, note.durationBeats));
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    std::sort(outBeats.begin(), outBeats.end());
    outBeats.erase(std::unique(outBeats.begin(), outBeats.end(),
                               [](double a, double b) { return std::abs(a - b) <= kEpsilon; }),
                   outBeats.end());
}

void ofxOceanodeTimelineManager::runScheduler(const ofxOceanodeTransportState& transport,
                                              bool loopWrappedThisFrame) {
    if(container == nullptr) return;
    // The instant the reported beat position belongs to. Everything the
    // scheduler computes is an offset from it, which is why a late or long
    // frame shifts nothing: only the window's far edge moves.
    const uint64_t nowUs = transport.steadyTimeUs != 0
        ? transport.steadyTimeUs : ofxOceanodeScheduling::steadyNowUs();
    const bool schedulingActive = schedulingEnabled && transport.isPlaying;

    const bool generationChanged = hasSeenTransportGeneration &&
        lastSeenTransportGeneration != transport.generation;
    lastSeenTransportGeneration = transport.generation;
    hasSeenTransportGeneration = true;
    // A loop wrap also bumps the generation, but it is not a user seek: the
    // cursor simply restarts at the new position below.
    const bool invalidated = generationChanged && !loopWrappedThisFrame;

    if(!schedulingActive || invalidated) {
        if(!scheduledPaths.empty()) sendScheduleCorrections(nowUs);
        scheduledBackendPaths.clear();
        hasScheduleCursor = false;
        if(!schedulingActive) return;
    }
    // The loop start is a transport discontinuity; the window restarts there.
    if(loopWrappedThisFrame) hasScheduleCursor = false;

    // ---- which paths can be scheduled at all -----------------------------
    struct PathTarget {
        ofxOceanodeAbstractParameter* parameter = nullptr;
        std::vector<const ofxOceanodeTimelineParameterBinding*> bindings;
        bool schedulable = true;
    };
    std::map<std::string, PathTarget> targets;
    for(const auto& track : tracks) {
        for(const auto& binding : track.bindings) {
            auto& target = targets[binding.parameterPath];
            target.bindings.push_back(&binding);
            // A live override (a held piano key, say) is a gesture, not a
            // timed event.
            if(binding.hasLiveOverride) target.schedulable = false;
        }
    }
    for(const auto& track : tracks) {
        for(const auto& clip : track.clips) {
            for(const auto& lane : clip.lanes) {
                if(lane.type != ofxOceanodeTimelineLaneType::Curve) continue;
                // A curve changes every frame: there is nothing discrete to
                // place in time, so its parameters stay on the frame path.
                for(const auto& bindingId : lane.bindingIds) {
                    if(const auto* binding = getBinding(track.id, bindingId))
                        targets[binding->parameterPath].schedulable = false;
                }
            }
        }
    }

    std::vector<std::string> schedulablePaths;
    for(auto& entry : targets) {
        if(!entry.second.schedulable) continue;
        entry.second.parameter = container->findCustomGuiParameter(entry.first);
        if(entry.second.parameter == nullptr) continue;
        // Only a backend that can execute a value at an instant gets events.
        if(!ofxOceanodeScheduling::hasParameterTarget(entry.second.parameter)) continue;
        schedulablePaths.push_back(entry.first);
    }
    if(schedulablePaths.empty()) {
        scheduledBackendPaths.clear();
        scheduledPaths.clear();
        return;
    }

    // ---- promote what became due, and repair anything unforeseen ---------
    std::set<std::string> activePaths;
    // Suppression starts one frame later than scheduling: on the frame a path
    // joins the scheduler the backend has not been given anything yet, so that
    // frame's ordinary send is what puts the current value in place.
    std::set<std::string> suppressedPaths;
    for(const auto& path : schedulablePaths) {
        auto& target = targets[path];
        std::string frameValue;
        if(!computeParameterValue(target.bindings, activeAutomationValues, target.parameter, frameValue))
            continue;
        auto& state = scheduledPaths[path];
        const bool wasInitialized = state.initialized;
        while(!state.pending.empty() && state.pending.front().first <= nowUs) {
            state.valueInEffect = state.pending.front().second;
            state.pending.pop_front();
        }
        if(!state.initialized) {
            state.initialized = true;
            state.valueInEffect = frameValue;
            state.lastScheduledValue = frameValue;
            state.lastScheduledDueUs = nowUs;
        } else if(frameValue != state.valueInEffect) {
            // The value the playhead actually has is not the one the backend
            // was told to play: the content was edited inside the window, or
            // something outside the timeline moved. Correct it after
            // everything already queued for this path.
            ofxOceanodeScheduledParameterEvent event;
            event.value = frameValue;
            event.dueSteadyTimeUs = std::max(nowUs, state.lastScheduledDueUs + 1000);
            event.generation = transport.generation;
            event.isCorrection = true;
            ofxOceanodeScheduling::dispatch(target.parameter, event);
            state.pending.clear();
            state.valueInEffect = frameValue;
            state.lastScheduledValue = frameValue;
            state.lastScheduledDueUs = event.dueSteadyTimeUs;
        }
        activePaths.insert(path);
        if(wasInitialized) suppressedPaths.insert(path);
    }
    for(auto it = scheduledPaths.begin(); it != scheduledPaths.end();) {
        if(activePaths.count(it->first) == 0) it = scheduledPaths.erase(it);
        else ++it;
    }
    scheduledBackendPaths = suppressedPaths;
    if(activePaths.empty()) return;

    // ---- walk the window -------------------------------------------------
    const double lookaheadSeconds = std::max(0.0, schedulingLookaheadMs) / 1000.0;
    const bool loopActive = loopEnabled && loopEndBeat > loopStartBeat + kEpsilon;
    if(loopActive && transport.beatPosition >= loopEndBeat) return; // wrap happens next frame
    if(!hasScheduleCursor) {
        scheduleCursorBeat = transport.beatPosition;
        hasScheduleCursor = true;
    }
    if(scheduleCursorBeat < transport.beatPosition) scheduleCursorBeat = transport.beatPosition;

    const double secondsNow = beatToSeconds(transport.beatPosition, transport.bpm);
    const double horizonBeat = beatAfterSeconds(transport.beatPosition, lookaheadSeconds, transport.bpm);
    // The window stops at the loop end: the loop boundary itself is taken on a
    // frame (the transport is seeked there), so events past it cannot be given
    // an exact instant yet -- they are scheduled once the new pass has begun.
    const double endBeat = loopActive ? std::min(horizonBeat, loopEndBeat) : horizonBeat;
    if(endBeat <= scheduleCursorBeat) return;

    std::vector<double> changeBeats;
    collectChangeBeats(scheduleCursorBeat, endBeat, false, changeBeats);
    scheduleCursorBeat = endBeat;

    for(const double beat : changeBeats) {
        const double offsetSeconds = beatToSeconds(beat, transport.bpm) - secondsNow;
        const uint64_t dueUs = nowUs + static_cast<uint64_t>(std::max(0.0, offsetSeconds) * 1000000.0);
        ActiveValueMap futureValues;
        // Evaluate just past the boundary: every lane's own test is
        // "start <= beat < end", so the probe must land inside the new
        // step/note/region, not exactly on its edge.
        collectActiveValues(beat + 1e-7, true, false, futureValues);
        for(const auto& path : activePaths) {
            auto& target = targets[path];
            std::string value;
            if(!computeParameterValue(target.bindings, futureValues, target.parameter, value)) continue;
            auto& state = scheduledPaths[path];
            if(value == state.lastScheduledValue) continue;
            ofxOceanodeScheduledParameterEvent event;
            event.value = value;
            // Never let one path's events swap order, whatever the rounding of
            // the tempo integral did.
            event.dueSteadyTimeUs = std::max(dueUs, state.lastScheduledDueUs + 1);
            event.generation = transport.generation;
            if(!ofxOceanodeScheduling::dispatch(target.parameter, event)) {
                // The backend could not take it (no synth yet): drop this path
                // back to the frame path for now.
                scheduledBackendPaths.erase(path);
                state.initialized = false;
                state.pending.clear();
                continue;
            }
            state.lastScheduledValue = value;
            state.lastScheduledDueUs = event.dueSteadyTimeUs;
            state.pending.emplace_back(event.dueSteadyTimeUs, value);
        }
    }
}

void ofxOceanodeTimelineManager::setLiveOverride(const std::string& trackId, const std::string& bindingId, const std::string& value) {
    if(auto* binding = getBinding(trackId, bindingId)) {
        binding->hasLiveOverride = true;
        binding->liveOverrideValue = value;
    }
}

void ofxOceanodeTimelineManager::clearLiveOverride(const std::string& trackId, const std::string& bindingId) {
    if(auto* binding = getBinding(trackId, bindingId)) {
        binding->hasLiveOverride = false;
        binding->liveOverrideValue.clear();
    }
}

void ofxOceanodeTimelineManager::clearTimelineFlag(const ofxOceanodeTimelineTrack& track) {
    if(container == nullptr) return;
    for(const auto& binding : track.bindings) {
        if(auto* parameter = container->findCustomGuiParameter(binding.parameterPath)) parameter->setTimelined(false);
    }
}

void ofxOceanodeTimelineManager::refreshTimelineFlag(const std::string& parameterPath) {
    if(container == nullptr) return;
    const bool isActive = std::any_of(tracks.begin(), tracks.end(), [&](const auto& track) {
        return std::any_of(track.bindings.begin(), track.bindings.end(), [&](const auto& binding) {
            return binding.parameterPath == parameterPath && !binding.bypass;
        });
    });
    if(auto* parameter = container->findCustomGuiParameter(parameterPath))
        parameter->setTimelined(isActive);
}

void ofxOceanodeTimelineManager::clear() {
    for(const auto& track : tracks) clearTimelineFlag(track);
    tracks.clear();
    clipGroups.clear();
    nextTrackNumber = 1;
    nextBindingNumber = 1;
    nextClipNumber = 1;
    nextLaneNumber = 1;
    nextGroupNumber = 1;
    pendingTrackRenameId.clear();
    pendingTrackRenameIsNew = false;
    activeAutomationValues.clear();
    invalidateSchedule();
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
    hasEvaluatedTransportBeat = false;
    lastEvaluatedTransportBeat = 0.0;
    timeSignatureNumerator = 4;
    timeSignatureDenominator = 4;
}

void ofxOceanodeTimelineManager::setTimeSignature(int numerator, int denominator) {
    const double previousBeatsPerBar = getBeatsPerBar();
    timeSignatureNumerator = std::max(1, numerator);
    timeSignatureDenominator = std::max(1, denominator);
    if(!loopEnabled && std::abs(loopStartBeat) <= kEpsilon &&
       std::abs(loopEndBeat - previousBeatsPerBar) <= kEpsilon) {
        loopEndBeat = getBeatsPerBar();
    }
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
        case ofxOceanodeTimelineLaneType::MultiValue: return "MultiValue";
        case ofxOceanodeTimelineLaneType::MultiSlider: return "MultiSlider";
        case ofxOceanodeTimelineLaneType::MultiGate: return "MultiGate";
        case ofxOceanodeTimelineLaneType::Wave: return "Wave";
        case ofxOceanodeTimelineLaneType::Step:
        default: return "Step";
    }
}

ofxOceanodeTimelineLaneType ofxOceanodeTimelineManager::laneTypeFromString(const std::string& laneType) {
    if(laneType == "PianoRoll") return ofxOceanodeTimelineLaneType::PianoRoll;
    if(laneType == "Curve") return ofxOceanodeTimelineLaneType::Curve;
    if(laneType == "MultiValue") return ofxOceanodeTimelineLaneType::MultiValue;
    if(laneType == "MultiSlider") return ofxOceanodeTimelineLaneType::MultiSlider;
    if(laneType == "MultiGate") return ofxOceanodeTimelineLaneType::MultiGate;
    if(laneType == "Wave") return ofxOceanodeTimelineLaneType::Wave;
    return ofxOceanodeTimelineLaneType::Step;
}

ofJson ofxOceanodeTimelineManager::toJson() const {
    ofJson json;
    json["version"] = 8; // 8 adds self-contained LFO clips
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
    json["scheduling"] = {
        {"enabled", schedulingEnabled},
        {"lookaheadMs", schedulingLookaheadMs}
    };
    json["tracks"] = ofJson::array();
    for(const auto& track : tracks) {
        ofJson trackJson;
        trackJson["id"] = track.id;
        trackJson["name"] = track.name;
        trackJson["collapsed"] = track.collapsed;
        trackJson["isWaveTrack"] = track.isWaveTrack;
        trackJson["waveTrackHeight"] = track.waveTrackHeight;
        trackJson["waveVolume"] = track.waveVolume;
        trackJson["waveVolumeAutomationEnabled"] = track.waveVolumeAutomationEnabled;
        trackJson["waveVolumeInterpolation"] = track.waveVolumeInterpolation;
        trackJson["waveVolumePoints"] = ofJson::array();
        for(const auto& point : track.waveVolumePoints)
            trackJson["waveVolumePoints"].push_back({{"beat", point.beat}, {"value", point.value}});
        trackJson["waveVolumeTensions"] = ofJson::array();
        for(const auto& tension : track.waveVolumeTensions)
            trackJson["waveVolumeTensions"].push_back({{"inflection", tension.inflection}, {"steepness", tension.steepness}});
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
                {"bypass", binding.bypass},
                {"clampToRange", binding.clampToParameterRange}
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
            clipJson["contentStretch"] = clip.contentStretch;
            clipJson["repeatContent"] = clip.repeatContent;
            clipJson["isLfo"] = clip.isLfo;
            clipJson["lfoOutputBindingId"] = clip.lfoOutputBindingId;
            clipJson["lfoOutputMin"] = clip.lfoOutputMin;
            clipJson["lfoOutputMax"] = clip.lfoOutputMax;
            clipJson["waveFilePath"] = clip.waveFilePath;
            clipJson["waveGain"] = clip.waveGain;
            clipJson["wavePlaybackRate"] = clip.wavePlaybackRate;
            clipJson["waveSourceStartBeat"] = clip.waveSourceStartBeat;
            clipJson["waveFileDurationBeats"] = clip.waveFileDurationBeats;
            clipJson["waveReverse"] = clip.waveReverse;
            clipJson["lanes"] = ofJson::array();
            for(const auto& lane : clip.lanes) {
                ofJson laneJson;
                laneJson["id"] = lane.id;
                laneJson["name"] = lane.name;
                laneJson["laneType"] = laneTypeToString(lane.type);
                laneJson["bindingIds"] = lane.bindingIds;
                laneJson["stepCount"] = lane.stepCount;
                laneJson["beatsPerStep"] = lane.beatsPerStep;
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
                laneJson["multiRowCount"] = lane.multiRowCount;
                laneJson["multiValueRows"] = ofJson::array();
                for(const auto& row : lane.multiValueRows) {
                    ofJson rowJson = ofJson::array();
                    for(const auto& region : row) rowJson.push_back({
                        {"startBeat", region.startBeat}, {"durationBeats", region.durationBeats},
                        {"value", region.value}
                    });
                    laneJson["multiValueRows"].push_back(std::move(rowJson));
                }
                laneJson["multiGateRows"] = ofJson::array();
                for(const auto& row : lane.multiGateRows) {
                    ofJson rowJson = ofJson::array();
                    for(const auto& region : row) rowJson.push_back({
                        {"startBeat", region.startBeat}, {"durationBeats", region.durationBeats}
                    });
                    laneJson["multiGateRows"].push_back(std::move(rowJson));
                }
                laneJson["multiValueInteger"] = lane.multiValueInteger;
                laneJson["multiSliderValues"] = lane.multiSliderValues;
                laneJson["valueQuantizeSteps"] = lane.valueQuantizeSteps;
                laneJson["isWaveVolume"] = lane.isWaveVolume;
                laneJson["lfoParameter"] = lane.lfoParameter;
                // Legacy Wave-lane fields remain readable below, but new
                // audio is serialized at clip level.
                laneJson["waveFilePath"] = lane.waveFilePath;
                laneJson["waveGain"] = lane.waveGain;
                clipJson["lanes"].push_back(std::move(laneJson));
            }
            trackJson["clips"].push_back(std::move(clipJson));
        }

        json["tracks"].push_back(std::move(trackJson));
    }
    json["clipGroups"] = ofJson::array();
    for(const auto& group : clipGroups) {
        ofJson groupJson;
        groupJson["id"] = group.id;
        groupJson["members"] = ofJson::array();
        for(const auto& member : group.members)
            groupJson["members"].push_back({{"trackId", member.first}, {"clipId", member.second}});
        json["clipGroups"].push_back(std::move(groupJson));
    }
    return json;
}

void ofxOceanodeTimelineManager::fromJson(const ofJson& json) {
    clear();
    if(!json.is_object() || !json.contains("tracks") || !json["tracks"].is_array()) return;

    std::set<std::string> loadedTrackIds;
    std::set<std::string> loadedBindingIds;
    std::set<std::string> loadedClipIds;
    std::set<std::string> loadedLaneIds;
    std::set<std::string> loadedGroupIds;
    // clipGroups (parsed after the tracks loop below) references clips by
    // the track/clip id *as saved in the file*. uniqueLoadedId can remap an
    // id that collides with one already loaded (or that was empty), so the
    // original-id -> final-id mapping is recorded here as each track/clip
    // loads, and consulted when resolving a group's members afterwards.
    std::map<std::string, std::string> loadedTrackIdRemap;
    std::map<std::string, std::string> loadedClipIdRemap;
    auto uniqueLoadedId = [&](std::string candidate, const char* prefix,
                              uint64_t& counter, std::set<std::string>& usedIds) {
        if(!candidate.empty() && usedIds.insert(candidate).second) return candidate;
        do {
            candidate = makeId(prefix, counter++);
        } while(!usedIds.insert(candidate).second);
        return candidate;
    };

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
    if(json.contains("scheduling") && json["scheduling"].is_object()) {
        const auto& scheduling = json["scheduling"];
        setSchedulingEnabled(scheduling.value("enabled", true));
        setSchedulingLookaheadMs(scheduling.value("lookaheadMs", 120.0));
    }
    // Every clip, lane and loop below is about to be replaced.
    invalidateSchedule();

    for(const auto& trackJson : json["tracks"]) {
        if(!trackJson.is_object()) continue;
        ofxOceanodeTimelineTrack track;
        const std::string originalTrackId = trackJson.value("id", std::string());
        track.id = uniqueLoadedId(originalTrackId, "timeline_track", nextTrackNumber, loadedTrackIds);
        if(!originalTrackId.empty()) loadedTrackIdRemap[originalTrackId] = track.id;
        track.name = makeUniqueTrackName(trackJson.value("name", std::string("Timeline Track")));
        track.collapsed = trackJson.value("collapsed", false);
        track.isWaveTrack = trackJson.value("isWaveTrack", false);
        track.waveTrackHeight = ofClamp(trackJson.value("waveTrackHeight", 96.0f), 48.0f, 360.0f);
        track.waveVolume = ofClamp(trackJson.value("waveVolume", 1.0f), 0.0f, 4.0f);
        // Track automation is always present for Wave Tracks. The old flag is
        // still read for compatibility, but it must not make legacy presets
        // lose the default automation lane.
        track.waveVolumeAutomationEnabled = track.isWaveTrack ||
            trackJson.value("waveVolumeAutomationEnabled", false);
        track.waveVolumeInterpolation = trackJson.value("waveVolumeInterpolation", std::string("Linear"));
        if(trackJson.contains("waveVolumePoints") && trackJson["waveVolumePoints"].is_array()) {
            for(const auto& pointJson : trackJson["waveVolumePoints"]) {
                if(!pointJson.is_object()) continue;
                track.waveVolumePoints.push_back({
                    std::max(0.0, pointJson.value("beat", 0.0)),
                    ofClamp(pointJson.value("value", 1.0f), 0.0f, 4.0f)
                });
            }
        }
        std::sort(track.waveVolumePoints.begin(), track.waveVolumePoints.end(),
                  [](const auto& a, const auto& b) { return a.beat < b.beat; });
        if(trackJson.contains("waveVolumeTensions") && trackJson["waveVolumeTensions"].is_array()) {
            for(const auto& tensionJson : trackJson["waveVolumeTensions"]) {
                if(!tensionJson.is_object()) continue;
                track.waveVolumeTensions.push_back({
                    ofClamp(tensionJson.value("inflection", 0.5f), 0.01f, 0.99f),
                    ofClamp(tensionJson.value("steepness", 1.0f), 0.1f, 10.0f)
                });
            }
        }
        track.waveVolumeTensions.resize(track.waveVolumePoints.size() > 1 ? track.waveVolumePoints.size() - 1 : 0);
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
                binding.id = uniqueLoadedId(bindingJson.value("id", std::string()),
                                             "timeline_binding", nextBindingNumber, loadedBindingIds);
                binding.parameterPath = bindingJson.value("parameterPath", std::string());
                if(binding.id.empty() || binding.parameterPath.empty()) continue;
                if(std::any_of(track.bindings.begin(), track.bindings.end(), [&](const auto& existing) {
                    return existing.parameterPath == binding.parameterPath;
                })) continue;
                binding.valueType = bindingJson.value("valueType", std::string());
                binding.defaultValue = bindingJson.value("defaultValue", std::string());
                binding.mode = modeFromString(bindingJson.value("mode", std::string("Replace")));
                binding.laneType = laneTypeFromString(bindingJson.value("laneType", std::string("Step")));
                binding.bypass = bindingJson.value("bypass", false);
                binding.clampToParameterRange = bindingJson.value("clampToRange", true);
                track.bindings.push_back(std::move(binding));
            }
        }

        if(trackJson.contains("clips") && trackJson["clips"].is_array()) {
            for(const auto& clipJson : trackJson["clips"]) {
                if(!clipJson.is_object()) continue;
                ofxOceanodeTimelineClip clip;
                const std::string originalClipId = clipJson.value("id", std::string());
                clip.id = uniqueLoadedId(originalClipId, "timeline_clip", nextClipNumber, loadedClipIds);
                if(!originalClipId.empty()) loadedClipIdRemap[originalClipId] = clip.id;
                clip.name = makeUniqueClipName(track, clipJson.value("name", std::string("Clip")));
                clip.startBeat = std::max(0.0, clipJson.value("startBeat", 0.0));
                clip.durationBeats = std::max(1.0 / 24.0, clipJson.value("durationBeats", 4.0));
                clip.contentDurationBeats = std::max(1.0 / 24.0, clipJson.value("contentDurationBeats", clip.durationBeats));
                clip.repeatContent = clipJson.value("repeatContent", true);
                clip.contentStretch = std::max(1.0 / 1024.0,
                    clipJson.value("contentStretch", clip.repeatContent
                        ? 1.0 : clip.durationBeats / clip.contentDurationBeats));
                clip.isLfo = clipJson.value("isLfo", false);
                clip.lfoOutputBindingId = clipJson.value("lfoOutputBindingId", std::string());
                clip.lfoOutputMin = clipJson.value("lfoOutputMin", 0.0f);
                clip.lfoOutputMax = clipJson.value("lfoOutputMax", 1.0f);
                clip.waveFilePath = clipJson.value("waveFilePath", std::string());
                clip.waveGain = ofClamp(clipJson.value("waveGain", 1.0f), 0.0f, 4.0f);
                clip.wavePlaybackRate = ofClamp(clipJson.value("wavePlaybackRate", 1.0f), 0.1f, 4.0f);
                clip.waveSourceStartBeat = std::max(0.0, clipJson.value("waveSourceStartBeat", 0.0));
                clip.waveFileDurationBeats = std::max(0.0, clipJson.value("waveFileDurationBeats", 0.0));
                clip.waveReverse = clipJson.value("waveReverse", false);
                if(clipJson.contains("lanes") && clipJson["lanes"].is_array()) {
                    for(const auto& laneJson : clipJson["lanes"]) {
                        if(!laneJson.is_object()) continue;
                        ofxOceanodeTimelineLane lane;
                        lane.id = uniqueLoadedId(laneJson.value("id", std::string()),
                                                 "timeline_lane", nextLaneNumber, loadedLaneIds);
                        lane.name = laneJson.value("name", std::string("Lane"));
                        lane.type = laneTypeFromString(laneJson.value("laneType", std::string("Step")));
                        lane.stepCount = std::max(1, laneJson.value("stepCount", 16));
                        lane.beatsPerStep = std::max(1.0 / 24.0, laneJson.value("beatsPerStep", 0.25));
                        lane.valueMin = laneJson.value("valueMin", 0.0f);
                        lane.valueMax = laneJson.value("valueMax", 1.0f);
                        lane.probabilityEnabled = laneJson.value("probabilityEnabled", true);
                        lane.behavior = laneJson.value("behavior", std::string("Probability"));
                        if(lane.behavior != "Probability" && lane.behavior != "Always" && lane.behavior != "Mute")
                            lane.behavior = "Probability";
                        lane.pianoLowPitch = ofClamp(laneJson.value("pianoLowPitch", 36), 0, 127);
                        lane.pianoHighPitch = ofClamp(laneJson.value("pianoHighPitch", 84), lane.pianoLowPitch, 127);
                        lane.pianoSnapToGrid = laneJson.value("pianoSnapToGrid", true);
                        lane.pianoPitchBindingId = laneJson.value("pianoPitchBindingId", std::string());
                        lane.pianoGateBindingId = laneJson.value("pianoGateBindingId", std::string());
                        lane.pianoVelocityBindingId = laneJson.value("pianoVelocityBindingId", std::string());
                        lane.pianoDefaultVelocity = ofClamp(laneJson.value("pianoDefaultVelocity", 0.8f), 0.0f, 1.0f);
                        lane.pianoMonophonic = laneJson.value("pianoMonophonic", false);
                        lane.curveClamp = laneJson.value("curveClamp", true);
                        lane.isWaveVolume = laneJson.value("isWaveVolume", false);
                        lane.lfoParameter = laneJson.value("lfoParameter", std::string());
                        lane.curveInterpolation = laneJson.value("curveInterpolation", std::string("Linear"));
                        if(lane.curveInterpolation != "Step" && lane.curveInterpolation != "Linear" &&
                           lane.curveInterpolation != "Log / Exp" && lane.curveInterpolation != "Sigmoid") {
                            lane.curveInterpolation = "Linear";
                        }
                        if(laneJson.contains("bindingIds") && laneJson["bindingIds"].is_array()) {
                            for(const auto& bindingId : laneJson["bindingIds"]) {
                                if(!bindingId.is_string()) continue;
                                const std::string id = bindingId.get<std::string>();
                                const bool belongsToTrack = std::any_of(track.bindings.begin(), track.bindings.end(),
                                    [&](const auto& binding) { return binding.id == id; });
                                if(belongsToTrack && std::find(lane.bindingIds.begin(), lane.bindingIds.end(), id) == lane.bindingIds.end())
                                    lane.bindingIds.push_back(id);
                            }
                        }
                        auto validateRole = [&](std::string& roleId) {
                            const bool belongsToTrack = std::any_of(track.bindings.begin(), track.bindings.end(),
                                [&](const auto& binding) { return binding.id == roleId; });
                            if(!belongsToTrack) roleId.clear();
                            else if(std::find(lane.bindingIds.begin(), lane.bindingIds.end(), roleId) == lane.bindingIds.end())
                                lane.bindingIds.push_back(roleId);
                        };
                        validateRole(lane.pianoPitchBindingId);
                        validateRole(lane.pianoGateBindingId);
                        validateRole(lane.pianoVelocityBindingId);
                        if(lane.pianoGateBindingId == lane.pianoPitchBindingId)
                            lane.pianoGateBindingId.clear();
                        if(lane.pianoVelocityBindingId == lane.pianoPitchBindingId ||
                           lane.pianoVelocityBindingId == lane.pianoGateBindingId)
                            lane.pianoVelocityBindingId.clear();
                        if(laneJson.contains("step") && laneJson["step"].is_object()) lane.step.fromJson(laneJson["step"]);
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
                                    std::max(0.0, noteJson.value("startBeat", 0.0)),
                                    std::max(1.0 / 24.0, noteJson.value("durationBeats", 0.25)),
                                    static_cast<int>(ofClamp(noteJson.value("pitch", 60), 0, 127)),
                                    ofClamp(noteJson.value("velocity", 1.0f), 0.0f, 1.0f),
                                    ofClamp(noteJson.value("probability", 1.0f), 0.0f, 1.0f)
                                });
                            }
                        }
                        lane.multiRowCount = ofClamp(laneJson.value("multiRowCount", 4), 1, 16);
                        if(laneJson.contains("multiValueRows") && laneJson["multiValueRows"].is_array()) {
                            for(const auto& rowJson : laneJson["multiValueRows"]) {
                                std::vector<ofxOceanodeTimelineValueRegion> row;
                                if(rowJson.is_array()) {
                                    for(const auto& regionJson : rowJson) {
                                        if(!regionJson.is_object()) continue;
                                        row.push_back({
                                            std::max(0.0, regionJson.value("startBeat", 0.0)),
                                            std::max(1.0 / 24.0, regionJson.value("durationBeats", 1.0)),
                                            regionJson.value("value", 0.0f)
                                        });
                                    }
                                }
                                lane.multiValueRows.push_back(std::move(row));
                            }
                        }
                        if(laneJson.contains("multiGateRows") && laneJson["multiGateRows"].is_array()) {
                            for(const auto& rowJson : laneJson["multiGateRows"]) {
                                std::vector<ofxOceanodeTimelineGateRegion> row;
                                if(rowJson.is_array()) {
                                    for(const auto& regionJson : rowJson) {
                                        if(!regionJson.is_object()) continue;
                                        row.push_back({
                                            std::max(0.0, regionJson.value("startBeat", 0.0)),
                                            std::max(1.0 / 24.0, regionJson.value("durationBeats", 1.0))
                                        });
                                    }
                                }
                                lane.multiGateRows.push_back(std::move(row));
                            }
                        }
                        lane.multiValueInteger = laneJson.value("multiValueInteger", false);
                        if(laneJson.contains("multiSliderValues") && laneJson["multiSliderValues"].is_array()) {
                            // Real values in the lane's own valueMin/valueMax
                            // units, not normalized 0..1 -- unlike curve
                            // points, so no 0..1 clamp here (an earlier
                            // version of this loader incorrectly clamped to
                            // 0..1, corrupting any lane whose range didn't
                            // happen to already be 0..1).
                            for(const auto& v : laneJson["multiSliderValues"])
                                lane.multiSliderValues.push_back(v.get<float>());
                        }
                        lane.valueQuantizeSteps = std::max(0, laneJson.value("valueQuantizeSteps", 0));
                        lane.waveFilePath = laneJson.value("waveFilePath", std::string());
                        lane.waveGain = ofClamp(laneJson.value("waveGain", 1.0f), 0.0f, 4.0f);
                        if(lane.isWaveVolume) {
                            // Migrate the first legacy per-clip volume lane
                            // into the new track-wide automation domain.
                            track.isWaveTrack = true;
                            if(track.waveVolumePoints.empty()) {
                                track.waveVolumeAutomationEnabled = true;
                                for(const auto& point : lane.curvePoints) {
                                    const float value = ofClamp(lane.valueMin +
                                        point.value * (lane.valueMax - lane.valueMin), 0.0f, 4.0f);
                                    track.waveVolumePoints.push_back({
                                        ofxOceanodeTimelineClipTime::sourceToTimelineBeat(clip, point.beat, 0),
                                        value
                                    });
                                }
                            }
                            continue;
                        }
                        if(lane.type == ofxOceanodeTimelineLaneType::Wave) {
                            // Migrate the old representation in which each
                            // audio clip contained a Wave lane.
                            track.isWaveTrack = true;
                            if(clip.waveFilePath.empty()) clip.waveFilePath = lane.waveFilePath;
                            clip.waveGain = lane.waveGain;
                            continue;
                        }
                        clip.lanes.push_back(std::move(lane));
                    }
                }
                track.clips.push_back(std::move(clip));
            }
        }

        if(track.waveVolumePoints.size() > 1) {
            std::sort(track.waveVolumePoints.begin(), track.waveVolumePoints.end(),
                      [](const auto& a, const auto& b) { return a.beat < b.beat; });
            track.waveVolumeTensions.resize(track.waveVolumePoints.size() - 1);
        }
        // Wave clips are independent audio slices, not repeating pattern
        // clips. Older presets could inherit the generic timeline's repeat
        // flag after an edge drag, which cropped a slice when its visible
        // duration was shorter than its source content. Preserve the visible
        // duration but map the complete source over it instead.
        // The stored contentStretch is also recomputed for clips that were
        // already non-repeating: a stale value (e.g. 1.0 saved next to a
        // shorter duration) made the audio rate disagree with the position.
        if(track.isWaveTrack) {
            for(auto& clip : track.clips) normalizeWaveClipMapping(clip);
        }
        if(track.isWaveTrack && track.waveVolumePoints.empty()) {
            track.waveVolumePoints = {{0.0, track.waveVolume}, {4.0, track.waveVolume}};
            track.waveVolumeTensions.assign(1, ofxOceanodeTimelineCurveTension{});
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

    // Rebuild waveform caches after loading, once ids and clip storage are
    // final. Legacy Wave lanes were migrated to their containing clip above.
    for(auto& track : tracks) {
        for(auto& clip : track.clips) {
            if(track.isWaveTrack && !clip.waveFilePath.empty()) {
                reloadWaveform(track.id, clip.id);
                if(clip.waveFileDurationBeats <= 0.0 && clip.waveFileDurationMs > 0.0) {
                    const double bpm = container != nullptr
                        ? std::max(1.0f, container->getTransportState().bpm) : 120.0;
                    clip.waveFileDurationBeats = clip.waveFileDurationMs * bpm / 60000.0;
                }
                normalizeWaveClipMapping(clip);
            }
        }
    }

    if(json.contains("clipGroups") && json["clipGroups"].is_array()) {
        for(const auto& groupJson : json["clipGroups"]) {
            if(!groupJson.is_object()) continue;
            ofxOceanodeTimelineClipGroup group;
            group.id = uniqueLoadedId(groupJson.value("id", std::string()),
                                      "timeline_group", nextGroupNumber, loadedGroupIds);
            if(groupJson.contains("members") && groupJson["members"].is_array()) {
                for(const auto& memberJson : groupJson["members"]) {
                    if(!memberJson.is_object()) continue;
                    const std::string rawTrackId = memberJson.value("trackId", std::string());
                    const std::string rawClipId = memberJson.value("clipId", std::string());
                    const auto trackRemapIt = loadedTrackIdRemap.find(rawTrackId);
                    const auto clipRemapIt = loadedClipIdRemap.find(rawClipId);
                    const std::string resolvedTrackId = trackRemapIt != loadedTrackIdRemap.end() ? trackRemapIt->second : rawTrackId;
                    const std::string resolvedClipId = clipRemapIt != loadedClipIdRemap.end() ? clipRemapIt->second : rawClipId;
                    // Drop a member whose clip didn't survive the load above
                    // (a hand-edited file, or a clip that no longer exists)
                    // rather than keep a dangling reference around.
                    if(getClip(resolvedTrackId, resolvedClipId) != nullptr)
                        group.members.emplace_back(resolvedTrackId, resolvedClipId);
                }
            }
            if(group.members.size() >= 2) clipGroups.push_back(std::move(group));
        }
    }
    nextGroupNumber = clipGroups.size() + 1;
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
