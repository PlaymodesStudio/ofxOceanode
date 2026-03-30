//
//  MacroRouterValueDispatch.h
//  ofxOceanode
//
//  Extracted from ofxOceanodeNodeMacro — Phase 1 refactoring.
//  Static utility class that eliminates duplicated type-dispatch chains
//  for capturing, applying, and interpolating router parameter values.
//

#ifndef MacroRouterValueDispatch_h
#define MacroRouterValueDispatch_h

#include <string>
#include "ofJson.h"

// Forward declarations — the full headers are included only in the .cpp
class ofxOceanodeAbstractParameter;

// RouterSnapshot is defined in MacroSnapshotSystem.h; we forward-reference it
// here to keep the header lightweight and avoid a circular include.
// (The struct is small and has no dependencies beyond std::string and ofJson.)
struct RouterSnapshot;

class MacroRouterValueDispatch {
public:
	/// Capture the current value of a router parameter into a RouterSnapshot.
	/// Handles: float, int, vector<float>, vector<int>, bool, vector<bool>,
	///          string, vector<string>, ofColor, ofFloatColor, void.
	/// Integer types are stored as floats (for interpolation compatibility).
	static RouterSnapshot captureValue(ofxOceanodeAbstractParameter* valueParam);

	/// Apply a RouterSnapshot value to a router parameter.
	/// Handles all the same types as captureValue().
	/// Integer types stored as floats are rounded back to int on apply.
	static void applyValue(ofxOceanodeAbstractParameter* valueParam, const RouterSnapshot& snapshot);

	/// Returns true if the given type name supports smooth interpolation.
	/// Types: float, int, vector<float>, vector<int>, ofColor, ofFloatColor.
	static bool shouldInterpolate(const std::string& type);

	/// Capture the current value in interpolation-compatible format.
	/// Same as captureValue() but only for interpolatable types, and
	/// converts int→float so interpolation math works on uniform types.
	static RouterSnapshot captureForInterpolation(ofxOceanodeAbstractParameter* valueParam);
};

#endif /* MacroRouterValueDispatch_h */
