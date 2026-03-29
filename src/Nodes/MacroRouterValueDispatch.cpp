//
//  MacroRouterValueDispatch.cpp
//  ofxOceanode
//
//  Extracted from ofxOceanodeNodeMacro — Phase 1 refactoring.
//

#include "MacroRouterValueDispatch.h"
#include "ofxOceanodeParameter.h"
#include "ofColor.h"

// RouterSnapshot is now defined in MacroSnapshotSystem.h (Phase 2 refactoring).
#include "MacroSnapshotSystem.h"   // for RouterSnapshot definition

// ─────────────────────────────────────────────────────────────────────────────
// captureValue — captures current router value into a RouterSnapshot
// ─────────────────────────────────────────────────────────────────────────────

RouterSnapshot MacroRouterValueDispatch::captureValue(ofxOceanodeAbstractParameter* valueParam) {
	RouterSnapshot snapshot;
	snapshot.type = valueParam->valueType();

	if(snapshot.type == typeid(float).name()) {
		snapshot.value = valueParam->cast<float>().getParameter().get();
	}
	else if(snapshot.type == typeid(std::vector<float>).name()) {
		snapshot.value = valueParam->cast<std::vector<float>>().getParameter().get();
	}
	else if(snapshot.type == typeid(int).name()) {
		int val = valueParam->cast<int>().getParameter().get();
		snapshot.value = static_cast<float>(val);
	}
	else if(snapshot.type == typeid(std::vector<int>).name()) {
		auto vec = valueParam->cast<std::vector<int>>().getParameter().get();
		std::vector<float> floatVec;
		for(auto val : vec) {
			floatVec.push_back(static_cast<float>(val));
		}
		snapshot.value = floatVec;
	}
	else if(snapshot.type == typeid(bool).name()) {
		snapshot.value = valueParam->cast<bool>().getParameter().get();
	}
	else if(snapshot.type == typeid(std::vector<bool>).name()) {
		auto vec = valueParam->cast<std::vector<bool>>().getParameter().get();
		std::vector<bool> boolVec(vec.begin(), vec.end());
		snapshot.value = boolVec;
	}
	else if(snapshot.type == typeid(std::string).name()) {
		snapshot.value = valueParam->cast<std::string>().getParameter().get();
	}
	else if(snapshot.type == typeid(std::vector<std::string>).name()) {
		snapshot.value = valueParam->cast<std::vector<std::string>>().getParameter().get();
	}
	else if(snapshot.type == typeid(ofColor).name()) {
		auto color = valueParam->cast<ofColor>().getParameter().get();
		snapshot.value = {
			{"r", color.r},
			{"g", color.g},
			{"b", color.b},
			{"a", color.a}
		};
	}
	else if(snapshot.type == typeid(ofFloatColor).name()) {
		auto color = valueParam->cast<ofFloatColor>().getParameter().get();
		snapshot.value = {
			{"r", color.r},
			{"g", color.g},
			{"b", color.b},
			{"a", color.a}
		};
	}
	else if(snapshot.type == typeid(void).name()) {
		snapshot.value = nullptr;  // Just store the trigger state
	}

	return snapshot;
}

// ─────────────────────────────────────────────────────────────────────────────
// applyValue — applies a RouterSnapshot value to a router parameter
// ─────────────────────────────────────────────────────────────────────────────

void MacroRouterValueDispatch::applyValue(ofxOceanodeAbstractParameter* valueParam, const RouterSnapshot& snapshot) {
	std::string vt = valueParam->valueType();

	if(vt == typeid(float).name()) {
		valueParam->cast<float>().getParameter() = snapshot.value.get<float>();
	}
	else if(vt == typeid(std::vector<float>).name()) {
		valueParam->cast<std::vector<float>>().getParameter() = snapshot.value.get<std::vector<float>>();
	}
	else if(vt == typeid(int).name()) {
		valueParam->cast<int>().getParameter() = static_cast<int>(round(snapshot.value.get<float>()));
	}
	else if(vt == typeid(std::vector<int>).name()) {
		auto floatVec = snapshot.value.get<std::vector<float>>();
		std::vector<int> intVec;
		for(auto val : floatVec) {
			intVec.push_back(static_cast<int>(round(val)));
		}
		valueParam->cast<std::vector<int>>().getParameter() = intVec;
	}
	else if(vt == typeid(bool).name()) {
		valueParam->cast<bool>().getParameter() = snapshot.value.get<bool>();
	}
	else if(vt == typeid(std::vector<bool>).name()) {
		valueParam->cast<std::vector<bool>>().getParameter() = snapshot.value.get<std::vector<bool>>();
	}
	else if(vt == typeid(std::string).name()) {
		valueParam->cast<std::string>().getParameter() = snapshot.value.get<std::string>();
	}
	else if(vt == typeid(std::vector<std::string>).name()) {
		valueParam->cast<std::vector<std::string>>().getParameter() = snapshot.value.get<std::vector<std::string>>();
	}
	else if(vt == typeid(ofColor).name()) {
		ofColor color;
		auto& colorJson = snapshot.value;
		color.r = colorJson["r"].get<int>();
		color.g = colorJson["g"].get<int>();
		color.b = colorJson["b"].get<int>();
		color.a = colorJson["a"].get<int>();
		valueParam->cast<ofColor>().getParameter() = color;
	}
	else if(vt == typeid(ofFloatColor).name()) {
		ofFloatColor color;
		auto& colorJson = snapshot.value;
		color.r = colorJson["r"].get<float>();
		color.g = colorJson["g"].get<float>();
		color.b = colorJson["b"].get<float>();
		color.a = colorJson["a"].get<float>();
		valueParam->cast<ofFloatColor>().getParameter() = color;
	}
	else if(vt == typeid(void).name()) {
		valueParam->cast<void>().getParameter().trigger();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// shouldInterpolate — returns true if a type supports smooth interpolation
// ─────────────────────────────────────────────────────────────────────────────

bool MacroRouterValueDispatch::shouldInterpolate(const std::string& type) {
	return type == typeid(float).name()
		|| type == typeid(int).name()
		|| type == typeid(std::vector<float>).name()
		|| type == typeid(std::vector<int>).name()
		|| type == typeid(ofColor).name()
		|| type == typeid(ofFloatColor).name();
}

// ─────────────────────────────────────────────────────────────────────────────
// captureForInterpolation — captures value for interpolation (ints→floats)
// ─────────────────────────────────────────────────────────────────────────────

RouterSnapshot MacroRouterValueDispatch::captureForInterpolation(ofxOceanodeAbstractParameter* valueParam) {
	RouterSnapshot snap;
	snap.type = valueParam->valueType();

	if(snap.type == typeid(float).name()) {
		snap.value = valueParam->cast<float>().getParameter().get();
	}
	else if(snap.type == typeid(int).name()) {
		snap.value = static_cast<float>(valueParam->cast<int>().getParameter().get());
	}
	else if(snap.type == typeid(std::vector<float>).name()) {
		snap.value = valueParam->cast<std::vector<float>>().getParameter().get();
	}
	else if(snap.type == typeid(std::vector<int>).name()) {
		auto vec = valueParam->cast<std::vector<int>>().getParameter().get();
		std::vector<float> fv;
		for(auto v : vec) fv.push_back(static_cast<float>(v));
		snap.value = fv;
	}
	else if(snap.type == typeid(ofColor).name()) {
		auto c = valueParam->cast<ofColor>().getParameter().get();
		snap.value = {{"r", (int)c.r}, {"g", (int)c.g}, {"b", (int)c.b}, {"a", (int)c.a}};
	}
	else if(snap.type == typeid(ofFloatColor).name()) {
		auto c = valueParam->cast<ofFloatColor>().getParameter().get();
		snap.value = {{"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a}};
	}

	return snap;
}
