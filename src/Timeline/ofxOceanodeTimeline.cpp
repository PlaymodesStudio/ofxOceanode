#include "Timeline/ofxOceanodeTimeline.h"

#include "Managers/ofxOceanodeContainer.h"
#include "ofxOceanodeParameter.h"
#include "Nodes/Default_Nodes/Base/baseOscillator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <unordered_map>
#include <unordered_set>
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

float valueAtBeat(const std::vector<ofxOceanodeTimelineCurvePoint>& points,
                  const std::vector<ofxOceanodeTimelineCurveTension>& tensions,
                  const std::string& interpolation, double beat, float fallback) {
    if(points.empty()) return fallback;
    if(points.size() == 1) return points.front().value;
    if(beat <= points.front().beat) return points.front().value;
    if(beat >= points.back().beat) return points.back().value;
    const auto mode = curveInterpolationMode(interpolation);
    const auto right = std::lower_bound(points.begin() + 1, points.end(), beat,
        [](const ofxOceanodeTimelineCurvePoint& point, double targetBeat) {
            return point.beat < targetBeat;
        });
    const size_t i = static_cast<size_t>(right - points.begin());
    const double span = std::max(1e-9, points[i].beat - points[i - 1].beat);
    const float t = static_cast<float>(ofClamp((beat - points[i - 1].beat) / span, 0.0, 1.0));
    const auto tension = i - 1 < tensions.size() ? tensions[i - 1] : ofxOceanodeTimelineCurveTension{};
    return ofLerp(points[i - 1].value, points[i].value, curveSegmentShape(t, mode, tension));
}

} // namespace ofxOceanodeTimelineCurve

namespace {
using ofxOceanodeTimelineCurve::valueAtBeat;
using ofxOceanodeTimelineCurve::CurveInterpolationMode;
using ofxOceanodeTimelineCurve::curveInterpolationMode;
using ofxOceanodeTimelineCurve::curveSegmentShape;

constexpr double kEpsilon = 1e-9;

// Bumped when the shape of the document changes in a way a reader has to
// know about. Every field is parsed with its own default, so an older file
// loads without any version test; this exists to notice a file written by a
// NEWER build, where a silent partial load would be the wrong answer.
// 6 clip groups, 7 wave tracks, 8 self-contained LFO clips,
// 9 preset-backed editor zoom and horizontal position,
// 10 state-capture curve clips.
constexpr int kPresetVersion = 10;
constexpr float kDefaultTimelinePixelsPerSecond = 140.0f;
constexpr float kMinTimelinePixelsPerSecond = 0.25f;
constexpr float kMaxTimelinePixelsPerSecond = 600.0f;

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

    auto parseValue = [](const std::string& value, bool scalar) {
        std::vector<double> result;
        if(scalar) {
            result.push_back(ofToDouble(value));
            return result;
        }
        const auto tokens = ofSplitString(value, ",", true, true);
        result.reserve(tokens.size());
        if(tokens.empty()) result.push_back(ofToDouble(value));
        else for(const auto& token : tokens) result.push_back(ofToDouble(token));
        if(result.empty()) result.push_back(0.0);
        return result;
    };

    std::vector<std::vector<double>> numericValues;
    numericValues.reserve(values.size());
    size_t outputSize = 1;
    const bool scalar = isScalarFloat || isScalarInt || isScalarBool;
    for(const auto& value : values) {
        numericValues.push_back(parseValue(value.second, scalar));
        if(!scalar)
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

// Reads a WAV file's channel count, duration and a fixed-size peak cache.
// PCM 8/16/24/32 only, first fmt/data chunks used -- the same reader, with
// the same limitations, as the standalone Wave Track node this mirrors, so
// both caches look identical. Pure file IO with no SuperCollider
// dependency, safe to call whether or not an audio provider is registered.
// filePath is in/out: it is rewritten when the file is found through the
// data/Samples search path rather than as given.
bool readWaveformCache(std::string& filePath, int& outNumChannels,
                       double& outDurationMs, std::vector<float>& outPeaks) {
    outNumChannels = 0;
    outDurationMs = 0.0;
    outPeaks.clear();
    if(filePath.empty()) return true;

    const std::string resolvedPath = resolveTimelineAudioPath(filePath);
    if(resolvedPath != filePath && ofFile::doesFileExist(resolvedPath))
        filePath = resolvedPath;

    ofFile file(filePath, ofFile::ReadOnly, true);
    if(!file.is_open()) {
        ofLogWarning("ofxOceanodeTimeline") << "Could not open wave file: " << filePath;
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
        ofLogWarning("ofxOceanodeTimeline") << "Invalid or truncated wave header: " << filePath;
        file.close();
        return false;
    }
    if(std::strncmp(riff, "RIFF", 4) != 0 || std::strncmp(wave, "WAVE", 4) != 0) {
        ofLogWarning("ofxOceanodeTimeline") << "Selected file is not a RIFF/WAVE file: " << filePath;
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
        ofLogWarning("ofxOceanodeTimeline") << "Wave file has no usable fmt/data chunks: " << filePath;
        file.close();
        return false;
    }

    const int bytesPerSample = bitsPerSample / 8;
    if((bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) ||
       bytesPerSample <= 0) {
        ofLogWarning("ofxOceanodeTimeline") << "Unsupported wave bit depth (" << bitsPerSample
            << ") in " << filePath;
        file.close();
        return false;
    }
    const int totalFrames = bytesPerSample > 0 ? static_cast<int>(dataSize / (bytesPerSample * numChannels)) : 0;
    outDurationMs = totalFrames > 0
        ? static_cast<double>(totalFrames) / sampleRate * 1000.0 : 0.0;
    outNumChannels = numChannels;

    constexpr int kPointsPerChannel = 2000;
    const int framesPerPoint = std::max(1, totalFrames / kPointsPerChannel);
    outPeaks.assign(static_cast<size_t>(numChannels) * kPointsPerChannel, 0.0f);

    file.seekg(dataStart, std::ios::beg);
    std::vector<char> frameBuf(static_cast<size_t>(bytesPerSample) * numChannels);
    std::vector<float> minVals(numChannels);
    std::vector<float> maxVals(numChannels);
    for(int pt = 0; pt < kPointsPerChannel; ++pt) {
        const int startFrame = pt * framesPerPoint;
        const int endFrame = std::min(startFrame + framesPerPoint, totalFrames);
        std::fill(minVals.begin(), minVals.end(), 1.0f);
        std::fill(maxVals.begin(), maxVals.end(), -1.0f);
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
            outPeaks[static_cast<size_t>(ch) * kPointsPerChannel + pt] = peak;
        }
    }
    file.close();
    return true;
}

bool clipSourceBeat(const ofxOceanodeTimelineClip& clip, double globalBeat, double& sourceBeat) {
    const double duration = std::max(1.0 / 24.0, clip.durationBeats);
    if(globalBeat < clip.startBeat - kEpsilon || globalBeat >= clip.startBeat + duration - kEpsilon) return false;

    sourceBeat = ofxOceanodeTimelineClipTime::timelineToSourceBeat(clip, globalBeat);
    return true;
}

bool evaluateCurve(const ofxOceanodeTimelineLane& lane, double beat, std::string& value) {
    if(lane.curvePoints.empty()) return false;
    value = ofToString(valueAtBeat(lane.curvePoints, lane.curveTensions,
                                   lane.curveInterpolation, beat, 0.0f));
    return true;
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
    const char* name; // lane name shown in the editor
};

// Single source for the LFO controls: evaluation ranges and the lanes a clip gets.
constexpr LfoParameterDefinition kLfoParameters[] = {
    {"frequency", 0.125f, 64.0f, 4.0f, "Frequency (beats)"},
    {"roundness", 0.0f, 1.0f, 0.5f, "Roundness"},
    {"skew", -1.0f, 1.0f, 0.0f, "Skew"},
    {"pw", 0.0f, 1.0f, 0.5f, "Pulse width"},
    {"pow", -1.0f, 1.0f, 0.0f, "Pow"},
    {"bipow", -1.0f, 1.0f, 0.0f, "BiPow"},
    {"phaseOffset", 0.0f, 1.0f, 0.0f, "Phase offset"},
    {"scale", 0.0f, 2.0f, 1.0f, "Scale"},
    {"yOffset", -1.0f, 1.0f, 0.0f, "Y offset"}
};

constexpr float kFrequencyGridFactors[3] = {1.0f, 2.0f / 3.0f, 1.5f}; // straight, triplet, dotted

const LfoParameterDefinition* lfoParameterDefinition(const std::string& id) {
    for(const auto& definition : kLfoParameters)
        if(id == definition.id) return &definition;
    return nullptr;
}

bool lfoLaneIsLog(const ofxOceanodeTimelineLane& lane) {
    return lane.lfoParameter == "frequency";
}

// Linear, or logarithmic when requested and the range is strictly positive.
double lfoMapNormalized(float minimum, float maximum, bool logScale, float normalized) {
    const double n = ofClamp(normalized, 0.0f, 1.0f);
    if(logScale && minimum > 0.0f && maximum > minimum)
        return minimum * std::pow(static_cast<double>(maximum) / minimum, n);
    return minimum + n * (maximum - minimum);
}
float lfoUnmapValue(float minimum, float maximum, bool logScale, float value) {
    if(logScale && minimum > 0.0f && maximum > minimum) {
        const double v = std::max<double>(value, minimum);
        return ofClamp(static_cast<float>(std::log(v / minimum) / std::log(static_cast<double>(maximum) / minimum)), 0.0f, 1.0f);
    }
    if(std::abs(maximum - minimum) < 1e-9f) return 0.0f;
    return ofClamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f);
}

float normalizedLfoLaneValue(const ofxOceanodeTimelineLane& lane, double beat,
                             float fallback) {
    if(lane.curveInterpolation == "Value") return lane.lfoValue;
    return valueAtBeat(lane.curvePoints, lane.curveTensions,
                       lane.curveInterpolation, beat, fallback);
}

// ---------------------------------------------------------------------------
// LFO phase with an automated "frequency" (= cycle length in beats).
//
// The phase is the number of cycles accumulated from source beat 0:
//     cycles(b) = integral_0^b  1 / period(x) dx
// so a frequency change bends the oscillator's speed smoothly instead of
// rescaling the whole elapsed time (sourceBeat / period), which made the
// phase jump more the further into the clip the change happened. With a
// constant period this is exactly sourceBeat / period, as before.
//
// Step and Linear segments are integrated in closed form; Log/Exp and
// Sigmoid with fixed-resolution Simpson (end-clustered), so results are deterministic and
// identical during playback, scrubbing and in the editor. Per-point running
// totals are cached per clip and rebuilt when the curve changes.
// ---------------------------------------------------------------------------

constexpr double kMinLfoPeriodBeats = 1.0 / 1024.0;
constexpr int kLfoSimpsonIntervals = 256; // even

struct LfoPeriodCurve {
    const LfoParameterDefinition* definition = nullptr;
    const ofxOceanodeTimelineLane* lane = nullptr;
    CurveInterpolationMode mode = CurveInterpolationMode::Linear;

    double periodForNormalized(float normalized) const {
        return std::max(kMinLfoPeriodBeats,
            lfoMapNormalized(definition->minimum, definition->maximum, lfoLaneIsLog(*lane), normalized));
    }
    double pointPeriod(size_t i) const {
        return periodForNormalized(lane->curvePoints[i].value);
    }
    // Cycles accumulated from point i-1 to fraction t (0..1) of segment i.
    double segmentCycles(size_t i, double t) const {
        const auto& p0 = lane->curvePoints[i - 1];
        const auto& p1 = lane->curvePoints[i];
        const double span = p1.beat - p0.beat;
        if(span <= kEpsilon || t <= 0.0) return 0.0;
        t = std::min(t, 1.0);
        const double P0 = pointPeriod(i - 1);
        const double P1 = pointPeriod(i);
        const bool inRange = p0.value >= 0.0f && p0.value <= 1.0f && p1.value >= 0.0f && p1.value <= 1.0f;
        if(mode == CurveInterpolationMode::Step) {
            return span * t / P0; // value holds until the next point
        }
        if(mode == CurveInterpolationMode::Linear && inRange) {
            if(lfoLaneIsLog(*lane) && definition->minimum > 0.0f) {
                // Log scale: the period is exponential in t, P(t) = P0 * r^t.
                const double r = P1 / P0;
                if(std::abs(r - 1.0) < 1e-9) return span * t / P0;
                return span / P0 * (1.0 - std::pow(r, -t)) / std::log(r);
            }
            const double dP = P1 - P0;
            if(std::abs(dP) < 1e-9) return span * t / P0;
            return span * std::log((P0 + dP * t) / P0) / dP;
        }
        const auto tension = i - 1 < lane->curveTensions.size()
            ? lane->curveTensions[i - 1] : ofxOceanodeTimelineCurveTension{};
        auto inversePeriod = [&](double u) {
            const float shaped = curveSegmentShape(static_cast<float>(u), mode, tension);
            return 1.0 / periodForNormalized(ofLerp(p0.value, p1.value, shaped));
        };
        // Simpson in s with u = t * (1 - cos(pi s)) / 2: samples cluster at both ends,
        // where steep Log/Exp and Sigmoid shapes have their sharp corners.
        auto integrand = [&](double s) {
            const double u = t * 0.5 * (1.0 - std::cos(PI * s));
            const double du = t * 0.5 * PI * std::sin(PI * s);
            return inversePeriod(u) * du;
        };
        const double h = 1.0 / kLfoSimpsonIntervals;
        double sum = integrand(0.0) + integrand(1.0);
        for(int k = 1; k < kLfoSimpsonIntervals; ++k) {
            sum += (k % 2 == 1 ? 4.0 : 2.0) * integrand(k * h);
        }
        return span * h / 3.0 * sum;
    }
};

struct LfoCyclesCache {
    uint64_t signature = 0;
    std::vector<double> atPoint; // cycles from the first point to point i
};

uint64_t lfoCurveSignature(const ofxOceanodeTimelineLane& lane, const LfoParameterDefinition& definition) {
    uint64_t h = 1469598103934665603ull; // FNV-1a over the curve data
    auto mixBytes = [&h](const void* data, size_t size) {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        for(size_t i = 0; i < size; ++i) { h ^= bytes[i]; h *= 1099511628211ull; }
    };
    for(const auto& point : lane.curvePoints) { mixBytes(&point.beat, sizeof(point.beat)); mixBytes(&point.value, sizeof(point.value)); }
    for(const auto& tension : lane.curveTensions) { mixBytes(&tension.inflection, sizeof(float)); mixBytes(&tension.steepness, sizeof(float)); }
    mixBytes(lane.curveInterpolation.data(), lane.curveInterpolation.size());
    mixBytes(&definition.minimum, sizeof(float));
    mixBytes(&definition.maximum, sizeof(float));
    const size_t count = lane.curvePoints.size();
    mixBytes(&count, sizeof(count));
    return h;
}

// Cycles from source beat 0 to sourceBeat.
double lfoCycles(const ofxOceanodeTimelineClip& clip, double sourceBeat) {
    const auto* definition = lfoParameterDefinition("frequency");
    const ofxOceanodeTimelineLane* frequencyLane = nullptr;
    for(const auto& lane : clip.lanes) {
        if(lane.lfoParameter == "frequency") { frequencyLane = &lane; break; }
    }
    if(definition != nullptr && frequencyLane != nullptr && frequencyLane->curveInterpolation == "Value") {
        const double period = std::max<double>(kMinLfoPeriodBeats, lfoMapNormalized(definition->minimum,
            definition->maximum, true, frequencyLane->lfoValue));
        return sourceBeat / period; // constant frequency
    }
    if(definition == nullptr || frequencyLane == nullptr || frequencyLane->curvePoints.empty()) {
        const double period = definition == nullptr ? 4.0 : std::max<double>(kMinLfoPeriodBeats, definition->defaultValue);
        return sourceBeat / period;
    }

    LfoPeriodCurve curve;
    curve.definition = definition;
    curve.lane = frequencyLane;
    curve.mode = curveInterpolationMode(frequencyLane->curveInterpolation);
    const auto& points = frequencyLane->curvePoints;
    if(points.size() == 1) return sourceBeat / curve.pointPeriod(0);

    // Running totals per point, cached per clip (rebuilt only when the curve changes).
    static thread_local std::map<std::string, LfoCyclesCache> caches;
    if(caches.size() > 256) caches.clear();
    LfoCyclesCache& cache = caches[clip.id];
    const uint64_t signature = lfoCurveSignature(*frequencyLane, *definition);
    if(cache.signature != signature || cache.atPoint.size() != points.size()) {
        cache.signature = signature;
        cache.atPoint.assign(points.size(), 0.0);
        for(size_t i = 1; i < points.size(); ++i) {
            cache.atPoint[i] = cache.atPoint[i - 1] + curve.segmentCycles(i, 1.0);
        }
    }

    // G(b): cycles from the first point to b (signed); constant period outside the points.
    auto G = [&](double b) {
        if(b <= points.front().beat) return (b - points.front().beat) / curve.pointPeriod(0);
        if(b >= points.back().beat) {
            const size_t last = points.size() - 1;
            return cache.atPoint[last] + (b - points.back().beat) / curve.pointPeriod(last);
        }
        const auto right = std::lower_bound(points.begin() + 1, points.end(), b,
            [](const ofxOceanodeTimelineCurvePoint& point, double targetBeat) {
                return point.beat < targetBeat;
            });
        const size_t i = static_cast<size_t>(right - points.begin());
        const double span = std::max(1e-9, points[i].beat - points[i - 1].beat);
        return cache.atPoint[i - 1] + curve.segmentCycles(i, (b - points[i - 1].beat) / span);
    };
    return G(sourceBeat) - G(0.0);
}

} // namespace

namespace ofxOceanodeTimelineLfo {

float laneValueFromNormalized(const ofxOceanodeTimelineLane& lane, float normalized) {
    return static_cast<float>(lfoMapNormalized(lane.valueMin, lane.valueMax, lfoLaneIsLog(lane), normalized));
}

float laneNormalizedFromValue(const ofxOceanodeTimelineLane& lane, float value) {
    return lfoUnmapValue(lane.valueMin, lane.valueMax, lfoLaneIsLog(lane), value);
}

const std::vector<float>& frequencyGridValues(int mode) {
    struct Grids {
        std::vector<float> values[3];
        Grids() {
            const auto* definition = lfoParameterDefinition("frequency");
            const float lo = definition != nullptr ? definition->minimum : 0.125f;
            const float hi = definition != nullptr ? definition->maximum : 64.0f;
            for(int m = 0; m < 3; ++m) {
                for(int exponent = -4; exponent <= 7; ++exponent) {
                    const float value = std::ldexp(1.0f, exponent) * kFrequencyGridFactors[m];
                    if(value >= lo - 1e-6f && value <= hi + 1e-6f) values[m].push_back(value);
                }
            }
        }
    };
    static const Grids grids;
    return grids.values[std::max(0, std::min(2, mode))];
}

std::string frequencyGridLabel(float beats, int mode) {
    const int m = std::max(0, std::min(2, mode));
    const float base = beats / kFrequencyGridFactors[m]; // the straight length it derives from
    std::string label = base < 1.0f ? "1/" + ofToString(static_cast<int>(std::lround(1.0f / base)))
                                    : ofToString(static_cast<int>(std::lround(base)));
    if(m == 1) label += "T";
    else if(m == 2) label += ".";
    return label;
}

float snapFrequencyValue(float beats, int mode) {
    // Nearest grid value in log distance (musically: nearest note length of the mode).
    const auto& grid = frequencyGridValues(mode);
    float best = grid.front();
    double bestDistance = 1e30;
    for(float g : grid) {
        const double distance = std::abs(std::log(std::max(1e-6f, beats) / g));
        if(distance < bestDistance) { bestDistance = distance; best = g; }
    }
    return best;
}

float evaluateParameter(const ofxOceanodeTimelineClip& clip,
                        const std::string& parameter, double sourceBeat,
                        float fallback) {
    const auto* definition = lfoParameterDefinition(parameter);
    if(definition == nullptr) return fallback;
    for(const auto& lane : clip.lanes) {
        if(lane.lfoParameter != parameter) continue;
        const float defaultNormalized = lfoUnmapValue(definition->minimum, definition->maximum,
                                                      lfoLaneIsLog(lane), definition->defaultValue);
        const float normalized = normalizedLfoLaneValue(lane, sourceBeat, defaultNormalized);
        return static_cast<float>(lfoMapNormalized(definition->minimum, definition->maximum,
                                                   lfoLaneIsLog(lane), normalized));
    }
    return fallback;
}

double cyclesAt(const ofxOceanodeTimelineClip& clip, double sourceBeat) {
    return lfoCycles(clip, sourceBeat);
}

float evaluate(const ofxOceanodeTimelineClip& clip, double sourceBeat) {
    // Phase from the integrated frequency curve (see lfoCycles), wrapped in double
    // precision before handing it to the oscillator.
    const double cycles = lfoCycles(clip, sourceBeat);
    const double phase = cycles - std::floor(cycles);
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
    return oscillator.computeFunc(static_cast<float>(phase));
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
        result.reserve(tokens.size());
        for(const auto& token : tokens) result.push_back(ofToFloat(token));
        parameter.cast<std::vector<float>>().getParameter().set(result);
        return true;
    }
    if(valueType == typeid(std::vector<int>).name()) {
        std::vector<int> result;
        result.reserve(tokens.size());
        for(const auto& token : tokens) result.push_back(ofToInt(token));
        parameter.cast<std::vector<int>>().getParameter().set(result);
        return true;
    }
    if(valueType == typeid(std::vector<bool>).name()) {
        std::vector<bool> result;
        result.reserve(tokens.size());
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

// Curve and Step lanes deliberately produce one scalar stream. When their
// destination is a numeric vector, keep the parameter's current width and
// broadcast that scalar to every component instead of shrinking the vector
// to one item. Explicit multi-component lanes already return comma-separated
// values and pass through untouched.
std::string broadcastScalarNumericVectorValue(ofxOceanodeAbstractParameter& parameter,
                                              const std::string& value,
                                              const std::string& valueType) {
    const bool floatVector = valueType == typeid(std::vector<float>).name();
    const bool intVector = valueType == typeid(std::vector<int>).name();
    const bool boolVector = valueType == typeid(std::vector<bool>).name();
    if(!floatVector && !intVector && !boolVector) return value;
    const auto tokens = ofSplitString(value, ",", true, true);
    if(tokens.size() != 1) return value;

    size_t componentCount = 0;
    if(floatVector) componentCount = parameter.cast<std::vector<float>>().getParameter().get().size();
    else if(intVector) componentCount = parameter.cast<std::vector<int>>().getParameter().get().size();
    else componentCount = parameter.cast<std::vector<bool>>().getParameter().get().size();
    if(componentCount <= 1) return value;

    std::string result;
    result.reserve((tokens.front().size() + 1) * componentCount);
    for(size_t component = 0; component < componentCount; ++component) {
        if(component > 0) result += ",";
        result += tokens.front();
    }
    return result;
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

bool stepProbabilityPasses(const ofxOceanodeTimelineStep& step, double cycle, int seed) {
    const float probability = ofClamp(step.probability, 0.0f, 1.0f);
    if(probability <= 0.0f) return false;
    if(probability >= 1.0f) return true;

    // The result must remain stable while the playhead is inside a step. A
    // per-frame ofRandom() roll would make automation flicker at audio/frame
    // rate. This small integer hash gives one repeatable roll per step/cycle.
    const int64_t start = static_cast<int64_t>(std::llround(step.startBeat * 960.0));
    const int64_t cycleIndex = static_cast<int64_t>(std::llround(cycle));
    uint64_t hash = static_cast<uint64_t>(start) ^ (static_cast<uint64_t>(cycleIndex) * 0x9e3779b97f4a7c15ULL);
    hash ^= static_cast<uint64_t>(static_cast<uint32_t>(seed)) * 0xd6e8feb86659fd93ULL;
    hash ^= hash >> 30;
    hash *= 0xbf58476d1ce4e5b9ULL;
    hash ^= hash >> 27;
    hash *= 0x94d049bb133111ebULL;
    hash ^= hash >> 31;
    const float roll = static_cast<float>(hash & 0xffffULL) / 65535.0f;
    return roll < probability;
}

bool pianoProbabilityPasses(const ofxOceanodeTimelinePianoNote& note, double cycle, int seed) {
    const float probability = ofClamp(note.probability, 0.0f, 1.0f);
    if(probability <= 0.0f) return false;
    if(probability >= 1.0f) return true;
    const int64_t start = static_cast<int64_t>(std::llround(note.startBeat * 960.0));
    const int64_t pitch = static_cast<int64_t>(note.pitch);
    const int64_t cycleIndex = static_cast<int64_t>(std::llround(cycle));
    uint64_t hash = static_cast<uint64_t>(start) ^ (static_cast<uint64_t>(pitch) * 0x9e3779b97f4a7c15ULL);
    hash ^= static_cast<uint64_t>(cycleIndex) * 0xbf58476d1ce4e5b9ULL;
    hash ^= static_cast<uint64_t>(static_cast<uint32_t>(seed)) * 0xd6e8feb86659fd93ULL;
    hash ^= hash >> 30;
    hash *= 0xbf58476d1ce4e5b9ULL;
    hash ^= hash >> 27;
    hash *= 0x94d049bb133111ebULL;
    hash ^= hash >> 31;
    return static_cast<float>(hash & 0xffffULL) / 65535.0f < probability;
}

// Retrigger rule shared by steps, piano notes and gate blocks: an event that
// starts exactly where an equal one ends (back to back) is off for the first
// `gap` source beats, so the two read as two triggers. When an equal event
// overlaps the join instead, they merge (no gap).
template<typename Events, typename StartOf, typename EndOf, typename Same>
bool inRetriggerGap(const Events& events, size_t index, double beat, double gap,
                    StartOf startOf, EndOf endOf, Same same, double wrapLength = 0.0) {
    if(gap <= 0.0) return false;
    const double start = startOf(events[index]);
    if(beat >= start + gap) return false;
    bool touching = false;
    for(size_t other = 0; other < events.size(); ++other) {
        if(other == index || !same(events[other], events[index])) continue;
        const double otherStart = startOf(events[other]);
        const double otherEnd = endOf(events[other]);
        if(otherStart < start - 1e-6 && otherEnd > start + 1e-6) return false; // overlapping: merge
        if(std::abs(otherEnd - start) <= 1e-6) touching = true;
        // Pattern wrap: an event ending at the pattern end touches one at 0.
        if(wrapLength > 0.0 && start <= 1e-6 && std::abs(otherEnd - wrapLength) <= 1e-6) touching = true;
    }
    return touching;
}

bool evaluateStepSequencer(const ofxOceanodeTimelineLane& lane,
                           double localBeat,
                           std::string& value,
                           double retriggerGap = 0.0) {
    if(lane.behavior == "Mute") return false;
    const double cellLength = std::max(1.0 / 24.0, lane.beatsPerStep);
    const double patternLength = std::max(cellLength, lane.stepCount * cellLength);
    const double cycle = std::floor(std::max(0.0, localBeat) / patternLength);
    const double patternBeat = positiveModulo(localBeat, patternLength);
    auto startOf = [](const ofxOceanodeTimelineStep& s) { return std::max(0.0, s.startBeat); };
    auto endOf = [&](const ofxOceanodeTimelineStep& s) {
        return std::max(0.0, s.startBeat) + (s.durationBeats > kEpsilon ? s.durationBeats : cellLength);
    };
    // Only gate steps (no value, or 1) retrigger: repeated value steps (a scene
    // index, say) must not dip to 0 between them.
    auto isGate = [](const ofxOceanodeTimelineStep& s) { return s.value.empty() || s.value == "1"; };
    auto same = [&](const ofxOceanodeTimelineStep& a, const ofxOceanodeTimelineStep& b) {
        return isGate(a) && isGate(b);
    };

    const auto& steps = lane.step.steps;
    for(size_t index = 0; index < steps.size(); ++index) {
        const auto& step = steps[index];
        const double start = startOf(step);
        if(patternBeat + kEpsilon < start || patternBeat >= endOf(step) - kEpsilon) continue;
        if(inRetriggerGap(steps, index, patternBeat, retriggerGap, startOf, endOf, same, patternLength)) continue;
        const bool useProbability = lane.probabilityEnabled && lane.behavior == "Probability";
        if(useProbability && !stepProbabilityPasses(step, cycle, lane.probabilitySeed)) return false;
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
    // A cell holds a REAL value in the lane's own valueMin..valueMax units
    // (that is the whole point of this lane type, and what the editor writes
    // and the previews read). Every lane evaluator here returns a normalized
    // 0..1 value that the caller maps back through the lane's range, so undo
    // the range here rather than handing the raw value over as if it were
    // already normalized -- doing that clamped anything above 1 and then
    // re-expanded it to valueMax, so a 0..127 lane answered 127 for every
    // cell a user could actually paint. No clamp: the round trip is then
    // exact for values outside the range too.
    const float span = lane.valueMax - lane.valueMin;
    value = ofToString(std::abs(span) < 1e-9f ? 0.0f : (raw - lane.valueMin) / span);
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

bool evaluateMultiGateRow(const std::vector<ofxOceanodeTimelineGateRegion>& row, double localBeat,
                          double retriggerGap = 0.0, double wrapLength = 0.0) {
    auto startOf = [](const ofxOceanodeTimelineGateRegion& r) { return r.startBeat; };
    auto endOf = [](const ofxOceanodeTimelineGateRegion& r) { return r.end(); };
    auto same = [](const ofxOceanodeTimelineGateRegion&, const ofxOceanodeTimelineGateRegion&) { return true; };
    for(size_t index = 0; index < row.size(); ++index) {
        const auto& region = row[index];
        if(localBeat < region.startBeat || localBeat >= region.end()) continue;
        if(inRetriggerGap(row, index, localBeat, retriggerGap, startOf, endOf, same, wrapLength)) continue;
        return true;
    }
    return false;
}

// Source beats per timeline beat of a clip (to express the retrigger gap,
// a timeline duration, in the clip's own beats).
double sourcePerTimelineBeat(const ofxOceanodeTimelineClip& clip) {
    if(clip.repeatContent) return 1.0 / std::max(1.0 / 1024.0, clip.contentStretch);
    return std::max(1.0 / 24.0, clip.contentDurationBeats) / std::max(1.0 / 24.0, clip.durationBeats);
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

void ofxOceanodeTimelineManager::invalidateEvaluationIndexes() {
    evaluationIndexesValid = false;
    bindingsByPathCache.clear();
    trackByIdCache.clear();
    trackEvaluationIndexes.clear();
    pianoPitchRangesCache.clear();
    zeroWhenInactiveBindingsCache.clear();
    continuouslyEvaluatedPathsCache.clear();
    parameterTrackColorCache.clear();
    parameterTrackColorCacheFrame = std::numeric_limits<uint64_t>::max();
}

void ofxOceanodeTimelineManager::rebuildParameterTrackColorCache() const {
    parameterTrackColorCache.clear();
    size_t bindingCount = 0;
    for(const auto& track : tracks) bindingCount += track.bindings.size();
    parameterTrackColorCache.reserve(bindingCount);
    // Preserve the previous first-track-wins behaviour for parameters that
    // are published to more than one track.
    for(const auto& track : tracks) {
        for(const auto& binding : track.bindings)
            parameterTrackColorCache.emplace(binding.parameterPath, track.color);
    }
    parameterTrackColorCacheFrame = static_cast<uint64_t>(ofGetFrameNum());
}

void ofxOceanodeTimelineManager::rebuildEvaluationIndexes() {
    bindingsByPathCache.clear();
    trackByIdCache.clear();
    trackEvaluationIndexes.clear();
    pianoPitchRangesCache.clear();
    zeroWhenInactiveBindingsCache.clear();
    continuouslyEvaluatedPathsCache.clear();

    size_t bindingCount = 0;
    for(const auto& track : tracks) bindingCount += track.bindings.size();
    bindingsByPathCache.reserve(bindingCount);
    trackByIdCache.reserve(tracks.size());
    trackEvaluationIndexes.reserve(tracks.size());

    for(auto& track : tracks) {
        trackByIdCache.emplace(track.id, &track);
        auto& index = trackEvaluationIndexes[&track];
        index.bindingById.reserve(track.bindings.size());
        index.clipOrder.reserve(track.clips.size());
        for(auto& binding : track.bindings) {
            index.bindingById.emplace(binding.id, &binding);
            bindingsByPathCache[binding.parameterPath].push_back(&binding);
        }
        for(size_t clipIndex = 0; clipIndex < track.clips.size(); ++clipIndex)
            index.clipOrder.emplace(&track.clips[clipIndex], clipIndex);
    }

    // These properties depend only on the timeline structure. Cache them once
    // for the current update instead of rediscovering them for the playhead and
    // again for every scheduler lookahead probe.
    for(const auto& track : tracks) {
        const auto trackIt = trackEvaluationIndexes.find(&track);
        if(trackIt == trackEvaluationIndexes.end()) continue;
        const auto bindingFor = [&](const std::string& bindingId) -> const ofxOceanodeTimelineParameterBinding* {
            const auto bindingIt = trackIt->second.bindingById.find(bindingId);
            return bindingIt == trackIt->second.bindingById.end() ? nullptr : bindingIt->second;
        };
        for(const auto& binding : track.bindings)
            if(binding.hasLiveOverride) continuouslyEvaluatedPathsCache.emplace(binding.parameterPath);
        for(const auto& clip : track.clips) {
            if(clip.isLfo) {
                if(const auto* binding = bindingFor(clip.lfoOutputBindingId))
                    continuouslyEvaluatedPathsCache.emplace(binding->parameterPath);
                continue;
            }
            for(const auto& lane : clip.lanes) {
                if(lane.type == ofxOceanodeTimelineLaneType::Curve) {
                    for(const auto& bindingId : lane.bindingIds)
                        if(const auto* binding = bindingFor(bindingId))
                            continuouslyEvaluatedPathsCache.emplace(binding->parameterPath);
                    continue;
                }
                if(lane.type != ofxOceanodeTimelineLaneType::PianoRoll) continue;
                if(const auto* binding = bindingFor(lane.pianoPitchBindingId)) {
                    auto& range = pianoPitchRangesCache[binding->parameterPath];
                    range.valueType = binding->valueType;
                    range.low = std::min(range.low, static_cast<int>(ofClamp(lane.pianoLowPitch, 0, 127)));
                    range.high = std::max(range.high, static_cast<int>(ofClamp(lane.pianoHighPitch, 0, 127)));
                }
                if(const auto* binding = bindingFor(lane.pianoGateBindingId))
                    zeroWhenInactiveBindingsCache.emplace(binding);
                if(const auto* binding = bindingFor(lane.pianoVelocityBindingId))
                    zeroWhenInactiveBindingsCache.emplace(binding);
            }
        }
    }
    evaluationIndexesValid = true;
    rebuildParameterTrackColorCache();
}

void ofxOceanodeTimelineManager::setContainer(ofxOceanodeContainer* owner) {
    if(container == owner) return;
    for(const auto& track : tracks) clearTimelineFlag(track);
    container = owner;
    invalidateEvaluationIndexes();
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
    invalidateEvaluationIndexes();
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
    if(track == nullptr || !track->isWaveTrack) return 1.0f;
    // The automation toggle is authoritative: with it off the track plays at
    // its plain gain, whatever points happen to be stored.
    if(!track->waveVolumeAutomationEnabled || track->waveVolumePoints.empty())
        return ofClamp(track->waveVolume, 0.0f, 4.0f);
    return ofClamp(valueAtBeat(track->waveVolumePoints, track->waveVolumeTensions,
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
    invalidateSchedule(); // correct queued events while the track still exists
    auto it = std::find_if(tracks.begin(), tracks.end(), [&](const auto& track) { return track.id == trackId; });
    if(it == tracks.end()) return false;
    std::set<std::string> affectedPaths;
    for(const auto& binding : it->bindings) {
        affectedPaths.insert(binding.parameterPath);
        stateCaptureManualOverrideKeys.erase(trackId + "\x1f" + binding.id);
        stateCaptureLastAppliedValues.erase(binding.parameterPath);
    }
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
    invalidateEvaluationIndexes();
    for(const auto& path : affectedPaths) refreshTimelineFlag(path);
    return true;
}

ofxOceanodeTimelineTrack* ofxOceanodeTimelineManager::getTrack(const std::string& trackId) {
    if(evaluationIndexesValid) {
        const auto cached = trackByIdCache.find(trackId);
        if(cached != trackByIdCache.end()) return cached->second;
    }
    auto it = std::find_if(tracks.begin(), tracks.end(), [&](auto& track) { return track.id == trackId; });
    return it == tracks.end() ? nullptr : &*it;
}

const ofxOceanodeTimelineTrack* ofxOceanodeTimelineManager::getTrack(const std::string& trackId) const {
    if(evaluationIndexesValid) {
        const auto cached = trackByIdCache.find(trackId);
        if(cached != trackByIdCache.end()) return cached->second;
    }
    auto it = std::find_if(tracks.begin(), tracks.end(), [&](const auto& track) { return track.id == trackId; });
    return it == tracks.end() ? nullptr : &*it;
}

std::string ofxOceanodeTimelineManager::addBinding(const std::string& trackId,
                                                   ofxOceanodeAbstractParameter& parameter,
                                                   ofxOceanodeTimelineAutomationMode mode) {
    auto* track = getTrack(trackId);
    if(track == nullptr || track->isWaveTrack || container == nullptr || !isStepLaneCompatible(parameter)) return std::string();

    const std::string path = container->getTimelineParameterPath(parameter);
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
    invalidateEvaluationIndexes();

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
    const std::string path = container->getTimelineParameterPath(const_cast<ofxOceanodeAbstractParameter&>(parameter));
    if(parameterTrackColorCacheFrame != static_cast<uint64_t>(ofGetFrameNum()))
        rebuildParameterTrackColorCache();
    return parameterTrackColorCache.find(path) != parameterTrackColorCache.end();
}

bool ofxOceanodeTimelineManager::getParameterTrackColor(const ofxOceanodeAbstractParameter& parameter, ofColor& color) const {
    if(container == nullptr) return false;
    const std::string path = container->getTimelineParameterPath(const_cast<ofxOceanodeAbstractParameter&>(parameter));
    if(parameterTrackColorCacheFrame != static_cast<uint64_t>(ofGetFrameNum()))
        rebuildParameterTrackColorCache();
    const auto it = parameterTrackColorCache.find(path);
    if(it == parameterTrackColorCache.end()) return false;
    color = it->second;
    return true;
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

bool ofxOceanodeTimelineManager::moveBinding(const std::string& trackId, const std::string& bindingId, int newIndex) {
    auto* track = getTrack(trackId);
    if(track == nullptr || track->bindings.empty()) return false;
    auto it = std::find_if(track->bindings.begin(), track->bindings.end(),
                           [&](const auto& binding) { return binding.id == bindingId; });
    if(it == track->bindings.end()) return false;
    const int from = static_cast<int>(it - track->bindings.begin());
    const int to = std::max(0, std::min(newIndex, static_cast<int>(track->bindings.size()) - 1));
    if(from == to) return false;
    auto binding = std::move(*it);
    track->bindings.erase(it);
    track->bindings.insert(track->bindings.begin() + to, std::move(binding));
    normalizeNoteGroups(*track);
    invalidateEvaluationIndexes();
    return true;
}


bool ofxOceanodeTimelineManager::setBindingOrder(const std::string& trackId, const std::vector<std::string>& orderedBindingIds) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return false;
    std::vector<ofxOceanodeTimelineParameterBinding> ordered;
    ordered.reserve(track->bindings.size());
    for(const auto& id : orderedBindingIds) {
        auto it = std::find_if(track->bindings.begin(), track->bindings.end(), [&](const auto& b) { return b.id == id; });
        if(it == track->bindings.end()) continue;
        ordered.push_back(std::move(*it));
        track->bindings.erase(it);
    }
    for(auto& remaining : track->bindings) ordered.push_back(std::move(remaining));
    track->bindings = std::move(ordered);
    normalizeNoteGroups(*track);
    invalidateEvaluationIndexes();
    return true;
}

const ofxOceanodeTimelineNoteGroup* ofxOceanodeTimelineManager::findNoteGroupForBinding(
    const ofxOceanodeTimelineTrack& track, const std::string& bindingId) {
    for(const auto& group : track.noteGroups) if(group.contains(bindingId)) return &group;
    return nullptr;
}

ofxOceanodeTimelineNoteGroup* ofxOceanodeTimelineManager::getNoteGroup(const std::string& trackId, const std::string& groupId) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return nullptr;
    for(auto& group : track->noteGroups) if(group.id == groupId) return &group;
    return nullptr;
}

void ofxOceanodeTimelineManager::normalizeNoteGroups(ofxOceanodeTimelineTrack& track) {
    auto hasBinding = [&](const std::string& id) {
        return std::any_of(track.bindings.begin(), track.bindings.end(), [&](const auto& b) { return b.id == id; });
    };
    for(auto& group : track.noteGroups) {
        for(auto* role : {&group.pitchBindingId, &group.gateBindingId, &group.velocityBindingId})
            if(!role->empty() && !hasBinding(*role)) role->clear();
        if(group.gateBindingId == group.pitchBindingId) group.gateBindingId.clear();
        if(group.velocityBindingId == group.pitchBindingId || group.velocityBindingId == group.gateBindingId)
            group.velocityBindingId.clear();
    }
    track.noteGroups.erase(std::remove_if(track.noteGroups.begin(), track.noteGroups.end(),
                                          [](const auto& group) { return group.members().empty(); }),
                           track.noteGroups.end());
    for(const auto& group : track.noteGroups) {
        const auto members = group.members();
        size_t insertAt = track.bindings.size();
        for(size_t i = 0; i < track.bindings.size(); ++i) {
            if(group.contains(track.bindings[i].id)) { insertAt = i; break; }
        }
        std::vector<ofxOceanodeTimelineParameterBinding> extracted;
        for(const auto& id : members) {
            auto it = std::find_if(track.bindings.begin(), track.bindings.end(), [&](const auto& b) { return b.id == id; });
            if(it == track.bindings.end()) continue;
            if(static_cast<size_t>(it - track.bindings.begin()) < insertAt) --insertAt;
            extracted.push_back(std::move(*it));
            track.bindings.erase(it);
        }
        insertAt = std::min(insertAt, track.bindings.size());
        track.bindings.insert(track.bindings.begin() + insertAt,
                              std::make_move_iterator(extracted.begin()), std::make_move_iterator(extracted.end()));
    }
}

void ofxOceanodeTimelineManager::syncNoteGroupLanes(ofxOceanodeTimelineTrack& track, const ofxOceanodeTimelineNoteGroup& group,
                                                    const std::vector<std::string>& affectedBindingIds) {
    const auto members = group.members();
    for(auto& clip : track.clips) {
        for(auto& lane : clip.lanes) {
            if(lane.type != ofxOceanodeTimelineLaneType::PianoRoll) continue;
            const bool uses = std::any_of(lane.bindingIds.begin(), lane.bindingIds.end(), [&](const auto& id) {
                return std::find(affectedBindingIds.begin(), affectedBindingIds.end(), id) != affectedBindingIds.end();
            });
            if(!uses) continue;
            lane.bindingIds = members;
            lane.pianoPitchBindingId = group.pitchBindingId;
            lane.pianoGateBindingId = group.gateBindingId;
            lane.pianoVelocityBindingId = group.velocityBindingId;
        }
    }
}

std::string ofxOceanodeTimelineManager::createNoteGroup(const std::string& trackId, const std::string& name,
                                                        const std::string& pitchBindingId, const std::string& gateBindingId,
                                                        const std::string& velocityBindingId) {
    auto* track = getTrack(trackId);
    if(track == nullptr || track->isWaveTrack) return std::string();
    ofxOceanodeTimelineNoteGroup group;
    uint64_t number = 1;
    do {
        group.id = makeId("timeline_notes", number++);
    } while(std::any_of(track->noteGroups.begin(), track->noteGroups.end(), [&](const auto& g) { return g.id == group.id; }));
    group.name = name.empty() ? std::string("Notes") : name;
    track->noteGroups.push_back(group);
    const std::string id = group.id;
    if(!setNoteGroupRoles(trackId, id, pitchBindingId, gateBindingId, velocityBindingId)) return std::string();
    return getNoteGroup(trackId, id) != nullptr ? id : std::string();
}

bool ofxOceanodeTimelineManager::setNoteGroupRoles(const std::string& trackId, const std::string& groupId,
                                                   const std::string& pitchBindingId, const std::string& gateBindingId,
                                                   const std::string& velocityBindingId) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return false;
    auto groupIt = std::find_if(track->noteGroups.begin(), track->noteGroups.end(), [&](const auto& g) { return g.id == groupId; });
    if(groupIt == track->noteGroups.end()) return false;
    const std::vector<std::string> newMembers = {pitchBindingId, gateBindingId, velocityBindingId};
    // A binding leaves any other group it was in (that group's lanes stop using it).
    for(auto& other : track->noteGroups) {
        if(other.id == groupId) continue;
        const auto before = other.members();
        bool changed = false;
        for(auto* role : {&other.pitchBindingId, &other.gateBindingId, &other.velocityBindingId}) {
            if(!role->empty() && std::find(newMembers.begin(), newMembers.end(), *role) != newMembers.end()) {
                role->clear();
                changed = true;
            }
        }
        if(changed) syncNoteGroupLanes(*track, other, before);
    }
    auto& group = *std::find_if(track->noteGroups.begin(), track->noteGroups.end(), [&](const auto& g) { return g.id == groupId; });
    std::vector<std::string> affected = group.members();
    group.pitchBindingId = pitchBindingId;
    group.gateBindingId = gateBindingId;
    group.velocityBindingId = velocityBindingId;
    const auto members = group.members();
    affected.insert(affected.end(), members.begin(), members.end());
    const auto groupCopy = group;
    syncNoteGroupLanes(*track, groupCopy, affected);
    normalizeNoteGroups(*track);
    invalidateEvaluationIndexes();
    return true;
}

bool ofxOceanodeTimelineManager::renameNoteGroup(const std::string& trackId, const std::string& groupId, const std::string& name) {
    auto* group = getNoteGroup(trackId, groupId);
    if(group == nullptr || name.empty()) return false;
    group->name = name;
    return true;
}

bool ofxOceanodeTimelineManager::removeNoteGroup(const std::string& trackId, const std::string& groupId, bool removeBindings) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return false;
    auto it = std::find_if(track->noteGroups.begin(), track->noteGroups.end(), [&](const auto& g) { return g.id == groupId; });
    if(it == track->noteGroups.end()) return false;
    const auto members = it->members();
    track->noteGroups.erase(it);
    if(removeBindings) {
        for(const auto& id : members) removeBinding(trackId, id);
    }
    return true;
}

std::string ofxOceanodeTimelineManager::createNoteGroupClip(const std::string& trackId, const std::string& groupId,
                                                            const std::string& requestedName, double startBeat,
                                                            double durationBeats, std::string* laneIdOut) {
    const auto* groupPtr = getNoteGroup(trackId, groupId);
    if(groupPtr == nullptr) return std::string();
    const auto group = *groupPtr;
    const std::string clipId = createClip(trackId, requestedName.empty() ? group.name : requestedName, startBeat, durationBeats);
    if(clipId.empty()) return clipId;
    const std::string laneId = createLane(trackId, clipId, group.name, ofxOceanodeTimelineLaneType::PianoRoll);
    if(laneId.empty()) return clipId;
    if(auto* lane = getLane(trackId, clipId, laneId)) {
        lane->pianoPitchBindingId = group.pitchBindingId;
        lane->pianoGateBindingId = group.gateBindingId;
        lane->pianoVelocityBindingId = group.velocityBindingId;
        lane->bindingIds = group.members();
        // Show the keys the pitch parameter can actually play, when that is a note range.
        lane->pianoLowPitch = 36;
        lane->pianoHighPitch = 84;
        if(const auto* pitchBinding = getBinding(trackId, group.pitchBindingId)) {
            if(auto* parameter = container == nullptr ? nullptr : container->findTimelineParameter(pitchBinding->parameterPath)) {
                float minimum = 0.0f, maximum = 0.0f;
                bool numeric = true;
                if(pitchBinding->valueType == typeid(float).name()) {
                    minimum = parameter->cast<float>().getParameter().getMin();
                    maximum = parameter->cast<float>().getParameter().getMax();
                } else if(pitchBinding->valueType == typeid(int).name()) {
                    minimum = static_cast<float>(parameter->cast<int>().getParameter().getMin());
                    maximum = static_cast<float>(parameter->cast<int>().getParameter().getMax());
                } else numeric = false;
                // Another piano roll may have narrowed it already: use the parameter's own range.
                const auto saved = savedPianoRanges.find(pitchBinding->parameterPath);
                if(numeric && saved != savedPianoRanges.end()) {
                    if(pitchBinding->valueType == typeid(float).name()) {
                        minimum = saved->second.floatMin; maximum = saved->second.floatMax;
                    } else {
                        minimum = static_cast<float>(saved->second.intMin); maximum = static_cast<float>(saved->second.intMax);
                    }
                }
                if(numeric && minimum >= 0.0f && maximum <= 127.0f && maximum - minimum >= 12.0f) {
                    lane->pianoLowPitch = static_cast<int>(std::ceil(minimum));
                    lane->pianoHighPitch = static_cast<int>(std::floor(maximum));
                }
            }
        }
    }
    if(laneIdOut != nullptr) *laneIdOut = laneId;
    return clipId;
}

bool ofxOceanodeTimelineManager::removeBinding(const std::string& trackId, const std::string& bindingId) {
    invalidateSchedule(); // correct queued events while the binding still exists
    auto* track = getTrack(trackId);
    if(track == nullptr) return false;
    auto it = std::find_if(track->bindings.begin(), track->bindings.end(), [&](const auto& binding) { return binding.id == bindingId; });
    if(it == track->bindings.end()) return false;
    const std::string parameterPath = it->parameterPath;
    stateCaptureManualOverrideKeys.erase(trackId + "\x1f" + bindingId);
    stateCaptureLastAppliedValues.erase(parameterPath);
    for(auto& clip : track->clips) {
        // An LFO clip points at its output binding from the clip itself, not
        // through a lane -- without this it kept a dangling id and silently
        // stopped driving anything.
        if(clip.lfoOutputBindingId == bindingId) clip.lfoOutputBindingId.clear();
        for(auto& lane : clip.lanes) {
            lane.bindingIds.erase(std::remove(lane.bindingIds.begin(), lane.bindingIds.end(), bindingId), lane.bindingIds.end());
            if(lane.pianoPitchBindingId == bindingId) lane.pianoPitchBindingId.clear();
            if(lane.pianoGateBindingId == bindingId) lane.pianoGateBindingId.clear();
            if(lane.pianoVelocityBindingId == bindingId) lane.pianoVelocityBindingId.clear();
        }
    }
    track->bindings.erase(it);
    normalizeNoteGroups(*track);
    invalidateEvaluationIndexes();
    refreshTimelineFlag(parameterPath);
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

int ofxOceanodeTimelineManager::captureStateAsCurvePoints(const std::string& trackId,
                                                          const std::string& bindingId,
                                                          double timelineBeat) {
    auto* track = getTrack(trackId);
    if(track == nullptr || track->isWaveTrack || container == nullptr) return 0;

    struct CaptureTarget {
        std::string bindingId;
        std::string parameterPath;
        std::string valueType;
        float value = 0.0f;
        float rangeMin = 0.0f;
        float rangeMax = 1.0f;
        bool boundedRange = true;
    };
    std::vector<CaptureTarget> targets;
    targets.reserve(bindingId.empty() ? track->bindings.size() : 1);

    for(const auto& binding : track->bindings) {
        if(!bindingId.empty() && binding.id != bindingId) continue;
        auto* parameter = container->findTimelineParameter(binding.parameterPath);
        if(parameter == nullptr) continue;

        CaptureTarget target;
        target.bindingId = binding.id;
        target.parameterPath = binding.parameterPath;
        target.valueType = binding.valueType;
        if(binding.valueType == typeid(float).name()) {
            auto& value = parameter->cast<float>().getParameter();
            target.value = value.get();
            target.rangeMin = value.getMin();
            target.rangeMax = value.getMax();
            target.boundedRange = std::isfinite(target.rangeMin) && std::isfinite(target.rangeMax) &&
                target.rangeMax > target.rangeMin &&
                target.rangeMin > std::numeric_limits<float>::lowest() * 0.5f &&
                target.rangeMax < std::numeric_limits<float>::max() * 0.5f;
        } else if(binding.valueType == typeid(int).name()) {
            auto& value = parameter->cast<int>().getParameter();
            target.value = static_cast<float>(value.get());
            target.rangeMin = static_cast<float>(value.getMin());
            target.rangeMax = static_cast<float>(value.getMax());
            target.boundedRange = value.getMin() != std::numeric_limits<int>::lowest() &&
                value.getMax() != std::numeric_limits<int>::max() && value.getMax() > value.getMin();
        } else if(binding.valueType == typeid(bool).name()) {
            target.value = parameter->cast<bool>().getParameter().get() ? 1.0f : 0.0f;
            target.rangeMin = 0.0f;
            target.rangeMax = 1.0f;
        } else if(binding.valueType == typeid(std::vector<float>).name()) {
            auto& value = parameter->cast<std::vector<float>>().getParameter();
            const auto current = value.get();
            const auto minimum = value.getMin();
            const auto maximum = value.getMax();
            target.value = current.empty() ? 0.0f : current.front();
            target.rangeMin = minimum.empty() ? std::min(0.0f, target.value) : minimum.front();
            target.rangeMax = maximum.empty() ? std::max(1.0f, target.value) : maximum.front();
            target.boundedRange = !minimum.empty() && !maximum.empty() &&
                std::isfinite(target.rangeMin) && std::isfinite(target.rangeMax) &&
                target.rangeMax > target.rangeMin &&
                target.rangeMin > std::numeric_limits<float>::lowest() * 0.5f &&
                target.rangeMax < std::numeric_limits<float>::max() * 0.5f;
        } else if(binding.valueType == typeid(std::vector<int>).name()) {
            auto& value = parameter->cast<std::vector<int>>().getParameter();
            const auto current = value.get();
            const auto minimum = value.getMin();
            const auto maximum = value.getMax();
            target.value = current.empty() ? 0.0f : static_cast<float>(current.front());
            target.rangeMin = minimum.empty() ? std::min(0.0f, target.value) : static_cast<float>(minimum.front());
            target.rangeMax = maximum.empty() ? std::max(1.0f, target.value) : static_cast<float>(maximum.front());
            target.boundedRange = !minimum.empty() && !maximum.empty() &&
                minimum.front() != std::numeric_limits<int>::lowest() &&
                maximum.front() != std::numeric_limits<int>::max() && maximum.front() > minimum.front();
        } else if(binding.valueType == typeid(std::vector<bool>).name()) {
            auto& value = parameter->cast<std::vector<bool>>().getParameter();
            const auto current = value.get();
            target.value = !current.empty() && current.front() ? 1.0f : 0.0f;
            target.rangeMin = 0.0f;
            target.rangeMax = 1.0f;
        } else {
            continue; // Text values cannot be interpolated as a curve.
        }

        if(!target.boundedRange) {
            target.rangeMin = std::min(0.0f, target.value);
            target.rangeMax = std::max(1.0f, target.value);
            if(target.rangeMax - target.rangeMin < 1e-6f) target.rangeMax = target.rangeMin + 1.0f;
        }
        targets.push_back(std::move(target));
    }
    if(targets.empty()) return 0;

    timelineBeat = std::max(0.0, timelineBeat);
    constexpr double captureTail = 1.0 / 24.0;
    std::string fallbackCaptureClipId;
    for(const auto& clip : track->clips) {
        if(!clip.isStateCapture) continue;
        fallbackCaptureClipId = clip.id;
        break;
    }
    if(fallbackCaptureClipId.empty()) {
        fallbackCaptureClipId = createClip(trackId, "Captured State", timelineBeat, captureTail);
        auto* created = getClip(trackId, fallbackCaptureClipId);
        if(created == nullptr) return 0;
        created->isStateCapture = true;
        created->repeatContent = false;
    }

    // A shared capture clip remains shared until the user explicitly
    // separates it. Once separated, locate each parameter's own destination
    // by its binding so later track-level captures preserve that independence.
    std::vector<std::string> targetClipIds;
    targetClipIds.reserve(targets.size());
    for(const auto& target : targets) {
        std::string exactClipId;
        if(const auto* currentTrack = getTrack(trackId)) {
            for(const auto& clip : currentTrack->clips) {
                if(!clip.isStateCapture) continue;
                const bool ownsBinding = std::any_of(clip.lanes.begin(), clip.lanes.end(), [&](const auto& lane) {
                    return lane.type == ofxOceanodeTimelineLaneType::Curve &&
                        std::find(lane.bindingIds.begin(), lane.bindingIds.end(), target.bindingId) != lane.bindingIds.end();
                });
                if(ownsBinding) {
                    exactClipId = clip.id;
                    break;
                }
            }
        }
        targetClipIds.push_back(exactClipId.empty() ? fallbackCaptureClipId : exactClipId);
    }

    // Bake any user stretch into absolute point positions before growing each
    // distinct destination clip. This is deliberately per clip: separated
    // parameter clips may have been moved or stretched independently.
    std::unordered_map<std::string, double> captureSourceBeats;
    for(const auto& captureClipId : targetClipIds) {
        if(captureSourceBeats.count(captureClipId) > 0) continue;
        auto* captureClip = getClip(trackId, captureClipId);
        if(captureClip == nullptr) continue;
        std::vector<std::vector<double>> absolutePointBeats(captureClip->lanes.size());
        for(size_t laneIndex = 0; laneIndex < captureClip->lanes.size(); ++laneIndex) {
            const auto& lane = captureClip->lanes[laneIndex];
            absolutePointBeats[laneIndex].reserve(lane.curvePoints.size());
            for(const auto& point : lane.curvePoints)
                absolutePointBeats[laneIndex].push_back(
                    ofxOceanodeTimelineClipTime::sourceToTimelineBeat(*captureClip, point.beat));
        }
        const double oldEnd = captureClip->startBeat + captureClip->durationBeats;
        const double newStart = std::min(captureClip->startBeat, timelineBeat);
        const double newEnd = std::max(oldEnd, timelineBeat + captureTail);
        for(size_t laneIndex = 0; laneIndex < captureClip->lanes.size(); ++laneIndex) {
            auto& lane = captureClip->lanes[laneIndex];
            for(size_t pointIndex = 0; pointIndex < lane.curvePoints.size(); ++pointIndex)
                lane.curvePoints[pointIndex].beat = absolutePointBeats[laneIndex][pointIndex] - newStart;
        }
        captureClip->startBeat = newStart;
        captureClip->durationBeats = std::max(captureTail, newEnd - newStart);
        captureClip->contentDurationBeats = captureClip->durationBeats;
        captureClip->contentStretch = 1.0;
        captureClip->repeatContent = false;
        captureSourceBeats[captureClipId] = timelineBeat - newStart;
    }

    auto remapCurveRange = [](ofxOceanodeTimelineLane& lane, float newMin, float newMax) {
        if(newMax - newMin < 1e-9f) newMax = newMin + 1.0f;
        const float oldMin = lane.valueMin;
        const float oldSpan = lane.valueMax - lane.valueMin;
        const float newSpan = newMax - newMin;
        for(auto& point : lane.curvePoints) {
            const float actual = std::abs(oldSpan) < 1e-9f ? oldMin : oldMin + point.value * oldSpan;
            point.value = ofClamp((actual - newMin) / newSpan, 0.0f, 1.0f);
        }
        lane.valueMin = newMin;
        lane.valueMax = newMax;
    };
    auto setCurvePoint = [](ofxOceanodeTimelineLane& lane, double beat, float value) {
        const auto existing = std::lower_bound(lane.curvePoints.begin(), lane.curvePoints.end(), beat,
            [](const auto& point, double candidate) { return point.beat < candidate; });
        if(existing != lane.curvePoints.end() && std::abs(existing->beat - beat) <= 1e-6) {
            existing->value = value;
            return;
        }
        const size_t oldCount = lane.curvePoints.size();
        auto oldTensions = lane.curveTensions;
        oldTensions.resize(oldCount > 0 ? oldCount - 1 : 0);
        const size_t insertionIndex = static_cast<size_t>(existing - lane.curvePoints.begin());
        lane.curvePoints.insert(existing, {beat, value});
        std::vector<ofxOceanodeTimelineCurveTension> tensions(
            lane.curvePoints.size() > 0 ? lane.curvePoints.size() - 1 : 0);
        for(size_t segment = 0; segment < tensions.size(); ++segment) {
            if((insertionIndex > 0 && segment == insertionIndex - 1) || segment == insertionIndex) continue;
            const size_t oldSegment = segment < insertionIndex ? segment : segment - 1;
            if(oldSegment < oldTensions.size()) tensions[segment] = oldTensions[oldSegment];
        }
        lane.curveTensions = std::move(tensions);
    };

    int captured = 0;
    for(size_t targetIndex = 0; targetIndex < targets.size(); ++targetIndex) {
        const auto& target = targets[targetIndex];
        const auto& captureClipId = targetClipIds[targetIndex];
        auto* captureClip = getClip(trackId, captureClipId); // createLane may move lane storage.
        const auto sourceBeatIt = captureSourceBeats.find(captureClipId);
        if(captureClip == nullptr || sourceBeatIt == captureSourceBeats.end()) continue;
        const double sourceBeat = sourceBeatIt->second;
        ofxOceanodeTimelineLane* lane = nullptr;
        for(auto& candidate : captureClip->lanes) {
            if(candidate.type == ofxOceanodeTimelineLaneType::Curve &&
               std::find(candidate.bindingIds.begin(), candidate.bindingIds.end(), target.bindingId) != candidate.bindingIds.end()) {
                lane = &candidate;
                break;
            }
        }
        if(lane == nullptr) {
            const auto laneId = createLane(trackId, captureClipId, target.parameterPath,
                                           ofxOceanodeTimelineLaneType::Curve);
            if(laneId.empty() || !addBindingToLane(trackId, captureClipId, laneId, target.bindingId)) continue;
            lane = getLane(trackId, captureClipId, laneId);
            if(lane == nullptr) continue;
            lane->curvePoints.clear(); // remove createLane's generic diagonal.
            lane->curveTensions.clear();
            lane->valueMin = target.rangeMin;
            lane->valueMax = target.rangeMax;
            lane->curveInterpolation = target.valueType == typeid(bool).name() ||
                target.valueType == typeid(std::vector<bool>).name() ? "Step" : "Linear";
        } else if(target.boundedRange) {
            if(std::abs(lane->valueMin - target.rangeMin) > 1e-6f ||
               std::abs(lane->valueMax - target.rangeMax) > 1e-6f)
                remapCurveRange(*lane, target.rangeMin, target.rangeMax);
        } else if(target.value < lane->valueMin || target.value > lane->valueMax) {
            remapCurveRange(*lane, std::min(lane->valueMin, target.value),
                            std::max(lane->valueMax, target.value));
        }

        const float span = lane->valueMax - lane->valueMin;
        const float normalized = std::abs(span) < 1e-9f ? 0.0f
            : ofClamp((target.value - lane->valueMin) / span, 0.0f, 1.0f);
        setCurvePoint(*lane, sourceBeat, normalized);

        const std::string overrideKey = trackId + "\x1f" + target.bindingId;
        if(stateCaptureManualOverrideKeys.erase(overrideKey) > 0) {
            if(auto* binding = getBinding(trackId, target.bindingId)) {
                binding->hasLiveOverride = false;
                binding->liveOverrideValue.clear();
            }
        }
        if(auto* parameter = container->findTimelineParameter(target.parameterPath))
            stateCaptureLastAppliedValues[target.parameterPath] = parameterValueForAutomation(*parameter);
        ++captured;
    }

    if(auto* sortedTrack = getTrack(trackId)) {
        std::sort(sortedTrack->clips.begin(), sortedTrack->clips.end(), [](const auto& a, const auto& b) {
            if(std::abs(a.startBeat - b.startBeat) > 1e-9) return a.startBeat < b.startBeat;
            return a.id < b.id;
        });
    }
    invalidateEvaluationIndexes();
    invalidateSchedule();
    return captured;
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
    invalidateEvaluationIndexes();
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
            if(auto* parameter = container->findTimelineParameter(binding->parameterPath)) {
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

    ensureLfoLanes(trackId, clipId);
    return clipId;
}

namespace {
// Cycles started in the first `sourceBeats` of an LFO clip's content: a partial
// final cycle counts as started, so the next segment begins a new cycle index.
double lfoCyclesStarted(const ofxOceanodeTimelineClip& clip, double sourceBeats) {
    if(sourceBeats <= kEpsilon) return 0.0;
    const double cycles = lfoCycles(clip, sourceBeats);
    return cycles <= 1e-9 ? 0.0 : std::ceil(cycles - 1e-9);
}

// Cycles started over a whole clip (all its repeats, including a partial last one).
double lfoClipCyclesStarted(const ofxOceanodeTimelineClip& clip) {
    using namespace ofxOceanodeTimelineClipTime;
    const double content = sourceDuration(clip);
    if(!clip.repeatContent) return lfoCyclesStarted(clip, content);
    const double duration = std::max(1.0 / 24.0, clip.durationBeats);
    const double repeatLength = cycleDuration(clip);
    const double fullRepeats = std::floor(duration / repeatLength + 1e-9);
    const double partialSource = (duration - fullRepeats * repeatLength) / stretch(clip);
    return fullRepeats * lfoCyclesStarted(clip, content) + lfoCyclesStarted(clip, partialSource);
}
} // namespace

ofxOceanodeTimelineLfoSample ofxOceanodeTimelineManager::evaluateLfoTrack(const std::string& trackId,
                                                                         double beat) const {
    ofxOceanodeTimelineLfoSample sample;
    const auto* track = getTrack(trackId);
    if(track == nullptr) return sample;

    std::vector<const ofxOceanodeTimelineClip*> clips;
    for(const auto& clip : track->clips) if(clip.isLfo) clips.push_back(&clip);
    std::sort(clips.begin(), clips.end(), [](const auto* a, const auto* b) { return a->startBeat < b->startBeat; });

    double cyclesBefore = 0.0;
    for(const auto* clip : clips) {
        double sourceBeat = 0.0;
        if(clipSourceBeat(*clip, beat, sourceBeat)) {
            // Earlier repeats of this clip, then the position inside the current one.
            const int64_t repeat = ofxOceanodeTimelineClipTime::cycleIndex(*clip, beat);
            cyclesBefore += static_cast<double>(repeat) *
                lfoCyclesStarted(*clip, ofxOceanodeTimelineClipTime::sourceDuration(*clip));
            const double cycles = lfoCycles(*clip, sourceBeat);
            const double whole = std::floor(cycles);
            sample.active = true;
            sample.cycle = cyclesBefore + whole;
            sample.phase = static_cast<float>(cycles - whole);
            sample.value = ofxOceanodeTimelineLfo::evaluate(*clip, sourceBeat);
            return sample;
        }
        if(beat < clip->startBeat) break; // in a gap before this clip
        cyclesBefore += lfoClipCyclesStarted(*clip);
    }
    sample.cycle = cyclesBefore;
    return sample;
}

void ofxOceanodeTimelineManager::ensureLfoLanes(const std::string& trackId, const std::string& clipId) {
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr || !clip->isLfo) return;
    for(const auto& definition : kLfoParameters) {
        const bool present = std::any_of(clip->lanes.begin(), clip->lanes.end(),
            [&](const auto& lane) { return lane.lfoParameter == definition.id; });
        if(present) continue;
        const auto laneId = createLane(trackId, clipId, definition.name, ofxOceanodeTimelineLaneType::Curve);
        clip = getClip(trackId, clipId); // createLane may reallocate
        if(auto* lane = getLane(trackId, clipId, laneId)) {
            lane->lfoParameter = definition.id;
            lane->valueMin = definition.minimum;
            lane->valueMax = definition.maximum;
            if(lane->lfoParameter == "frequency") lane->lfoSnap = true;
            const float normalized = lfoUnmapValue(definition.minimum, definition.maximum,
                                                   lfoLaneIsLog(*lane), definition.defaultValue);
            lane->curvePoints = {{0.0, normalized}, {clip->contentDurationBeats, normalized}};
            lane->curveTensions.assign(1, ofxOceanodeTimelineCurveTension{});
        }
    }
}

namespace {
// ---- Clip splitting helpers (all positions in a clip's source beats) ----

// The part of a curve between from and to, moved to placeAt. Boundary
// points carry the curve's value there; segments kept whole keep their
// tension, and a cut segment gets the tension that best reproduces its
// part of the original shape.
void appendCurveWindow(const ofxOceanodeTimelineLane& lane, double from, double to, double placeAt,
                       std::vector<ofxOceanodeTimelineCurvePoint>& outPoints,
                       std::vector<ofxOceanodeTimelineCurveTension>& outTensions) {
    auto points = lane.curvePoints;
    std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) { return a.beat < b.beat; });
    const float fallback = points.empty() ? 0.0f : points.front().value;
    auto valueAt = [&](double beat) {
        return valueAtBeat(points, lane.curveTensions, lane.curveInterpolation, beat, fallback);
    };
    const auto mode = curveInterpolationMode(lane.curveInterpolation);
    auto fitTension = [&](double x0, double x1) {
        ofxOceanodeTimelineCurveTension best{};
        if(mode == CurveInterpolationMode::Linear || mode == CurveInterpolationMode::Step) return best;
        const float v0 = valueAt(x0), v1 = valueAt(x1);
        if(std::abs(v1 - v0) < 1e-6f) return best;
        constexpr int kSamples = 16;
        float target[kSamples];
        for(int i = 0; i < kSamples; ++i) target[i] = valueAt(x0 + (x1 - x0) * (i + 1) / (kSamples + 1.0));
        double bestError = 1e30;
        for(int a = 0; a < 50; ++a) {
            const float inflection = 0.01f + a * (0.98f / 49.0f);
            for(int b = 0; b < 48; ++b) {
                const float steepness = 0.1f * std::pow(100.0f, b / 47.0f); // 0.1 .. 10
                const ofxOceanodeTimelineCurveTension candidate{inflection, steepness};
                double error = 0.0;
                for(int i = 0; i < kSamples && error < bestError; ++i) {
                    const float u = static_cast<float>((i + 1) / (kSamples + 1.0));
                    const float d = ofLerp(v0, v1, curveSegmentShape(u, mode, candidate)) - target[i];
                    error += d * d;
                }
                if(error < bestError) { bestError = error; best = candidate; }
            }
        }
        // Refine around the best grid point.
        const ofxOceanodeTimelineCurveTension coarse = best;
        for(int a = -20; a <= 20; ++a) {
            const float inflection = ofClamp(coarse.inflection + a * 0.001f, 0.01f, 0.99f);
            for(int b = -20; b <= 20; ++b) {
                const float steepness = ofClamp(coarse.steepness * std::pow(1.005f, static_cast<float>(b)), 0.1f, 10.0f);
                const ofxOceanodeTimelineCurveTension candidate{inflection, steepness};
                double error = 0.0;
                for(int i = 0; i < kSamples && error < bestError; ++i) {
                    const float u = static_cast<float>((i + 1) / (kSamples + 1.0));
                    const float d = ofLerp(v0, v1, curveSegmentShape(u, mode, candidate)) - target[i];
                    error += d * d;
                }
                if(error < bestError) { bestError = error; best = candidate; }
            }
        }
        return best;
    };
    // Emitted points with their original beat and index (-1: a boundary point).
    struct WindowPoint { double originalBeat; float value; int index; };
    std::vector<WindowPoint> window;
    auto pointIndexAt = [&](double beat) {
        for(size_t i = 0; i < points.size(); ++i)
            if(std::abs(points[i].beat - beat) <= kEpsilon) return static_cast<int>(i);
        return -1;
    };
    window.push_back({from, valueAt(from), pointIndexAt(from)});
    for(size_t i = 0; i < points.size(); ++i) {
        if(points[i].beat > from + kEpsilon && points[i].beat < to - kEpsilon)
            window.push_back({points[i].beat, points[i].value, static_cast<int>(i)});
    }
    window.push_back({to, valueAt(to), pointIndexAt(to)});
    // Joining a second window: the previous window's last point stays and a
    // new point at the same beat starts this one (a jump, as at a cycle wrap).
    if(!outPoints.empty()) outTensions.push_back({});
    for(size_t i = 0; i < window.size(); ++i) {
        if(i > 0) {
            const int a = window[i - 1].index, b = window[i].index;
            const bool wholeSegment = a >= 0 && b == a + 1 && static_cast<size_t>(a) < lane.curveTensions.size();
            outTensions.push_back(wholeSegment ? lane.curveTensions[a]
                                               : fitTension(window[i - 1].originalBeat, window[i].originalBeat));
        }
        outPoints.push_back({placeAt + window[i].originalBeat - from, window[i].value});
    }
}

// Events ([start, start + length)) that overlap [from, to), clipped to it and moved to placeAt.
template<typename Event>
void appendEventWindow(const std::vector<Event>& events, double from, double to, double placeAt,
                       std::vector<Event>& out) {
    for(const auto& event : events) {
        const double start = std::max(event.startBeat, from);
        const double end = std::min(event.startBeat + event.durationBeats, to);
        if(end - start <= 1e-6) continue;
        Event piece = event;
        piece.startBeat = placeAt + start - from;
        piece.durationBeats = end - start;
        out.push_back(piece);
    }
}
}

bool ofxOceanodeTimelineManager::computeClipSplit(const ofxOceanodeTimelineClip& original, double timelineBeat,
                                                  ofxOceanodeTimelineClip& left, ofxOceanodeTimelineClip& right) const {
    const double minimum = 1.0 / 96.0;
    const double clipEnd = original.startBeat + original.durationBeats;
    if(timelineBeat <= original.startBeat + minimum || timelineBeat >= clipEnd - minimum) return false;

    const double content = ofxOceanodeTimelineClipTime::sourceDuration(original);
    const bool repeating = original.repeatContent;
    // Source beat at the cut. A repeating clip keeps its content and the
    // right part starts that far into it; a stretched one-shot is cut there.
    const double cut = ofClamp(ofxOceanodeTimelineClipTime::timelineToSourceBeat(original, timelineBeat), 0.0, content);

    left = original;
    right = original;
    left.durationBeats = timelineBeat - original.startBeat;
    right.startBeat = timelineBeat;
    right.durationBeats = clipEnd - timelineBeat;
    if(!repeating) {
        left.contentDurationBeats = std::max(1.0 / 24.0, cut);
        right.contentDurationBeats = std::max(1.0 / 24.0, content - cut);
    }

    for(size_t laneIndex = 0; laneIndex < original.lanes.size(); ++laneIndex) {
        const auto& lane = original.lanes[laneIndex];
        auto& leftLane = left.lanes[laneIndex];
        auto& rightLane = right.lanes[laneIndex];
        // Right part, as source ranges of the original placed one after the
        // other: [cut, content) then, when repeating, [0, cut).
        struct Piece { double from, to, placeAt; };
        std::vector<Piece> rightPieces = {{cut, content, 0.0}};
        if(repeating && cut > kEpsilon) rightPieces.push_back({0.0, cut, content - cut});

        const bool curveData = !lane.curvePoints.empty() &&
            (lane.type == ofxOceanodeTimelineLaneType::Curve || original.isLfo);
        if(curveData && lane.curveInterpolation != "Value") {
            rightLane.curvePoints.clear();
            rightLane.curveTensions.clear();
            for(const auto& piece : rightPieces)
                appendCurveWindow(lane, piece.from, piece.to, piece.placeAt, rightLane.curvePoints, rightLane.curveTensions);
            if(!repeating) {
                leftLane.curvePoints.clear();
                leftLane.curveTensions.clear();
                appendCurveWindow(lane, 0.0, cut, 0.0, leftLane.curvePoints, leftLane.curveTensions);
            }
        }
        auto splitEvents = [&](const auto& events, auto& leftEvents, auto& rightEvents) {
            rightEvents.clear();
            for(const auto& piece : rightPieces) appendEventWindow(events, piece.from, piece.to, piece.placeAt, rightEvents);
            if(!repeating) {
                leftEvents.clear();
                appendEventWindow(events, 0.0, cut, 0.0, leftEvents);
            }
        };
        splitEvents(lane.pianoNotes, leftLane.pianoNotes, rightLane.pianoNotes);
        for(size_t row = 0; row < lane.multiValueRows.size(); ++row)
            splitEvents(lane.multiValueRows[row], leftLane.multiValueRows[row], rightLane.multiValueRows[row]);
        for(size_t row = 0; row < lane.multiGateRows.size(); ++row)
            splitEvents(lane.multiGateRows[row], leftLane.multiGateRows[row], rightLane.multiGateRows[row]);

        // Step patterns repeat every pattern length inside the content: the
        // right part's pattern starts that far into it (steps crossing the
        // start are split in two).
        const double cellLength = std::max(1.0 / 24.0, lane.beatsPerStep);
        const double patternLength = std::max(cellLength, lane.stepCount * cellLength);
        const double patternShift = positiveModulo(cut, patternLength);
        if(!lane.step.steps.empty() && patternShift > kEpsilon) {
            rightLane.step.steps.clear();
            for(const auto& step : lane.step.steps) {
                const double duration = step.durationBeats > kEpsilon ? step.durationBeats : cellLength;
                const double start = step.startBeat;
                const double end = start + duration;
                auto addStep = [&](double s, double d, bool whole) {
                    if(d <= 1e-6) return;
                    auto piece = step;
                    piece.startBeat = s;
                    if(!whole) piece.durationBeats = d; // a whole step keeps "one cell" (0) as is
                    rightLane.step.steps.push_back(piece);
                };
                if(start >= patternShift - kEpsilon) addStep(start - patternShift, duration, true);
                else if(end > patternShift + kEpsilon) {
                    addStep(0.0, end - patternShift, false);
                    addStep(start - patternShift + patternLength, patternShift - start, false);
                } else addStep(start - patternShift + patternLength, duration, true);
            }
            rightLane.step.sortSteps();
        }
        if(lane.type == ofxOceanodeTimelineLaneType::MultiSlider && !lane.multiSliderValues.empty()) {
            const int count = static_cast<int>(lane.multiSliderValues.size());
            const int shiftCells = static_cast<int>(std::llround(patternShift / cellLength)) % std::max(1, count);
            std::rotate(rightLane.multiSliderValues.begin(), rightLane.multiSliderValues.begin() + shiftCells,
                        rightLane.multiSliderValues.end());
        }
    }

    // An LFO restarts its phase at the start of a clip: carry the phase it
    // had reached at the cut into the right part's phase offset.
    if(original.isLfo) {
        const double cycles = ofxOceanodeTimelineLfo::cyclesAt(original, cut);
        const float phase = static_cast<float>(cycles - std::floor(cycles));
        for(auto& lane : right.lanes) {
            if(lane.lfoParameter != "phaseOffset") continue;
            auto wrap = [&](float value) { float v = value + phase; return v - std::floor(v); };
            if(lane.curveInterpolation == "Value") lane.lfoValue = wrap(lane.lfoValue);
            else for(auto& point : lane.curvePoints) point.value = wrap(point.value);
        }
    }

    return true;
}

std::string ofxOceanodeTimelineManager::splitClip(const std::string& trackId, const std::string& clipId, double timelineBeat) {
    auto* track = getTrack(trackId);
    const auto* sourceClip = getClip(trackId, clipId);
    if(track == nullptr || sourceClip == nullptr || track->isWaveTrack) return std::string();
    const ofxOceanodeTimelineClip original = *sourceClip;
    ofxOceanodeTimelineClip left, right;
    if(!computeClipSplit(original, timelineBeat, left, right)) return std::string();
    // Keep the original/left half as the track's capture destination. The
    // right half is an ordinary editable curve clip after the split.
    right.isStateCapture = false;

    // Splitting dissolves a clip group: the halves become independent.
    if(getGroupForClip(trackId, clipId) != nullptr) ungroupClip(trackId, clipId);

    const std::string rightId = createClip(trackId, original.name, right.startBeat, right.durationBeats);
    if(rightId.empty()) return std::string();
    auto* rightClip = getClip(trackId, rightId);
    const std::string rightName = rightClip->name;
    auto rightLanes = std::move(right.lanes);
    right.lanes.clear();
    right.id = rightId;
    right.name = rightName;
    *rightClip = right;
    for(auto& lane : rightLanes) {
        const std::string laneId = makeUniqueLaneId();
        ++nextLaneNumber;
        lane.id = laneId;
        if(auto* clip = getClip(trackId, rightId)) clip->lanes.push_back(lane);
    }
    if(auto* leftClip = getClip(trackId, clipId)) *leftClip = left;

    if(auto* sortedTrack = getTrack(trackId)) {
        std::sort(sortedTrack->clips.begin(), sortedTrack->clips.end(), [](const auto& a, const auto& b) {
            if(std::abs(a.startBeat - b.startBeat) > 1e-9) return a.startBeat < b.startBeat;
            return a.id < b.id;
        });
    }
    invalidateEvaluationIndexes();
    invalidateSchedule();
    return rightId;
}

std::string ofxOceanodeTimelineManager::duplicateClip(const std::string& srcTrackId, const std::string& clipId,
                                                      const std::string& dstTrackId, double newStartBeat,
                                                      const std::string& srcLaneId,
                                                      const std::string& dstBindingId) {
    const auto* srcTrack = getTrack(srcTrackId);
    const auto* dstTrack = getTrack(dstTrackId);
    const auto* sourceClip = getClip(srcTrackId, clipId);
    if(srcTrack == nullptr || dstTrack == nullptr || sourceClip == nullptr) return std::string();
    if(srcTrack->isWaveTrack != dstTrack->isWaveTrack) return std::string();
    // A value copy: createClip below may reallocate the source track's clips.
    ofxOceanodeTimelineClip copy = *sourceClip;
    // A duplicate is ordinary automation; otherwise the next capture would
    // have two ambiguous destination clips on the same track.
    copy.isStateCapture = false;
    const bool sameTrack = srcTrackId == dstTrackId;

    // The destination track's binding for the parameter a source binding drives.
    auto mapBinding = [&](const std::string& bindingId) -> std::string {
        if(bindingId.empty() || sameTrack) return bindingId;
        const auto* binding = getBinding(srcTrackId, bindingId);
        if(binding == nullptr) return std::string();
        for(const auto& candidate : dstTrack->bindings)
            if(candidate.parameterPath == binding->parameterPath) return candidate.id;
        return std::string();
    };
    auto mapIds = [&](std::vector<std::string>& ids) {
        std::vector<std::string> mapped;
        for(const auto& id : ids) {
            const std::string target = mapBinding(id);
            if(!target.empty() && std::find(mapped.begin(), mapped.end(), target) == mapped.end()) mapped.push_back(target);
        }
        ids = std::move(mapped);
    };

    if(!dstBindingId.empty()) {
        if(dstTrack->isWaveTrack || getBinding(dstTrackId, dstBindingId) == nullptr) return std::string();
        if(copy.isLfo) {
            // The lanes are the oscillator's own controls; only the output moves.
            copy.lfoOutputBindingId = dstBindingId;
            for(auto& lane : copy.lanes) mapIds(lane.bindingIds);
        } else {
            const auto laneIt = std::find_if(copy.lanes.begin(), copy.lanes.end(),
                                             [&](const auto& lane) { return lane.id == srcLaneId; });
            if(laneIt == copy.lanes.end()) return std::string();
            ofxOceanodeTimelineLane lane = *laneIt;
            if(lane.type == ofxOceanodeTimelineLaneType::PianoRoll) {
                const auto* group = findNoteGroupForBinding(*dstTrack, dstBindingId);
                if(group == nullptr) return std::string();
                lane.pianoPitchBindingId = group->pitchBindingId;
                lane.pianoGateBindingId = group->gateBindingId;
                lane.pianoVelocityBindingId = group->velocityBindingId;
                lane.bindingIds = group->members();
            } else {
                lane.bindingIds = {dstBindingId};
            }
            copy.lanes = {lane};
        }
    } else if(!sameTrack && !dstTrack->isWaveTrack) {
        if(copy.isLfo) {
            copy.lfoOutputBindingId = mapBinding(copy.lfoOutputBindingId);
            if(copy.lfoOutputBindingId.empty()) return std::string();
            for(auto& lane : copy.lanes) mapIds(lane.bindingIds);
        } else {
            std::vector<ofxOceanodeTimelineLane> kept;
            for(auto lane : copy.lanes) {
                mapIds(lane.bindingIds);
                if(lane.type == ofxOceanodeTimelineLaneType::PianoRoll) {
                    lane.pianoPitchBindingId = mapBinding(lane.pianoPitchBindingId);
                    lane.pianoGateBindingId = mapBinding(lane.pianoGateBindingId);
                    lane.pianoVelocityBindingId = mapBinding(lane.pianoVelocityBindingId);
                }
                if(!lane.bindingIds.empty()) kept.push_back(std::move(lane));
            }
            if(kept.empty()) return std::string();
            copy.lanes = std::move(kept);
        }
    }

    const std::string newId = createClip(dstTrackId, copy.name, newStartBeat, copy.durationBeats);
    auto* newClip = getClip(dstTrackId, newId);
    if(newClip == nullptr) return std::string();
    copy.id = newId;
    copy.name = newClip->name;
    copy.startBeat = std::max(0.0, newStartBeat);
    // Lanes go in one at a time: makeUniqueLaneId() only avoids ids already
    // in the timeline, so ids handed out before insertion could repeat.
    auto lanes = std::move(copy.lanes);
    copy.lanes.clear();
    *newClip = std::move(copy);
    for(auto& lane : lanes) {
        lane.id = makeUniqueLaneId();
        ++nextLaneNumber;
        if(auto* clip = getClip(dstTrackId, newId)) clip->lanes.push_back(std::move(lane));
    }
    invalidateSchedule();
    return newId;
}

int ofxOceanodeTimelineManager::separateStateCaptureClip(const std::string& trackId,
                                                         const std::string& clipId) {
    const auto* source = getClip(trackId, clipId);
    const auto* track = getTrack(trackId);
    if(source == nullptr || track == nullptr || track->isWaveTrack || source->isLfo ||
       !source->isStateCapture || source->lanes.size() < 2) return 0;

    // Copy the descriptors before duplicateClip() grows the track's clip
    // vector and invalidates every pointer/reference into it.
    const double startBeat = source->startBeat;
    const auto lanes = source->lanes;
    for(const auto& lane : lanes)
        if(lane.bindingIds.empty()) return 0;

    std::vector<std::string> createdClipIds;
    createdClipIds.reserve(lanes.size() - 1);
    for(size_t laneIndex = 1; laneIndex < lanes.size(); ++laneIndex) {
        const auto newClipId = duplicateClip(trackId, clipId, trackId, startBeat,
                                             lanes[laneIndex].id,
                                             lanes[laneIndex].bindingIds.front());
        if(newClipId.empty()) {
            for(const auto& createdId : createdClipIds) removeClip(trackId, createdId);
            return 0;
        }
        if(getClip(trackId, newClipId) != nullptr) {
            createdClipIds.push_back(newClipId);
        } else {
            removeClip(trackId, newClipId);
            for(const auto& createdId : createdClipIds) removeClip(trackId, createdId);
            return 0;
        }
    }

    for(const auto& createdId : createdClipIds)
        if(auto* created = getClip(trackId, createdId)) created->isStateCapture = true;
    if(getGroupForClip(trackId, clipId) != nullptr) ungroupClip(trackId, clipId);
    if(auto* original = getClip(trackId, clipId)) {
        original->lanes = {lanes.front()};
        original->isStateCapture = true;
    }
    invalidateEvaluationIndexes();
    invalidateSchedule();
    return static_cast<int>(createdClipIds.size() + 1);
}

bool ofxOceanodeTimelineManager::trimClipStart(const std::string& trackId, const std::string& clipId, double newStartBeat) {
    auto* track = getTrack(trackId);
    auto* clip = getClip(trackId, clipId);
    if(track == nullptr || clip == nullptr || track->isWaveTrack) return false;
    ofxOceanodeTimelineClip base = *clip;
    newStartBeat = std::max(0.0, newStartBeat);
    const double end = base.startBeat + base.durationBeats;
    if(newStartBeat >= end - 1.0 / 96.0) return false;
    if(newStartBeat < base.startBeat) {
        // Growing to the left: only repeating content can (it is the same
        // content one cycle earlier). Extend by whole cycles, then cut.
        if(!base.repeatContent) return false;
        const double cycle = ofxOceanodeTimelineClipTime::cycleDuration(base);
        const double cycles = std::ceil((base.startBeat - newStartBeat) / std::max(kEpsilon, cycle));
        base.startBeat -= cycles * cycle;
        base.durationBeats += cycles * cycle;
    }
    ofxOceanodeTimelineClip trimmed = base;
    if(std::abs(newStartBeat - base.startBeat) > 1e-9) {
        ofxOceanodeTimelineClip left;
        if(!computeClipSplit(base, newStartBeat, left, trimmed)) return false;
    }
    trimmed.startBeat = newStartBeat;
    trimmed.durationBeats = end - newStartBeat;
    // Same clip (id, name, lane ids): only its start and content moved.
    *clip = trimmed;
    std::sort(track->clips.begin(), track->clips.end(), [](const auto& a, const auto& b) {
        if(std::abs(a.startBeat - b.startBeat) > 1e-9) return a.startBeat < b.startBeat;
        return a.id < b.id;
    });
    invalidateEvaluationIndexes();
    invalidateSchedule();
    return true;
}

bool ofxOceanodeTimelineManager::extendClipStart(const std::string& trackId, const std::string& clipId, double newStartBeat) {
    auto* track = getTrack(trackId);
    auto* clip = getClip(trackId, clipId);
    if(track == nullptr || clip == nullptr || track->isWaveTrack) return false;
    newStartBeat = std::max(0.0, newStartBeat);
    if(newStartBeat >= clip->startBeat - 1e-9 || clip->isLfo) return trimClipStart(trackId, clipId, newStartBeat);

    const double stretch = ofxOceanodeTimelineClipTime::stretch(*clip);
    auto isGridLane = [](const ofxOceanodeTimelineLane& lane) {
        return lane.type == ofxOceanodeTimelineLaneType::Step || lane.type == ofxOceanodeTimelineLaneType::MultiSlider;
    };
    // Step grids (Step, Multi Slider) can only grow by whole cells, so the
    // added source time is a whole number of the first grid lane's cells --
    // never more than fits before beat 0.
    const double maxSourceDelta = clip->startBeat / stretch;
    double sourceDelta = (clip->startBeat - newStartBeat) / stretch;
    for(const auto& lane : clip->lanes) {
        if(!isGridLane(lane)) continue;
        const double cell = std::max(1.0 / 24.0, lane.beatsPerStep);
        sourceDelta = std::min(std::round(sourceDelta / cell), std::floor(maxSourceDelta / cell + 1e-9)) * cell;
        break;
    }
    sourceDelta = std::min(sourceDelta, maxSourceDelta);
    if(sourceDelta <= 1e-9) return false;

    for(auto& lane : clip->lanes) {
        for(auto& point : lane.curvePoints) point.beat += sourceDelta;
        for(auto& note : lane.pianoNotes) note.startBeat += sourceDelta;
        for(auto& row : lane.multiValueRows) for(auto& region : row) region.startBeat += sourceDelta;
        for(auto& row : lane.multiGateRows) for(auto& region : row) region.startBeat += sourceDelta;
        if(!isGridLane(lane)) continue;
        // A grid lane with another cell size shifts by as many of its own
        // cells as fit (its pattern may then sit slightly off).
        const double cell = std::max(1.0 / 24.0, lane.beatsPerStep);
        const int addedCells = static_cast<int>(std::llround(sourceDelta / cell));
        if(addedCells <= 0) continue;
        for(auto& step : lane.step.steps) step.startBeat += addedCells * cell;
        lane.step.sortSteps();
        if(lane.type == ofxOceanodeTimelineLaneType::MultiSlider)
            lane.multiSliderValues.insert(lane.multiSliderValues.begin(), static_cast<size_t>(addedCells), 0.0f);
        lane.stepCount += addedCells;
    }

    clip->contentDurationBeats = ofxOceanodeTimelineClipTime::sourceDuration(*clip) + sourceDelta;
    const double end = clip->startBeat + clip->durationBeats;
    clip->startBeat = std::max(0.0, clip->startBeat - sourceDelta * stretch);
    clip->durationBeats = end - clip->startBeat;
    std::sort(track->clips.begin(), track->clips.end(), [](const auto& a, const auto& b) {
        if(std::abs(a.startBeat - b.startBeat) > 1e-9) return a.startBeat < b.startBeat;
        return a.id < b.id;
    });
    invalidateEvaluationIndexes();
    invalidateSchedule();
    return true;
}

bool ofxOceanodeTimelineManager::renameClip(const std::string& trackId, const std::string& clipId,
                                            const std::string& requestedName) {
    auto* track = getTrack(trackId);
    auto* clip = getClip(trackId, clipId);
    if(track == nullptr || clip == nullptr || requestedName.empty()) return false;
    if(clip->name == requestedName) return true;
    clip->name = makeUniqueClipName(*track, requestedName);
    return true;
}

bool ofxOceanodeTimelineManager::removeClip(const std::string& trackId, const std::string& clipId) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return false;
    if(const auto* clip = getClip(trackId, clipId)) {
        releaseWaveClipIfNeeded(trackId, clip->id);
        if(clip->isStateCapture) {
            for(const auto& lane : clip->lanes) {
                for(const auto& bindingId : lane.bindingIds) {
                    const std::string key = trackId + "\x1f" + bindingId;
                    if(stateCaptureManualOverrideKeys.erase(key) > 0) {
                        if(auto* binding = getBinding(trackId, bindingId)) {
                            binding->hasLiveOverride = false;
                            binding->liveOverrideValue.clear();
                        }
                    }
                    if(const auto* binding = getBinding(trackId, bindingId))
                        stateCaptureLastAppliedValues.erase(binding->parameterPath);
                }
            }
        }
    }
    const auto oldSize = track->clips.size();
    track->clips.erase(std::remove_if(track->clips.begin(), track->clips.end(), [&](const auto& clip) {
        return clip.id == clipId;
    }), track->clips.end());
    const bool removed = track->clips.size() != oldSize;
    if(removed) {
        removeClipFromGroups(trackId, clipId);
        invalidateEvaluationIndexes();
    }
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
    lane.probabilitySeed = static_cast<int>(ofRandom(1.0f, 100000.0f));
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
        if(clip->isStateCapture) {
            for(const auto& bindingId : lane->bindingIds) {
                const std::string key = trackId + "\x1f" + bindingId;
                if(stateCaptureManualOverrideKeys.erase(key) == 0) continue;
                if(auto* binding = getBinding(trackId, bindingId)) {
                    binding->hasLiveOverride = false;
                    binding->liveOverrideValue.clear();
                }
            }
        }
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

bool ofxOceanodeTimelineManager::reloadWaveform(const std::string& trackId, const std::string& clipId) {
    auto* clip = getClip(trackId, clipId);
    if(clip == nullptr) return false;
    const bool result = readWaveformCache(clip->waveFilePath, clip->waveNumChannels,
                                          clip->waveFileDurationMs, clip->waveformPeaks);
    // waveFileDurationMs is tempo-independent; waveFileDurationBeats is not.
    // Re-derive the beat-domain length at the tempo in force now and carry
    // this clip's own source range across with it, so a slice keeps pointing
    // at the same audio instead of being clamped (by normalizeWaveClipMapping
    // below) against a length measured at whatever tempo was set when the
    // file was first imported.
    if(container != nullptr && clip->waveFileDurationMs > 0.0) {
        const double bpm = std::max(1.0f, container->getTransportState().bpm);
        const double refreshedBeats = clip->waveFileDurationMs * bpm / 60000.0;
        if(clip->waveFileDurationBeats > kEpsilon) {
            const double ratio = refreshedBeats / clip->waveFileDurationBeats;
            if(std::abs(ratio - 1.0) > 1e-9) {
                clip->waveSourceStartBeat *= ratio;
                clip->contentDurationBeats *= ratio;
            }
        }
        clip->waveFileDurationBeats = refreshedBeats;
        normalizeWaveClipMapping(*clip);
    }
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
    const bool removed = lane->bindingIds.size() != oldSize;
    const auto* clip = getClip(trackId, clipId);
    if(removed && clip != nullptr && clip->isStateCapture) {
        const std::string key = trackId + "\x1f" + bindingId;
        if(stateCaptureManualOverrideKeys.erase(key) > 0) {
            if(auto* binding = getBinding(trackId, bindingId)) {
                binding->hasLiveOverride = false;
                binding->liveOverrideValue.clear();
            }
        }
    }
    return removed;
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
    // Wave is a track type, not a lane type -- createLane refuses it and
    // loading folds the old Wave-lane format onto the clip, so nothing may
    // reintroduce one here either.
    if(type == ofxOceanodeTimelineLaneType::Wave) return false;
    const auto previousType = lane->type;
    if(clip != nullptr && clip->isStateCapture &&
       previousType == ofxOceanodeTimelineLaneType::Curve && type != previousType) {
        for(const auto& bindingId : lane->bindingIds) {
            const std::string key = trackId + "\x1f" + bindingId;
            if(stateCaptureManualOverrideKeys.erase(key) == 0) continue;
            if(auto* binding = getBinding(trackId, bindingId)) {
                binding->hasLiveOverride = false;
                binding->liveOverrideValue.clear();
            }
        }
    }
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
        } else if(lane.type == ofxOceanodeTimelineLaneType::MultiValue ||
                  lane.type == ofxOceanodeTimelineLaneType::MultiGate) {
            // Blocks live in source beats like steps and notes do, so they
            // get the same treatment: drop the ones that start past the new
            // end, crop the one straddling it.
            auto trimRows = [&](auto& rows) {
                for(auto& row : rows) {
                    const auto oldSize = row.size();
                    row.erase(std::remove_if(row.begin(), row.end(),
                        [&](const auto& region) { return region.startBeat >= contentEnd - kEpsilon; }),
                        row.end());
                    changed |= row.size() != oldSize;
                    for(auto& region : row) {
                        const double maximumDuration = std::max(kEpsilon, contentEnd - region.startBeat);
                        if(region.durationBeats > maximumDuration) {
                            region.durationBeats = maximumDuration;
                            changed = true;
                        }
                    }
                }
            };
            if(lane.type == ofxOceanodeTimelineLaneType::MultiValue) trimRows(lane.multiValueRows);
            else trimRows(lane.multiGateRows);
        } else if(lane.type == ofxOceanodeTimelineLaneType::MultiSlider) {
            // A dense grid has no per-cell start to test: shortening it is
            // shortening the grid, exactly as the Step branch does.
            const double cellLength = std::max(1.0 / 24.0, lane.beatsPerStep);
            const int consolidatedSteps = std::max(1, static_cast<int>(std::ceil(contentEnd / cellLength)));
            if(lane.stepCount > consolidatedSteps) {
                lane.stepCount = consolidatedSteps;
                changed = true;
            }
            if(static_cast<int>(lane.multiSliderValues.size()) != lane.stepCount) {
                lane.multiSliderValues.resize(static_cast<size_t>(std::max(1, lane.stepCount)), 0.0f);
                changed = true;
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
    if(lane == nullptr || lane->type != ofxOceanodeTimelineLaneType::Step) return false;
    return lane->step.removeStep(startBeat);
}

ofxOceanodeTimelineParameterBinding* ofxOceanodeTimelineManager::getBinding(const std::string& trackId, const std::string& bindingId) {
    auto* track = getTrack(trackId);
    if(track == nullptr) return nullptr;
    if(evaluationIndexesValid) {
        const auto trackIndex = trackEvaluationIndexes.find(track);
        if(trackIndex != trackEvaluationIndexes.end()) {
            const auto cached = trackIndex->second.bindingById.find(bindingId);
            if(cached != trackIndex->second.bindingById.end())
                return cached->second;
        }
    }
    auto it = std::find_if(track->bindings.begin(), track->bindings.end(), [&](auto& binding) { return binding.id == bindingId; });
    return it == track->bindings.end() ? nullptr : &*it;
}

const ofxOceanodeTimelineParameterBinding* ofxOceanodeTimelineManager::getBinding(const std::string& trackId, const std::string& bindingId) const {
    const auto* track = getTrack(trackId);
    if(track == nullptr) return nullptr;
    if(evaluationIndexesValid) {
        const auto trackIndex = trackEvaluationIndexes.find(track);
        if(trackIndex != trackEvaluationIndexes.end()) {
            const auto cached = trackIndex->second.bindingById.find(bindingId);
            if(cached != trackIndex->second.bindingById.end()) return cached->second;
        }
    }
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

    // Seconds at every point, cached: this runs for every beat<->pixel
    // conversion (dozens per frame), and the curved segments are integrated
    // numerically. Rebuilt whenever the tempo map changes (signature).
    uint64_t signature = 1469598103934665603ULL;
    auto mix = [&](const void* data, size_t size) {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for(size_t i = 0; i < size; ++i) { signature ^= bytes[i]; signature *= 1099511628211ULL; }
    };
    for(const auto& point : points) { mix(&point.beat, sizeof(point.beat)); mix(&point.value, sizeof(point.value)); }
    for(const auto& tension : bpmCurveTensions) { mix(&tension.inflection, sizeof(float)); mix(&tension.steepness, sizeof(float)); }
    mix(&bpmMinimum, sizeof(bpmMinimum)); mix(&bpmMaximum, sizeof(bpmMaximum));
    mix(bpmInterpolation.data(), bpmInterpolation.size());
    if(!tempoSecondsCacheValid || tempoSecondsCacheSignature != signature || tempoSecondsAtPoint.size() != points.size()) {
        tempoSecondsAtPoint.assign(points.size(), 0.0);
        tempoSecondsAtPoint[0] = firstBeat * 60.0 / firstBpm;
        for(size_t i = 1; i < points.size(); ++i) {
            const double segmentStart = std::max(0.0, points[i - 1].beat);
            const double segmentEnd = std::max(segmentStart, points[i].beat);
            const double segmentDuration = segmentEnd - segmentStart;
            const auto tension = i - 1 < bpmCurveTensions.size() ? bpmCurveTensions[i - 1] : ofxOceanodeTimelineCurveTension{};
            tempoSecondsAtPoint[i] = tempoSecondsAtPoint[i - 1] + (segmentDuration <= kEpsilon ? 0.0
                : integrateSegmentSeconds(segmentDuration, clampedBpm(points[i - 1].value),
                                          clampedBpm(points[i].value), tension, 1.0));
        }
        tempoSecondsCacheSignature = signature;
        tempoSecondsCacheValid = true;
    }
    const double lastBeat = std::max(0.0, points.back().beat);
    if(beat >= lastBeat) return tempoSecondsAtPoint.back() + (beat - lastBeat) * 60.0 / clampedBpm(points.back().value);
    // First point past the beat (points are kept sorted by beat).
    const auto upper = std::upper_bound(points.begin(), points.end(), beat,
                                        [](double b, const auto& point) { return b < point.beat; });
    const size_t i = std::max<size_t>(1, static_cast<size_t>(upper - points.begin()));
    const double segmentStart = std::max(0.0, points[i - 1].beat);
    const double segmentEnd = std::max(segmentStart, points[i].beat);
    const double segmentDuration = segmentEnd - segmentStart;
    if(segmentDuration <= kEpsilon || beat <= segmentStart) return tempoSecondsAtPoint[i - 1];
    const auto tension = i - 1 < bpmCurveTensions.size() ? bpmCurveTensions[i - 1] : ofxOceanodeTimelineCurveTension{};
    return tempoSecondsAtPoint[i - 1] + integrateSegmentSeconds(segmentDuration, clampedBpm(points[i - 1].value),
                                                                  clampedBpm(points[i].value), tension,
                                                                  (beat - segmentStart) / segmentDuration);
}

namespace {
void sortMarkers(std::vector<ofxOceanodeTimelineMarker>& markers) {
    std::stable_sort(markers.begin(), markers.end(), [](const auto& a, const auto& b) { return a.beat < b.beat; });
}
}

std::string ofxOceanodeTimelineManager::addMarker(double beat, const std::string& name) {
    ofxOceanodeTimelineMarker marker;
    uint64_t number = markers.size() + 1;
    do {
        marker.id = makeId("timeline_marker", number++);
    } while(std::any_of(markers.begin(), markers.end(), [&](const auto& m) { return m.id == marker.id; }));
    marker.beat = std::max(0.0, beat);
    marker.name = name.empty() ? "Marker " + ofToString(markers.size() + 1) : name;
    markers.push_back(marker);
    sortMarkers(markers);
    return marker.id;
}

bool ofxOceanodeTimelineManager::removeMarker(const std::string& markerId) {
    const auto oldSize = markers.size();
    markers.erase(std::remove_if(markers.begin(), markers.end(), [&](const auto& m) { return m.id == markerId; }), markers.end());
    return markers.size() != oldSize;
}

bool ofxOceanodeTimelineManager::moveMarker(const std::string& markerId, double beat) {
    for(auto& marker : markers) {
        if(marker.id != markerId) continue;
        marker.beat = std::max(0.0, beat);
        sortMarkers(markers);
        return true;
    }
    return false;
}

bool ofxOceanodeTimelineManager::renameMarker(const std::string& markerId, const std::string& name) {
    if(name.empty()) return false;
    for(auto& marker : markers) if(marker.id == markerId) { marker.name = name; return true; }
    return false;
}

const ofxOceanodeTimelineMarker* ofxOceanodeTimelineManager::getMarker(const std::string& markerId) const {
    for(const auto& marker : markers) if(marker.id == markerId) return &marker;
    return nullptr;
}

void ofxOceanodeTimelineManager::syncLoopToTransport() {
    if(container == nullptr) return;
    const auto transport = container->getTransport();
    if(transport == nullptr) return;
    // An external clock owns position and looping. Preserve the timeline's
    // local setting so it can resume when sync is released, but do not apply
    // it while the external source is in control.
    transport->setLoop(!transport->hasExternalClock() && loopEnabled,
                       loopStartBeat, loopEndBeat);
}

void ofxOceanodeTimelineManager::setLoopEnabled(bool enabled) {
    if(loopEnabled == enabled) return;
    loopEnabled = enabled;
    hasEvaluatedTransportBeat = false;
    invalidateSchedule();
    syncLoopToTransport();
}

void ofxOceanodeTimelineManager::setLoopRange(double startBeat, double endBeat) {
    const double minimumLength = 1.0 / 24.0;
    loopStartBeat = std::max(0.0, std::min(startBeat, endBeat - minimumLength));
    loopEndBeat = std::max(loopStartBeat + minimumLength, endBeat);
    hasEvaluatedTransportBeat = false;
    // Events past the old boundary belong to a loop that no longer exists.
    invalidateSchedule();
    syncLoopToTransport();
}

void ofxOceanodeTimelineManager::update() {
    beginParameterCache();
    evaluateAutomation();
    applyAutomation();
    endParameterCache();
}

void ofxOceanodeTimelineManager::beginParameterCache() {
    parameterCache.clear();
    parameterCacheActive = true;
}

void ofxOceanodeTimelineManager::endParameterCache() {
    parameterCacheActive = false;
    parameterCache.clear();
}

ofxOceanodeAbstractParameter* ofxOceanodeTimelineManager::findParameterCached(const std::string& path) const {
    if(container == nullptr) return nullptr;
    if(!parameterCacheActive) return container->findTimelineParameter(path);
    const auto it = parameterCache.find(path);
    if(it != parameterCache.end()) return it->second;
    auto* parameter = container->findTimelineParameter(path);
    parameterCache.emplace(path, parameter);
    return parameter;
}

namespace {
// A binding of a track we are already iterating (no track lookup by id).
const ofxOceanodeTimelineParameterBinding* bindingInTrack(const ofxOceanodeTimelineTrack& track,
                                                          const std::string& bindingId) {
    if(bindingId.empty()) return nullptr;
    for(const auto& binding : track.bindings) if(binding.id == bindingId) return &binding;
    return nullptr;
}
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
    if(!evaluationIndexesValid)
        const_cast<ofxOceanodeTimelineManager*>(this)->rebuildEvaluationIndexes();
    activeValues.clear();
    activeValues.reserve(bindingsByPathCache.size());
    using LaneContributions = std::vector<std::pair<ofxOceanodeTimelineAutomationMode, std::string>>;
    using ClipContributions = std::unordered_map<const ofxOceanodeTimelineClip*, LaneContributions>;
    std::unordered_map<const ofxOceanodeTimelineParameterBinding*, ClipContributions> bindingClipValues;
    bindingClipValues.reserve(bindingsByPathCache.size());

    auto bindingFor = [&](const ofxOceanodeTimelineTrack& track,
                          const std::string& bindingId) -> const ofxOceanodeTimelineParameterBinding* {
        if(bindingId.empty()) return nullptr;
        const auto trackIt = trackEvaluationIndexes.find(&track);
        if(trackIt == trackEvaluationIndexes.end()) return bindingInTrack(track, bindingId);
        const auto bindingIt = trackIt->second.bindingById.find(bindingId);
        return bindingIt == trackIt->second.bindingById.end() ? nullptr : bindingIt->second;
    };

    if(applyPianoRanges) {
        for(const auto& entry : pianoPitchRangesCache) {
            if(auto* parameter = findParameterCached(entry.first)) {
                if(savedPianoRanges.count(entry.first) == 0) {
                    // Remember the parameter's own range before narrowing it.
                    SavedParameterRange saved;
                    saved.valueType = entry.second.valueType;
                    const auto& type = entry.second.valueType;
                    if(type == typeid(float).name()) {
                        saved.floatMin = parameter->cast<float>().getParameter().getMin();
                        saved.floatMax = parameter->cast<float>().getParameter().getMax();
                    } else if(type == typeid(int).name()) {
                        saved.intMin = parameter->cast<int>().getParameter().getMin();
                        saved.intMax = parameter->cast<int>().getParameter().getMax();
                    } else if(type == typeid(std::vector<float>).name()) {
                        saved.floatVectorMin = parameter->cast<std::vector<float>>().getParameter().getMin();
                        saved.floatVectorMax = parameter->cast<std::vector<float>>().getParameter().getMax();
                    } else if(type == typeid(std::vector<int>).name()) {
                        saved.intVectorMin = parameter->cast<std::vector<int>>().getParameter().getMin();
                        saved.intVectorMax = parameter->cast<std::vector<int>>().getParameter().getMax();
                    }
                    savedPianoRanges[entry.first] = saved;
                }
                applyPianoPitchRange(*parameter, entry.second.valueType, entry.second.low, entry.second.high);
            }
        }
        // Give back the original range of parameters no piano roll drives any more.
        for(auto it = savedPianoRanges.begin(); it != savedPianoRanges.end();) {
            if(pianoPitchRangesCache.find(it->first) != pianoPitchRangesCache.end()) { ++it; continue; }
            if(auto* parameter = findParameterCached(it->first)) {
                const auto& saved = it->second;
                const auto& type = saved.valueType;
                if(type == typeid(float).name()) {
                    auto& p = parameter->cast<float>().getParameter();
                    p.setMin(saved.floatMin); p.setMax(saved.floatMax);
                } else if(type == typeid(int).name()) {
                    auto& p = parameter->cast<int>().getParameter();
                    p.setMin(saved.intMin); p.setMax(saved.intMax);
                } else if(type == typeid(std::vector<float>).name()) {
                    auto& p = parameter->cast<std::vector<float>>().getParameter();
                    p.setMin(saved.floatVectorMin); p.setMax(saved.floatVectorMax);
                } else if(type == typeid(std::vector<int>).name()) {
                    auto& p = parameter->cast<std::vector<int>>().getParameter();
                    p.setMin(saved.intVectorMin); p.setMax(saved.intVectorMax);
                }
            }
            it = savedPianoRanges.erase(it);
        }
    }
    for(const auto& track : tracks) {
        for(const auto& clip : track.clips) {
            double sourceBeat = 0.0;
            if(!clipSourceBeat(clip, beatPosition, sourceBeat)) continue;
            if(clip.isLfo) {
                const auto* binding = bindingFor(track, clip.lfoOutputBindingId);
                if(binding != nullptr && !binding->bypass) {
                    const float normalized = ofxOceanodeTimelineLfo::evaluate(clip, sourceBeat);
                    const float mapped = clip.lfoOutputMin + normalized *
                        (clip.lfoOutputMax - clip.lfoOutputMin);
                    std::string value = ofToString(mapped);
                    if(binding->valueType == typeid(int).name())
                        value = ofToString(static_cast<int>(std::lround(mapped)));
                    bindingClipValues[binding][&clip].emplace_back(
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
                        ? evaluateStepSequencer(lane, sourceBeat, value, retriggerGapBeats * sourcePerTimelineBeat(clip))
                        : lane.type == ofxOceanodeTimelineLaneType::MultiSlider
                        ? evaluateMultiSlider(lane, sourceBeat, value)
                        : evaluateCurve(lane, sourceBeat, value);
                    if(!hasValue && lane.type != ofxOceanodeTimelineLaneType::Step) continue;
                    if(!hasValue) value = "0";
                    for(const auto& bindingId : lane.bindingIds) {
                        if(const auto* binding = bindingFor(track, bindingId)) {
                            // A bypassed binding shouldn't contribute to a
                            // shared parameter's combined value at all.
                            if(binding->bypass) continue;
                            bindingClipValues[binding][&clip].emplace_back(
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
                                evaluateMultiGateRow(lane.multiGateRows[row], sourceBeat,
                                                     retriggerGapBeats * sourcePerTimelineBeat(clip),
                                                     clip.repeatContent ? ofxOceanodeTimelineClipTime::sourceDuration(clip) : 0.0);
                            components.push_back(active ? "1" : "0");
                        }
                    }
                    const std::string value = joinAutomationValues(components);
                    for(const auto& bindingId : lane.bindingIds) {
                        if(const auto* binding = bindingFor(track, bindingId)) {
                            if(binding->bypass) continue;
                            bindingClipValues[binding][&clip].emplace_back(binding->mode, value);
                        }
                    }
                } else {
                    std::vector<const ofxOceanodeTimelinePianoNote*> activeNotes;
                    activeNotes.reserve(lane.pianoNotes.size());
                    const double pianoCycle = static_cast<double>(
                        ofxOceanodeTimelineClipTime::cycleIndex(clip, beatPosition));
                    const double noteGap = retriggerGapBeats * sourcePerTimelineBeat(clip);
                    auto noteStart = [](const ofxOceanodeTimelinePianoNote& n) { return n.startBeat; };
                    auto noteEnd = [](const ofxOceanodeTimelinePianoNote& n) {
                        return n.startBeat + std::max(1.0 / 24.0, n.durationBeats);
                    };
                    auto samePitch = [](const ofxOceanodeTimelinePianoNote& a, const ofxOceanodeTimelinePianoNote& b) {
                        return a.pitch == b.pitch;
                    };
                    for(size_t noteIndex = 0; noteIndex < lane.pianoNotes.size(); ++noteIndex) {
                        const auto& note = lane.pianoNotes[noteIndex];
                        if(isPlaying && sourceBeat >= note.startBeat && sourceBeat < noteEnd(note) &&
                           !inRetriggerGap(lane.pianoNotes, noteIndex, sourceBeat, noteGap, noteStart, noteEnd, samePitch,
                                           clip.repeatContent ? ofxOceanodeTimelineClipTime::sourceDuration(clip) : 0.0) &&
                           pianoProbabilityPasses(note, pianoCycle, lane.probabilitySeed)) {
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
                            if(const auto* binding = bindingFor(track, roleBindingIds[bindingIndex])) {
                                if(binding->bypass) continue;
                                bindingClipValues[binding][&clip].emplace_back(
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
                        if(const auto* binding = bindingFor(track, roleBindingIds[bindingIndex])) {
                            if(binding->bypass) continue;
                            std::vector<std::string> noteValues;
                            noteValues.reserve(activeNotes.size());
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
                            bindingClipValues[binding][&clip].emplace_back(
                                binding->mode, std::move(value));
                        }
                    }
                }
            }
        }
    }

    // Lanes inside one clip use the binding's blend mode (for example, a
    // Multiply binding shared by a Curve and a piano Gate evaluates as
    // curve * gate). Where clips on one row overlap, the last one wins: the
    // clip that starts latest (then the later one in the track) is the one
    // heard. Finally, expose one contribution per binding for
    // cross-track/cross-binding blending.
    for(const auto& track : tracks) {
        const auto trackIndexIt = trackEvaluationIndexes.find(&track);
        for(const auto& binding : track.bindings) {
            if(binding.bypass) continue;
            const auto clipsIt = bindingClipValues.find(&binding);
            std::string value;
            if(binding.hasLiveOverride) value = binding.liveOverrideValue;
            else if(clipsIt != bindingClipValues.end()) {
                const LaneContributions* winner = nullptr;
                std::pair<double, size_t> winnerOrder{-1.0, 0};
                for(const auto& clipEntry : clipsIt->second) {
                    size_t clipIndex = 0;
                    if(trackIndexIt != trackEvaluationIndexes.end()) {
                        const auto orderIt = trackIndexIt->second.clipOrder.find(clipEntry.first);
                        if(orderIt != trackIndexIt->second.clipOrder.end()) clipIndex = orderIt->second;
                    }
                    const std::pair<double, size_t> order{clipEntry.first->startBeat, clipIndex};
                    if(winner == nullptr || order > winnerOrder) {
                        winner = &clipEntry.second;
                        winnerOrder = order;
                    }
                }
                if(winner != nullptr) value = combineAutomationValues(*winner, binding.valueType);
                if(value.empty()) continue;
            }
            else if(zeroWhenInactiveBindingsCache.count(&binding) > 0) value = "0";
            else continue;
            activeValues[binding.parameterPath].emplace_back(binding.mode, std::move(value));
        }
    }

}

void ofxOceanodeTimelineManager::evaluateAutomation() {
    if(container == nullptr) return;
    // Track/binding/clip storage may have changed through the editor since
    // the previous update. Rebuild once, then share the indexes across the
    // frame evaluation, scheduler probes and both applyAutomation passes.
    rebuildEvaluationIndexes();
    loopWrappedThisFrame = false;
    auto transport = container->getTransportState();
    // Following an external clock (MIDI clock): the master owns position, tempo and looping.
    const auto ownerTransportPtr = container->getTransport();
    const bool externalClock = ownerTransportPtr != nullptr && ownerTransportPtr->hasExternalClock();

    // The transport owns the loop (it wraps exactly at the loop end, keeping
    // the overshoot). Hand it the range; an external clock owns looping itself.
    // There is a single timeline (the root's), so it alone sets the loop.
    if(auto ownerTransport = container->getTransport()) {
        ownerTransport->setLoop(!externalClock && loopEnabled, loopStartBeat, loopEndBeat);
        transport = container->getTransportState();
    }

    // A stopped state-capture curve must still be editable from the node GUI.
    // The GUI writes after update(), so compare the value found at the start of
    // the next update with the last value automation itself applied. A mismatch
    // becomes a live override and is committed/cleared by the next capture.
    if(transport.isPlaying) {
        if(!stateCaptureManualOverrideKeys.empty()) {
            for(auto& track : tracks) {
                for(auto& binding : track.bindings) {
                    const std::string key = track.id + "\x1f" + binding.id;
                    if(stateCaptureManualOverrideKeys.count(key) == 0) continue;
                    binding.hasLiveOverride = false;
                    binding.liveOverrideValue.clear();
                }
            }
            stateCaptureManualOverrideKeys.clear();
        }
    } else {
        std::unordered_set<std::string> checkedBindings;
        for(auto& track : tracks) {
            for(const auto& clip : track.clips) {
                if(!clip.isStateCapture ||
                   transport.beatPosition < clip.startBeat - kEpsilon ||
                   transport.beatPosition >= clip.startBeat + clip.durationBeats - kEpsilon) continue;
                for(const auto& lane : clip.lanes) {
                    if(lane.type != ofxOceanodeTimelineLaneType::Curve) continue;
                    for(const auto& bindingId : lane.bindingIds) {
                        const std::string key = track.id + "\x1f" + bindingId;
                        if(!checkedBindings.insert(key).second) continue;
                        auto* binding = getBinding(track.id, bindingId);
                        if(binding == nullptr) continue;
                        // A piano-key preview (or another explicit caller of
                        // setLiveOverride) owns its override lifetime. Do not
                        // adopt and later clear it as a state-capture edit.
                        if(binding->hasLiveOverride &&
                           stateCaptureManualOverrideKeys.count(key) == 0) continue;
                        auto* parameter = findParameterCached(binding->parameterPath);
                        const auto previous = stateCaptureLastAppliedValues.find(binding->parameterPath);
                        if(parameter == nullptr || previous == stateCaptureLastAppliedValues.end()) continue;
                        const std::string current = parameterValueForAutomation(*parameter);
                        if(current == previous->second) continue;
                        binding->hasLiveOverride = true;
                        binding->liveOverrideValue = current;
                        stateCaptureManualOverrideKeys.insert(key);
                    }
                }
            }
        }
    }
    if(hasSeenLoopCount && transport.loopCount != lastSeenLoopCount) loopWrappedThisFrame = true;
    lastSeenLoopCount = transport.loopCount;
    hasSeenLoopCount = true;
    scheduleUsesTempoMap = !externalClock;
    {
        // Retrigger gap: at least 10 ms and 1.2 frames of playback, so the
        // per-frame path always samples the gate-off between repeated notes.
        const double frameSeconds = std::max(1.0 / 240.0, static_cast<double>(ofGetLastFrameTime()));
        const double beatsPerSecond = std::max(1.0f, transport.bpm) / 60.0;
        const double target = ofClamp(std::max(0.010, 1.2 * frameSeconds) * beatsPerSecond, 1.0 / 192.0, 0.25);
        // Only follow real changes (frame rate, tempo), not frame-to-frame
        // jitter: the scheduler placed its gate-offs with the current value.
        if(std::abs(target - retriggerGapBeats) > 0.25 * retriggerGapBeats) {
            retriggerGapBeats = target;
            invalidateSchedule();
        }
    }
    lastEvaluatedTransportBeat = transport.beatPosition;
    hasEvaluatedTransportBeat = true;

    if(bpmAutomationEnabled && !externalClock) { // an external clock sets the tempo
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
        const std::vector<ofxOceanodeTimelineParameterBinding*>& bindings,
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
    // Outside every clip the parameter holds its last value: nothing is
    // written, so it can also be changed by hand between clips.
    if(valuesIt == activeValues.end()) return false;
    std::string value = combineAutomationValues(valuesIt->second, defaultBinding->valueType);
    value = broadcastScalarNumericVectorValue(*parameter, value, defaultBinding->valueType);
    if(defaultBinding->clampToParameterRange)
        value = clampValueToParameterRange(*parameter, value, defaultBinding->valueType);
    outValue = std::move(value);
    return true;
}

void ofxOceanodeTimelineManager::applyAutomation() {
    if(container == nullptr) return;
    if(!evaluationIndexesValid) rebuildEvaluationIndexes();
    const auto& activeValues = activeAutomationValues;
    for(auto& entry : bindingsByPathCache) {
        auto* parameter = findParameterCached(entry.first);
        ofxOceanodeTimelineParameterBinding* defaultBinding = nullptr;
        for(auto* binding : entry.second) {
            binding->missingTarget = parameter == nullptr;
            if(!binding->bypass && defaultBinding == nullptr) defaultBinding = binding;
        }
        if(parameter == nullptr) continue;
        parameter->setTimelined(defaultBinding != nullptr);
        if(defaultBinding == nullptr) continue;

        std::string value;
        if(!computeParameterValue(entry.second, activeValues, parameter, value)) {
            stateCaptureLastAppliedValues.erase(entry.first);
            continue;
        }
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
        stateCaptureLastAppliedValues[entry.first] = parameterValueForAutomation(*parameter);
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
void ofxOceanodeTimelineManager::sendScheduleCorrections(uint64_t nowUs, const std::set<std::string>* onlyPaths) {
    if(container == nullptr) {
        scheduledPaths.clear();
        return;
    }
    if(!evaluationIndexesValid) rebuildEvaluationIndexes();
    for(auto& entry : scheduledPaths) {
        if(onlyPaths != nullptr && onlyPaths->count(entry.first) == 0) continue;
        auto& state = entry.second;
        if(state.pending.empty() && state.lastScheduledDueUs <= nowUs) continue;
        auto* parameter = findParameterCached(entry.first);
        if(parameter == nullptr) continue;
        const auto bindingsIt = bindingsByPathCache.find(entry.first);
        std::string value;
        if(bindingsIt == bindingsByPathCache.end() ||
           !computeParameterValue(bindingsIt->second, activeAutomationValues, parameter, value)) {
            // No longer automated: land on what is playing now, after the
            // queued events, so none of them is left standing.
            while(!state.pending.empty() && state.pending.front().first <= nowUs) {
                state.valueInEffect = state.pending.front().second;
                state.pending.pop_front();
            }
            value = state.valueInEffect;
            if(value.empty()) continue;
        }
        ofxOceanodeScheduledParameterEvent event;
        event.value = value;
        event.dueSteadyTimeUs = std::max(nowUs, state.lastScheduledDueUs + 1000);
        event.isCorrection = true;
        ofxOceanodeScheduling::dispatch(parameter, event);
    }
    if(onlyPaths == nullptr) scheduledPaths.clear();
    else for(const auto& path : *onlyPaths) scheduledPaths.erase(path);
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
                                    addSourceBeat(base + start + retriggerGapBeats * sourcePerTimelineBeat(clip));
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
                                    addSourceBeat(region.startBeat + retriggerGapBeats * sourcePerTimelineBeat(clip));
                                    addSourceBeat(region.end());
                                }
                            break;
                        }
                        case ofxOceanodeTimelineLaneType::PianoRoll: {
                            for(const auto& note : lane.pianoNotes) {
                                addSourceBeat(note.startBeat);
                                addSourceBeat(note.startBeat + retriggerGapBeats * sourcePerTimelineBeat(clip));
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
    // A loop wrap is continuous playback (the transport does not bump the
    // generation for it); only seeks and stops invalidate what was scheduled.
    const bool invalidated = generationChanged;

    if(!schedulingActive || invalidated) {
        if(!scheduledPaths.empty()) sendScheduleCorrections(nowUs);
        scheduledBackendPaths.clear();
        hasScheduleCursor = false;
        if(!schedulingActive) return;
    }

    // ---- which paths can be scheduled at all -----------------------------
    struct PathTarget {
        ofxOceanodeAbstractParameter* parameter = nullptr;
        std::vector<ofxOceanodeTimelineParameterBinding*> bindings;
        bool schedulable = true;
    };
    std::map<std::string, PathTarget> targets;
    for(const auto& entry : bindingsByPathCache) {
        auto& target = targets[entry.first];
        target.bindings = entry.second;
        target.schedulable = continuouslyEvaluatedPathsCache.count(entry.first) == 0;
    }

    std::vector<std::string> schedulablePaths;
    for(auto& entry : targets) {
        if(!entry.second.schedulable) continue;
        entry.second.parameter = findParameterCached(entry.first);
        if(entry.second.parameter == nullptr) continue;
        // Only a backend that can execute a value at an instant gets events.
        if(!ofxOceanodeScheduling::hasParameterTarget(entry.second.parameter)) continue;
        schedulablePaths.push_back(entry.first);
    }
    if(schedulablePaths.empty()) {
        // Same rule as every other invalidation path: events already handed
        // to a backend cannot be recalled, so correct them before forgetting
        // they exist. sendScheduleCorrections clears scheduledPaths itself.
        if(!scheduledPaths.empty()) sendScheduleCorrections(nowUs);
        scheduledPaths.clear();
        scheduledBackendPaths.clear();
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
            frameValue = parameterValueForAutomation(*target.parameter); // between clips: the held value
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
    {
        // Paths that stopped being scheduled (their binding went away, a curve
        // or a live override took them over): correct what is still queued.
        std::set<std::string> dropped;
        for(const auto& entry : scheduledPaths)
            if(activePaths.count(entry.first) == 0) dropped.insert(entry.first);
        if(!dropped.empty()) sendScheduleCorrections(nowUs, &dropped);
    }
    scheduledBackendPaths = suppressedPaths;
    if(activePaths.empty()) return;

    // ---- walk the window -------------------------------------------------
    // The window can run past the loop end: events of the next pass are
    // scheduled at the exact instant the transport will wrap, so the loop's
    // first beat is on time too.
    const double lookaheadSeconds = std::max(0.0, schedulingLookaheadMs) / 1000.0;
    const bool loopActive = transport.loopActive() &&
        transport.beatPosition < transport.loopEndBeat - kEpsilon;
    const uint64_t pass = transport.loopCount;
    if(!hasScheduleCursor || scheduleCursorPass < pass) {
        // First window, or the transport wrapped before anything past the
        // loop end was scheduled: start from where it is now.
        scheduleCursorBeat = transport.beatPosition;
        scheduleCursorPass = pass;
        hasScheduleCursor = true;
    }
    auto secondsAt = [&](double beat) {
        if(!scheduleUsesTempoMap) return beat * 60.0 / std::max(1.0f, transport.bpm);
        return beatToSeconds(beat, transport.bpm);
    };
    const double secondsNow = secondsAt(transport.beatPosition);
    const double horizonBeat = scheduleUsesTempoMap
        ? beatAfterSeconds(transport.beatPosition, lookaheadSeconds, transport.bpm)
        : transport.beatPosition + lookaheadSeconds * std::max(1.0f, transport.bpm) / 60.0;

    // (beat, seconds from now) pairs to schedule, in time order.
    std::vector<std::pair<double, double>> changes;
    if(scheduleCursorPass == pass) {
        if(scheduleCursorBeat < transport.beatPosition) scheduleCursorBeat = transport.beatPosition;
        const double thisPassEnd = loopActive ? std::min(horizonBeat, transport.loopEndBeat) : horizonBeat;
        if(thisPassEnd > scheduleCursorBeat) {
            std::vector<double> beats;
            collectChangeBeats(scheduleCursorBeat, thisPassEnd, false, beats);
            for(const double beat : beats) {
                // The loop end itself belongs to the next pass (its start).
                if(loopActive && beat >= transport.loopEndBeat - kEpsilon) continue;
                changes.push_back({beat, secondsAt(beat) - secondsNow});
            }
            scheduleCursorBeat = thisPassEnd;
        }
    }
    if(loopActive && horizonBeat > transport.loopEndBeat) {
        // The next pass: from the loop start (inclusive) for as long as the
        // window reaches past the end.
        const double untilWrap = secondsAt(transport.loopEndBeat) - secondsNow;
        const double nextPassEnd = std::min(transport.loopEndBeat,
            transport.loopStartBeat + (horizonBeat - transport.loopEndBeat));
        const bool freshPass = scheduleCursorPass == pass;
        const double nextPassFrom = freshPass ? transport.loopStartBeat : scheduleCursorBeat;
        if(nextPassEnd > nextPassFrom || freshPass) {
            std::vector<double> beats;
            collectChangeBeats(nextPassFrom, nextPassEnd, freshPass, beats);
            const double startSeconds = secondsAt(transport.loopStartBeat);
            for(const double beat : beats)
                changes.push_back({beat, untilWrap + secondsAt(beat) - startSeconds});
            scheduleCursorBeat = std::max(nextPassFrom, nextPassEnd);
            scheduleCursorPass = pass + 1;
        }
    }
    if(changes.empty()) return;

    ActiveValueMap futureValues;
    futureValues.reserve(bindingsByPathCache.size());
    for(const auto& change : changes) {
        const double beat = change.first;
        const double offsetSeconds = change.second;
        const uint64_t dueUs = nowUs + static_cast<uint64_t>(std::max(0.0, offsetSeconds) * 1000000.0);
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
        stateCaptureManualOverrideKeys.erase(trackId + "\x1f" + bindingId);
        binding->hasLiveOverride = true;
        binding->liveOverrideValue = value;
    }
}

void ofxOceanodeTimelineManager::clearLiveOverride(const std::string& trackId, const std::string& bindingId) {
    if(auto* binding = getBinding(trackId, bindingId)) {
        stateCaptureManualOverrideKeys.erase(trackId + "\x1f" + bindingId);
        binding->hasLiveOverride = false;
        binding->liveOverrideValue.clear();
    }
}

void ofxOceanodeTimelineManager::clearTimelineFlag(const ofxOceanodeTimelineTrack& track) {
    if(container == nullptr) return;
    for(const auto& binding : track.bindings) {
        if(auto* parameter = container->findTimelineParameter(binding.parameterPath)) parameter->setTimelined(false);
    }
}

void ofxOceanodeTimelineManager::refreshTimelineFlag(const std::string& parameterPath) {
    if(container == nullptr) return;
    const bool isActive = std::any_of(tracks.begin(), tracks.end(), [&](const auto& track) {
        return std::any_of(track.bindings.begin(), track.bindings.end(), [&](const auto& binding) {
            return binding.parameterPath == parameterPath && !binding.bypass;
        });
    });
    if(auto* parameter = container->findTimelineParameter(parameterPath))
        parameter->setTimelined(isActive);
}

void ofxOceanodeTimelineManager::clear() {
    // Correct anything already queued on a backend while the bindings that
    // know the right values still exist (afterwards a queued gate-on would
    // play with nothing to turn it off).
    invalidateSchedule();
    for(const auto& track : tracks) clearTimelineFlag(track);
    tracks.clear();
    invalidateEvaluationIndexes();
    clipGroups.clear();
    markers.clear();
    viewState = {};
    nextTrackNumber = 1;
    nextBindingNumber = 1;
    nextClipNumber = 1;
    nextLaneNumber = 1;
    nextGroupNumber = 1;
    pendingTrackRenameId.clear();
    pendingTrackRenameIsNew = false;
    activeAutomationValues.clear();
    stateCaptureLastAppliedValues.clear();
    stateCaptureManualOverrideKeys.clear();
    invalidateSchedule();
    bpmAutomationEnabled = false;
    bpmLaneCollapsed = true;
    bpmLaneVisible = false;
    bpmMinimum = 20.0f;
    bpmMaximum = 300.0f;
    bpmAutomationPoints.clear();
    bpmCurveTensions.clear();
    bpmInterpolation = "Linear";
    loopEnabled = false;
    loopStartBeat = 0.0;
    loopEndBeat = 4.0;
    syncLoopToTransport();
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
    json["version"] = kPresetVersion;
    json["view"] = {
        {"pixelsPerSecond", viewState.pixelsPerSecond},
        {"scrollX", viewState.scrollX}
    };
    json["tempo"] = {
        {"enabled", bpmAutomationEnabled},
        {"collapsed", bpmLaneCollapsed},
        {"visible", bpmLaneVisible},
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
    json["markers"] = ofJson::array();
    for(const auto& marker : markers)
        json["markers"].push_back({{"id", marker.id}, {"name", marker.name}, {"beat", marker.beat}});
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
                {"clampToRange", binding.clampToParameterRange},
                {"rowHeight", binding.rowHeight}
            });
        }
        trackJson["noteGroups"] = ofJson::array();
        for(const auto& group : track.noteGroups) {
            trackJson["noteGroups"].push_back({
                {"id", group.id},
                {"name", group.name},
                {"pitch", group.pitchBindingId},
                {"gate", group.gateBindingId},
                {"velocity", group.velocityBindingId},
                {"expanded", group.expanded},
                {"rowHeight", group.rowHeight}
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
            clipJson["isStateCapture"] = clip.isStateCapture;
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
                laneJson["probabilitySeed"] = lane.probabilitySeed;
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
                laneJson["valueSnap"] = lane.valueSnap;
                laneJson["lfoParameter"] = lane.lfoParameter;
                laneJson["lfoValue"] = lane.lfoValue;
                laneJson["lfoSnap"] = lane.lfoSnap;
                laneJson["lfoSnapMode"] = lane.lfoSnapMode;
                // The Wave lane and the per-clip volume lane are load-only
                // (see ofxOceanodeTimelineLane): audio and its envelope are
                // written at clip and track level instead.
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
    const int fileVersion = json.value("version", kPresetVersion);
    if(fileVersion > kPresetVersion) {
        ofLogWarning("ofxOceanodeTimeline")
            << "Timeline was written by a newer version (" << fileVersion << " > " << kPresetVersion
            << "); anything this build does not know about will be dropped on the next save.";
    }
    clear();
    if(!json.is_object()) return;
    if(json.contains("view") && json["view"].is_object()) {
        const auto& view = json["view"];
        viewState.pixelsPerSecond = ofClamp(
            view.value("pixelsPerSecond", kDefaultTimelinePixelsPerSecond),
            kMinTimelinePixelsPerSecond, kMaxTimelinePixelsPerSecond);
        viewState.scrollX = std::max(0.0f, view.value("scrollX", 0.0f));
    }
    if(!json.contains("tracks") || !json["tracks"].is_array()) return;

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
        bpmLaneVisible = tempo.value("visible", false);
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
    if(json.contains("markers") && json["markers"].is_array()) {
        for(const auto& markerJson : json["markers"]) {
            if(!markerJson.is_object()) continue;
            try {
                const std::string id = addMarker(markerJson.value("beat", 0.0), markerJson.value("name", std::string()));
                const std::string savedId = markerJson.value("id", std::string());
                if(!savedId.empty() && getMarker(savedId) == nullptr)
                    for(auto& marker : markers) if(marker.id == id) marker.id = savedId;
            } catch(const std::exception& e) {
                ofLogError("ofxOceanodeTimeline") << "Skipped a marker that could not be read: " << e.what();
            }
        }
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
        // One unreadable track (a field of the wrong type throws) is skipped
        // instead of losing the whole timeline.
        try {
        ofxOceanodeTimelineTrack track;
        // Saved binding id -> loaded binding id (ids can be renamed on a
        // collision, and a duplicate parameter maps onto the row kept).
        std::map<std::string, std::string> bindingRemap;
        auto mapBinding = [&](const std::string& savedId) {
            const auto it = bindingRemap.find(savedId);
            return it == bindingRemap.end() ? savedId : it->second;
        };
        const std::string originalTrackId = trackJson.value("id", std::string());
        track.id = uniqueLoadedId(originalTrackId, "timeline_track", nextTrackNumber, loadedTrackIds);
        if(!originalTrackId.empty()) loadedTrackIdRemap[originalTrackId] = track.id;
        track.name = makeUniqueTrackName(trackJson.value("name", std::string("Timeline Track")));
        track.collapsed = trackJson.value("collapsed", false);
        track.isWaveTrack = trackJson.value("isWaveTrack", false);
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
                const std::string savedBindingId = bindingJson.value("id", std::string());
                binding.parameterPath = bindingJson.value("parameterPath", std::string());
                if(binding.parameterPath.empty()) continue;
                const auto duplicate = std::find_if(track.bindings.begin(), track.bindings.end(), [&](const auto& existing) {
                    return existing.parameterPath == binding.parameterPath;
                });
                if(duplicate != track.bindings.end()) {
                    if(!savedBindingId.empty()) bindingRemap[savedBindingId] = duplicate->id;
                    continue;
                }
                binding.id = uniqueLoadedId(savedBindingId, "timeline_binding", nextBindingNumber, loadedBindingIds);
                if(!savedBindingId.empty()) bindingRemap[savedBindingId] = binding.id;
                binding.valueType = bindingJson.value("valueType", std::string());
                binding.defaultValue = bindingJson.value("defaultValue", std::string());
                binding.mode = modeFromString(bindingJson.value("mode", std::string("Replace")));
                binding.laneType = laneTypeFromString(bindingJson.value("laneType", std::string("Step")));
                binding.bypass = bindingJson.value("bypass", false);
                binding.clampToParameterRange = bindingJson.value("clampToRange", true);
                binding.rowHeight = ofClamp(bindingJson.value("rowHeight", 28.0f), 20.0f, 300.0f);
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
                clip.isStateCapture = clipJson.value("isStateCapture", false);
                clip.lfoOutputBindingId = mapBinding(clipJson.value("lfoOutputBindingId", std::string()));
                if(std::none_of(track.bindings.begin(), track.bindings.end(),
                                [&](const auto& b) { return b.id == clip.lfoOutputBindingId; }))
                    clip.lfoOutputBindingId.clear();
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
                        lane.probabilitySeed = laneJson.value("probabilitySeed", 0);
                        lane.behavior = laneJson.value("behavior", std::string("Probability"));
                        if(lane.behavior != "Probability" && lane.behavior != "Always" && lane.behavior != "Mute")
                            lane.behavior = "Probability";
                        lane.pianoLowPitch = ofClamp(laneJson.value("pianoLowPitch", 36), 0, 127);
                        lane.pianoHighPitch = ofClamp(laneJson.value("pianoHighPitch", 84), lane.pianoLowPitch, 127);
                        lane.pianoSnapToGrid = laneJson.value("pianoSnapToGrid", true);
                        lane.pianoPitchBindingId = mapBinding(laneJson.value("pianoPitchBindingId", std::string()));
                        lane.pianoGateBindingId = mapBinding(laneJson.value("pianoGateBindingId", std::string()));
                        lane.pianoVelocityBindingId = mapBinding(laneJson.value("pianoVelocityBindingId", std::string()));
                        lane.pianoDefaultVelocity = ofClamp(laneJson.value("pianoDefaultVelocity", 0.8f), 0.0f, 1.0f);
                        lane.pianoMonophonic = laneJson.value("pianoMonophonic", false);
                        lane.curveClamp = laneJson.value("curveClamp", true);
                        lane.isWaveVolume = laneJson.value("isWaveVolume", false);
                        lane.lfoParameter = laneJson.value("lfoParameter", std::string());
                        lane.lfoValue = ofClamp(laneJson.value("lfoValue", 0.5f), 0.0f, 1.0f);
                        lane.lfoSnap = laneJson.value("lfoSnap", false);
                        lane.lfoSnapMode = std::max(0, std::min(2, laneJson.value("lfoSnapMode", 0)));
                        lane.curveInterpolation = laneJson.value("curveInterpolation", std::string("Linear"));
                        const bool lfoValueMode = !lane.lfoParameter.empty() && lane.curveInterpolation == "Value";
                        if(!lfoValueMode && lane.curveInterpolation != "Step" && lane.curveInterpolation != "Linear" &&
                           lane.curveInterpolation != "Log / Exp" && lane.curveInterpolation != "Sigmoid") {
                            lane.curveInterpolation = "Linear";
                        }
                        if(laneJson.contains("bindingIds") && laneJson["bindingIds"].is_array()) {
                            for(const auto& bindingId : laneJson["bindingIds"]) {
                                if(!bindingId.is_string()) continue;
                                const std::string id = mapBinding(bindingId.get<std::string>());
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
                        // Before the toggle existed, a step count of 2+ meant snapping was on.
                        lane.valueSnap = laneJson.value("valueSnap", lane.valueQuantizeSteps >= 2);
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
        // A shared capture has one destination; after "Separate parameter
        // clips" there is intentionally one destination per parameter.
        // Wave/LFO clips can never participate in either form.
        for(auto& clip : track.clips) {
            if(track.isWaveTrack || clip.isLfo) clip.isStateCapture = false;
        }
        if(track.isWaveTrack && track.waveVolumePoints.empty()) {
            track.waveVolumePoints = {{0.0, track.waveVolume}, {4.0, track.waveVolume}};
            track.waveVolumeTensions.assign(1, ofxOceanodeTimelineCurveTension{});
        }

        if(trackJson.contains("noteGroups") && trackJson["noteGroups"].is_array()) {
            for(const auto& groupJson : trackJson["noteGroups"]) {
                if(!groupJson.is_object()) continue;
                ofxOceanodeTimelineNoteGroup group;
                group.id = groupJson.value("id", std::string());
                if(group.id.empty() || std::any_of(track.noteGroups.begin(), track.noteGroups.end(),
                                                   [&](const auto& g) { return g.id == group.id; })) continue;
                group.name = groupJson.value("name", std::string("Notes"));
                group.pitchBindingId = mapBinding(groupJson.value("pitch", std::string()));
                group.gateBindingId = mapBinding(groupJson.value("gate", std::string()));
                group.velocityBindingId = mapBinding(groupJson.value("velocity", std::string()));
                group.expanded = groupJson.value("expanded", false);
                group.rowHeight = ofClamp(groupJson.value("rowHeight", 48.0f), 20.0f, 300.0f);
                // A binding belongs to one group only.
                for(auto* role : {&group.pitchBindingId, &group.gateBindingId, &group.velocityBindingId})
                    if(findNoteGroupForBinding(track, *role) != nullptr) role->clear();
                track.noteGroups.push_back(group);
            }
            normalizeNoteGroups(track);
        }

        tracks.push_back(std::move(track));
        } catch(const std::exception& e) {
            ofLogError("ofxOceanodeTimeline") << "Skipped a timeline track that could not be read: " << e.what();
        }
    }

    // Continue numbering after the highest id in use, so an id is never
    // handed out again while something may still remember it.
    auto nextAfter = [](const std::string& id, uint64_t current) {
        const auto underscore = id.find_last_of('_');
        if(underscore == std::string::npos || underscore + 1 >= id.size()) return current;
        const std::string digits = id.substr(underscore + 1);
        if(!std::all_of(digits.begin(), digits.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) return current;
        if(digits.size() > 18) return current;
        return std::max<uint64_t>(current, std::stoull(digits) + 1);
    };
    nextTrackNumber = tracks.size() + 1;
    nextBindingNumber = 1;
    nextClipNumber = 1;
    nextLaneNumber = 1;
    for(const auto& track : tracks) {
        nextTrackNumber = nextAfter(track.id, nextTrackNumber);
        for(const auto& binding : track.bindings) nextBindingNumber = nextAfter(binding.id, nextBindingNumber);
        for(const auto& clip : track.clips) {
            nextClipNumber = nextAfter(clip.id, nextClipNumber);
            for(const auto& lane : clip.lanes) nextLaneNumber = nextAfter(lane.id, nextLaneNumber);
        }
    }

    // Rebuild waveform caches after loading, once ids and clip storage are
    // final. Legacy Wave lanes were migrated to their containing clip above.
    for(auto& track : tracks) {
        for(auto& clip : track.clips) {
            if(track.isWaveTrack && !clip.waveFilePath.empty()) reloadWaveform(track.id, clip.id);
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
    for(const auto& group : clipGroups) nextGroupNumber = nextAfter(group.id, nextGroupNumber);

    // Give loaded LFO clips any control lane they are missing (collect ids first:
    // adding lanes must not happen while iterating the clip containers).
    std::vector<std::pair<std::string, std::string>> lfoClipIds;
    for(const auto& track : tracks) {
        for(const auto& clip : track.clips) {
            if(clip.isLfo) lfoClipIds.emplace_back(track.id, clip.id);
        }
    }
    for(const auto& ids : lfoClipIds) ensureLfoLanes(ids.first, ids.second);
}

bool ofxOceanodeTimelineManager::savePreset(const std::string& presetFolderPath) const {
    const std::string path = presetFolderPath + "/timeline.json";
    if(!failedLoadPath.empty() && failedLoadPath == path && ofFile::doesFileExist(path)) {
        // This file could not be read when it was loaded: keep a copy before
        // overwriting it, so nothing is lost for good.
        ofFile::copyFromTo(path, presetFolderPath + "/timeline.unreadable.json", false, true);
        failedLoadPath.clear();
    }
    return ofSavePrettyJson(path, toJson());
}

bool ofxOceanodeTimelineManager::loadPreset(const std::string& presetFolderPath) {
    const std::string path = presetFolderPath + "/timeline.json";
    if(!ofFile::doesFileExist(path)) {
        clear();
        return false;
    }
    failedLoadPath.clear();
    try {
        const ofJson json = ofLoadJson(path);
        if(!json.is_object()) {
            // ofLoadJson reports a parse error and hands back nothing.
            ofLogError("ofxOceanodeTimeline") << "Could not read " << path
                << " (a copy is kept as timeline.unreadable.json on the next save)";
            clear();
            failedLoadPath = path;
            return false;
        }
        fromJson(json);
        return true;
    } catch(const std::exception& e) {
        ofLogError("ofxOceanodeTimeline") << "Could not load timeline: " << e.what()
            << " (a copy is kept as timeline.unreadable.json on the next save)";
        clear();
        failedLoadPath = path;
        return false;
    }
}
