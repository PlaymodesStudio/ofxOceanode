//
//  ofxOceanodeContainerMidiOsc.cpp
//  ofxOceanode
//
//  MIDI clock, MIDI bindings and OSC message handling.
//  Split out of ofxOceanodeContainer.cpp; bodies are unchanged.
//


#include "ofxOceanodeContainer.h"
#include "ofxOceanodeAutomationExtension.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeTypesRegistry.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeScope.h"
#include "Nodes/MacroSnapshotSystem.h"
#include "Nodes/MacroRouterValueDispatch.h"
#include "portal.h"
#include "portalSelector.h"
#include "imgui.h"
#include <cmath>
#include <cfloat>
#include <deque>
#include <filesystem>
#include <stdexcept>
#include "PresetSaveCleanup.h"

#ifdef OFXOCEANODE_USE_MIDI
#include "ofxOceanodeMidiClock.h"
#include "ofxOceanodeMidiBinding.h"
#include "ofxOceanodeMidiKeyboardBinding.h"
#include "ofxMidiIn.h"
#include "ofxMidiOut.h"
#endif


#ifdef OFXOCEANODE_USE_MIDI
void ofxOceanodeContainer::setMidiClockSyncEnabled(bool enabled){
    if(!midiClockSync) return;
    midiClockSyncEnabled = enabled;
    if(enabled){
        midiClockSync->open(midiClockSync->getPortName());
        midiClockSync->setDriveTransport(transport, true);
    }else{
        midiClockSync->setDriveTransport(transport, false);
        midiClockSync->close(); // keeps the port name for next time
    }
}

void ofxOceanodeContainer::setMidiClockSyncPort(const std::string& port){
    if(!midiClockSync) return;
    if(midiClockSyncEnabled) midiClockSync->open(port);
    else midiClockSync->rememberPort(port);
}

//TODO: Review for ofxOceanodeParameter
void ofxOceanodeContainer::setIsListeningMidi(bool b){
    isListeningMidi = b;
    for(auto &nodeTypeMap : dynamicNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->getNodeGui().setIsListeningMidi(b);
        }
    }
    for(auto &nodeTypeMap : persistentNodes){
        for(auto &node : nodeTypeMap.second){
            node.second->getNodeGui().setIsListeningMidi(b);
        }
    }
}

shared_ptr<ofxOceanodeAbstractMidiBinding> ofxOceanodeContainer::createMidiBinding(ofxOceanodeAbstractParameter &p, bool isPersistent, int _id, bool keyboard){
    string name = p.getGroupHierarchyNames()[0] + "-|-" + p.getEscapedName();
    
    if(_id == -1){
        _id = midiBindings[name].size();
    }
    
    shared_ptr<ofxOceanodeAbstractMidiBinding> midiBinding = nullptr;
    if(keyboard){
        auto* model = p.getNodeModel();
        ofxOceanodeTimelineInstrument instrument;
        if(!model || !model->getMidiKeyboardInstrument(instrument, true) || !ofxOceanodeMidiKeyboardBinding::supports(instrument)) return nullptr;
        const auto nodeKey = model->getParameterGroup().getEscapedName();
        midiBinding = std::make_shared<ofxOceanodeMidiKeyboardBinding>([this, nodeKey, model](){
            ofxOceanodeTimelineInstrument current;
            auto node = parameterGroupNodesMap.find(nodeKey);
            if(node != parameterGroupNodesMap.end() && &node->second->getNodeModel() == model)
                node->second->getNodeModel().getMidiKeyboardInstrument(current);
            return current;
        }, name);
    }
    else if(p.valueType() == typeid(float).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<float>>(p.cast<float>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(int).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<int>>(p.cast<int>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(bool).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<bool>>(p.cast<bool>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(void).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<void>>(p.cast<void>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(vector<float>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<vector<float>>>(p.cast<vector<float>>().getParameter(), _id);
    }
    else if(p.valueType() == typeid(vector<int>).name()){
        midiBinding = make_unique<ofxOceanodeMidiBinding<vector<int>>>(p.cast<vector<int>>().getParameter(), _id);
    }
    if(midiBinding != nullptr){
        for(auto &midiInPair : midiIns){
            midiInPair.second.addListener(midiBinding.get());
        }
        midiUnregisterlisteners.push(midiBinding->unregisterUnusedMidiIns.newListener(this, &ofxOceanodeContainer::midiBindingBound));
        if(!isPersistent){
            midiBindings[name].push_back(midiBinding);
        }else{
            persistentMidiBindings[name].push_back(midiBinding);
        }
        return midiBinding;
    }
    return nullptr;
}

void ofxOceanodeContainer::drawMidiKeyboardMenu(ofxOceanodeNodeModel& model){
    ofxOceanodeTimelineInstrument instrument;
    const auto nodeKey = model.getParameterGroup().getEscapedName();
    auto isThisKeyboard = [&](const auto& entry){
        return entry.first.substr(0, entry.first.find("-|-")) == nodeKey;
    };
    bool bound = false;
    for(auto* bindings : {&midiBindings, &persistentMidiBindings})
        for(auto& entry : *bindings) if(isThisKeyboard(entry))
            for(auto& binding : entry.second) if(dynamic_cast<ofxOceanodeMidiKeyboardBinding*>(binding.get())) bound = true;
    if(!bound && !model.getMidiKeyboardInstrument(instrument)) return;
    ImGui::Separator();
    if(bound){
        if(ImGui::Selectable("Unbind MIDI Keyboard (Pitch / Gate)")){
            for(auto* bindings : {&midiBindings, &persistentMidiBindings}){
                for(auto entry = bindings->begin(); entry != bindings->end();){
                    if(isThisKeyboard(*entry)){
                        auto& list = entry->second;
                        list.erase(std::remove_if(list.begin(), list.end(), [&](const auto& binding){
                            auto* keyboard = dynamic_cast<ofxOceanodeMidiKeyboardBinding*>(binding.get());
                            if(!keyboard) return false;
                            for(auto& input : midiIns) input.second.removeListener(binding.get());
                            keyboard->release();
                            return true;
                        }), list.end());
                        if(list.empty()) { entry = bindings->erase(entry); continue; }
                    }
                    ++entry;
                }
            }
        }
    }else if(ImGui::Selectable("Bind MIDI Keyboard (Pitch / Gate)")){
        if(model.getMidiKeyboardInstrument(instrument, true) && ofxOceanodeMidiKeyboardBinding::supports(instrument)){
            auto node = parameterGroupNodesMap.find(nodeKey);
            createMidiBinding(*(instrument.pitch ? instrument.pitch : instrument.gate),
                node != parameterGroupNodesMap.end() && node->second->getIsPersistent(), -1, true);
        }
    }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Play a note to learn the MIDI device and channel. Binds the instrument's note inputs together.");
}

bool ofxOceanodeContainer::removeLastMidiBinding(ofxOceanodeAbstractParameter &p){
    string midiBindingName = p.getGroupHierarchyNames()[0] + "-|-" + p.getEscapedName();
    if(midiBindings.count(midiBindingName) != 0){
        for(auto &midiInPair : midiIns){
            midiInPair.second.removeListener(midiBindings[midiBindingName].back().get());
        }
        midiBindings[midiBindingName].pop_back();
        if(midiBindings[midiBindingName].size() == 0){
            midiBindings.erase(midiBindingName);
        }
        return true;
    }
    return false;
}

void ofxOceanodeContainer::midiBindingBound(const void * sender, string &portName){
    ofxOceanodeAbstractMidiBinding * midiBinding = static_cast <ofxOceanodeAbstractMidiBinding *> (const_cast <void *> (sender));
    for(auto &midiInPair : midiIns){
        if(midiInPair.first != portName){
            midiInPair.second.removeListener(midiBinding);
        }
    }
    if(midiOuts.count(portName) != 0){
        midiBinding->bindParameter();
        midiSenderListeners.push(midiBinding->midiMessageSender.newListener([this, portName](ofxMidiMessage& message){
            switch(message.status){
                case MIDI_CONTROL_CHANGE:{
                    midiOuts[portName].sendControlChange(message.channel, message.control, message.value);
                    break;
                }
                case MIDI_NOTE_ON:{
                    midiOuts[portName].sendNoteOn(message.channel, message.pitch, message.velocity);
                }
                default:{
                    
                }
            }
        }));
    }
}

shared_ptr<ofxOceanodeAbstractMidiBinding> ofxOceanodeContainer::createMidiBindingFromInfo(string module, string parameter, bool isPersistent, int _id, bool keyboard){
    auto &collection = !isPersistent ? dynamicNodes : persistentNodes;
    string moduleId = ofSplitString(module, "_").back();
    module.erase(module.rfind(moduleId)-1);
    ofStringReplace(module, "_", " ");
    if(collection.count(module) != 0){
        if(collection[module].count(ofToInt(moduleId))){
            if(collection[module][ofToInt(moduleId)]->getParameters().contains(parameter)){
                return createMidiBinding(static_cast<ofxOceanodeAbstractParameter&>(collection[module][ofToInt(moduleId)]->getParameters().get(parameter)), isPersistent, _id, keyboard);
            }
        }
    }
	return nullptr;
}

void ofxOceanodeContainer::addNewMidiMessageListener(ofxMidiListener* listener){
    for(auto &midiInPair : midiIns){
        midiInPair.second.addListener(listener);
    }
}

#endif


#ifdef OFXOCEANODE_USE_OSC

void ofxOceanodeContainer::receiveOscMessage(ofxOscMessage &m){
	//Todo: Fix for ofxOceanodeParameter
    auto setParameterFromMidiMessage = [this](ofAbstractParameter& _absParam, ofxOscMessage& m){
		ofxOceanodeAbstractParameter& absParam = static_cast<ofxOceanodeAbstractParameter &>(_absParam);
        if(absParam.valueType() == typeid(float).name()){
            ofParameter<float> castedParam = absParam.cast<float>().getParameter();
			castedParam = ofClamp(m.getArgAsFloat(0), castedParam.getMin(), castedParam.getMax());
        }else if(absParam.valueType() == typeid(int).name()){
            ofParameter<int> castedParam = absParam.cast<int>().getParameter();
            if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                castedParam = ofMap(m.getArgAsFloat(0), 0, 1, castedParam.getMin(), castedParam.getMax(), true);
            }
            else if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT32 || m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT64){
                castedParam = ofClamp(m.getArgAsInt(0), castedParam.getMin(), castedParam.getMax());
            }
            
        }else if(absParam.valueType() == typeid(bool).name()){
            absParam.cast<bool>().getParameter() = m.getArgAsBool(0);
        }else if(absParam.valueType() == typeid(void).name()){
            absParam.cast<void>().getParameter().trigger();
        }else if(absParam.valueType() == typeid(string).name()){
            absParam.cast<string>().getParameter() = m.getArgAsString(0);
        }else if(absParam.valueType() == typeid(vector<float>).name()){
            ofParameter<vector<float>> castedParam = absParam.cast<vector<float>>().getParameter();
            if(m.getNumArgs() == 0){
                castedParam = castedParam;;
            }else{
                vector<float> tempVec;
                tempVec.resize(m.getNumArgs(), 0);
                for(int i = 0; i < tempVec.size(); i++){
					tempVec[i] = ofClamp(m.getArgAsFloat(i), castedParam.getMin()[0], castedParam.getMax()[0]);
                }
                castedParam = tempVec;
            }
        }
        else if(absParam.valueType() == typeid(vector<int>).name()){
            ofParameter<vector<int>> castedParam = absParam.cast<vector<int>>().getParameter();
            if(m.getNumArgs() == 0){
                castedParam = castedParam;;
            }else{
                vector<int> tempVec;
                tempVec.resize(m.getNumArgs(), 0);
                if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                    for(int i = 0; i < tempVec.size(); i++){
                        tempVec[i] = ofMap(m.getArgAsFloat(i), 0, 1, castedParam.getMin()[0], castedParam.getMax()[0], true);
                    }
                }
                else if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT32 || m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT64){
                    for(int i = 0; i < tempVec.size(); i++){
                        tempVec[i] = ofClamp(m.getArgAsInt(i), castedParam.getMin()[0], castedParam.getMax()[0]);
                    }
                }
                castedParam = tempVec;
            }
        }
    };
    
    auto modulateParameterFromOscMessage = [this](ofAbstractParameter& _absParam, ofxOscMessage& m){
		ofxOceanodeAbstractParameter& absParam = static_cast<ofxOceanodeAbstractParameter &>(_absParam);
        if(absParam.valueType() == typeid(float).name()){
            ofParameter<float> castedParam = absParam.cast<float>().getParameter();
            castedParam = castedParam + m.getArgAsFloat(0);
        }else if(absParam.valueType() == typeid(int).name()){
            ofParameter<int> castedParam = absParam.cast<int>().getParameter();
            if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                int range = castedParam.getMax() - castedParam.getMin();
                castedParam += ofMap(m.getArgAsFloat(0), -1, 1, -range, range, false);
            }else{
                castedParam += m.getArgAsInt(0);
            }
        }else if(absParam.valueType() == typeid(bool).name()){
            absParam.cast<bool>().getParameter() = !absParam.cast<bool>().getParameter();
        }else if(absParam.valueType() == typeid(vector<float>).name()){
            ofParameter<vector<float>> castedParam = absParam.cast<vector<float>>().getParameter();
            if(castedParam->size() !=1) return;
            vector<float> tempVec;
            if(m.getNumArgs() == 1 && castedParam->size() != 1){
                tempVec = castedParam;
                for(int i = 0; i < tempVec.size(); i++){
                    tempVec[i] = tempVec[i] + m.getArgAsFloat(0);
                }
            }else{
                if(m.getNumArgs() == castedParam->size()){
                    tempVec = castedParam;
                }else if(m.getNumArgs() > castedParam->size()){
                    tempVec = vector<float>(m.getNumArgs(), castedParam.get()[0]);
                }
                for(int i = 0; i < tempVec.size(); i++){
                    tempVec[i] = tempVec[i] + m.getArgAsFloat(i);
                }
            }
            castedParam = tempVec;
        }
        else if(absParam.valueType() == typeid(vector<int>).name()){
            ofParameter<vector<int>> castedParam = absParam.cast<vector<int>>().getParameter();
            if(castedParam->size() !=1) return;
            vector<int> tempVec;
            tempVec.resize(m.getNumArgs(), 0);
            if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_FLOAT){
                int range = castedParam.getMax()[0] - castedParam.getMin()[0];
                for(int i = 0; i < tempVec.size(); i++){
                    tempVec[i] += ofMap(m.getArgAsFloat(i), -1, 1, -range, range, true);
                }
            }
            else if(m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT32 || m.getArgType(0) == ofxOscArgType::OFXOSC_TYPE_INT64){
                if(m.getNumArgs() == 1 && castedParam->size() != 1){
                    tempVec = castedParam;
                    for(int i = 0; i < tempVec.size(); i++){
                        tempVec[i] = tempVec[i] + m.getArgAsInt(0);
                    }
                }else{
                    if(m.getNumArgs() == castedParam->size()){
                        tempVec = castedParam;
                    }else if(m.getNumArgs() > castedParam->size()){
                        tempVec = vector<int>(m.getNumArgs(), castedParam.get()[0]);
                    }
                    for(int i = 0; i < tempVec.size(); i++){
                        tempVec[i] = tempVec[i] + m.getArgAsInt(i);
                    }
                }
            }
            castedParam = tempVec;
        }
    };
    
    
    vector<string> splitAddress = ofSplitString(m.getAddress(), "/");
    if(splitAddress[0].size() == 0) splitAddress.erase(splitAddress.begin());
    if(splitAddress.size() == 1){
        if(splitAddress[0] == "phaseReset"){
            resetPhase();
        }else if(splitAddress[0] == "bpm"){
            float newBpm = m.getArgAsFloat(0);
            ofNotifyEvent(changedBpmEvent, newBpm);
        }
    }else if(splitAddress.size() == 2){ //Load preset by name
        if(splitAddress[0] == "presetLoad"){
            auto toSendPair = make_pair(splitAddress[1], m.getArgAsString(0));
            loadPresetEvent.notify(toSendPair);
        }else if(splitAddress[0] == "presetLoadi"){ //Load preset by number
			if(m.getArgAsInt(0) != 0){
				auto toSendPair = make_pair(splitAddress[1], m.getArgAsInt(0));
				loadPresetNumEvent.notify(toSendPair);
			}
//        }else if(splitAddress[0] == "presetSave"){
//            savePreset("Presets/" + splitAddress[1] + "/" + m.getArgAsString(0));
        }else if(splitAddress[0] == "Global"){
            for(auto &nodeType  : dynamicNodes){
                for(auto &node : nodeType.second){
                    node.second->getNodeModel().receiveOscMessage(m);
                    ofParameterGroup& groupParam = node.second->getParameters();
                    if(groupParam.contains(splitAddress[1])){
                        ofAbstractParameter &absParam = groupParam.get(splitAddress[1]);
                        setParameterFromMidiMessage(absParam, m);
                    }
                }
            }
            for(auto &nodeType  : persistentNodes){
                for(auto &node : nodeType.second){
                    node.second->getNodeModel().receiveOscMessage(m);
                    ofParameterGroup& groupParam = node.second->getParameters();
                    if(groupParam.contains(splitAddress[1])){
                        ofAbstractParameter &absParam = groupParam.get(splitAddress[1]);
                        setParameterFromMidiMessage(absParam, m);
                    }
                }
            }
        }else{
            //TODO: check if is in the form of NAME_ID
            string moduleName = splitAddress[0];
            string moduleId = ofSplitString(moduleName, "_").back();
            if(moduleName.find("_") != std::string::npos){
                moduleName.erase(moduleName.rfind(moduleId)-1);
                ofStringReplace(moduleName, "_", " ");
                bool validOSC = false;
                if(dynamicNodes.count(moduleName) == 1){
                    if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup& groupParam = dynamicNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam.contains(splitAddress[1])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[1]);
                            setParameterFromMidiMessage(absParam, m);
                            validOSC = true;
                        }
                    }
                }
                if(persistentNodes.count(moduleName) == 1){
                    if(persistentNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup& groupParam = persistentNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam.contains(splitAddress[1])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[1]);
                            setParameterFromMidiMessage(absParam, m);
                            validOSC = true;
                        }
                    }
                }
                if(!validOSC){
                    string moduleName = splitAddress[0];
                    string moduleId = ofSplitString(moduleName, "_").back();
                    moduleName.erase(moduleName.rfind(moduleId)-1);
                    ofStringReplace(moduleName, "_", " ");
                    if(dynamicNodes.count(moduleName) == 1){
                        if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                            string newAddress;
                            for(int i = 1; i < splitAddress.size(); i++) newAddress += "/" + splitAddress[i];
                            m.setAddress(newAddress);
                            dynamicNodes[moduleName][ofToInt(moduleId)]->getNodeModel().receiveOscMessage(m);
                        }
                    }
                }
            }
        }
    }
    else if(splitAddress.size() == 3){
		if(splitAddress[0] == "relative"){
            if(splitAddress[1] == "Global"){
                for(auto &nodeType  : dynamicNodes){
                    for(auto &node : nodeType.second){
                        node.second->getNodeModel().receiveOscMessage(m);
                        ofParameterGroup& groupParam = node.second->getParameters();
                        if(groupParam.contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
                for(auto &nodeType  : persistentNodes){
                    for(auto &node : nodeType.second){
                        node.second->getNodeModel().receiveOscMessage(m);
                        ofParameterGroup& groupParam = node.second->getParameters();
                        if(groupParam.contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
            }else{
                string moduleName = splitAddress[1];
                string moduleId = ofSplitString(moduleName, "_").back();
                moduleName.erase(moduleName.rfind(moduleId)-1);
                ofStringReplace(moduleName, "_", " ");
                if(dynamicNodes.count(moduleName) == 1){
                    if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup& groupParam = dynamicNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam.contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
                if(persistentNodes.count(moduleName) == 1){
                    if(persistentNodes[moduleName].count(ofToInt(moduleId))){
                        ofParameterGroup& groupParam = persistentNodes[moduleName][ofToInt(moduleId)]->getParameters();
                        if(groupParam.contains(splitAddress[2])){
                            ofAbstractParameter &absParam = groupParam.get(splitAddress[2]);
                            modulateParameterFromOscMessage(absParam, m);
                        }
                    }
                }
            }
        }else{
            string moduleName = splitAddress[0];
            string moduleId = ofSplitString(moduleName, "_").back();
            moduleName.erase(moduleName.rfind(moduleId)-1);
            ofStringReplace(moduleName, "_", " ");
            if(dynamicNodes.count(moduleName) == 1){
                if(dynamicNodes[moduleName].count(ofToInt(moduleId))){
                    string newAddress;
                    for(int i = 1; i < splitAddress.size(); i++) newAddress += "/" + splitAddress[i];
                    m.setAddress(newAddress);
                    dynamicNodes[moduleName][ofToInt(moduleId)]->getNodeModel().receiveOscMessage(m);
                }
            }
        }
    }
}

#endif
