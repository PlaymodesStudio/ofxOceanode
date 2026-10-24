#pragma once

#include "ofJson.h"
#include "ofEvents.h"
#include <map>
#include <string>

class ofxOceanodeWindowManager;

// Preset-owned, explicit snapshots. Saving a preset never overwrites a named
// snapshot; the user chooses Update when a new arrangement should replace it.
// Each entry is {"ini": <ImGui ini text>, "view": <captured window visibility>}.
class ofxOceanodePresetLayouts {
public:
    using Json = ofJson;
    std::map<std::string, Json> entries;
    std::string active;
    std::string defaultLayout;

    std::string layoutOnLoad() const { return entries.count(defaultLayout) ? defaultLayout : active; }

    static std::string cleanName(const std::string& name);
    static bool validName(const std::string& name);
    void create(const std::string& name, Json snapshot);
    void overwrite(const std::string& name, Json snapshot);
    void rename(const std::string& from, const std::string& to);
    void erase(const std::string& name);
    Json toJson() const;              // {"version": 1, "layouts", "active", "default"}
    void fromJson(const Json& json);  // throws on an invalid collection, leaving this unchanged
};

// Owns global layouts (config/guiLayouts/*.ini, config/guiLayouts.json) and the
// preset's layout.json: current docking and visibility plus named snapshots.
class ofxOceanodeLayouts {
public:
    explicit ofxOceanodeLayouts(ofxOceanodeWindowManager& windows);
    void setup();                          // loads config/guiLayouts.json and queues the default global layout
    void onPresetLoaded();                 // reads <preset>/layout.json, queues application
    void onPresetSaved();                  // writes the whole layout.json, current.view = live view
    void saveDocking(const std::string& presetPath); // writes layout.json with live ini, keeps current.view
    void beforeFrame();                    // applies queued ini / snapshot / view (before ImGui::NewFrame)
    void drawMenu();                       // Layouts menu
    void drawPopups();

private:
    ofxOceanodeWindowManager& windows;
    ofEventListener dockingSaveListener;

    // Named snapshots and the view saved with the current preset.
    ofxOceanodePresetLayouts collection;
    ofJson savedView;                      // current.view, null when the preset has none
    std::string layoutError;
    bool readFailed = false;               // never write layout.json until the next preset load

    // Requests applied before NewFrame, in this order.
    std::string pendingIni;
    std::string pendingSnapshot;
    ofJson pendingView;

    enum class PresetLayoutPopup { None, Save, Overwrite, Rename, Delete };
    PresetLayoutPopup presetPopup = PresetLayoutPopup::None;
    std::string presetTarget;
    char presetName[256] = {};
    static const char* presetLayoutPopupTitle(PresetLayoutPopup popup);

    // Global layouts: config/guiLayouts/<name>.ini, default in config/guiLayouts.json.
    std::string globalLayoutName;
    std::string globalDefault;
    bool openSaveGlobalLayoutPopup = false;
    char globalName[256] = {};
    std::string globalError;
    static bool validGlobalName(const std::string& name);

    ofJson capturePresetLayout();
    void recallPresetLayout(const std::string& name);
    bool writeLayoutFile();
    void queueGlobalLayout(const std::string& name);
    void setGlobalDefault(const std::string& name);
    void drawPresetLayoutsMenu();
    void drawPresetLayoutPopup();
    void drawGlobalLayoutPopup();
};
