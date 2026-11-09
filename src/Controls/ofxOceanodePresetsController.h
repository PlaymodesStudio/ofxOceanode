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
    // Interactive root loads and host quit actions share the same save guard.
    void requestLoadPreset(const string& path);
    void requestDestructiveAction(std::function<void()> action);
    bool hasUnsavedChanges();
    void rememberSavedState();
    
private:
    void newPreset();
    void reloadPresetList();
    void drawPresetList();
    
    // Presets are addressed by their position in bankPresets[bank], which is
    // the number in the folder name. Two presets may share a name.
    void loadPreset(int index, const string& bank);
    void savePreset(int index, const string& bank);
    // A negative index creates a new preset called name; otherwise the
    // preset at index is overwritten and name is ignored.
    void beginSavePreset(string name, string bank, int index);
    void finishSavePreset();
    vector<int> saveAsReplacementTargets(const string& bank, const string& name) const;
    void drawGlobalMacroSaveReview();
    void deletePreset(int index, const string& bank);
    // Absolute folder of a listed preset: Presets/<bank>/<index>--<name>.
    // Empty when the bank or preset is not listed.
    string presetFolderPath(int index, const string& bankName) const;
    // Empty when the bank or preset is not listed.
    string presetNameAt(int index, const string& bankName) const;
    
    map<string, vector<string>> bankPresets;
    // Index of the current preset of each bank, -1 for none.
    map<string, int> currentPreset;
    vector<string> banks;
    int currentBank = -1;

    enum class PopupRequest { None, NewBank, SaveAs, Delete, ReviewSave, SaveResult, UnsavedChanges };
    PopupRequest popupRequest = PopupRequest::None;
    char bankNameBuffer[256] = {};
    char presetNameBuffer[256] = {};
    bool presetNameCursorPending = false;
    // Save As starts from this name; New Preset keeps it.
    string lastPresetName;
    int saveAsBank = 0;
    int saveAsReplacementIndex = -1;
    string replacementBank, replacementName;
    int replacementIndex = -1;
    string deleteBankName;
    string deletePresetName;
    int deletePresetIndex = -1;
    // Save As writes the macro to a new library folder and points that
    // instance at it, leaving the original folder and other instances alone.
    enum class MacroSaveChoice { Skip, Save, SaveAs };
    struct PendingSave {
        string name;
        string bank;
        bool createNew = false;
        int index = -1; // Position in bankPresets[bank] when overwriting.
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
    // Tells the other instances of each global macro written so far to reload.
    void broadcastSavedMacros(PendingSave& request);
    string saveResultText;

    ofJson savedEditState;
    std::function<void()> pendingAction;
    bool actionApproved = false;
    bool savingBeforeAction = false;
    bool bypassSaveGuard = false;
    void cancelPendingAction();
    ofEventListener presetLoadedListener, presetSavedListener;

    bool newPresetCreated;
    bool newPresetRequested = false;
    int loadPresetInNextUpdate;
    
    ofEventListener presetListener;
    ofEventListener presetNumListener;
    ofEventListener saveCurrentPresetListener;
    
    shared_ptr<ofxOceanodeContainer> container;
};

#endif /* ofxOceanodePresetsController_h */
