//
//  MacroPresetManager.cpp
//  ofxOceanode
//
//  Extracted from ofxOceanodeNodeMacro — Phase 5 refactoring.
//  Owns preset/category management state and pure utility methods.
//

#include "MacroPresetManager.h"
#include "ofxOceanodeShared.h"
#include "ofUtils.h"

MacroPresetManager::MacroPresetManager()
    : localPreset(true)
    , presetPath("")
    , nextPresetPath("")
    , currentMacro("")
    , currentMacroPath("")
    , innerPresetLoadingPath("")
    , previousBank(0)
    , currentPreset(-1)
{
}

void MacroPresetManager::updateCategoryFromPath(const std::string& path) {
#ifdef TARGET_WIN32
    std::vector<std::string> splittedInfo = ofSplitString(path, "\\");
#else
    std::vector<std::string> splittedInfo = ofSplitString(path, "/");
#endif
    currentCategory.clear();
    currentMacro = splittedInfo.back();
    for(int i = splittedInfo.size() - 2; i >= 0; i--){
        if(splittedInfo[i] != "Macros" && splittedInfo[i] != "data") {
            currentCategory.push_front(splittedInfo[i]);
        } else {
            break;
        }
    }

    auto macroDirectoryStructure = ofxOceanodeShared::getMacroDirectoryStructure();
    for(int i = 0; i < currentCategory.size(); i++){
        std::string categoryNameToCompare = currentCategory[i];
        macroDirectoryStructure = *std::find_if(macroDirectoryStructure->categories.begin(), macroDirectoryStructure->categories.end(),
                                                [categoryNameToCompare](std::shared_ptr<macroCategory> &mc){ return mc->name == categoryNameToCompare; });
    }
    currentCategoryMacro = macroDirectoryStructure;
}

void MacroPresetManager::loadMacroInsideCategory(int newPresetIndex) {
    if(newPresetIndex < currentCategoryMacro->macros.size() && currentCategoryMacro->macros[newPresetIndex].first != currentMacro){
        nextPresetPath = currentCategoryMacro->macros[newPresetIndex].second;
        currentMacroPath = nextPresetPath;
        currentMacro = currentCategoryMacro->macros[newPresetIndex].first;
    }
}
