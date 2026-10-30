//
//  ofxOceanodeNode.cpp
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#include "ofxOceanodeNode.h"
#include <atomic>
#include <cstdint>
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeConnection.h"
#include "ofxOceanodeNodeGui.h"
#include "PresetSaveCleanup.h"
#include "ofxOceanodeGlDiagnostics.h"
#include "ofxOceanodeScope.h"
#include "MacroMorphEngine.h"

namespace {
std::atomic<std::uint64_t> graphGenerationCounter{0};
}

std::uint64_t ofxOceanodeNode::graphGeneration(){
    return graphGenerationCounter.load(std::memory_order_relaxed);
}

ofxOceanodeNode::ofxOceanodeNode(unique_ptr<ofxOceanodeNodeModel> && _nodeModel) : nodeModel(move(_nodeModel)), snapshots(std::make_unique<ofxOceanodeNodeSnapshots>()){
    graphGenerationCounter.fetch_add(1, std::memory_order_relaxed);
    nodeModelListeners.push(nodeModel->deleteModule.newListener([this](){
        deleteSelf();
    }));
}

ofxOceanodeNode::~ofxOceanodeNode(){
    graphGenerationCounter.fetch_add(1, std::memory_order_relaxed);
    for(auto &p : getParameters()) dynamic_pointer_cast<ofxOceanodeAbstractParameter>(p)->removeAllConnections();
}

void ofxOceanodeNode::setup(string additionalInfo){
	if(additionalInfo == ""){
		nodeModel->setup();
	}else{
		nodeModel->setup(additionalInfo);
	}
    active = true;
}

//TODO: remove event args
void ofxOceanodeNode::update(ofEventArgs &e){
    auto identity = [this](){
        const auto& record = nodeModel->getProfilingRecord();
        const auto path = record->identity ? record->identity().path : nodeModel->getParents();
        return path + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier());
    };
    updateSnapshotGlide();
    {
        oceanodeGlDiagnostics::Scope glErrors(identity, "update");
        oceanodeProfiling::Scope timing(nodeModel->getProfilingRecord(), oceanodeProfiling::Phase::Update);
        nodeModel->update(e);
    }
    oceanodeGlDiagnostics::Scope glErrors(identity, "GUI update");
    nodeGui->update(e);
}

void ofxOceanodeNode::draw(ofEventArgs &e){
    auto identity = [this](){
        const auto& record = nodeModel->getProfilingRecord();
        const auto path = record->identity ? record->identity().path : nodeModel->getParents();
        return path + "/" + nodeModel->nodeName() + "_" + ofToString(nodeModel->getNumIdentifier());
    };
    {
        oceanodeGlDiagnostics::Scope glErrors(identity, "draw");
        oceanodeProfiling::Scope timing(nodeModel->getProfilingRecord(), oceanodeProfiling::Phase::Draw);
        nodeModel->draw(e);
    }
    oceanodeGlDiagnostics::Scope glErrors(identity, "GUI draw");
    nodeGui->draw(e);
}

void ofxOceanodeNode::setGui(std::unique_ptr<ofxOceanodeNodeGui>&& gui){
    nodeGui = std::move(gui);
}

ofxOceanodeNodeGui& ofxOceanodeNode::getNodeGui(){
    return *nodeGui.get();
}

ofxOceanodeNodeModel& ofxOceanodeNode::getNodeModel(){
    return *nodeModel.get();
}


ofColor ofxOceanodeNode::getColor(){
    return nodeModel->getColor();
}

void ofxOceanodeNode::deleteSelf(){
    ofNotifyEvent(deleteModule);
}

bool ofxOceanodeNode::loadPreset(string presetFolderPath){
    string scapedNodeName = nodeModel->nodeName();
    ofStringReplace(scapedNodeName, " ", "_");
	string filename = presetFolderPath + "/" + scapedNodeName + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
	ofJson json = ofLoadJson(filename);
	if(json.empty()) return false;
	
	if(false)
		nodeModel->loadCustomPersistent(json);
	
	nodeModel->presetRecallBeforeSettingParameters(json);
	loadParametersFromJson(json, false);
	loadInspectorParametersFromJson(json);
	nodeModel->presetRecallAfterSettingParameters(json);
	loadNodeSettings(json);
    if(json.count("expanded") == 1){
        nodeGui->setExpanded(json["expanded"]);
    }
	return true;
}

void ofxOceanodeNode::savePreset(string presetFolderPath, bool saveBeforeConnectionValues){
    oceanodePresetSave::Scope save;
    string scapedNodeName = nodeModel->nodeName();
    ofStringReplace(scapedNodeName, " ", "_");
    string filename = presetFolderPath + "/" + scapedNodeName + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
    ofJson json = saveParametersToJson(false, saveBeforeConnectionValues);
    saveInspectorParametersToJson(json);
    nodeModel->presetSave(json);
	nodeModel->macroSave(json, presetFolderPath);
    saveNodeSettings(json);
    json["expanded"] = nodeGui->getExpanded();
    oceanodePresetSave::saveJson(filename, json);
    save.finish();
}

bool ofxOceanodeNode::loadPersistentPreset(string presetFolderPath){
    string scapedNodeName = nodeModel->nodeName();
    ofStringReplace(scapedNodeName, " ", "_");
    string filename = presetFolderPath + "/" + scapedNodeName + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
	ofJson json = ofLoadJson(filename);
	if(json.empty()) return false;
	
	nodeModel->loadCustomPersistent(json);
	
	nodeModel->presetRecallBeforeSettingParameters(json);
	loadParametersFromJson(json, true);
	loadInspectorParametersFromJson(json);
	nodeModel->presetRecallAfterSettingParameters(json);
	loadNodeSettings(json);
	return true;
}

void ofxOceanodeNode::savePersistentPreset(string presetFolderPath){
    oceanodePresetSave::Scope save;
    string scapedNodeName = nodeModel->nodeName();
    ofStringReplace(scapedNodeName, " ", "_");
    string filename = presetFolderPath + "/" + scapedNodeName + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
    ofJson json = saveParametersToJson(true);
    saveInspectorParametersToJson(json);
    nodeModel->presetSave(json);
	nodeModel->macroSave(json, presetFolderPath);
    saveNodeSettings(json);
    oceanodePresetSave::saveJson(filename, json);
    save.finish();
}

void ofxOceanodeNode::presetWillBeLoaded(){
    nodeModel->presetWillBeLoaded();
}

void ofxOceanodeNode::presetHasLoaded(){
    nodeModel->presetHasLoaded();
}

void ofxOceanodeNode::activateConnections(){
    nodeModel->activateConnections();
}

void ofxOceanodeNode::deactivateConnections(){
    nodeModel->deactivateConnections();
}

void ofxOceanodeNode::loadPresetBeforeConnections(string presetFolderPath){
    string scapedNodeName = nodeModel->nodeName();
    ofStringReplace(scapedNodeName, " ", "_");
    string filename = presetFolderPath + "/" + scapedNodeName + "_" + ofToString(nodeModel->getNumIdentifier()) + ".json";
    ofJson json = ofLoadJson(filename);
    if(json.empty()) json = ofLoadJson(filename);
    
    if(json.empty()) return;
    
    nodeModel->loadBeforeConnections(json);
	nodeModel->macroLoad(json, presetFolderPath);
}

bool ofxOceanodeNode::loadConfig(string filename, bool persistentPreset){
    ofJson json = ofLoadJson(filename);
    if(json.empty()) json = ofLoadJson(filename);
    
    if(json.empty()) return false;
    
    if(persistentPreset)
        nodeModel->loadCustomPersistent(json);
    
    //Hack Put all faders to 0;
	//TODO: Review hack
//    if(getParameters().contains("Fader")){
//        if(getParameters().get("Fader").type() == typeid(ofParameter<float>()).name()){
//            getParameters().getFloat("Fader") = 0;
//        }else{
//            getParameters().get<vector<float>>("Fader") = {0};
//        }
//    }
    
    nodeModel->presetRecallBeforeSettingParameters(json);
    loadParametersFromJson(json, persistentPreset);
    loadInspectorParametersFromJson(json);
    nodeModel->presetRecallAfterSettingParameters(json);
    return true;
}

void ofxOceanodeNode::saveConfig(string filename, bool persistentPreset){
    
}

ofJson ofxOceanodeNode::saveParametersToJson(bool persistentPreset, bool saveBeforeConnectionValues){
    ofJson json;
    for(int i = 0; i < getParameters().size(); i++){
        ofxOceanodeAbstractParameter& p = static_cast<ofxOceanodeAbstractParameter&>(getParameters().get(i));
        saveParameterToJson(json, p, persistentPreset, saveBeforeConnectionValues);
    }
    return json;
}

void ofxOceanodeNode::saveParameterToJson(ofJson &json, ofxOceanodeAbstractParameter &p,
                                           bool persistentPreset, bool saveBeforeConnectionValues){
    if((!persistentPreset && (p.getFlags() & ofxOceanodeParameterFlags_DisableSavePreset)) ||
       (persistentPreset && (p.getFlags() & ofxOceanodeParameterFlags_DisableSaveProject))) return;

    if(saveBeforeConnectionValues && p.hasInConnection() &&
       p.getInConnection()->serializeBeforeConnectionValue(json)) return;

    if(p.valueType() == typeid(vector<float>).name()){
        const auto vecF = p.cast<vector<float>>().getParameter().get();
        if(vecF.size() == 1) json[p.getEscapedName()] = vecF[0];
        else json[p.getEscapedName()] = vecF;
    }else if(p.valueType() == typeid(vector<int>).name()){
        const auto vecI = p.cast<vector<int>>().getParameter().get();
        if(vecI.size() == 1) json[p.getEscapedName()] = vecI[0];
        else json[p.getEscapedName()] = vecI;
    }else{
        ofSerialize(json, p);
    }
}
bool ofxOceanodeNode::loadParametersFromJson(ofJson json, bool persistentPreset){
    for (ofJson::iterator it = json.begin(); it != json.end(); ++it) {
        if(getParameters().contains(it.key())){
            ofxOceanodeAbstractParameter& p = static_cast<ofxOceanodeAbstractParameter&>(getParameters().get(it.key()));
            deserializeParameter(json, p, persistentPreset);
        }
    }
    return true;
}

void ofxOceanodeNode::saveInspectorParametersToJson(ofJson &json){
    for(int i = 0; i < getInspectorParameters().size(); i++){
        ofSerialize(json, getInspectorParameters().get(i));
    }
}

void ofxOceanodeNode::loadInspectorParametersFromJson(ofJson json){
    for (ofJson::iterator it = json.begin(); it != json.end(); ++it) {
        if(getInspectorParameters().contains(it.key())){
            ofDeserialize(json, getInspectorParameters().get(it.key()));
        }
    }
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

void ofxOceanodeNode::deserializeParameter(ofJson &json, ofxOceanodeAbstractParameter &p, bool persistentPreset){
	if((((!persistentPreset && !(p.getFlags() & ofxOceanodeParameterFlags_DisableSavePreset)) || (persistentPreset && !(p.getFlags() & ofxOceanodeParameterFlags_DisableSaveProject)))) && json.count(p.getEscapedName()) && !p.hasInConnection()){
		if(p.valueType() == typeid(vector<float>).name()){
			auto& param = p.cast<vector<float>>().getParameter();
			vector<float> values;
			if(!oceanodeJsonToVector(json[p.getEscapedName()], values)) return;
			if(!values.empty()){
				auto mins = param.getMin();
				auto maxs = param.getMax();
				float fillMin = mins.empty() ? 0.0f : mins.back();
				float fillMax = maxs.empty() ? 1.0f : maxs.back();
				mins.resize(values.size(), fillMin);
				maxs.resize(values.size(), fillMax);
				param.setMin(mins);
				param.setMax(maxs);
			}
			param = values;
		}
		else if(p.valueType() == typeid(vector<int>).name()){
			auto& param = p.cast<vector<int>>().getParameter();
			vector<int> values;
			if(!oceanodeJsonToVector(json[p.getEscapedName()], values)) return;
			if(!values.empty()){
				auto mins = param.getMin();
				auto maxs = param.getMax();
				int fillMin = mins.empty() ? 0 : mins.back();
				int fillMax = maxs.empty() ? 1 : maxs.back();
				mins.resize(values.size(), fillMin);
				maxs.resize(values.size(), fillMax);
				param.setMin(mins);
				param.setMax(maxs);
			}
			param = values;
		}else if(p.valueType() == typeid(std::string).name()){
			if(json[p.getEscapedName()].is_string()){
				p.cast<std::string>().getParameter() = json[p.getEscapedName()].get<std::string>();
			}else if(json[p.getEscapedName()].is_number_integer() || json[p.getEscapedName()].is_number_unsigned() || json[p.getEscapedName()].is_number_float() || json[p.getEscapedName()].is_boolean()){
				p.cast<std::string>().getParameter() = json[p.getEscapedName()].dump();
			}else{
				ofLogWarning("ofxOceanodeNode") << "Failed to deserialize preset value for " << p.getName() << ": invalid json type for string parameter";
			}
		}else{
			// p wraps an ofParameter but is not itself one. ofDeserialize
			// checks p.type() and then casts the ofAbstractParameter reference
			// directly to ofParameter<T>, which is undefined behavior for this
			// wrapper (and can crash on numeric preset values).
			const auto &value = json[p.getEscapedName()];
			if(p.valueType() == typeid(int).name() && value.is_number_integer()){
				p.cast<int>().getParameter() = value.get<int>();
			}else if(p.valueType() == typeid(float).name() && value.is_number_float()){
				p.cast<float>().getParameter() = value.get<float>();
			}else if(p.valueType() == typeid(bool).name() && value.is_boolean()){
				p.cast<bool>().getParameter() = value.get<bool>();
			}else if(p.valueType() == typeid(int64_t).name() && value.is_number_integer()){
				p.cast<int64_t>().getParameter() = value.get<int64_t>();
			}else if(!value.is_null()){
				p.fromString(value.is_string() ? value.get<std::string>() : value.dump());
			}
		}
    }
}

void ofxOceanodeNode::setBpm(float bpm){
    nodeModel->setBpm(bpm);
}

void ofxOceanodeNode::resetPhase(){
    nodeModel->resetPhase();
}

ofParameterGroup& ofxOceanodeNode::getParameters(){
    return nodeModel->getParameterGroup();
}

ofParameterGroup& ofxOceanodeNode::getInspectorParameters(){
    return nodeModel->getInspectorParameterGroup();
}

void ofxOceanodeNode::setActive(bool act){
    if(act == active) return;
    active = act;
    nodeModel->setActive(active);
    if(active) nodeModel->activate();
    else nodeModel->deactivate();
    if(active){
        nodeGui->enable();
    }else{
        nodeGui->disable();
    }
}

// ── Per-parameter settings ───────────────────────────────────────────────────

bool ofxOceanodeNode::isParameterHidden(ofxOceanodeAbstractParameter& p) const{
    return hiddenParameters.count(p.getEscapedName()) != 0;
}

bool ofxOceanodeNode::isParameterShownInGui(ofxOceanodeAbstractParameter& p) const{
    return !isParameterHidden(p) || p.hasInConnection() || p.hasOutConnections();
}

void ofxOceanodeNode::setParameterHidden(ofxOceanodeAbstractParameter& p, bool hidden){
    if(hidden) hiddenParameters.insert(p.getEscapedName());
    else hiddenParameters.erase(p.getEscapedName());
}

bool ofxOceanodeNode::canSnapshotParameter(ofxOceanodeAbstractParameter& p){
    if(p.isNodeOutput()) return false;
    if(p.getFlags() & (ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_ReadOnly)) return false;
    const auto type = p.valueType();
    return type == typeid(float).name() || type == typeid(int).name() || type == typeid(bool).name() ||
           type == typeid(int64_t).name() || type == typeid(std::string).name() ||
           type == typeid(vector<float>).name() || type == typeid(vector<int>).name() ||
           type == typeid(ofColor).name() || type == typeid(ofFloatColor).name();
}

bool ofxOceanodeNode::isParameterInSnapshots(ofxOceanodeAbstractParameter& p) const{
    if(!canSnapshotParameter(p)) return false;
    auto it = snapshotOverrides.find(p.getEscapedName());
    return it == snapshotOverrides.end() ? true : it->second;
}

void ofxOceanodeNode::setParameterInSnapshots(ofxOceanodeAbstractParameter& p, bool included){
    if(included) snapshotOverrides.erase(p.getEscapedName());
    else snapshotOverrides[p.getEscapedName()] = false;
}

bool ofxOceanodeNode::isParameterScopedWhenFolded(ofxOceanodeAbstractParameter& p) const{
    auto it = scopeOverrides.find(p.getEscapedName());
    if(it != scopeOverrides.end()) return it->second;
    return p.getFlags() & ofxOceanodeParameterFlags_DisplayMinimized;
}

void ofxOceanodeNode::setParameterScopedWhenFolded(ofxOceanodeAbstractParameter& p, bool scoped){
    const bool byDefault = p.getFlags() & ofxOceanodeParameterFlags_DisplayMinimized;
    if(scoped == byDefault) scopeOverrides.erase(p.getEscapedName());
    else scopeOverrides[p.getEscapedName()] = scoped;
}

// ── Node Snapshots ───────────────────────────────────────────────────────────

namespace {
enum class GlideKind { None, Float, Int, FloatVector, IntVector, Color, FloatColor };

// Dropdowns, sizes that change and non-numeric values switch at once.
GlideKind glideKind(ofxOceanodeAbstractParameter& p){
    const auto type = p.valueType();
    if(type == typeid(float).name()) return p.cast<float>().getDropdownOptions().empty() ? GlideKind::Float : GlideKind::None;
    if(type == typeid(int).name()) return p.cast<int>().getDropdownOptions().empty() ? GlideKind::Int : GlideKind::None;
    if(type == typeid(vector<float>).name()) return p.cast<vector<float>>().getDropdownOptions().empty() ? GlideKind::FloatVector : GlideKind::None;
    if(type == typeid(vector<int>).name()) return p.cast<vector<int>>().getDropdownOptions().empty() ? GlideKind::IntVector : GlideKind::None;
    if(type == typeid(ofColor).name()) return GlideKind::Color;
    if(type == typeid(ofFloatColor).name()) return GlideKind::FloatColor;
    return GlideKind::None;
}

std::vector<float> currentGlideValue(ofxOceanodeAbstractParameter& p, GlideKind kind){
    switch(kind){
        case GlideKind::Float: return {p.cast<float>().getParameter().get()};
        case GlideKind::Int: return {float(p.cast<int>().getParameter().get())};
        case GlideKind::FloatVector: return p.cast<vector<float>>().getParameter().get();
        case GlideKind::IntVector: {
            const auto& values = p.cast<vector<int>>().getParameter().get();
            return std::vector<float>(values.begin(), values.end());
        }
        case GlideKind::Color: {
            const ofColor c = p.cast<ofColor>().getParameter().get();
            return {float(c.r), float(c.g), float(c.b), float(c.a)};
        }
        case GlideKind::FloatColor: {
            const ofFloatColor c = p.cast<ofFloatColor>().getParameter().get();
            return {c.r, c.g, c.b, c.a};
        }
        default: return {};
    }
}

// Reads a value as saveParameterToJson wrote it; false if it cannot glide.
bool savedGlideValue(const ofJson& value, GlideKind kind, std::vector<float>& out){
    out.clear();
    switch(kind){
        case GlideKind::Float:
        case GlideKind::Int:
            if(!value.is_number()) return false;
            out.push_back(value.get<float>());
            return true;
        case GlideKind::FloatVector:
        case GlideKind::IntVector:
            if(value.is_number()){ out.push_back(value.get<float>()); return true; }
            if(!value.is_array()) return false;
            for(const auto& v : value){
                if(!v.is_number()) return false;
                out.push_back(v.get<float>());
            }
            return true;
        case GlideKind::Color: {
            if(!value.is_string()) return false;
            const ofColor c = ofFromString<ofColor>(value.get<std::string>());
            out = {float(c.r), float(c.g), float(c.b), float(c.a)};
            return true;
        }
        case GlideKind::FloatColor: {
            if(!value.is_string()) return false;
            const ofFloatColor c = ofFromString<ofFloatColor>(value.get<std::string>());
            out = {c.r, c.g, c.b, c.a};
            return true;
        }
        default: return false;
    }
}

void setGlideValue(ofxOceanodeAbstractParameter& p, GlideKind kind, const std::vector<float>& values){
    switch(kind){
        case GlideKind::Float: p.cast<float>().getParameter() = values[0]; break;
        case GlideKind::Int: p.cast<int>().getParameter() = int(std::round(values[0])); break;
        case GlideKind::FloatVector: p.cast<vector<float>>().getParameter() = values; break;
        case GlideKind::IntVector: {
            vector<int> rounded(values.size());
            for(size_t i = 0; i < values.size(); i++) rounded[i] = int(std::round(values[i]));
            p.cast<vector<int>>().getParameter() = rounded;
            break;
        }
        case GlideKind::Color: {
            auto channel = [&](size_t i){ return (unsigned char)ofClamp(std::round(values[i]), 0, 255); };
            p.cast<ofColor>().getParameter() = ofColor(channel(0), channel(1), channel(2), channel(3));
            break;
        }
        case GlideKind::FloatColor:
            p.cast<ofFloatColor>().getParameter() = ofFloatColor(values[0], values[1], values[2], values[3]);
            break;
        default: break;
    }
}
}

bool ofxOceanodeNode::supportsSnapshots(){
    if(!nodeModel->supportsNodeSnapshots()) return false;
    for(auto& abstract : getParameters()){
        auto* p = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get());
        if(p != nullptr && canSnapshotParameter(*p)) return true;
    }
    return false;
}

void ofxOceanodeNode::storeSnapshot(int slot){
    if(slot < 0 || slot >= ofxOceanodeNodeSnapshots::numSlots) return;
    ofJson values = ofJson::object();
    for(int i = 0; i < getParameters().size(); i++){
        auto& p = static_cast<ofxOceanodeAbstractParameter&>(getParameters().get(i));
        if(isParameterInSnapshots(p)) saveParameterToJson(values, p);
    }
    snapshots->slots[slot].values = values;
    snapshots->currentSlot = slot;
}

void ofxOceanodeNode::recallSnapshot(int slot){
    auto found = snapshots->slots.find(slot);
    if(found == snapshots->slots.end()) return;
    ofJson target = found->second.values;
    snapshots->currentSlot = slot;
    snapshots->glideTracks.clear();
    snapshots->gliding = false;
    snapshots->progress = 1.f;
    const bool glide = snapshots->morphMs > 0.f;
    for(int i = 0; i < getParameters().size(); i++){
        auto& p = static_cast<ofxOceanodeAbstractParameter&>(getParameters().get(i));
        const string key = p.getEscapedName();
        if(!isParameterInSnapshots(p) || !target.contains(key) || p.hasInConnection()) continue;
        const GlideKind kind = glide ? glideKind(p) : GlideKind::None;
        ofxOceanodeNodeSnapshots::GlideTrack track;
        track.parameter = key;
        track.from = currentGlideValue(p, kind);
        if(kind != GlideKind::None && savedGlideValue(target[key], kind, track.to) &&
           !track.to.empty() && track.from.size() == track.to.size()){
            snapshots->glideTracks.push_back(std::move(track));
        }else{
            deserializeParameter(target, p);
        }
    }
    if(snapshots->glideTracks.empty()) return;
    snapshots->gliding = true;
    snapshots->glideStartMs = ofGetElapsedTimeMillis();
    snapshots->glideMs = snapshots->morphMs;
    snapshots->glideBiPow = snapshots->morphBiPow;
    snapshots->progress = 0.f;
}

void ofxOceanodeNode::removeSnapshot(int slot){
    snapshots->slots.erase(slot);
    if(snapshots->currentSlot == slot){
        snapshots->currentSlot = -1;
        snapshots->gliding = false;
        snapshots->glideTracks.clear();
    }
}

void ofxOceanodeNode::updateSnapshotGlide(){
    auto& s = *snapshots;
    if(!s.gliding) return;
    const float elapsed = float(ofGetElapsedTimeMillis() - s.glideStartMs);
    const float t = ofClamp(elapsed / std::max(s.glideMs, 1.f), 0.f, 1.f);
    float mapped = t;
    if(s.glideBiPow != 0.f){
        mapped = mapped * 2.f - 1.f;
        MacroMorphEngine::customPow(mapped, s.glideBiPow);
        mapped = (mapped + 1.f) * 0.5f;
    }
    if(t >= 1.f) mapped = 1.f;
    s.progress = mapped;
    std::vector<float> values;
    for(const auto& track : s.glideTracks){
        if(!getParameters().contains(track.parameter)) continue;
        auto& p = static_cast<ofxOceanodeAbstractParameter&>(getParameters().get(track.parameter));
        // A cable connected during the glide takes over.
        if(p.hasInConnection()) continue;
        const GlideKind kind = glideKind(p);
        if(currentGlideValue(p, kind).size() != track.from.size()) continue;
        values.resize(track.from.size());
        for(size_t i = 0; i < values.size(); i++) values[i] = track.from[i] + (track.to[i] - track.from[i]) * mapped;
        setGlideValue(p, kind, values);
    }
    if(t >= 1.f){
        s.gliding = false;
        s.glideTracks.clear();
    }
}

// ── Saved settings ───────────────────────────────────────────────────────────

const std::vector<std::string>& ofxOceanodeNode::nodeSettingsKeys(){
    static const std::vector<std::string> keys = {"__NodeSnapshots", "__ParameterSettings"};
    return keys;
}

void ofxOceanodeNode::saveNodeSettings(ofJson& json){
    if(!snapshots->isDefault()) json["__NodeSnapshots"] = snapshots->toJson();
    if(hiddenParameters.empty() && snapshotOverrides.empty() && scopeOverrides.empty()) return;
    ofJson settings = ofJson::object();
    if(!hiddenParameters.empty()) settings["Hidden"] = std::vector<std::string>(hiddenParameters.begin(), hiddenParameters.end());
    if(!snapshotOverrides.empty()) settings["Snapshot"] = snapshotOverrides;
    if(!scopeOverrides.empty()) settings["Scope"] = scopeOverrides;
    json["__ParameterSettings"] = settings;
}

void ofxOceanodeNode::loadNodeSettings(const ofJson& json){
    snapshots->fromJson(json.contains("__NodeSnapshots") ? json["__NodeSnapshots"] : ofJson());
    hiddenParameters.clear();
    snapshotOverrides.clear();
    scopeOverrides.clear();
    if(!json.contains("__ParameterSettings") || !json["__ParameterSettings"].is_object()) return;
    const auto& settings = json["__ParameterSettings"];
    auto readFlags = [&](const char* key, std::map<std::string, bool>& out){
        if(!settings.contains(key) || !settings[key].is_object()) return;
        for(auto it = settings[key].begin(); it != settings[key].end(); ++it){
            if(it.value().is_boolean()) out[it.key()] = it.value().get<bool>();
        }
    };
    if(settings.contains("Hidden") && settings["Hidden"].is_array()){
        for(const auto& name : settings["Hidden"]) if(name.is_string()) hiddenParameters.insert(name.get<std::string>());
    }
    readFlags("Snapshot", snapshotOverrides);
    readFlags("Scope", scopeOverrides);
}
