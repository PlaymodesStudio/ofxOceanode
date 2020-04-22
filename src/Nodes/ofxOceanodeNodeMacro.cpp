//
//  ofxOceanodeNodeMacro.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 20/06/2019.
//

#include "ofxOceanodeNodeMacro.h"

ofxOceanodeNodeMacro::ofxOceanodeNodeMacro() : ofxOceanodeNodeModel("Macro"){
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
    if(showWindow){
        canvas.draw((bool*)&showWindow.get());
        showWindow = showWindow;
    }
    container->draw();
}

void ofxOceanodeNodeMacro::setContainer(ofxOceanodeContainer* container){
    registry = container->getRegistry();
    typesRegistry = container->getTypesRegistry();
    canvasParentID = container->getCanvasID();
}

void ofxOceanodeNodeMacro::setup(){
    addParameter(showWindow.set("Show", true));
    
//    registry->registerModel<inlet<vector<float>>>("I/O");
//    registry->registerModel<outlet<vector<float>>>("I/O");
//    registry->registerModel<inlet<ofTexture*>>("I/O");
//    registry->registerModel<outlet<ofTexture*>>("I/O");
    container = make_shared<ofxOceanodeContainer>(registry, typesRegistry);
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
	
    addParameterDropdown(bank, "Bank", 0, bankNames, ofxOceanodeParameterFlags_DisableSavePreset);
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
    
    addParameter(savePreset.set("Save Preset?", false));
    
    addParameterDropdown(preset, "Preset", 0, presetsInBank);
    
	addParameter(savePresetField.set("Save Preset", ""), ofxOceanodeParameterFlags_DisableSavePreset);
    
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
    addParameter(separate.set("=========================", 'c'));
    
    
    canvas.setContainer(container);
    canvas.setup("Macro " + ofToString(getNumIdentifier()), canvasParentID);
}

void ofxOceanodeNodeMacro::newNodeCreated(ofxOceanodeNode* &node){
    string nodeName = node->getParameters().getName();
    if(ofSplitString(nodeName, " ")[0] == "Router"){
        auto newCreatedParam = typesRegistry->createRouterFromType(node);
        string paramName = newCreatedParam->getName();
        while (getParameterGroup().contains(paramName)) {
            paramName += paramName;
        }
        newCreatedParam->setName(paramName);
        addParameter(*newCreatedParam.get());
        
        parameterGroupChanged.notify(this);
        deleteListeners.push(node->deleteModule.newListener([this, node](){
            string nodeName = node->getParameters().getName();
            //getParameters().remove(nodeName);
            inoutListeners.erase(nodeName);
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
//    else deserializeParameter(json, *bankDropdown);
//    deserializeParameter(json, *presetDropdown);
}
