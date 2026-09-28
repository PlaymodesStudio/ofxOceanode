#pragma once

#include <string>
#include <vector>

class ofxOceanodeContainer;
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
