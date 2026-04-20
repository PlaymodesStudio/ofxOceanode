//
//  MacroMorphEngine.cpp
//  ofxOceanode
//
//  Extracted from ofxOceanodeNodeMacro — Phase 3 refactoring.
//  Manages morph/interpolation between router snapshots.
//

#include "MacroMorphEngine.h"
#include "MacroSnapshotSystem.h"
#include "MacroRouterValueDispatch.h"
#include "ofxOceanodeNodeMacro.h"
#include "router.h"
#include "ofMain.h"

MacroMorphEngine::MacroMorphEngine()
	: interpolationMs(0.f)
	, interpolationBiPow(0.f)
	, isInterpolating(false)
	, interpolationStartTime(0)
	, interpolationBiPowCapture(0.f)
	, currentMorphProgress(0.f)
{
}

void MacroMorphEngine::startMorph(float ms, float biPow,
                                  const std::map<std::string, RouterSnapshot>& startValues,
                                  const std::map<std::string, RouterSnapshot>& targetValues)
{
	// Stop any ongoing interpolation
	isInterpolating = false;

	interpolationStartValues = startValues;
	interpolationTargetValues = targetValues;

	// Start interpolation
	interpolationBiPowCapture = biPow;
	interpolationStartTime = ofGetElapsedTimeMillis();
	interpolationMs = ms;
	isInterpolating = true;
}

void MacroMorphEngine::stop() {
	isInterpolating = false;
}

bool MacroMorphEngine::update(const std::map<std::string, RouterInfo>& routerNodes) {
	if(!isInterpolating) return false;

	float elapsed = (float)(ofGetElapsedTimeMillis() - interpolationStartTime);
	float t = ofClamp(elapsed / std::max(interpolationMs, 1.f), 0.f, 1.f);
	float mappedT = t;
	if(interpolationBiPowCapture != 0.f) {
		mappedT = mappedT * 2.f - 1.f;       // [0,1] → [-1,1]
		customPow(mappedT, interpolationBiPowCapture);
		mappedT = (mappedT + 1.f) * 0.5f;    // [-1,1] → [0,1]
	}
	currentMorphProgress = mappedT;
	applyInterpolatedValues(mappedT, routerNodes);
	if(t >= 1.f) {
		isInterpolating = false;
		currentMorphProgress = 1.f;
	}
	return isInterpolating;
}

void MacroMorphEngine::setComplete() {
	isInterpolating = false;
	currentMorphProgress = 1.f;
}

void MacroMorphEngine::customPow(float& value, float pow) {
	float k1 = 2.f * pow * 0.99999f;
	float k2 = k1 / ((-pow * 0.999999f) + 1.f);
	float k3 = k2 * std::abs(value) + 1.f;
	value = value * (k2 + 1.f) / k3;
}

void MacroMorphEngine::applyInterpolatedValues(float t, const std::map<std::string, RouterInfo>& routerNodes) {
	for(auto& routerPair : routerNodes) {
		if(!routerPair.second.isInput) continue;

		auto targetIt = interpolationTargetValues.find(routerPair.second.routerName);
		if(targetIt == interpolationTargetValues.end()) continue;
		if(!MacroRouterValueDispatch::shouldInterpolate(targetIt->second.type)) continue;

		auto startIt = interpolationStartValues.find(routerPair.second.routerName);

		auto& router = routerPair.second;
		auto& params = router.node->getParameters();
		if(!params.contains("Value")) continue;
		auto valueParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get("Value"));
		if(!valueParam) continue;
		abstractRouter* absRouter = dynamic_cast<abstractRouter*>(&router.node->getNodeModel());
		if(absRouter == nullptr || absRouter->isExcludeFromSnapshot()) continue;

		try {
			if(valueParam->valueType() == typeid(float).name()) {
				float sv = (startIt != interpolationStartValues.end())
					? startIt->second.value.get<float>()
					: targetIt->second.value.get<float>();
				float tv = targetIt->second.value.get<float>();
				valueParam->cast<float>().getParameter() = sv + (tv - sv) * t;
			}
			else if(valueParam->valueType() == typeid(int).name()) {
				float sv = (startIt != interpolationStartValues.end())
					? startIt->second.value.get<float>()
					: targetIt->second.value.get<float>();
				float tv = targetIt->second.value.get<float>();
				valueParam->cast<int>().getParameter() = static_cast<int>(round(sv + (tv - sv) * t));
			}
			else if(valueParam->valueType() == typeid(vector<float>).name()) {
				auto tv = targetIt->second.value.get<vector<float>>();
				vector<float> sv = (startIt != interpolationStartValues.end())
					? startIt->second.value.get<vector<float>>()
					: tv;
				vector<float> result;
				for(size_t i = 0; i < tv.size(); i++) {
					float s = (i < sv.size()) ? sv[i] : tv[i];
					result.push_back(s + (tv[i] - s) * t);
				}
				valueParam->cast<vector<float>>().getParameter() = result;
			}
			else if(valueParam->valueType() == typeid(vector<int>).name()) {
				auto tv = targetIt->second.value.get<vector<float>>();
				vector<float> sv = (startIt != interpolationStartValues.end())
					? startIt->second.value.get<vector<float>>()
					: tv;
				vector<int> result;
				for(size_t i = 0; i < tv.size(); i++) {
					float s = (i < sv.size()) ? sv[i] : tv[i];
					result.push_back(static_cast<int>(round(s + (tv[i] - s) * t)));
				}
				valueParam->cast<vector<int>>().getParameter() = result;
			}
			else if(valueParam->valueType() == typeid(ofColor).name()) {
				auto& tcj = targetIt->second.value;
				ofColor tc; tc.r = tcj["r"].get<int>(); tc.g = tcj["g"].get<int>(); tc.b = tcj["b"].get<int>(); tc.a = tcj["a"].get<int>();
				ofColor sc = tc;
				if(startIt != interpolationStartValues.end()) {
					auto& scj = startIt->second.value;
					sc.r = scj["r"].get<int>(); sc.g = scj["g"].get<int>(); sc.b = scj["b"].get<int>(); sc.a = scj["a"].get<int>();
				}
				ofColor result;
				result.r = (int)(sc.r + (tc.r - sc.r) * t);
				result.g = (int)(sc.g + (tc.g - sc.g) * t);
				result.b = (int)(sc.b + (tc.b - sc.b) * t);
				result.a = (int)(sc.a + (tc.a - sc.a) * t);
				valueParam->cast<ofColor>().getParameter() = result;
			}
			else if(valueParam->valueType() == typeid(ofFloatColor).name()) {
				auto& tcj = targetIt->second.value;
				ofFloatColor tc; tc.r = tcj["r"].get<float>(); tc.g = tcj["g"].get<float>(); tc.b = tcj["b"].get<float>(); tc.a = tcj["a"].get<float>();
				ofFloatColor sc = tc;
				if(startIt != interpolationStartValues.end()) {
					auto& scj = startIt->second.value;
					sc.r = scj["r"].get<float>(); sc.g = scj["g"].get<float>(); sc.b = scj["b"].get<float>(); sc.a = scj["a"].get<float>();
				}
				ofFloatColor result;
				result.r = sc.r + (tc.r - sc.r) * t;
				result.g = sc.g + (tc.g - sc.g) * t;
				result.b = sc.b + (tc.b - sc.b) * t;
				result.a = sc.a + (tc.a - sc.a) * t;
				valueParam->cast<ofFloatColor>().getParameter() = result;
			}
		} catch(const std::exception& e) {
			ofLogError("Snapshot") << "Error interpolating value: " << e.what();
		}
	}
}
