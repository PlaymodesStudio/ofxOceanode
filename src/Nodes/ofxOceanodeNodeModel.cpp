//
//  ofxOceanodeNodeModel.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeTypesRegistry.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNodeHelpCatalog.h"

string ofxOceanodeNodeModel::getDescription(){
    if(!description.empty()) return description;
    return ofxOceanodeNodeHelpDescription(nodeName());
}

ofColor colorFromString(const std::string& input) {
    // Hash the input string
    std::hash<std::string> hasher;
    auto hashedValue = hasher(input);

    // Use the hashed value to seed the random generator
    std::srand(hashedValue);

    // Generate random HSL values
    float hue = static_cast<float>(std::rand()) / RAND_MAX * 255;
    float saturation = 255;//static_cast<float>(std::rand()) / RAND_MAX * 255;
    float lightness = 255;//static_cast<float>(std::rand()) / RAND_MAX * 255;

    // Return the color
    return ofColor::fromHsb(hue, saturation, lightness);
}

ofxOceanodeNodeModel::ofxOceanodeNodeModel(string _name) : nameIdentifier(_name){
    profilingRecord->identity = [this](){
        auto name = parameters.getName();
        const auto suffix = getNodeTitleSuffix();
        if(!suffix.empty()) name += " (" + suffix + ")";
        return oceanodeProfiling::Record::Identity{name,
            hostContainer ? hostContainer->getCanvasDisplayPath() : std::string()};
    };
    for(auto* group : {&parameters, &inspectorParameters}){
        auto hook = oceanodeProfiling::ParameterHook::bindGroup(*group);
        hook->setRecord(profilingRecord);
        inspectorProfilingHooks.push_back(std::move(hook));
    }
    parameters.setName(_name);
    color = colorFromString(_name);
    numIdentifier = -1;
    flags = 0;
    active = true;
}

void ofxOceanodeNodeModel::setNumIdentifier(unsigned int num){
    numIdentifier = num;
    parameters.setName(nameIdentifier + " " + ofToString(num));
}

shared_ptr<ofxOceanodeAbstractParameter> ofxOceanodeNodeModel::addParameter(ofAbstractParameter& p, ofxOceanodeParameterFlags flags){
    shared_ptr<ofxOceanodeAbstractParameter> oceanodeParam = ofxOceanodeTypesRegistry::getInstance()->createOceanodeAbstractFromAbstract(p);
    oceanodeParam->setFlags(flags);
    oceanodeParam->setNodeModel(this);
    parameters.add(*oceanodeParam);
    return dynamic_pointer_cast<ofxOceanodeAbstractParameter>(*(parameters.end()-1));
}

// Tolerant JSON -> vector conversion for preset loading. Presets can contain
// null entries (NaN/inf are written as null by nlohmann::json) or numbers stored
// as strings; nlohmann's get<vector<T>>() throws on those. Unreadable entries
// become 0. Returns false if the value can't be read at all (keep current value).
template<typename T>
static bool oceanodeJsonToVector(const ofJson &value, vector<T> &out){
	auto toScalar = [](const ofJson &v) -> T {
		if(v.is_number() || v.is_boolean()) return v.get<T>();
		if(v.is_string()) return (T)ofToFloat(v.get<string>());
		return T(0);
	};
	if(value.is_array()){
		out.clear();
		out.reserve(value.size());
		for(const auto &v : value) out.push_back(toScalar(v));
		return true;
	}
	if(value.is_number() || value.is_boolean() || value.is_string()){
		out = vector<T>(1, toScalar(value));
		return true;
	}
	return false;
}

void ofxOceanodeNodeModel::deserializeParameter(ofJson &json, ofAbstractParameter &p){
	const string name = p.getEscapedName();
	// Missing key: keep the current value (json[name] on a missing key yields null and throws on conversion)
	if(!json.contains(name) || json[name].is_null()) return;
	const ofJson &value = json[name];
	if(p.valueType() == typeid(vector<float>).name()){
		if(value.is_array()){
			p.cast<vector<float>>() = value.get<vector<float>>();
		}else if(value.is_string()){
			p.cast<vector<float>>() = vector<float>(1, ofToFloat(value.get<string>()));
		}else if(value.is_number() || value.is_boolean()){
			p.cast<vector<float>>() = vector<float>(1, value.get<float>());
		}
	}
	else if(p.valueType() == typeid(vector<int>).name()){
		if(value.is_array()){
			p.cast<vector<int>>() = value.get<vector<int>>();
		}else if(value.is_string()){
			p.cast<vector<int>>() = vector<int>(1, ofToInt(value.get<string>()));
		}else if(value.is_number() || value.is_boolean()){
			p.cast<vector<int>>() = vector<int>(1, value.get<int>());
		}
	}else{
		ofDeserialize(json, p);
	}
}

void ofxOceanodeNodeModel::setContainer(ofxOceanodeContainer* container){
    hostContainer = container;
    container->getProfilingSession()->attach(profilingRecord);
	canvasID = container->getCanvasID();
};

std::shared_ptr<ofxOceanodeTransport> ofxOceanodeNodeModel::getTransport() const{
    if(hostContainer == nullptr){
        return nullptr;
    }
    auto canvasTransport = hostContainer->getTransport();
    if(!timeSourceEnabled || canvasTransport == nullptr) return canvasTransport;
    const std::string& id = timeSourceIdParameter.get();
    if(id.empty() || id == TimeSourceAuto || id == TimeSourceFree) return canvasTransport;
    if(id == TimeSourceClock) return canvasTransport->getClockTransport();
    return canvasTransport->findTimeSource(id);
}

// What a node following a missing time source sees: stopped at the start,
// on the root's clock.
ofxOceanodeTransportState ofxOceanodeNodeModel::missingTimeSourceState(const ofxOceanodeContainer* container){
    ofxOceanodeTransportState state;
    if(container != nullptr && container->getRootTransport() != nullptr){
        const auto rootState = container->getRootTransport()->getState();
        state.steadyTimeUs = rootState.steadyTimeUs;
        state.clockSeconds = rootState.clockSeconds;
        state.driverMode = rootState.driverMode;
        state.bpm = rootState.bpm;
    }
    state.isPlaying = false;
    return state;
}

ofxOceanodeTransportState ofxOceanodeNodeModel::getTransportState() const{
    if(hostContainer == nullptr){
        return {};
    }
    if(auto transport = getTransport()) return transport->getState();
    return missingTimeSourceState(hostContainer);
}

ofxOceanodeFrameTransportState ofxOceanodeNodeModel::getFrameTransportState() const{
    if(hostContainer == nullptr){
        return {};
    }
    if(auto transport = getTransport()) return transport->getFrameState();
    const auto state = missingTimeSourceState(hostContainer);
    return {state, state};
}

void ofxOceanodeNodeModel::addTimeSourceParameter(ofParameter<bool>& syncToggle){
    timeSourceSync = &syncToggle;
    addTimeSourceParameter();
}

void ofxOceanodeNodeModel::addTimeSourceParameter(){
    if(timeSourceEnabled) return;
    timeSourceEnabled = true;
    addInspectorParameter(timeSourceIdParameter.set("Time Source", TimeSourceAuto));
}

bool ofxOceanodeNodeModel::isTimeSourceParameter(const ofAbstractParameter& p) const{
    return timeSourceEnabled && p.getName() == timeSourceIdParameter.getName() &&
        p.valueType() == timeSourceIdParameter.valueType();
}

bool ofxOceanodeNodeModel::isTimeSourceSyncToggle(const ofAbstractParameter& p) const{
    return timeSourceSync != nullptr && p.getName() == timeSourceSync->getName() && p.valueType() == timeSourceSync->valueType();
}

std::string ofxOceanodeNodeModel::getTimeSourceId() const{
    if(timeSourceSync != nullptr && !timeSourceSync->get()) return TimeSourceFree;
    const std::string& id = timeSourceIdParameter.get();
    return id.empty() || id == TimeSourceFree ? std::string(TimeSourceAuto) : id;
}

void ofxOceanodeNodeModel::setTimeSourceId(const std::string& id){
    if(id == TimeSourceFree){
        if(timeSourceSync != nullptr && timeSourceSync->get()) *timeSourceSync = false;
        return;
    }
    if(timeSourceIdParameter.get() != id) timeSourceIdParameter = id;
    if(timeSourceSync != nullptr && !timeSourceSync->get()) *timeSourceSync = true;
}

bool ofxOceanodeNodeModel::isTimeSourceMissing() const{
    if(!timeSourceEnabled || hostContainer == nullptr) return false;
    const std::string id = getTimeSourceId();
    if(id == TimeSourceFree || id == TimeSourceAuto || id == TimeSourceClock) return false;
    return getTransport() == nullptr;
}

ofxOceanodeTimeState ofxOceanodeNodeModel::getGlobalTimeState() const{
    return ofxOceanodeTime::getInstance()->getGlobalTimeState();
}

ofxOceanodeFrameTimeState ofxOceanodeNodeModel::getFrameGlobalTimeState() const{
    return ofxOceanodeTime::getInstance()->getFrameGlobalTimeState();
}

//parameterInfo& ofxOceanodeNodeModel::addParameterToGroupAndInfo(ofAbstractParameter& p){
//    addParameter(p);
//    if(parametersInfo.count(p.getName()) == 0) parametersInfo[p.getName()] = parameterInfo();
//    return parametersInfo[p.getName()];
//}
//
//parameterInfo& ofxOceanodeNodeModel::addOutputParameterToGroupAndInfo(ofAbstractParameter& p){
//    parameters->add(p);
//
//    if(parametersInfo.count(p.getName()) == 0) parametersInfo[p.getName()] = parameterInfo(false, true, false, true);
//    else{
//        parametersInfo[p.getName()].acceptInConnection = false;
//        parametersInfo[p.getName()].isSavePreset = false;
//    }
//
//    return parametersInfo[p.getName()];
//}

string ofxOceanodeNodeModel::getParents(){
    return canvasID;
}

void ofxOceanodeNodeModel::addSeparator(string name, ofColor color){
    ofParameter<std::function<void()>> separator;
    
    // Store the label and color as a string that will be parsed in the GUI
    // Format: "SEPARATOR:|label|r,g,b,a"
    string separatorData = "SEPARATOR:|" + name + "|" +
                          ofToString((int)color.r) + "," +
                          ofToString((int)color.g) + "," +
                          ofToString((int)color.b) + "," +
                          ofToString((int)color.a);
    separator.setName(separatorData);
    addCustomRegion(separator, [](){});  // Empty function, rendering will be handled by GUI
}

void ofxOceanodeNodeModel::addInspectorSeparator(string name, ofColor color){
    ofParameter<std::function<void()>> separator;
    
    // Store the label and color as a string that will be parsed in the GUI
    // Format: "SEPARATOR:|label|r,g,b,a"
    string separatorData = "SEPARATOR:|" + name + "|" +
                          ofToString((int)color.r) + "," +
                          ofToString((int)color.g) + "," +
                          ofToString((int)color.b) + "," +
                          ofToString((int)color.a);
    separator.setName(separatorData);
    separator.set([](){}); // Empty function, rendering will be handled by GUI
    inspectorParameters.add(separator);
}

void ofxOceanodeNodeModel::addInspectorSeparator(string name, ofColor labelColor, ofColor backgroundColor){
    ofParameter<std::function<void()>> separator;
    // Format: "SEPARATOR:|label|r,g,b,a|r,g,b,a", the last being the background.
    const auto rgba = [](const ofColor& c){
        return ofToString((int)c.r) + "," + ofToString((int)c.g) + "," + ofToString((int)c.b) + "," + ofToString((int)c.a);
    };
    separator.setName("SEPARATOR:|" + name + "|" + rgba(labelColor) + "|" + rgba(backgroundColor));
    separator.set([](){});
    inspectorParameters.add(separator);
}

void ofxOceanodeNodeModel::removeSeparator(string _name)
{
	// Build the separator pattern to search for
	string separatorPattern = "SEPARATOR:|" + _name + "|";
	
	// Collect parameters to remove (avoid iterator invalidation)
	vector<string> paramsToRemove;
	
	for (auto & p : parameters) {
		string paramName = p.get()->getName();
		// Check if this parameter is the separator we're looking for
		if (paramName.find(separatorPattern) == 0) {
			paramsToRemove.push_back(p.get()->getEscapedName());
		}
	}
	
	// Now safely remove the collected parameters
	for (const auto& paramEscName : paramsToRemove) {
		removeParameter(paramEscName);
	}
}
