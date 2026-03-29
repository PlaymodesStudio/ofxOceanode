//
//  MacroMorphEngine.h
//  ofxOceanode
//
//  Extracted from ofxOceanodeNodeMacro — Phase 3 refactoring.
//  Manages morph/interpolation between router snapshots.
//

#ifndef MacroMorphEngine_h
#define MacroMorphEngine_h

#include <string>
#include <map>
#include <cstdint>

// Forward declarations
struct RouterInfo;
struct RouterSnapshot;

class MacroMorphEngine {
public:
	MacroMorphEngine();

	// Configuration (these are the morph slider values, not per-snapshot)
	float& getMorphMs() { return interpolationMs; }
	float& getMorphBiPow() { return interpolationBiPow; }

	// State
	bool isMorphing() const { return isInterpolating; }
	float getProgress() const { return currentMorphProgress; }

	// Start a morph from current values to target values
	void startMorph(float ms, float biPow,
	                const std::map<std::string, RouterSnapshot>& startValues,
	                const std::map<std::string, RouterSnapshot>& targetValues);

	// Stop any ongoing morph
	void stop();

	// Call each frame — returns true if still interpolating
	// Applies interpolated values to the provided routerNodes
	bool update(const std::map<std::string, RouterInfo>& routerNodes);

	// Set progress to complete (used for immediate apply)
	void setComplete();

	// BiPow curve utility
	static void customPow(float& value, float pow);

private:
	void applyInterpolatedValues(float t, const std::map<std::string, RouterInfo>& routerNodes);

	float interpolationMs;
	float interpolationBiPow;
	bool isInterpolating;
	uint64_t interpolationStartTime;
	float interpolationBiPowCapture;
	std::map<std::string, RouterSnapshot> interpolationStartValues;
	std::map<std::string, RouterSnapshot> interpolationTargetValues;
	float currentMorphProgress;
};

#endif /* MacroMorphEngine_h */
