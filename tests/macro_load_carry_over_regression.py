#!/usr/bin/env python3
"""Native Macro Load options: Reuse connections and Keep values.

Loads library Macros and a preset into a Macro that has outer cables and edited
input values, with real OF and ASan/UBSan. Uses the headless Macro fixture.
"""
from pathlib import Path
import tempfile
import macro_mixer_regression as native

CASES = r'''
static auto& addRouter(ofxOceanodeContainer& c, int id, const string& name, float value) {
    auto& model = c.createNode(std::make_unique<router<float>>("f",0.f,-100.f,100.f), id).getNodeModel();
    model.getParameter<string>("Name") = name; model.getParameter<float>("Value") = value;
    return model;
}
static std::set<string> cables(ofxOceanodeContainer& c) {
    std::set<string> result;
    for(auto& connection : c.getAllConnections())
        result.insert(connection->getSourceParameter().getGroupHierarchyNames()[0] + "." + connection->getSourceParameter().getName() + ">" +
                      connection->getSinkParameter().getGroupHierarchyNames()[0] + "." + connection->getSinkParameter().getName());
    return result;
}
int main(int argc, char** argv) {
    assert(argc == 3);
    namespace fs = std::filesystem;
    fs::current_path(argv[2]); ofDisableDataPath(); ofSeedRandom();
    auto window = std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry = std::make_shared<ofxOceanodeNodeRegistry>();
    auto types = std::make_shared<ofxOceanodeTypesRegistry>();
    auto singleton = ofxOceanodeTypesRegistry::getInstance();
    types->registerType<float>("f"); singleton->registerType<float>("f");
    types->registerType<int>("i"); singleton->registerType<int>("i");
    registry->registerModel<router<float>>("Router","f",0.f,-100.f,100.f);
    registry->registerModel<router<int>>("Router","i",0,INT_MIN,INT_MAX);
    registry->registerModel<ofxOceanodeNodeMacro>("Macro");
    auto transport = std::make_shared<ofxOceanodeTransport>();
    ofEventArgs event;

    // Old: Gain -> Out, plus inputs Trim (float) and Mode (int).
    {
        ofxOceanodeContainer c(registry, types, transport); c.setCanvasID("Old");
        addRouter(c, 1, "Gain", .2f); addRouter(c, 2, "Out", 0.f); addRouter(c, 3, "Trim", .3f);
        auto& mode = c.createNode(std::make_unique<router<int>>("i",0,INT_MIN,INT_MAX), 4).getNodeModel();
        mode.getParameter<string>("Name") = "Mode"; mode.getParameter<int>("Value") = 1;
        assert(c.createConnectionFromInfo("Router_f_1","Value","Router_f_2","Value"));
        assert(c.savePreset("Macros/Lib/Old"));
    }
    // New reuses router ids under other names: id 1 is "Other", id 2 is "Gain".
    // Mode is a float here, so its old int value does not carry over.
    for(const string path : {"Macros/Lib/New", "Presets/Bank/01--New"}) {
        ofxOceanodeContainer c(registry, types, transport); c.setCanvasID("New");
        addRouter(c, 1, "Other", .6f); addRouter(c, 2, "Gain", .9f); addRouter(c, 3, "Out", 0.f);
        addRouter(c, 4, "Trim", .1f); addRouter(c, 5, "Mode", .7f);
        assert(c.createConnectionFromInfo("Router_f_2","Value","Router_f_3","Value"));
        assert(c.savePreset(path));
    }
    ofxOceanodeShared::updateMacrosStructure();

    ofxOceanodeContainer parent(registry, types, transport); parent.setCanvasID("Parent");
    auto& wrapper = parent.createNode(std::make_unique<ofxOceanodeNodeMacro>(), 1, false, "Macros/Lib/Old");
    auto& macro = static_cast<ofxOceanodeNodeMacro&>(wrapper.getNodeModel());
    addRouter(parent, 7, "Source", .5f); addRouter(parent, 8, "Sink", 0.f);
    auto connectOld = [&]() {
        assert(parent.createConnectionFromInfo("Router_f_7","Value","Macro_1","Gain"));
        assert(parent.createConnectionFromInfo("Macro_1","Out","Router_f_8","Value"));
    };
    connectOld();
    macro.getParameter<float>("Trim") = .44f;
    auto load = [&](const string& path, bool isPreset, bool reuse, bool keep) {
        macro.guiState.loadReuseConnections = reuse; macro.guiState.loadKeepValues = keep;
        macro.requestMacroLoad(path, isPreset); macro.update(event);
        assert(!macro.pendingLoadCarryOver);
    };
    const std::set<string> reconnected = {"Router_f_7.Value>Macro_1.Gain", "Macro_1.Out>Router_f_8.Value"};

    // Both options: cables follow pin names, not router ids. New reuses ids 1-3
    // under other names, so each pin gets its saved name only from empty contents.
    load("Macros/Lib/New", false, true, true);
    assert(macro.getCurrentMacroName() == "New");
    for(const string name : {"Other", "Gain", "Out", "Trim", "Mode"}) assert(macro.getParameterGroup().contains(name));
    assert(!macro.getParameterGroup().contains("_Trim"));
    assert(cables(parent) == reconnected && parent.getAllConnections().size() == 2);
    assert(macro.getParameter<float>("Gain") == .5f);   // Driven by the reused cable.
    assert(macro.getParameter<float>("Trim") == .44f);  // Kept edited value.
    assert(macro.getParameter<float>("Mode") == .7f);   // Type changed: new value.
    assert(macro.getParameter<float>("Other") == .6f);

    // Neither option: every old cable is dropped and the saved values load.
    load("Macros/Lib/Old", false, false, false);
    assert(parent.getAllConnections().empty());
    assert(macro.getParameter<float>("Trim") == .3f && macro.getParameter<int>("Mode") == 1);

    // Presets load through the same options; values off keeps the preset's values.
    connectOld(); macro.getParameter<float>("Trim") = .55f;
    load("Presets/Bank/01--New", true, true, false);
    assert(macro.isLocal() && cables(parent) == reconnected && parent.getAllConnections().size() == 2);
    assert(macro.getParameter<float>("Trim") == .1f);

    // A failed load leaves nothing pending for a later load.
    macro.guiState.loadReuseConnections = false;
    macro.requestMacroLoad("Macros/Lib/Missing", false); macro.update(event);
    assert(!macro.pendingLoadCarryOver && cables(parent) == reconnected);
    std::cout << "Macro load carry-over regression passed: cables reused by pin name, values kept by name and type, both off drop cables and keep saved values, preset loads and failed loads.\n";
}
'''

def main():
    # Expose private members only in the fixture TU so production files retain their API.
    with tempfile.TemporaryDirectory(prefix="macro-load-access-") as tmp:
        access = Path(tmp) / "MacroLoadTestAccess.h"
        access.write_text((native.ROOT / "src/Nodes/ofxOceanodeNodeMacro.h").read_text().replace("private:", "public:"))
        native.PRELUDE = '#include "' + str(access) + '"\n' + native.PRELUDE
        native.CASES = CASES
        native.main()

if __name__ == "__main__":
    main()
