//
//  MacroPresetManager.h
//  ofxOceanode
//
//  Extracted from ofxOceanodeNodeMacro — Phase 5 refactoring.
//  Owns preset/category management state and pure utility methods.
//

#ifndef MacroPresetManager_h
#define MacroPresetManager_h

#include <string>
#include <deque>
#include <vector>
#include <memory>
#include "ofParameter.h"
#include "ofxOceanodeParameter.h"
#include "ofxOceanodeShared.h"

class MacroPresetManager {
public:
    MacroPresetManager();

    // ── Local/global preset flag ────────────────────────────────────────────
    bool isLocal() const { return localPreset; }
    void setLocal(bool local) { localPreset = local; }
    bool& getLocalRef() { return localPreset; }

    // ── Preset path (local path for future snapshot saving) ─────────────────
    const std::string& getPresetPath() const { return presetPath; }
    void setPresetPath(const std::string& path) { presetPath = path; }

    // ── Next preset path (queued for loading in update()) ───────────────────
    const std::string& getNextPresetPath() const { return nextPresetPath; }
    void setNextPresetPath(const std::string& path) { nextPresetPath = path; }
    void clearNextPresetPath() { nextPresetPath = ""; }

    // ── Current macro name ──────────────────────────────────────────────────
    const std::string& getCurrentMacro() const { return currentMacro; }
    void setCurrentMacro(const std::string& name) { currentMacro = name; }

    // ── Current macro path ──────────────────────────────────────────────────
    const std::string& getCurrentMacroPath() const { return currentMacroPath; }
    void setCurrentMacroPath(const std::string& path) { currentMacroPath = path; }

    // ── Inner preset loading path ───────────────────────────────────────────
    const std::string& getInnerPresetLoadingPath() const { return innerPresetLoadingPath; }
    void setInnerPresetLoadingPath(const std::string& path) { innerPresetLoadingPath = path; }

    // ── Category state ──────────────────────────────────────────────────────
    std::deque<std::string>& getCurrentCategory() { return currentCategory; }
    const std::deque<std::string>& getCurrentCategory() const { return currentCategory; }

    std::shared_ptr<macroCategory>& getCurrentCategoryMacro() { return currentCategoryMacro; }
    const std::shared_ptr<macroCategory>& getCurrentCategoryMacro() const { return currentCategoryMacro; }
    void setCurrentCategoryMacro(std::shared_ptr<macroCategory> cat) { currentCategoryMacro = cat; }

    std::deque<std::string>& getSaveAsTempCategory() { return saveAsTempCategory; }

    // ── Logic ───────────────────────────────────────────────────────────────
    /// Parse a file path to populate currentCategory, currentMacro, currentCategoryMacro.
    void updateCategoryFromPath(const std::string& path);

    /// Load a macro by index within the current category; sets nextPresetPath
    /// and currentMacroPath/currentMacro for deferred loading in update().
    void loadMacroInsideCategory(int newPresetIndex);

    // ── Bank/preset state (possibly legacy, moved to reduce clutter) ────────
    ofParameter<int>& getBank() { return bank; }
    int& getPreviousBank() { return previousBank; }
    std::shared_ptr<ofxOceanodeParameter<int>>& getBankDropdown() { return bankDropdown; }
    std::vector<std::string>& getBankNames() { return bankNames; }
    ofParameter<int>& getPreset() { return preset; }
    std::shared_ptr<ofxOceanodeParameter<int>>& getPresetDropdown() { return presetDropdown; }
    int& getCurrentPreset() { return currentPreset; }
    ofParameter<std::string>& getSavePresetField() { return savePresetField; }
    std::vector<std::string>& getPresetsInBank() { return presetsInBank; }
    ofParameter<std::string>& getPresetName() { return presetName; }
    ofParameter<bool>& getSavePreset() { return savePreset; }

private:
    bool localPreset;
    std::string presetPath;
    std::string nextPresetPath;
    std::string currentMacro;
    std::string currentMacroPath;
    std::string innerPresetLoadingPath;
    std::deque<std::string> currentCategory;
    std::shared_ptr<macroCategory> currentCategoryMacro;
    std::deque<std::string> saveAsTempCategory;

    // Bank/preset parameters (possibly legacy)
    ofParameter<int> bank;
    int previousBank;
    std::shared_ptr<ofxOceanodeParameter<int>> bankDropdown;
    std::vector<std::string> bankNames;
    ofParameter<int> preset;
    std::shared_ptr<ofxOceanodeParameter<int>> presetDropdown;
    int currentPreset;
    ofParameter<std::string> savePresetField;
    std::vector<std::string> presetsInBank;
    ofParameter<std::string> presetName;
    ofParameter<bool> savePreset;
};

#endif /* MacroPresetManager_h */
