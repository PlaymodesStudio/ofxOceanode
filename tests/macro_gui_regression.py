#!/usr/bin/env python3
"""Native Macro GUI/persistence checks with real OF, ImGui and ASan/UBSan.

Uses the existing headless Macro fixture. Renders the new controls in an ImGui
context; no host window, input backend or docking integration is exercised.
"""
from pathlib import Path
import tempfile
import macro_mixer_regression as native
from new_preset_regression import block

CASES = r'''
#include "imgui_internal.h"
static void write(const std::string& path, const ofJson& value) { std::ofstream(path) << value; }
static float value(ofxOceanodeNodeMacro& macro) {
    auto* node = macro.getContainer()->getAllModules().front();
    return node->getNodeModel().getParameter<float>("Value").get();
}
int main(int argc, char** argv) {
    assert(argc == 3);
    namespace fs = std::filesystem;
    fs::current_path(argv[2]); ofDisableDataPath(); ofSeedRandom();
    auto window = std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry = std::make_shared<ofxOceanodeNodeRegistry>();
    auto types = std::make_shared<ofxOceanodeTypesRegistry>();
    types->registerType<float>("f"); ofxOceanodeTypesRegistry::getInstance()->registerType<float>("f");
    registry->registerModel<simpleNumberGenerator>("Generators");
    registry->registerModel<ofxOceanodeNodeMacro>("Macro");
    auto transport = std::make_shared<ofxOceanodeTransport>();
    auto saveSource = [&](const string& path, float number) {
        ofxOceanodeContainer source(registry, types, transport); source.setCanvasID("Source");
        auto& node = source.createNode(std::make_unique<simpleNumberGenerator>(), 1);
        node.getNodeModel().getParameter<float>("Value") = number;
        source.savePreset(path);
    };
    saveSource("Presets/Bank/01--Whole_Preset", .25f);
    write("Presets/Bank/01--Whole_Preset/description.json", {{"Description", "Original preset"}});
    saveSource("Macros/Folder/A", .4f);
    saveSource("Macros/Folder/B", .8f);
    fs::create_directories("Macros/Folder/Empty");
    // Files alongside subfolders must not turn a library category into a Macro.
    write("Macros/Folder/notes.json", {{"note", "category metadata"}});
    write("Macros/Folder/A/description.json", {{"Description", "Macro A description"}});
    ofxOceanodeShared::updateMacrosStructure();
    auto structure = ofxOceanodeShared::getMacroDirectoryStructure();
    assert(structure->categories.size() == 1);
    assert(structure->categories.front()->macros.size() == 2);
    assert(structure->categories.front()->categories.size() == 1);
    ofxOceanodeContainer parent(registry, types, transport); parent.setCanvasID("Parent");
    auto& node = parent.createNode(std::make_unique<ofxOceanodeNodeMacro>(), 1);
    auto& macro = static_cast<ofxOceanodeNodeMacro&>(node.getNodeModel());
    assert(macro.isLocal() && macro.getLocalMacroName().empty());
    assert(macro.getParameterGroup().contains("Active"));
    assert(macro.getParameterGroup().contains("Preset Control Gui"));
    // Import the entire router-free preset, with independent working storage.
    assert(macro.importPresetContents("Presets/Bank/01--Whole_Preset"));
    assert(macro.isLocal() && macro.getCurrentMacroPath().empty());
    assert(macro.getLocalMacroName() == "Whole_Preset");
    assert(macro.getContainer()->getAllModules().size() == 1 && value(macro) == .25f);
    assert(macro.getCanvas()->getLayoutIniPath().find("Presets/Bank") == string::npos);
    macro.getContainer()->getAllModules().front()->getNodeModel().getParameter<float>("Value") = .6f;
    macro.localName = "Local %s";
    macro.macroDescription = "Description 100%";
    // Cache the parent location as a native load would; the fixture omits custom GUI storage.
    parent.savePreset("SavedParent");
    fs::remove("SavedParent/modules.json");
    assert(macro.saveLocalDefinition());
    assert(macro.isLocal());
    const auto saved = ofLoadJson("SavedParent/Macro_1.json");
    assert(saved.at("Local_Name") == "Local %s");
    assert(saved.at("Description") == "Description 100%");
    assert(saved.at("LocalPreset") == true);
    assert(ofLoadJson("SavedParent/Macro_1/description.json").at("Description") == "Description 100%");
    assert(ofLoadJson("Presets/Bank/01--Whole_Preset/Number_1.json").at("Value") == "0.25");
    assert(!fs::exists("Presets/Bank/01--Whole_Preset/Macro_1.json"));
    assert(!fs::exists("SavedParent/modules.json")); // No whole-parent save.
    macro.storeRouterSnapshot(0);
    assert(fs::exists("SavedParent/Macro_1/snapshots.json"));
    macro.snapshotSystem.clearAll();
    assert(macro.saveLocalDefinition());
    assert(!fs::exists("SavedParent/Macro_1/snapshots.json"));
    // Recalling the saved local Macro restores contents and Inspector metadata.
    ofxOceanodeContainer recalled(registry, types, transport); recalled.setCanvasID("Recalled");
    auto& recalledNode = recalled.createNode(std::make_unique<ofxOceanodeNodeMacro>(), 1);
    recalledNode.loadPresetBeforeConnections("SavedParent"); recalledNode.loadPreset("SavedParent");
    auto& copy = static_cast<ofxOceanodeNodeMacro&>(recalledNode.getNodeModel());
    assert(copy.isLocal() && copy.getLocalMacroName() == "Local %s" && value(copy) == .6f);
    assert(copy.getMacroDescription() == "Description 100%");
    // All four function-parameter input types still select a library Macro by index.
    ofEventArgs event;
    macro.presetManager.setNextPresetPath("Macros/Folder/A"); macro.update(event);
    assert(!macro.isLocal() && value(macro) == .4f);
    auto& function = static_cast<ofxOceanodeAbstractParameter&>(macro.getParameterGroup().get("Preset Control Gui"));
    ofParameter<int> index("Index", 1); assert(function.receiveParameter(&index)); macro.update(event);
    assert(value(macro) == .8f);
    ofParameter<float> floatIndex("Index", 0.f); assert(function.receiveParameter(&floatIndex)); macro.update(event);
    assert(value(macro) == .4f);
    ofParameter<vector<int>> intVector("Index", {1}); assert(function.receiveParameter(&intVector)); macro.update(event);
    assert(value(macro) == .8f);
    ofParameter<vector<float>> floatVector("Index", {0.f}); assert(function.receiveParameter(&floatVector)); macro.update(event);
    assert(value(macro) == .4f);
    ofParameter<vector<int>> emptyInts("Empty", {}); assert(function.receiveParameter(&emptyInts));
    ofParameter<vector<float>> emptyFloats("Empty", {}); assert(function.receiveParameter(&emptyFloats));
    macro.update(event); assert(value(macro) == .4f);
    index = -1; assert(function.receiveParameter(&index)); macro.update(event); assert(value(macro) == .4f);
    index = 999; assert(function.receiveParameter(&index)); macro.update(event); assert(value(macro) == .4f);
    // A global save preserves description and snapshots and updates other instances.
    auto& instanceNode = parent.createNode(std::make_unique<ofxOceanodeNodeMacro>(), 2);
    auto& instance = static_cast<ofxOceanodeNodeMacro&>(instanceNode.getNodeModel());
    instance.presetManager.setNextPresetPath("Macros/Folder/A"); instance.update(event);
    macro.getContainer()->getAllModules().front()->getNodeModel().getParameter<float>("Value") = .7f;
    macro.macroDescription = "Updated library description";
    macro.storeRouterSnapshot(0);
    assert(macro.saveGlobalDefinition());
    assert(instance.snapshotSystem.hasSnapshot(0));
    assert(value(instance) == .7f && instance.getMacroDescription() == "Updated library description");
    // Save As writes the full definition without prematurely changing identity.
    assert(copy.saveLibraryDefinition("Macros/NewDefinition"));
    assert(copy.isLocal());
    assert(fs::exists("Macros/NewDefinition/modules.json"));
    assert(ofLoadJson("Macros/NewDefinition/description.json").at("Description") == "Description 100%");
    // Canvas nodes share a window: test distant origins and long names, not
    // just a node drawn at the window padding. Width must equal the default.
    auto* context = ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = ImVec2(2400, 1200);
    unsigned char* pixels; int fontWidth, fontHeight; io.Fonts->GetTexDataAsRGBA32(&pixels, &fontWidth, &fontHeight);
    const string originalName = macro.presetManager.getCurrentMacro();
    macro.presetManager.setCurrentMacro(string(500, 'W'));
    for(float zoom : {.75f, 1.f, 1.75f}) {
        ofxOceanodeShared::setZoomLevel(zoom);
        for(float origin : {40.f, 400.f, 1400.f}) {
            for(int frame = 0; frame < 3; ++frame) {
                io.DeltaTime = 1.f / 60; ImGui::NewFrame();
                ImGui::SetNextWindowSize(ImVec2(2300, 1000));
                ImGui::Begin("Macro GUI test");
                ImGui::SetWindowFontScale(zoom);
                ImGui::SetCursorScreenPos(ImVec2(origin, 140));
                ImGui::BeginGroup();
                ImGui::PushID("Active");
                assert(macro.drawNodeParameter(macro.getOceanodeParameter(macro.active), 90 * zoom, 150 * zoom));
                assert(std::abs(ImGui::GetItemRectSize().x - 240 * zoom) < .1f);
                ImGui::PopID();
                ImGui::PushID("Preset Control Gui"); macro.renderPresetControlGui(); ImGui::PopID();
                assert(std::abs(ImGui::GetItemRectSize().x - 240 * zoom) < .1f);
                ImGui::EndGroup();
                assert(std::abs(ImGui::GetItemRectSize().x - 240 * zoom) < .1f);
                ImGui::SetWindowFontScale(1.f);
                ImGui::End(); ImGui::Render();
            }
        }
    }
    macro.presetManager.setCurrentMacro(originalName);
    ofxOceanodeShared::setZoomLevel(1.f);
    auto frame = [&]() {
        io.DeltaTime = 1.f / 60; ImGui::NewFrame();
        ImGui::Begin("Save dialog test");
        ImGui::PushID("Preset Control Gui"); copy.renderPresetControlGui(); ImGui::PopID();
        assert(std::abs(ImGui::GetItemRectSize().x - 240) < .1f);
        ImGui::End(); ImGui::Render();
    };
    auto pressSave = [&](const char* label) {
        auto* popup = ImGui::FindWindowByName("Save Macro"); assert(popup);
        ImGui::ActivateItemByID(ImHashStr(label, 0, popup->ID));
        frame();
    };
    copy.localName = "";
    copy.openMacroSaveDialog(false);
    std::snprintf(copy.guiState.saveName, sizeof(copy.guiState.saveName), "%s", "Named locally");
    frame(); frame(); pressSave("Save");
    assert(bool(copy.pendingGuiAction)); copy.update(event); frame();
    assert(copy.isLocal() && copy.getLocalMacroName() == "Named locally");
    copy.openMacroSaveDialog(true);
    copy.presetManager.getSaveAsTempCategory() = {"Folder", "Empty"};
    std::snprintf(copy.guiState.saveName, sizeof(copy.guiState.saveName), "%s", "From Dialog");
    frame(); frame(); pressSave("Save");
    assert(bool(copy.pendingGuiAction)); copy.update(event); frame();
    assert(!copy.isLocal() && copy.getCurrentMacroName() == "From_Dialog");
    assert(fs::exists("Macros/Folder/Empty/From_Dialog/modules.json"));
    assert(ofLoadJson("Macros/Folder/Empty/From_Dialog/description.json").at("Description") == "Description 100%");
    // Existing library definitions require the explicit overwrite button.
    copy.openMacroSaveDialog(true);
    frame(); frame(); pressSave("Save");
    assert(copy.guiState.confirmOverwrite && !copy.pendingGuiAction);
    pressSave("Overwrite"); assert(bool(copy.pendingGuiAction));
    copy.update(event); frame();
    assert(copy.guiState.saveError.empty());
    // Save As exposes the same library tree, including selectable folders and
    // existing Macro targets, and creates folders at the root and deeper levels.
    copy.openMacroSaveDialog(true);
    frame(); frame();
    auto* savePopup = ImGui::FindWindowByName("Save Macro"); assert(savePopup);
    ImGuiWindow* saveLibrary = nullptr;
    for(auto* child : savePopup->DC.ChildWindows)
        if(child->ChildId == ImHashStr("Save Library", 0, savePopup->ID)) saveLibrary = child;
    assert(saveLibrary);
    const auto saveRootID = ImHashStr("Macros (root)", 0, saveLibrary->ID);
    const auto saveFolderID = ImHashStr("Folder", 0, saveRootID);
    const auto saveMacroPathID = ImHashStr("Macros/Folder/A", 0, saveFolderID);
    ImGui::ActivateItemByID(ImHashStr("A", 0, saveMacroPathID)); frame();
    assert(copy.presetManager.getSaveAsTempCategory() == std::deque<string>({"Folder"}));
    assert(string(copy.guiState.saveName) == "A");
    assert(string(copy.guiState.saveDescription.data()) == "Description 100%");
    ImGui::ActivateItemByID(saveRootID); frame();
    assert(copy.presetManager.getSaveAsTempCategory().empty());
    std::snprintf(copy.guiState.saveName, sizeof(copy.guiState.saveName), "%s", "From Tree");
    pressSave("New Folder");
    assert(copy.guiState.creatingSaveFolder);
    frame(); io.AddInputCharactersUTF8("New Root"); frame();
    pressSave("Create Folder"); frame();
    assert(!copy.guiState.creatingSaveFolder);
    assert(copy.presetManager.getSaveAsTempCategory() == std::deque<string>({"New Root"}));
    assert(fs::is_directory("Macros/New Root"));
    pressSave("New Folder");
    frame(); io.AddInputCharactersUTF8("Nested"); frame();
    pressSave("Create Folder"); frame();
    assert(copy.presetManager.getSaveAsTempCategory() == std::deque<string>({"New Root", "Nested"}));
    assert(fs::is_directory("Macros/New Root/Nested"));
    assert(string(copy.guiState.saveName) == "From Tree");
    assert(string(copy.guiState.saveDescription.data()) == "Description 100%");
    // Invalid names, existing folders, Macro definitions and files cannot be replaced.
    for(const string invalid : {"", "..", "../escape", "a/b", "a\\b", "trailing."})
        assert(!copy.createMacroLibraryFolder(invalid));
    copy.presetManager.getSaveAsTempCategory() = {"New Root"};
    assert(!copy.createMacroLibraryFolder("Nested"));
    copy.presetManager.getSaveAsTempCategory() = {"Folder"};
    assert(!copy.createMacroLibraryFolder("A"));
    assert(!copy.createMacroLibraryFolder("notes.json"));
    copy.presetManager.getSaveAsTempCategory() = {"New Root", "Nested"};
    copy.guiState.saveError.clear();
    pressSave("Save"); assert(bool(copy.pendingGuiAction));
    copy.update(event); frame();
    assert(copy.getCurrentMacroName() == "From_Tree");
    assert(fs::exists("Macros/New Root/Nested/From_Tree/modules.json"));
    assert(ofLoadJson("Macros/New Root/Nested/From_Tree/description.json").at("Description") == "Description 100%");
    // The Load dialog uses the same expandable tree/Selectable interaction as
    // Presets > Load Preset, and keeps the node width while the modal is open.
    ofxOceanodeShared::setCurrentBankName("Bank");
    copy.guiState.openLoadPopup = true;
    copy.guiState.loadSelectionPath.clear();
    frame(); frame();
    auto* loadPopup = ImGui::FindWindowByName("Load Macro Contents"); assert(loadPopup);
    assert(loadPopup->Flags & ImGuiWindowFlags_Modal);
    assert(loadPopup->DC.ChildWindows.Size == 1);
    auto* sources = loadPopup->DC.ChildWindows[0];
    const auto presetRoot = ImHashStr("Preset", 0, sources->ID);
    const string bankPath = ofToDataPath("Presets", true) + "/Bank";
    const auto bankPathID = ImHashStr(bankPath.c_str(), 0, presetRoot);
    const auto bankID = ImHashStr("Bank", 0, bankPathID);
    const string presetPath = bankPath + "/01--Whole_Preset";
    const auto presetPathID = ImHashStr(presetPath.c_str(), 0, bankID);
    ImGui::ActivateItemByID(ImHashStr("1 | Whole_Preset", 0, presetPathID));
    frame();
    assert(copy.guiState.loadSelectionIsPreset && copy.guiState.loadSelectionPath == presetPath);
    assert(!copy.pendingGuiAction); // Selecting a row waits for Load.
    ImGui::ActivateItemByID(ImHashStr("Load", 0, loadPopup->ID)); frame();
    assert(bool(copy.pendingGuiAction)); copy.update(event); frame();
    assert(copy.isLocal() && copy.getLocalMacroName() == "Whole_Preset" && value(copy) == .25f);
    copy.guiState.openLoadPopup = true;
    copy.guiState.loadSelectionPath.clear();
    frame(); frame();
    const auto macroRoot = ImHashStr("Macro", 0, sources->ID);
    const auto folderID = ImHashStr("Folder", 0, macroRoot);
    ImGui::ActivateItemByID(folderID); frame();
    const auto macroPathID = ImHashStr("Macros/Folder/A", 0, folderID);
    ImGui::ActivateItemByID(ImHashStr("A", 0, macroPathID)); frame();
    assert(!copy.guiState.loadSelectionIsPreset && copy.guiState.loadSelectionPath == "Macros/Folder/A");
    ImGui::ActivateItemByID(ImHashStr("Load", 0, loadPopup->ID)); frame();
    copy.update(event); frame();
    assert(!copy.isLocal() && copy.getCurrentMacroName() == "A" && value(copy) == .7f);
    // Delete confirms inside the Load dialog; Escape cancels only the confirmation.
    auto selectSource = [&](const string& path, const char* name) {
        copy.guiState.openLoadPopup = true; copy.guiState.loadSelectionPath.clear(); frame(); frame();
        ImGui::ActivateItemByID(ImHashStr(name, 0, ImHashStr(path.c_str(), 0, folderID))); frame();
        assert(copy.guiState.loadSelectionPath == path);
    };
    saveSource("Macros/Folder/ToDelete", .1f); ofxOceanodeShared::updateMacrosStructure();
    selectSource("Macros/Folder/ToDelete", "ToDelete");
    ImGui::ActivateItemByID(ImHashStr("Delete", 0, loadPopup->ID)); frame(); frame();
    auto* deletePopup = ImGui::FindWindowByName("Delete Macro?"); assert(deletePopup && deletePopup->Active);
    assert(!copy.guiState.deleteKeepsLocalCopy);
    io.AddKeyEvent(ImGuiKey_Escape, true); frame(); io.AddKeyEvent(ImGuiKey_Escape, false); frame();
    assert(!deletePopup->Active && loadPopup->Active && fs::exists("Macros/Folder/ToDelete"));
    ImGui::ActivateItemByID(ImHashStr("Delete", 0, loadPopup->ID)); frame(); frame();
    assert(deletePopup->Active);
    ImGui::ActivateItemByID(ImHashStr("Delete", 0, deletePopup->ID)); frame();
    assert(bool(copy.pendingGuiAction)); copy.update(event); frame();
    assert(!fs::exists("Macros/Folder/ToDelete") && copy.guiState.loadSelectionPath.empty());
    assert(loadPopup->Active && !copy.isLocal() && copy.getCurrentMacroName() == "A");
    ImGui::ActivateItemByID(ImHashStr("Cancel", 0, loadPopup->ID)); frame();
    // Deleting the library Macro this node uses keeps its contents as a local Macro.
    saveSource("Macros/Folder/InUse", .3f); ofxOceanodeShared::updateMacrosStructure();
    copy.presetManager.setNextPresetPath("Macros/Folder/InUse"); copy.update(event);
    assert(!copy.isLocal() && value(copy) == .3f);
    copy.openMacroSaveDialog(true); frame(); frame();
    assert(copy.presetManager.getSaveAsTempCategory() == std::deque<string>({"Folder"}));
    pressSave("Delete"); frame();
    assert(copy.guiState.deleteKeepsLocalCopy && deletePopup->Active);
    ImGui::ActivateItemByID(ImHashStr("Delete", 0, deletePopup->ID)); frame();
    copy.update(event); frame();
    assert(!fs::exists("Macros/Folder/InUse") && fs::exists("Macros/Folder/A/modules.json"));
    assert(copy.isLocal() && copy.getLocalMacroName() == "InUse" && value(copy) == .3f);
    pressSave("Cancel");
    // Only Macro definitions inside the library can be deleted.
    assert(!copy.deleteLibraryMacro("Presets/Bank/01--Whole_Preset") && fs::exists("Presets/Bank/01--Whole_Preset"));
    assert(!copy.deleteLibraryMacro("Macros/Folder") && fs::exists("Macros/Folder/A"));
    // Separator recolor and rename keep the parameter, renamed in place.
    {
        auto separators = [&]() {
            vector<string> names;
            for(int i = 0; i < (int)macro.getParameterGroup().size(); i++) {
                const string name = macro.getParameterGroup().get(i).getName();
                if(name.rfind("SEPARATOR:|", 0) == 0) names.push_back(name);
            }
            return names;
        };
        const ofColor original(10, 20, 30, 255), recolored(200, 100, 50, 128);
        macro.addSeparator("Group", original);
        macro.sortOrder.getOrder().push_back(macro.makeSortSeparatorEntry("Group", original));
        const int index = macro.sortOrder.getOrder().size() - 1;
        macro.setSortSeparator(index, "Group", recolored);
        assert(separators() == vector<string>({"SEPARATOR:|Group|200,100,50,128"}));
        assert(macro.getSortSeparatorColor(macro.sortOrder.getOrder()[index]) == recolored);
        macro.setSortSeparator(index, "Renamed", recolored);
        assert(separators() == vector<string>({"SEPARATOR:|Renamed|200,100,50,128"}));
        assert(macro.getSortSeparatorLabel(macro.sortOrder.getOrder()[index]) == "Renamed");
    }
    // Real container history must retain unsaved local AND library macro graphs.
    {
        ofxOceanodeContainer history(registry, types, transport); history.setCanvasID("History");
        history.beginCanvasEdit();
        auto& wrapper = history.createNode(std::make_unique<ofxOceanodeNodeMacro>(), 1);
        auto& local = static_cast<ofxOceanodeNodeMacro&>(wrapper.getNodeModel());
        assert(local.loadMacroSource("Presets/Bank/01--Whole_Preset"));
        local.getContainer()->getAllModules().front()->getNodeModel().getParameter<float>("Value") = .91f;
        history.endCanvasEdit();
        assert(history.undoCanvasEdit() && history.getAllModules().empty());
        assert(history.redoCanvasEdit());
        auto& restored = static_cast<ofxOceanodeNodeMacro&>(history.getAllModules().front()->getNodeModel());
        assert(restored.isLocal() && value(restored) == .91f);
        history.beginCanvasEdit(); history.getAllModules().front()->deleteSelf(); history.endCanvasEdit();
        assert(history.undoCanvasEdit());
        assert(value(static_cast<ofxOceanodeNodeMacro&>(history.getAllModules().front()->getNodeModel())) == .91f);
        history.clearCanvasHistory(); // Restored local data must outlive eviction.

        history.beginCanvasEdit();
        auto& globalWrapper = history.createNode(std::make_unique<ofxOceanodeNodeMacro>(), 2, false, "Macros/Folder/A");
        auto& global = static_cast<ofxOceanodeNodeMacro&>(globalWrapper.getNodeModel());
        assert(!global.isLocal());
        global.getContainer()->getAllModules().front()->getNodeModel().getParameter<float>("Value") = .92f;
        history.endCanvasEdit();
        assert(history.undoCanvasEdit() && history.getAllModules().size() == 1);
        assert(history.redoCanvasEdit());
        auto& restoredGlobal = static_cast<ofxOceanodeNodeMacro&>(history.getDynamicNodes().at("Macro").at(2)->getNodeModel());
        assert(!restoredGlobal.isLocal() && restoredGlobal.getCurrentMacroName() == "A");
        assert(value(restoredGlobal) == .92f);
        assert(restoredGlobal.presetManager.getCurrentCategoryMacro());
        assert(!restoredGlobal.presetManager.getCurrentCategoryMacro()->macros.empty());
        assert(history.undoCanvasEdit()); assert(history.redoCanvasEdit());
    }
    ImGui::DestroyContext(context);
    // A stale category path must safely disable indexed selection.
    macro.presetManager.updateCategoryFromPath("Macros/Missing/A");
    macro.presetManager.loadMacroInsideCategory(0);
    assert(macro.presetManager.getNextPresetPath().empty());
    std::cout << "Macro GUI regression passed: local import/save/recall, library save, four index input types, default width at canvas offsets/zooms, Load/Save modal trees, nested folder creation, confirmed library deletion and separator recolor.\n";
}
'''

def main():
    # Expose private members only in the fixture TU so production files retain their API.
    with tempfile.TemporaryDirectory(prefix="macro-gui-access-") as tmp:
        access = Path(tmp) / "MacroGuiTestAccess.h"
        access.write_text((native.ROOT / "src/Nodes/ofxOceanodeNodeMacro.h").read_text().replace("private:", "public:"))
        native.PRELUDE = '#include "' + str(access) + '"\n' + native.PRELUDE
        native.SUPPORT = native.SUPPORT.replace('void ofxOceanodeNodeMacro::renderPresetControlGui() { assert(false); }', '')
        native.SUPPORT = native.SUPPORT.replace('bool ofxOceanodeNodeMacro::drawNodeParameter(ofxOceanodeAbstractParameter&, float, float) { assert(false); return false; }', '')
        gui = (native.ROOT / "src/Nodes/ofxOceanodeNodeMacroGui.cpp").read_text()
        native.SUPPORT += '\n#include "ofxOceanodeColors.h"\n#include "MacroDialogAppearance.h"\n'
        for signature in ['bool ofxOceanodeNodeMacro::drawNodeParameter(',
                          'void ofxOceanodeNodeMacro::openMacroSaveDialog(',
                          'void ofxOceanodeNodeMacro::renderPresetControlGui()',
                          'void ofxOceanodeNodeMacro::renderMacroLoadDialog()',
                          'void ofxOceanodeNodeMacro::openMacroDeleteDialog(',
                          'void ofxOceanodeNodeMacro::renderMacroDeleteDialog()',
                          'void ofxOceanodeNodeMacro::renderMacroSaveDialog()',
                          'void ofxOceanodeNodeMacro::renderMacroSaveLibraryTree()',
                          'bool ofxOceanodeNodeMacro::createMacroLibraryFolder(',
                          'void ofxOceanodeNodeMacro::setSortSeparator(']:
            native.SUPPORT += block(gui, signature) + '\n'
        # The root save/load adapters omit UI services; retain storage ownership.
        native.SUPPORT = native.SUPPORT.replace('void ofxOceanodeContainer::saveCustomGuis(const std::string&) {',
            'void ofxOceanodeContainer::saveCustomGuis(const std::string& path) { customGuiStoragePath = path;')
        native.CASES = CASES
        native.main()

if __name__ == "__main__":
    main()
