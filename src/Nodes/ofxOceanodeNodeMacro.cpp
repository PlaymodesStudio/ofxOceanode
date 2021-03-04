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
    
    /*addParameter(presetControl.set("Preset Control Gui", [this](){
        bool addBank = false;
        
        if (ImGui::Button("Load Subpatch..."))
            ImGui::OpenPopup("loadSubpatchMenu");
        if (ImGui::BeginPopup("loadSubpatchMenu"))
        {
            if (ImGui::BeginMenu("First Bank")) {
                if (ImGui::MenuItem("A Preset")) {ofLog() << "Load a preset";}
                if (ImGui::MenuItem("Ohter Preset")) {ofLog() << "Load other preset";}
                if (ImGui::MenuItem("Some other Preset")) {ofLog() << "Load some other preset";}
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Second Bank")) {
                if (ImGui::MenuItem("A Preset")) {ofLog() << "Load a preset";}
                if (ImGui::MenuItem("Ohter Preset")) {ofLog() << "Load other preset";}
                if (ImGui::MenuItem("Some other Preset")) {ofLog() << "Load some other preset";}
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Add Bank")) {
                addBank = true;
            }
//            ImGui::MenuItem("(demo menu)", NULL, false, false);
//            if (ImGui::MenuItem("New")) {}
//            if (ImGui::MenuItem("Open", "Ctrl+O")) {}
//            if (ImGui::BeginMenu("Open Recent"))
//            {
//                ImGui::MenuItem("fish_hat.c");
//                ImGui::MenuItem("fish_hat.inl");
//                ImGui::MenuItem("fish_hat.h");
//                if (ImGui::BeginMenu("More.."))
//                {
//                    ImGui::MenuItem("Hello");
//                    ImGui::MenuItem("Sailor");
//                    ImGui::EndMenu();
//                }
//                ImGui::EndMenu();
//            }
//            if (ImGui::MenuItem("Save", "Ctrl+S")) {}
//            if (ImGui::MenuItem("Save As..")) {}
            
            ImGui::EndPopup();
        }
        
        if(addBank){
            ImGui::OpenPopup("Add New Macro Bank");
        }
                
        if(ImGui::BeginPopupModal("Add New Macro Bank", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
            static char cString[256];
            if (ImGui::InputText("Bank Name", cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
            {
//                string proposedNewName(cString);
//                ofStringReplace(proposedNewName, " ", "_");
//                if(find(banks.begin(), banks.end(), proposedNewName) == banks.end()){
//                    if(proposedNewName != ""){
//                        banks.push_back(proposedNewName);
//                        currentBank = banks.size()-1;
//                        currentPreset[banks[currentBank]] = "";
//                    }
                    ImGui::CloseCurrentPopup();
//                }
//                strcpy(cString, "");
            }
            if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsItemActive()){
                strcpy(cString, "");
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        
        if (ImGui::Button("Save Subpatch")){}
        
        ImGui::SameLine();
        
        if (ImGui::Button("Save As"))
            ImGui::OpenPopup("newSubpatchPopup");
        
    }));
     */
    
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
	
    bankDropdown = addParameterDropdown(bank, "Bank", 0, bankNames, ofxOceanodeParameterFlags_DisableSavePreset);
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
    
    presetDropdown = addParameterDropdown(preset, "Preset", 0, presetsInBank);
    
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
            
            presetDropdown->setDropdownOptions(presetsInBank);
            preset.setMax(presetsInBank.size() - 1);
//            if(tempStr != presetDropdown->castGroup().getString(0).get()){ //Added a bank
//                presetDropdown->castGroup().getString(0) = tempStr;
//                string paramName = presetDropdown->getName();
//                dropdownChanged.notify(paramName);
//            }
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
            presetDropdown->setDropdownOptions(presetsInBank);
            preset.setMax(presetsInBank.size());
            container->savePreset("MacroPresets/" + bankNames[bank] + "/" + newPresetName + "/");
            ofDirectory dir;
            dir.open("MacroPresets/" + bankNames[bank] + "/" + newPresetName);
//            presetLastChanged.clear();
//            for(int i = 0; i < dir.listDir(); i++){
//                presetLastChanged[dir.getPath(i)] = std::filesystem::last_write_time(ofToDataPath(dir.getPath(i)));
//            }
            s = "";
        }
    }));
    
    presetActionsListeners.push(preset.newListener([this](int &i){
        if(i > 0 && i <= preset.getMax()){
            if(savePreset){
                container->savePreset("MacroPresets/" + bankNames[bank] + "/" + presetsInBank[i]);
                savePreset = false;
            }else{
                if(i != currentPreset){
                    container->loadPreset("MacroPresets/" + bankNames[bank] + "/" + presetsInBank[i]);
                    currentPreset = i;
                }
            }
            ofDirectory dir;
            dir.open("MacroPresets/" + bankNames[bank] + "/" + presetsInBank[i] + "/");
//            presetLastChanged.clear();
//            for(int i = 0; i < dir.listDir(); i++){
//                presetLastChanged[dir.getPath(i)] = std::filesystem::last_write_time(ofToDataPath(dir.getPath(i)));
//            }
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
            paramName = "_" + paramName;
        }
        newCreatedParam->setName(paramName);
        addParameter(*newCreatedParam.get());
        
        ofParameter<string> nameParamFromRouter = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam();
        nameParamFromRouter = paramName;
        
        parameterGroupChanged.notify(this);
        deleteListeners.push(node->deleteModule.newListener([this, nameParamFromRouter](){
            getParameterGroup().remove(nameParamFromRouter);
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
    else ofDeserialize(json, bank);
    ofDeserialize(json, preset);
}
