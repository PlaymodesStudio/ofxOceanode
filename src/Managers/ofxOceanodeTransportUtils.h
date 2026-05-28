#ifndef ofxOceanodeTransportUtils_h
#define ofxOceanodeTransportUtils_h

#include "ofxOceanodeTransport.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

struct ofxOceanodeTransportStepRange {
    bool valid = false;
    int64_t firstStep = 0;
    int64_t lastStep = -1;

    int64_t count() const {
        return valid ? std::max<int64_t>(0, lastStep - firstStep + 1) : 0;
    }
};

namespace ofxOceanodeTransportUtils {
    static constexpr double StepEpsilon = 1e-6;

    inline double normalizeStepsPerBeat(double stepsPerBeat) {
        return std::max(std::abs(stepsPerBeat), StepEpsilon);
    }

    inline double beatToStepPosition(double beatPosition, double stepsPerBeat) {
        return std::max(0.0, beatPosition) * normalizeStepsPerBeat(stepsPerBeat);
    }

    inline int64_t beatToStepIndex(double beatPosition, double stepsPerBeat, double epsilon = StepEpsilon) {
        return static_cast<int64_t>(std::floor(beatToStepPosition(beatPosition, stepsPerBeat) + epsilon));
    }

    inline double stepIndexToBeat(int64_t stepIndex, double stepsPerBeat) {
        return static_cast<double>(stepIndex) / normalizeStepsPerBeat(stepsPerBeat);
    }

    inline bool isStepBoundary(double beatPosition, double stepsPerBeat, double epsilon = StepEpsilon) {
        const double stepPosition = beatToStepPosition(beatPosition, stepsPerBeat);
        return std::abs(stepPosition - std::round(stepPosition)) <= epsilon;
    }

    inline bool didGenerationChange(const ofxOceanodeFrameTransportState &frameState) {
        return frameState.previous.generation != frameState.current.generation;
    }

    inline bool didBeatRewind(const ofxOceanodeFrameTransportState &frameState, double epsilon = StepEpsilon) {
        return frameState.current.beatPosition + epsilon < frameState.previous.beatPosition;
    }

    inline bool didTransportDiscontinuity(const ofxOceanodeFrameTransportState &frameState, double epsilon = StepEpsilon) {
        return didGenerationChange(frameState) || didBeatRewind(frameState, epsilon);
    }

    inline ofxOceanodeTransportStepRange getCrossedStepRange(const ofxOceanodeFrameTransportState &frameState,
                                                             double stepsPerBeat,
                                                             double epsilon = StepEpsilon) {
        ofxOceanodeTransportStepRange range;
        if(didTransportDiscontinuity(frameState, epsilon)) {
            return range;
        }

        const int64_t previousStep = beatToStepIndex(frameState.previous.beatPosition, stepsPerBeat, epsilon);
        const int64_t currentStep = beatToStepIndex(frameState.current.beatPosition, stepsPerBeat, epsilon);
        if(currentStep <= previousStep) {
            return range;
        }

        range.valid = true;
        range.firstStep = previousStep + 1;
        range.lastStep = currentStep;
        return range;
    }

    inline int64_t positiveModulo(int64_t value, int64_t size) {
        if(size <= 0) {
            return 0;
        }
        int64_t wrapped = value % size;
        if(wrapped < 0) {
            wrapped += size;
        }
        return wrapped;
    }

    inline double wrapPhase(double value, double length) {
        if(length <= StepEpsilon) {
            return 0.0;
        }
        double wrapped = std::fmod(value, length);
        if(wrapped < 0.0) {
            wrapped += length;
        }
        return wrapped;
    }
}

#endif
