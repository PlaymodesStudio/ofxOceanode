//
//  ofxOceanodePresetsController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#ifndef ofxOceanodePresetsController_h
#define ofxOceanodePresetsController_h

#include "ofxOceanodeBaseController.h"
#include "GlobalMacroSaveReview.h"
#include <memory>

class ofxOceanodePresetsController: public ofxOceanodeBaseController{
public:
    ofxOceanodePresetsController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodePresetsController(){};
    
    void draw() override;
    void drawPopups() override;
    void update() override;
    bool isMenuController() const override { return true; }
    
    void loadPresetFromNumber(int num);
    
private:
    void newPreset();
    void drawPresetList();
    
    void loadPreset(string name, string bank);
    void savePreset(string name, string bank);
    void beginSavePreset(string name, string bank, bool createNew);
    void finishSavePreset();
    void drawGlobalMacroSaveReview();
    void deletePreset(string name, string bank);
    
    map<string, vector<string>> bankPresets;
    map<string, string> currentPreset;
    vector<string> banks;
    int currentBank;

    enum class PopupRequest { None, NewBank, SaveAs, Delete, ReviewSave, SaveResult };
    PopupRequest popupRequest = PopupRequest::None;
    char bankNameBuffer[256] = {};
    char presetNameBuffer[256] = {};
    int saveAsBank = 0;
    string deleteBankName;
    string deletePresetName;
    struct PendingSave {
        string name;
        string bank;
        bool createNew = false;
        bool writing = false;
        size_t reviewIndex = 0;
        size_t nextWriteIndex = 0;
        string warning;
        vector<GlobalMacroSaveReview> reviews;
        vector<bool> saveChoices;
        vector<string> savedMacros;
        vector<string> skippedMacros;
    };
    std::unique_ptr<PendingSave> pendingSave;
    string saveResultText;

    bool newPresetCreated;
    bool newPresetRequested = false;
    int loadPresetInNextUpdate;
    
    ofEventListener presetListener;
    ofEventListener presetNumListener;
    ofEventListener saveCurrentPresetListener;
    
    shared_ptr<ofxOceanodeContainer> container;
};

#endif /* ofxOceanodePresetsController_h */
