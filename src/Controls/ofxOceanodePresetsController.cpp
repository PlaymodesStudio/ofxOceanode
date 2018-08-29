//
//  ofxOceanodePresetsController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#include "ofxOceanodePresetsController.h"
#include "ofxOceanodeContainer.h"

ofxOceanodePresetsController::ofxOceanodePresetsController(shared_ptr<ofxOceanodeContainer> _container) : ofxOceanodeBaseController(_container, "Presets"){
    //Preset Control
    ofDirectory dir;
    vector<string> banks;
    dir.open("Presets");
    if(!dir.exists()){
        dir.createDirectory("Presets");
    }
    for(int i = 0; i < dir.listDir() ; i++){
        banks.push_back(dir.getName(i));
    }
    if(dir.listDir() == 0){
        banks.push_back("Initial_Bank");
    }
    banks.push_back(" -- NEW BANK -- ");
    bankSelect = gui->addDropdown("Bank Select", banks);
    bankSelect->select(0);
    gui->addLabel("<== Presets List ==>")->setStripe(ofColor::red, 10);
    
    int layoutHeight = mainGuiTheme->layout.height;
    int numItemsInScrollView = floor((ofGetHeight()-((layoutHeight+1.5)*3))/(float)(layoutHeight+1.5));
    presetsList = gui->addScrollView("test", numItemsInScrollView);
    
    loadBank();
    
    gui->addTextInput("New Preset");
    
    //ControlGui Events
    gui->onDropdownEvent(this, &ofxOceanodePresetsController::onGuiDropdownEvent);
    gui->onScrollViewEvent(this, &ofxOceanodePresetsController::onGuiScrollViewEvent);
    gui->onTextInputEvent(this, &ofxOceanodePresetsController::onGuiTextInputEvent);
    
    oldPresetButton = nullptr;
    
    presetListener = container->loadPresetEvent.newListener([this](string preset){
        vector<string> presetInfo = ofSplitString(preset, "/");
        oldPresetButton = nullptr;
        bool foundBank = false;
        for(int i = 0; i < bankSelect->getNumOptions(); i++){
            if(bankSelect->getChildAt(i)->getName() == presetInfo[0]){
                bankSelect->select(i);
                foundBank = true;
                break;
            }
        }
        if(foundBank == true){
            loadBank();
            if(presetsList->get(presetInfo[1]) != nullptr)
               changePresetLabelHighliht(presetsList->get(presetInfo[1]));
               loadPreset(presetInfo[1], presetInfo[0]);
        }
    });
    
    int loadPresetInNextUpdate = 0;
}

void ofxOceanodePresetsController::draw(){
    if(isActive)
        gui->draw();
}

void ofxOceanodePresetsController::update(){
    if(loadPresetInNextUpdate != 0){
        if(currentBankPresets.count(loadPresetInNextUpdate) > 0){
            changePresetLabelHighliht(presetsList->get(currentBankPresets[loadPresetInNextUpdate]));
            loadPreset(currentBankPresets[loadPresetInNextUpdate], bankSelect->getSelected()->getName());
        }
        loadPresetInNextUpdate = 0;
    }
    if(isActive)
        gui->update();
}


void ofxOceanodePresetsController::loadPresetFromNumber(int num){
    loadPresetInNextUpdate = num;
}

void ofxOceanodePresetsController::onGuiDropdownEvent(ofxDatGuiDropdownEvent e){
    oldPresetButton = nullptr;
    if(e.child == bankSelect->getNumOptions()-1){
        bankSelect->addOption("Bank_" + ofGetTimestampString(), bankSelect->getNumOptions()-1);
        bankSelect->select(bankSelect->getNumOptions()-2);
        bankSelect->setTheme(mainGuiTheme);
    }
    loadBank();
}

void ofxOceanodePresetsController::onGuiScrollViewEvent(ofxDatGuiScrollViewEvent e){
    if(ofGetKeyPressed(OF_KEY_SHIFT)){
        changePresetLabelHighliht(e.target);
        savePreset(e.target->getName(), bankSelect->getSelected()->getName());
    }else{
        changePresetLabelHighliht(e.target);
        loadPreset(e.target->getName(), bankSelect->getSelected()->getName());
    }
}

void ofxOceanodePresetsController::onGuiTextInputEvent(ofxDatGuiTextInputEvent e){
    if(e.text != ""){
        string newPresetName;
        int newPresetNum;
        if(presetsList->getNumItems() != 0){
            string lastPreset = presetsList->get(presetsList->getNumItems()-1)->getName();
            newPresetNum = ofToInt(ofSplitString(lastPreset, "--")[0]) + 1;
            newPresetName = ofToString(newPresetNum) + "--" + e.text;
        }else
            newPresetName = "1--" + e.text;
        
        ofStringReplace(newPresetName, " ", "_"); 
        presetsList->add(newPresetName);
        currentBankPresets[newPresetNum] = newPresetName;
        changePresetLabelHighliht(presetsList->get(presetsList->getNumItems()-1));
        savePreset(newPresetName, bankSelect->getSelected()->getName());
        
        e.text = "";
    }
}

void ofxOceanodePresetsController::windowResized(ofResizeEventArgs &a){
    ofxOceanodeBaseController::windowResized(a);
    int layoutHeight = mainGuiTheme->layout.height;
    presetsList->setNumVisible(floor((ofGetHeight()-((layoutHeight+1.5)*3))/(float)(layoutHeight+1.5)));
}

void ofxOceanodePresetsController::changePresetLabelHighliht(ofxDatGuiButton *presetToHighlight){
    if(presetToHighlight != nullptr){
        if(oldPresetButton != nullptr) oldPresetButton->setTheme(mainGuiTheme);
        presetToHighlight->setLabelColor(ofColor::red);
        oldPresetButton = presetToHighlight;
    }
}

void ofxOceanodePresetsController::loadBank(){
    string bankName = bankSelect->getSelected()->getName();
    
    ofDirectory dir;
    vector<pair<int, string>> presets;
    dir.open("Presets/" + bankName);
    if(!dir.exists())
        dir.createDirectory("Presets/" + bankName);
    dir.sort();
    int numPresets = dir.listDir();
    ofLog() << "Dir size: " << ofToString(numPresets);
    for ( int i = 0 ; i < numPresets; i++){
        presets.push_back(pair<int, string>(ofToInt(ofSplitString(dir.getName(i), "--")[0]), dir.getName(i)));
        currentBankPresets[ofToInt(ofSplitString(dir.getName(i), "--")[0])] = dir.getName(i);
    }
    
    std::sort(presets.begin(), presets.end(), [](pair<int, string> &left, pair<int, string> &right) {
        return left.first< right.first;
    });
    
    presetsList->clear();
    
    
    for(auto preset : presets){
        presetsList->add(preset.second);
    }
    if(presets.size() > 0){
        presetsList->resetScroll();
    }
}

void ofxOceanodePresetsController::loadPreset(string name, string bank){
    container->loadPreset("Presets/" + bank + "/" + name);
}

void ofxOceanodePresetsController::savePreset(string name, string bank){
    container->savePreset("Presets/" + bank + "/" + name);
}
