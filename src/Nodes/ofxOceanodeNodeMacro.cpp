//
//  ofxOceanodeNodeMacro.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 20/06/2019.
//

#include "ofxOceanodeNodeMacro.h"

ofxOceanodeNodeMacro::ofxOceanodeNodeMacro() : ofxOceanodeNodeModelExternalWindow("Macro"){
    canvas = nullptr;
    presetPath = "";
    currentPreset = -1;
}

void ofxOceanodeNodeMacro::setContainer(ofxOceanodeContainer* container){
    registry = container->getRegistry();
}

void ofxOceanodeNodeMacro::setup(){
    auto treg = make_shared<ofxOceanodeTypesRegistry>();
    registry->registerModel<inlet<vector<float>>>("I/O");
    registry->registerModel<outlet<vector<float>>>("I/O");
    container = make_shared<ofxOceanodeContainer>(registry, treg);
    container->setWindow(nullptr);
    newNodeListener = container->newNodeCreated.newListener(this, &ofxOceanodeNodeMacro::newNodeCreated);
    ofDirectory dir;
    if(!dir.doesDirectoryExist("MacroPresets")){
        dir.createDirectory("MacroPresets");
    }
    dir.open("MacroPresets");
    dir.sort();
    if(dir.listDir() == 0) dir.createDirectory("Other");
    if(dir.listDir() == 1 && dir.getName(0) == "Project") dir.createDirectory("Other");
    for(int i = 0; i < dir.listDir(); i++){
        if(dir.getName(i) != "Project"){
            bankNames.push_back(dir.getName(i));
        }
    }
    addParameterToGroupAndInfo(createDropdownAbstractParameter("Bank", bankNames, bank));
    
    vector<pair<int, string>> presets;
    presetsInBank = {"None"};
    dir.open("MacroPresets/Other");
    if(!dir.exists())
        dir.createDirectory("MacroPresets/Other");
    dir.sort();
    int numPresets = dir.listDir();
    for ( int i = 0 ; i < numPresets; i++){
        presets.push_back(pair<int, string>(ofToInt(ofSplitString(dir.getName(i), "--")[0]), dir.getName(i)));
    }
    
    std::sort(presets.begin(), presets.end(), [](pair<int, string> &left, pair<int, string> &right) {
        return left.first< right.first;
    });
    
    for(auto &p : presets){
        presetsInBank.push_back(p.second);
    }
    
    addParameterToGroupAndInfo(savePreset.set("Save Preset?", false));
    addParameterToGroupAndInfo(createDropdownAbstractParameter("Preset", presetsInBank, preset));
    
    addParameterToGroupAndInfo(savePresetField.set("Save Preset", "")).isSavePreset = false;
    
    presetActionsListeners.push(bank.newListener([this](int &i){
        vector<pair<int, string>> presets;
        presetsInBank = {"None"};
        ofDirectory dir;
        dir.open("MacroPresets/" + bankNames[i]);
        if(!dir.exists())
            dir.createDirectory("MacroPresets/" + bankNames[i]);
        dir.sort();
        int numPresets = dir.listDir();
        for ( int i = 0 ; i < numPresets; i++){
            presets.push_back(pair<int, string>(ofToInt(ofSplitString(dir.getName(i), "--")[0]), dir.getName(i)));
        }
        
        std::sort(presets.begin(), presets.end(), [](pair<int, string> &left, pair<int, string> &right) {
            return left.first< right.first;
        });
        
        for(auto &p : presets){
            presetsInBank.push_back(p.second);
        }
    }));
    
    presetActionsListeners.push(savePresetField.newListener([this](string &s){
        if(s != ""){
            string newPresetName;
            int newPresetNum;
            if(presetsInBank.size() > 1){
                string lastPreset = presetsInBank.back();
                newPresetNum = ofToInt(ofSplitString(lastPreset, "--")[0]) + 1;
                newPresetName = ofToString(newPresetNum) + "--" + s;
            }else
                newPresetName = "1--" + s;
            
            ofStringReplace(newPresetName, " ", "_");
            presetsInBank.push_back(newPresetName);
            container->savePreset("MacroPresets/" + bankNames[bank] + "/" + newPresetName);
            s = "";
            string name = "Preset Selector";
            string optionsString = parameters->getGroup(name).getString(0);
            optionsString += "-|-" + newPresetName;
            parameters->getGroup(name).getString(0).set(optionsString);
            parameters->getGroup(name).getInt(1).setMax(presetsInBank.size()-1);
            parameters->getGroup(name).getInt(1).set(presetsInBank.size()-1);
            ofNotifyEvent(dropdownChanged, name);
        }
    }));
    
    presetActionsListeners.push(preset.newListener([this](int &i){
        if(i != 0){
            if(savePreset){
                container->savePreset("MacroPresets/" + bankNames[bank] + "/" + presetsInBank[i]);
                savePreset = false;
            }else{
                container->loadPreset("MacroPresets/" + bankNames[bank] + "/" + presetsInBank[i]);
            }
        }
    }));
    
    ofParameter<char> separate;
    parameters->add(separate.set("=========================", 'c'));
}

void ofxOceanodeNodeMacro::setupForExternalWindow(){
    container->setWindow(externalWindow);
    canvas = new ofxOceanodeCanvas;
    canvas->setContainer(container);
    canvas->setup(externalWindow);
}

void ofxOceanodeNodeMacro::closeExternalWindow(ofEventArgs &e){
    ofxOceanodeNodeModelExternalWindow::closeExternalWindow(e);
    delete canvas;
    canvas = nullptr;
}

void ofxOceanodeNodeMacro::newNodeCreated(ofxOceanodeNode* &node){
    bool inletOutletCreated = true;
    string nodeName = node->getParameters()->getName();
    if(node->getNodeModel().nodeName() == "Inlet"){
        if(node->getParameters()->get("Input").type() == typeid(ofParameter<vector<float>>).name()){
            ofParameter<vector<float>> *input = new ofParameter<vector<float>>();
            paramsStore[nodeName] = input;
            parameters->add(input->set(nodeName, {0}, {0}, {1}));
            inoutListeners[node->getParameters()->getName()].push(input->newListener([this, node](vector<float> &f){
                node->getParameters()->get<vector<float>>("Input") = f;
            }));
            inoutListeners[node->getParameters()->getName()].push(node->getParameters()->getString("Min").newListener([this, node](string &s){
                float f = ofToFloat(s);
                node->getParameters()->get<vector<float>>("Input").setMin(vector<float>(1, f));
                string parameterName = "Input";
                ofNotifyEvent(node->getNodeModel().parameterChangedMinMax, parameterName);
                
                parameters->get<vector<float>>(node->getParameters()->getName()).setMin(vector<float>(1, f));
                parameterName = node->getParameters()->getName();
                ofNotifyEvent(parameterChangedMinMax, parameterName);
            }));
            inoutListeners[node->getParameters()->getName()].push(node->getParameters()->getString("Max").newListener([this, node](string &s){
                float f = ofToFloat(s);
                node->getParameters()->get<vector<float>>("Input").setMax(vector<float>(1, f));
                string parameterName = "Input";
                ofNotifyEvent(node->getNodeModel().parameterChangedMinMax, parameterName);
                
                parameters->get<vector<float>>(node->getParameters()->getName()).setMax(vector<float>(1, f));
                parameterName = node->getParameters()->getName();
                ofNotifyEvent(parameterChangedMinMax, parameterName);
            }));
        }
    }else if(node->getNodeModel().nodeName() == "Outlet"){
        if(node->getParameters()->get("Output").type() == typeid(ofParameter<vector<float>>).name()){
            ofParameter<vector<float>> *output = new ofParameter<vector<float>>();
            paramsStore[nodeName] = output;
            parameters->add(output->set(node->getParameters()->getName(), {0}, {0}, {1}));
            inoutListeners[node->getParameters()->getName()].push(node->getParameters()->get<vector<float>>("Output").newListener([this, output](vector<float> &f){
                parameters->get<vector<float>>(output->getName()) = f;
            }));
            inoutListeners[node->getParameters()->getName()].push(node->getParameters()->getString("Min").newListener([this, node](string &s){
                float f = ofToFloat(s);
                node->getParameters()->get<vector<float>>("Output").setMin(vector<float>(1, f));
                string parameterName = "Output";
                ofNotifyEvent(node->getNodeModel().parameterChangedMinMax, parameterName);
                
                parameters->get<vector<float>>(node->getParameters()->getName()).setMin(vector<float>(1, f));
                parameterName = node->getParameters()->getName();
                ofNotifyEvent(parameterChangedMinMax, parameterName);
            }));
            inoutListeners[node->getParameters()->getName()].push(node->getParameters()->getString("Max").newListener([this, node](string &s){
                float f = ofToFloat(s);
                node->getParameters()->get<vector<float>>("Output").setMax(vector<float>(1, f));
                string parameterName = "Output";
                ofNotifyEvent(node->getNodeModel().parameterChangedMinMax, parameterName);
                
                parameters->get<vector<float>>(node->getParameters()->getName()).setMax(vector<float>(1, f));
                parameterName = node->getParameters()->getName();
                ofNotifyEvent(parameterChangedMinMax, parameterName);
            }));
        }
    }else{
        inletOutletCreated = false;
    }
    
    if(inletOutletCreated){
        parameterGroupChanged.notify(this);
        deleteListeners.push(node->deleteModuleAndConnections.newListener([this, node](vector<ofxOceanodeAbstractConnection*> &c){
            string nodeName = node->getParameters()->getName();
            disconnectConnectionsForParameter.notify(nodeName);
            parameters->remove(nodeName);
            inoutListeners.erase(nodeName);
            parameterGroupChanged.notify(this);
            delete paramsStore[nodeName];
            paramsStore.erase(nodeName);
        }, 0));
    }
}

void ofxOceanodeNodeMacro::presetSave(ofJson &json){
    if(preset == 0){ //None Preset selected, save to json a custom preset
        string path = presetPath;
        //if(path == ""){
            ofDirectory dir;
            if(!dir.doesDirectoryExist("MacroPresets/Project")){
                dir.createDirectory("MacroPresets/Project");
            }
            dir.open("MacroPresets/Project");
            dir.sort();
            vector<int> presets = {0};
            for(int i = 0; i < dir.listDir(); i++){
                presets.push_back(ofToInt(dir.getName(i)));
            }
            std::sort(presets.begin(), presets.end(), [](int &left, int &right) {
                return left < right;
            });
            
            path = "MacroPresets/Project/" + ofToString(presets.back()+1);
            presetPath = path;
        //}
        json["preset"] = presetPath;
        container->savePreset(presetPath);
    }
}

void ofxOceanodeNodeMacro::loadBeforeConnections(ofJson &json){
    savePreset = false;
    if(json.count("preset") != 0){
        string path = json["preset"];
        container->loadPreset(path);
        presetPath = path;
    }
}
