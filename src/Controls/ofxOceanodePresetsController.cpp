//
//  ofxOceanodePresetsController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#include "ofxOceanodePresetsController.h"
#include "ofxOceanodeContainer.h"

ofxOceanodePresetsController::ofxOceanodePresetsController(shared_ptr<ofxOceanodeContainer> _container) : container(_container), ofxOceanodeBaseController("Presets"){
    //DatGui
    
    ofxDatGuiLog::quiet();
    ofxDatGui::setAssetPath("");
    
    mainGuiTheme = new ofxDatGuiThemeCharcoal;
    ofColor randColor =  ofColor::indianRed;
    mainGuiTheme->color.slider.fill = randColor;
    mainGuiTheme->color.textInput.text = randColor;
    mainGuiTheme->color.icons = randColor;
    int layoutHeight = ofGetWidth()/15;
    mainGuiTheme->font.size = ofGetWidth()/40;
    mainGuiTheme->layout.height = layoutHeight;
    mainGuiTheme->layout.width = ofGetWidth();
    mainGuiTheme->init();
    
    gui = new ofxDatGui();
    gui->setTheme(mainGuiTheme);
    gui->setPosition(0, 30);
    gui->setWidth(ofGetWidth());
    
    gui->setVisible(false);
    
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
        banks.push_back("Inital Bank");
    }
    banks.push_back(" -- NEW BANK -- ");
    bankSelect = gui->addDropdown("Bank Select", banks);
    bankSelect->select(0);
    gui->addLabel("<== Presets List ==>")->setStripe(ofColor::red, 10);
    
    int numItemsInScrollView = floor((ofGetHeight()-((layoutHeight+1.5)*3))/(float)(layoutHeight+1.5));
    presetsList = gui->addScrollView("test", numItemsInScrollView);
    
    loadBank();
    
    gui->addTextInput("New Preset");
    //gui("Automatic Preset");
    //gui("Reload Sequence");
    //gui->addSlider(fadeTime.set("Fade Time", 0, 0, 10));
    //gui->addSlider(presetChangeBeatsPeriod.set("Beats Period", 4, 1, 120));
    
    //ControlGui Events
    gui->onDropdownEvent(this, &ofxOceanodePresetsController::onGuiDropdownEvent);
    gui->onScrollViewEvent(this, &ofxOceanodePresetsController::onGuiScrollViewEvent);
    gui->onTextInputEvent(this, &ofxOceanodePresetsController::onGuiTextInputEvent);
    
    
    oldPresetButton == nullptr;
}

void ofxOceanodePresetsController::activate(){
    ofxOceanodeBaseController::activate();
    gui->setVisible(true);
}

void ofxOceanodePresetsController::deactivate(){
    ofxOceanodeBaseController::deactivate();
    gui->setVisible(false);
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
//        if(autoPreset)
//            presetChangedTimeStamp = ofGetElapsedTimef();
    }
}

void ofxOceanodePresetsController::onGuiTextInputEvent(ofxDatGuiTextInputEvent e){
    if(e.text != ""){
        string newPresetName;
        if(presetsList->getNumItems() != 0){
            string lastPreset = presetsList->get(presetsList->getNumItems()-1)->getName();
            newPresetName = ofToString(ofToInt(ofSplitString(lastPreset, "|")[0])+1) + "|" + e.text;
        }else
            newPresetName = "1|" + e.text;
        
        presetsList->add(newPresetName);
        changePresetLabelHighliht(presetsList->get(presetsList->getNumItems()-1));
        savePreset(newPresetName, bankSelect->getSelected()->getName());
        
        e.text = "";
    }
}

void ofxOceanodePresetsController::windowResized(ofResizeEventArgs &a){
    int layoutHeight = ofGetWidth()/15;
    mainGuiTheme->font.size = ofGetWidth()/40;
    mainGuiTheme->layout.height = layoutHeight;
    mainGuiTheme->layout.width = ofGetWidth();
    mainGuiTheme->init();
    
    gui->setTheme(mainGuiTheme);
    gui->setWidth(ofGetWidth());
    
    presetsList->setNumVisible(floor((ofGetHeight()-((layoutHeight+1.5)*3))/(float)(layoutHeight+1.5)));
}

void ofxOceanodePresetsController::changePresetLabelHighliht(ofxDatGuiButton *presetToHighlight){
    if(oldPresetButton != nullptr) oldPresetButton->setTheme(mainGuiTheme);
    presetToHighlight->setLabelColor(ofColor::red);
    oldPresetButton = presetToHighlight;
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
    for ( int i = 0 ; i < numPresets; i++)
        presets.push_back(pair<int, string>(ofToInt(ofSplitString(dir.getName(i), "|")[0]), ofSplitString(dir.getName(i), ".")[0]));
    
    std::sort(presets.begin(), presets.end(), [](pair<int, string> &left, pair<int, string> &right) {
        return left.first< right.first;
    });
    
    presetsList->clear();
    
    for(auto preset : presets)
        presetsList->add(preset.second);
}

void ofxOceanodePresetsController::loadPreset(string name, string bank){
    container->loadPreset("Presets/" + bank + "/" + name);
}

void ofxOceanodePresetsController::savePreset(string name, string bank){
    container->savePreset("Presets/" + bank + "/" + name);
}
