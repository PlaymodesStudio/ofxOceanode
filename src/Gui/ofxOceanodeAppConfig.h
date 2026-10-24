#pragma once

class ofxOceanodeCanvas;
class ofxOceanodeControls;
class ofxOceanodeWindowManager;

// Owns config/config.json: rendering and editor preferences. Load applies only
// the keys present; Save writes every key from its live target.
class ofxOceanodeAppConfig {
public:
    void setup(ofxOceanodeCanvas& canvas, ofxOceanodeControls& controls, ofxOceanodeWindowManager& windows);
    void load();               // config/config.json -> targets, also called by setup()
    void save();               // targets -> config/config.json
    void drawMenuSections();   // Config menu sections "Rendering" and "Editor"
    void drawConfigButtons();  // Config menu section "Configuration": Save Config / Load Config

private:
    ofxOceanodeCanvas* canvas = nullptr;
    ofxOceanodeControls* controls = nullptr;
    ofxOceanodeWindowManager* windows = nullptr;
    int desiredFPS = 60;
    bool vsync = true;         // openFrameworks' GLFW window enables vertical sync on setup
};
