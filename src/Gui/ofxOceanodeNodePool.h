#pragma once

#include "ofxOceanodeContainer.h"
#include "ofxOceanodeCanvas.h"

// A metadata-only browser: browsing never creates or sets up live models.
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
    void open();
    void drawTab(bool macro);
    void add(const Entry& entry, bool macro);
    bool findTarget(ofxOceanodeContainer* owner, ofxOceanodeCanvas* canvas,
                    ofxOceanodeContainer*& target, ofxOceanodeCanvas*& targetCanvas);
};
