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
#include <deque>
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
    bool presetNameCursorPending = false;
    // Save As starts from this name; New Preset keeps it.
    string lastPresetName;
    int saveAsBank = 0;
    string deleteBankName;
    string deletePresetName;
    // Save As writes the macro to a new library folder and points that
    // instance at it, leaving the original folder and other instances alone.
    enum class MacroSaveChoice { Skip, Save, SaveAs };
    struct PendingSave {
        string name;
        string bank;
        bool createNew = false;
        bool writing = false;
        size_t reviewIndex = 0;
        size_t nextWriteIndex = 0;
        string warning;
        vector<GlobalMacroSaveReview> reviews;
        vector<MacroSaveChoice> saveChoices;
        vector<string> saveAsPaths;
        vector<string> savedMacros;
        vector<string> savedAsMacros;
        vector<string> skippedMacros;
        // Save As form for the macro under review.
        bool editingSaveAs = false;
        bool saveAsFocusPending = false;
        bool saveAsCursorPending = false;
        bool revealSaveAsFolder = false;
        char saveAsName[256] = {};
        std::deque<string> saveAsFolder;
        string saveAsError;
    };
    std::unique_ptr<PendingSave> pendingSave;
    // Returns true once a new macro name and folder are confirmed.
    bool drawGlobalMacroSaveAsForm(PendingSave& request, const GlobalMacroSaveReview& review);
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
