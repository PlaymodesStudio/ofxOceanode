#pragma once

#include <string>
#include <vector>

class ofxOceanodeContainer;
class ofxOceanodeNode;
class ofxOceanodeNodeMacro;

struct GlobalMacroSaveChange {
    enum class Kind { Value, MacroReplacement };

    std::string location;
    std::string item;
    std::string field;
    std::string saved;
    std::string current;
    std::string savedFull;
    std::string currentFull;
    Kind kind = Kind::Value;
};

struct GlobalMacroSaveReview {
    ofxOceanodeNodeMacro* instance = nullptr;
    std::string name;
    std::string globalPath;
    std::string instancePath;
    std::vector<std::string> alsoUsedAt;
    std::vector<GlobalMacroSaveChange> changes;
};

// Reads the global macro folders but never writes to them.
std::vector<GlobalMacroSaveReview> collectGlobalMacroSaveReviews(ofxOceanodeContainer& root);

// Compares one global Macro node, owned by `owner`, with its library folder.
// A local Macro returns no changes; nested global Macros are not included.
GlobalMacroSaveReview collectGlobalMacroSaveReview(ofxOceanodeContainer& owner, ofxOceanodeNode& node);

// Draws the review's change list inside the current ImGui window.
void drawGlobalMacroSaveChanges(const GlobalMacroSaveReview& review, float width);
