#pragma once

#include "ofxOceanodeContainer.h"
#include "ofxOceanodeCanvas.h"

// Browser with a private, paused node instance for the canvas preview.
class ofxOceanodeNodePool {
public:
    ofxOceanodeNodePool(std::shared_ptr<ofxOceanodeContainer> root, ofxOceanodeCanvas* canvas);
    void draw();
    void drawMenu();
private:
    struct Entry {
        std::string name, category, path;
    };
    struct Tab {
        char search[256] = {};
        std::string selected;
    };
    std::shared_ptr<ofxOceanodeContainer> root;
    ofxOceanodeCanvas* rootCanvas;
    bool visible = false, focusRequested = false;
    Tab nodes, macros;
    std::string error, macroDescription, descriptionPath;
    std::shared_ptr<ofxOceanodeContainer> previewOwner;
    ofxOceanodeNode* previewNode = nullptr;
    std::string previewKey, previewError;
    void resetPreview();
    void drawPreview(const Entry& entry, bool macro);
    void open();
    void drawTab(bool macro);
    void add(const Entry& entry, bool macro);
    bool findTarget(ofxOceanodeContainer* owner, ofxOceanodeCanvas* canvas,
                    ofxOceanodeContainer*& target, ofxOceanodeCanvas*& targetCanvas);
};
