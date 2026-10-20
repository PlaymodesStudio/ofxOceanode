#pragma once

#include "ofxOceanodeAIAssistant.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeControls.h"

// Owns the searchable reference and the main-thread application of AI proposals.
// No model is instantiated merely to search the catalog.
class ofxOceanodeHelp {
public:
    ofxOceanodeHelp(std::shared_ptr<ofxOceanodeContainer> root);
    void update();
    void draw();
    void drawMenu();
    void drawWindowMenu(ofxOceanodeControls& controls);
private:
    std::shared_ptr<ofxOceanodeContainer> root;
    ofxOceanodeAIAssistant assistant;
    bool visible = false;
    std::map<std::string, bool> pendingControllerVisibility;
    bool openAssistant = false;
    int requestedTab = -1;
    bool patchRequested = false;
    char keywords[256] = {};
    std::string answer, error, targetCanvas;
    std::weak_ptr<ofxOceanodeContainer> targetOwner;
    std::uint64_t targetRevision = 0;
    ofJson proposal;
    std::string buildPrompt(const std::string& request, bool patch);
    std::string acceptResponse(const std::string& response);
    void applyProposal();
};
