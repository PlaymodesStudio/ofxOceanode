//
//  ofxOceanodeNodeMacro.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 20/06/2019.
//

#include "ofxOceanodeNodeMacro.h"

ofxOceanodeNodeMacro::ofxOceanodeNodeMacro() : ofxOceanodeNodeModel("Macro"){
#ifndef OFXOCEANODE_HEADLESS
    canvas = nullptr;
#endif
    presetPath = "";
    currentPreset = -1;
}

void ofxOceanodeNodeMacro::update(ofEventArgs &a){
    container->update();
//    auto currentBankLocale = std::filesystem::last_write_time(ofToDataPath("MacroPresets/"));
//    auto currentPresetInBankLocale = std::filesystem::last_write_time(ofToDataPath("MacroPresets/" + bankNames[bank] + "/"));
//    if(currentBankLocale != bankLastChanged){
//        string currentBank = bankNames[bank];
//        ofDirectory dir;
//        dir.open("MacroPresets");
//        dir.sort();
//        bankNames.clear();
//        for(int i = 0; i < dir.listDir(); i++){
//            if(dir.getName(i) != "Project"){
//                bankNames.push_back(dir.getName(i));
//            }
//        }
//        string  tempStr;
//        for(auto opt : bankNames)
//            tempStr += opt + "-|-";
//        tempStr.erase(tempStr.end()-3, tempStr.end());
//        if(tempStr != bankDropdown->castGroup().getString(0).get()){ //Added a bank
//            bankDropdown->castGroup().getString(0) = tempStr;
//            string paramName = bankDropdown->getName();
//            dropdownChanged.notify(paramName);
//            auto currentBankPos = std::find(bankNames.begin(), bankNames.end(), currentBank);
//            if(currentBankPos != bankNames.end())
//                bank.set(std::distance(bankNames.begin(), currentBankPos));
//        }
//        bankLastChanged = currentBankLocale;
//    }
//    else if(currentPresetInBankLocale != presetsInBankLastChanged){
//        vector<pair<int, string>> presets;
//        presetsInBank = {"None"};
//        ofDirectory dir;
//        dir.open("MacroPresets/" + bankNames[bank]);
//        dir.sort();
//        int numPresets = dir.listDir();
//        for ( int i = 0 ; i < numPresets; i++){
//            presets.push_back(pair<int, string>(ofToInt(ofSplitString(dir.getName(i), "--")[0]), dir.getName(i)));
//        }
//
//        std::sort(presets.begin(), presets.end(), [](pair<int, string> &left, pair<int, string> &right) {
//            return left.first< right.first;
//        });
//
//        for(auto &p : presets){
//            presetsInBank.push_back(p.second);
//        }
//        string  tempStr;
//        for(auto opt : presetsInBank)
//            tempStr += opt + "-|-";
//        tempStr.erase(tempStr.end()-3, tempStr.end());
//        if(tempStr != presetDropdown->castGroup().getString(0).get()){ //Added a bank
//            presetDropdown->castGroup().getString(0) = tempStr;
//            string paramName = presetDropdown->getName();
//            dropdownChanged.notify(paramName);
//        }
//        presetsInBankLastChanged = currentPresetInBankLocale;
//    }
//    else if(preset != 0){
//        ofDirectory dir;
//        dir.open("MacroPresets/" + bankNames[bank] + "/" + presetsInBank[preset] + "/");
//        bool presetChanged = false;
//        if(dir.listDir() != presetLastChanged.size()) presetChanged = true;
//        else{
//            for(int i = 0; i < dir.listDir(); i++){
//                if(presetLastChanged.count(dir.getPath(i)) == 0){
//                    presetChanged = true;
//                    break;
//                }
//                if(presetLastChanged[dir.getPath(i)] != std::filesystem::last_write_time(ofToDataPath(dir.getPath(i)))){
//                    presetChanged = true;
//                    break;
//                }
//            }
//        }
//        if(presetChanged){
//            container->loadPreset("MacroPresets/" + bankNames[bank] + "/" + presetsInBank[preset]);
//            ofDirectory dir;
//            dir.open("MacroPresets/" + bankNames[bank] + "/" + presetsInBank[preset] + "/");
//            presetLastChanged.clear();
//            for(int i = 0; i < dir.listDir(); i++){
//                presetLastChanged[dir.getPath(i)] = std::filesystem::last_write_time(ofToDataPath(dir.getPath(i)));
//            }
//        }
//    }
}

void ofxOceanodeNodeMacro::draw(ofEventArgs &a){
    canvas->draw();
    container->draw();
}

void ofxOceanodeNodeMacro::setContainer(ofxOceanodeContainer* container){
    registry = container->getRegistry();
    typesRegistry = container->getTypesRegistry();
}

void ofxOceanodeNodeMacro::setup(){
    registry->registerModel<inlet<vector<float>>>("I/O");
    registry->registerModel<outlet<vector<float>>>("I/O");
    registry->registerModel<inlet<ofTexture*>>("I/O");
    registry->registerModel<outlet<ofTexture*>>("I/O");
    container = make_shared<ofxOceanodeContainer>(registry, typesRegistry);
#ifndef OFXOCEANODE_HEADLESS
    container->setAutoUpdateAndDraw(false);
    container->setWindow(nullptr);
#endif
    newNodeListener = container->newNodeCreated.newListener(this, &ofxOceanodeNodeMacro::newNodeCreated);
    ofDirectory dir;
    if(!dir.doesDirectoryExist("MacroPresets")){
        dir.createDirectory("MacroPresets");
    }
    bankLastChanged = std::filesystem::last_write_time(ofToDataPath("MacroPresets"));
    dir.open("MacroPresets");
    dir.sort();
    if(dir.listDir() == 0) dir.createDirectory("MacroPresets/Other");
    if(dir.listDir() == 1 && dir.getName(0) == "Project") dir.createDirectory("MacroPresets/Other");
    for(int i = 0; i < dir.listDir(); i++){
        if(dir.getName(i) != "Project"){
            bankNames.push_back(dir.getName(i));
        }
    }
    
    bankDropdown = &createDropdownAbstractParameter("Bank", bankNames, bank);
    addParameterToGroupAndInfo(*bankDropdown).isSavePreset = false;
    auto BankNamePos = std::find(bankNames.begin(), bankNames.end(), "Other");
    if(BankNamePos != bankNames.end()){
        bank.set(std::distance(bankNames.begin(), BankNamePos));
    }
    previousBank = bank;
    
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
    
    presetDropdown = &createDropdownAbstractParameter("Preset", presetsInBank, preset);
    addParameterToGroupAndInfo(*presetDropdown);
    
    addParameterToGroupAndInfo(savePresetField.set("Save Preset", "")).isSavePreset = false;
    
    presetActionsListeners.push(bank.newListener([this](int &i){
        if(previousBank != bank){
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
            string  tempStr;
            for(auto opt : presetsInBank)
                tempStr += opt + "-|-";
            tempStr.erase(tempStr.end()-3, tempStr.end());
            if(tempStr != presetDropdown->castGroup().getString(0).get()){ //Added a bank
                presetDropdown->castGroup().getString(0) = tempStr;
                string paramName = presetDropdown->getName();
                dropdownChanged.notify(paramName);
            }
            preset = 0;
            previousBank = bank;
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
            container->savePreset("MacroPresets/" + bankNames[bank] + "/" + newPresetName + "/");
            ofDirectory dir;
            dir.open("MacroPresets/" + bankNames[bank] + "/" + newPresetName);
            presetLastChanged.clear();
            for(int i = 0; i < dir.listDir(); i++){
                presetLastChanged[dir.getPath(i)] = std::filesystem::last_write_time(ofToDataPath(dir.getPath(i)));
            }
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
            ofDirectory dir;
            dir.open("MacroPresets/" + bankNames[bank] + "/" + presetsInBank[i] + "/");
            presetLastChanged.clear();
            for(int i = 0; i < dir.listDir(); i++){
                presetLastChanged[dir.getPath(i)] = std::filesystem::last_write_time(ofToDataPath(dir.getPath(i)));
            }
        }
    }));
    
    ofParameter<char> separate;
    parameters->add(separate.set("=========================", 'c'));
    
    
    canvas = new ofxOceanodeCanvas;
    canvas->setContainer(container);
    canvas->setup();
    canvas->setUniqueID("Macro " + ofToString(getNumIdentifier()));
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
    }else if(node->getNodeModel().nodeName() == "Inlet Tex"){
        if(node->getParameters()->get("Input").type() == typeid(ofParameter<ofTexture*>).name()){
            ofParameter<ofTexture*> *input = new ofParameter<ofTexture*>();
            paramsStore[nodeName] = input;
            parameters->add(input->set(nodeName, nullptr));
            inoutListeners[node->getParameters()->getName()].push(input->newListener([this, node](ofTexture* &f){
                node->getParameters()->get<ofTexture*>("Input") = f;
            }));
        }
    }else if(node->getNodeModel().nodeName() == "Outlet Tex"){
        if(node->getParameters()->get("Output").type() == typeid(ofParameter<ofTexture*>).name()){
            ofParameter<ofTexture*> *output = new ofParameter<ofTexture*>();
            paramsStore[nodeName] = output;
            parameters->add(output->set(node->getParameters()->getName(), nullptr));
            inoutListeners[node->getParameters()->getName()].push(node->getParameters()->get<ofTexture*>("Output").newListener([this, output](ofTexture* &f){
                parameters->get<ofTexture*>(output->getName()) = f;
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
    }else{
        json["BankName"] = bankNames[bank];
    }
}

void ofxOceanodeNodeMacro::loadBeforeConnections(ofJson &json){
    savePreset = false;
    if(json.count("preset") != 0){
        string path = json["preset"];
        container->loadPreset(path);
        presetPath = path;
    }
    else if(json.count("BankName") != 0){
        string bankName = json["BankName"];
        auto BankNamePos = std::find(bankNames.begin(), bankNames.end(), bankName);
        if(BankNamePos != bankNames.end())
            bank.set(std::distance(bankNames.begin(), BankNamePos));
    }
    else deserializeParameter(json, *bankDropdown);
    deserializeParameter(json, *presetDropdown);
}
