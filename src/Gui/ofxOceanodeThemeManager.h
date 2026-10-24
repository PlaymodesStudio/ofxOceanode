#pragma once

#include "imgui.h"
#include "ofxOceanodeThemeColorPicker.h"
#include <string>
#include <vector>

class ofxOceanodeCanvas;

// Owns the theme: colours, the font family, the Theme Editor with its Alt+hover
// colour lookup, and config/themes/<name>.json with the default in config/themes.json.
class ofxOceanodeThemeManager {
public:
    void setup(ofxOceanodeCanvas& canvas); // refreshThemeFonts(); load default from config/themes.json
    void beforeFrame();                    // font atlas changes, before NewFrame
    void afterRender();                    // Alt+hover lookup, needs this frame's rendered draw data
    void drawMenuSection();                // the Theme section of the Config menu
    void drawPopups();                     // "Save Theme##modal"
    void drawWindows();                    // editor window + pick tooltip when open
    void save(const std::string& name);
    void load(const std::string& name);

private:
    ofxOceanodeCanvas* canvas = nullptr;

    // Theme editor state
    bool showThemeEditor = false;
    bool showThemeColorSearch = false;
    char themeColorSearch[128] = "";
    void drawThemeEditorWindow();
    // Alt+hover lookup: while the editor is open, holding Alt over any GUI
    // element selects the theme colour that drew it (see ofxOceanodeThemeColorPicker.h).
    std::vector<ofxOceanodeThemeColorPicker::Layer> themeColorPickLayers; // last non-empty lookup, topmost first
    int themeColorPickFrame = -1;
    const ImDrawList* themeColorPickTooltipDrawList = nullptr;
    bool themeEditorHovered = false;
    ofxOceanodeThemeColorPicker::Match themeSelectedColor;
    bool themeColorRevealPending = false; // switch tab and scroll to themeSelectedColor
    void selectThemeColor(const ofxOceanodeThemeColorPicker::Match& match);
    void pickThemeColorUnderMouse();
    void drawThemeColorPickTooltip();
    void drawThemeColorPickLayers(bool selectable);
    struct ThemeFontFamily {
        std::string name;
        std::string regularPath;
        std::string boldPath;
    };
    std::vector<ThemeFontFamily> themeFontFamilies;
    std::string themeFontFamily; // Empty selects the built-in JetBrains Mono pair.
    std::string themeFontWarning;
    bool pendingThemeFontChange = true;
    void refreshThemeFonts();
    void applyThemeFont();

    // Theme save/load state
    std::string currentThemeName;
    std::string defaultThemeName;          // config/themes.json
    bool openSaveThemePopup = false;
    char themeNameBuf[128] = {};
    std::string themeSaveError;
    void setDefaultTheme(const std::string& name);
};
