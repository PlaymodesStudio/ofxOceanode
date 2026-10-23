#!/usr/bin/env python3
"""Production preset/Macro save and filesystem cleanup under ASan/UBSan.

Real OF node and Macro persistence; UI, Scope and Timeline use the existing
headless native fixture. All files are created in a temporary directory.
"""
from pathlib import Path
import tempfile
import macro_mixer_regression as native
from new_preset_regression import block

CASES = r'''
static void write(const std::string& path, const ofJson& value){ std::ofstream(path) << value; }
int main(int argc, char** argv){
    assert(argc == 3);
    namespace fs = std::filesystem;
    fs::current_path(argv[2]); ofDisableDataPath(); ofSeedRandom();
    auto window = std::make_shared<ofAppNoWindow>(); ofGetMainLoop()->setCurrentWindow(window);
    auto registry = std::make_shared<ofxOceanodeNodeRegistry>();
    auto types = std::make_shared<ofxOceanodeTypesRegistry>();
    registry->registerModel<simpleNumberGenerator>("Generators");
    registry->registerModel<ofxOceanodeNodeMacro>("Macro");
    auto transport = std::make_shared<ofxOceanodeTransport>();
    auto make = [&](){
        auto c = std::make_shared<ofxOceanodeContainer>(registry, types, transport);
        c->setCanvasID("CleanupTest"); return c;
    };
    auto add = [](ofxOceanodeContainer& c, int id = 1) -> ofxOceanodeNode& {
        return c.createNode(std::make_unique<simpleNumberGenerator>(), id);
    };
    auto macro = [](ofxOceanodeContainer& c, int id = 1) -> ofxOceanodeNodeMacro& {
        return static_cast<ofxOceanodeNodeMacro&>(c.createNode(std::make_unique<ofxOceanodeNodeMacro>(), id).getNodeModel());
    };
    auto c = make();
    auto& first = add(*c); add(*c, 2);
    assert(c->savePreset("Preset"));
    const auto oldModules = ofLoadJson("Preset/modules.json");
    fs::create_directories("Preset/samples"); std::ofstream("Preset/samples/keep.wav") << "sample";
    write("Preset/notes.json", {{"note", "keep"}});
    write("Preset/Number_999.json", {{"Value", 9}}); // Untracked legacy file: no guessed ownership.
    first.deleteSelf();
    assert(fs::exists("Preset/Number_1.json"));
    assert(ofLoadJson("Preset/modules.json") == oldModules); // Unsaved deletion is reversible.
    fs::remove("Preset/.oceanode-node-files.json"); // Upgrade an existing preset using its saved node list.
    assert(c->savePreset("Preset"));
    assert(!fs::exists("Preset/Number_1.json") && fs::exists("Preset/Number_2.json"));
    assert(fs::exists("Preset/Number_999.json") && fs::exists("Preset/notes.json"));
    assert(fs::exists("Preset/samples/keep.wav"));
    // ID reuse keeps the newly saved node, and Save As never cleans the source.
    add(*c); assert(c->savePreset("Preset"));
    c->getDynamicNodes().at("Number").at(1)->deleteSelf(); add(*c);
    assert(c->savePreset("Preset")); assert(fs::exists("Preset/Number_1.json"));
    c->getDynamicNodes().at("Number").at(1)->deleteSelf();
    assert(c->savePreset("Copy")); assert(fs::exists("Preset/Number_1.json"));
    assert(!fs::exists("Copy/Number_1.json"));
    // Recursive saves wait for the complete outer save, even after child writers succeeded.
    auto root = make();
    auto& outer = macro(*root); outer.getContainer()->setCanvasID("Outer");
    auto& inner = macro(*outer.getContainer()); inner.getContainer()->setCanvasID("Inner");
    add(*inner.getContainer());
    assert(root->savePreset("Nested"));
    const string child = "Nested/Macro_1/Macro_1";
    auto childModules = ofLoadJson(child + "/modules.json");
    inner.getContainer()->getAllModules().front()->deleteSelf();
    fs::remove("Nested/comments.json"); fs::create_directory("Nested/comments.json");
    assert(!root->savePreset("Nested"));
    assert(fs::exists(child + "/Number_1.json"));
    assert(ofLoadJson(child + "/modules.json") == childModules);
    fs::remove_all("Nested/comments.json");
    assert(root->savePreset("Nested")); assert(!fs::exists(child + "/Number_1.json"));
    // Removing an embedded Macro removes its complete owned subtree on save.
    outer.getContainer()->getAllModules().front()->deleteSelf();
    assert(root->savePreset("Nested"));
    assert(!fs::exists(child) && !fs::exists(child + ".json"));
    // Build a shared library definition, including a deletable node.
    auto library = make(); add(*library); add(*library, 2);
    assert(library->savePreset("Macros/Group/Shared")); ofxOceanodeShared::updateMacrosStructure();
    ofEventArgs event;
    outer.presetManager.setNextPresetPath("Macros/Group/Shared"); outer.update(event);
    assert(!outer.isLocal());
    outer.getContainer()->getAllModules().front()->deleteSelf();
    assert(root->savePreset("Nested"));
    assert(!fs::exists("Nested/Macro_1")); // Local -> global removes old embedded contents.
    assert(fs::exists("Nested/Macro_1.json"));
    assert(fs::exists("Macros/Group/Shared/Number_1.json") && fs::exists("Macros/Group/Shared/Number_2.json"));
    // Explicit global save prunes that definition; removing its instance preserves the library.
    fs::remove("Macros/Group/Shared/description.json");
    fs::create_directory("Macros/Group/Shared/description.json");
    assert(!outer.saveGlobalDefinition(false));
    assert(fs::exists("Macros/Group/Shared/Number_1.json") && fs::exists("Macros/Group/Shared/Number_2.json"));
    fs::remove_all("Macros/Group/Shared/description.json");
    assert(outer.saveGlobalDefinition(false));
    assert(outer.getContainer()->getAllModules().size() == 1);
    const auto remaining = outer.getContainer()->getAllModules().front()->getNodeModel().getNumIdentifier();
    assert(!fs::exists("Macros/Group/Shared/Number_" + std::to_string(remaining == 1 ? 2 : 1) + ".json"));
    root->getAllModules().front()->deleteSelf(); assert(root->savePreset("Nested"));
    assert(!fs::exists("Nested/Macro_1.json"));
    assert(fs::exists("Macros/Group/Shared/modules.json"));
    // A failed node write retains old membership and obsolete files; retry cleans them.
    auto failure = make(); auto& gone = add(*failure); add(*failure, 2);
    assert(failure->savePreset("Failure")); gone.deleteSelf();
    fs::remove("Failure/Number_2.json"); fs::create_directory("Failure/Number_2.json");
    assert(!failure->savePreset("Failure")); assert(fs::exists("Failure/Number_1.json"));
    fs::remove_all("Failure/Number_2.json");
    assert(failure->savePreset("Failure")); assert(!fs::exists("Failure/Number_1.json"));
    // Failure while publishing node lists also retains cleanup ownership for a retry.
    auto& embedded = macro(*failure); embedded.getContainer()->setCanvasID("CommitFailure");
    add(*embedded.getContainer()); assert(failure->savePreset("Failure"));
    failure->getDynamicNodes().at("Number").at(2)->deleteSelf();
    fs::remove("Failure/Macro_1/modules.json"); fs::create_directory("Failure/Macro_1/modules.json");
    // prepare rejects unreadable previous node lists before any cleanup.
    assert(!failure->savePreset("Failure")); assert(fs::exists("Failure/Number_2.json"));
    fs::remove_all("Failure/Macro_1/modules.json");
    assert(failure->savePreset("Failure")); assert(!fs::exists("Failure/Number_2.json"));
    // An initially failed save journals newly created embedded folders as well.
    auto initialFailure = make(); auto& initialMacro = macro(*initialFailure);
    initialMacro.getContainer()->setCanvasID("InitialFailure"); add(*initialMacro.getContainer());
    fs::create_directories("InitialFailure/comments.json");
    assert(!initialFailure->savePreset("InitialFailure"));
    assert(fs::exists("InitialFailure/Macro_1/modules.json") == false);
    assert(fs::exists("InitialFailure/Macro_1/Number_1.json"));
    initialFailure->getAllModules().front()->deleteSelf(); fs::remove_all("InitialFailure/comments.json");
    assert(initialFailure->savePreset("InitialFailure"));
    assert(!fs::exists("InitialFailure/Macro_1") && !fs::exists("InitialFailure/Macro_1.json"));
    // The explicit Persistent saver uses the same ownership and failure checks.
    auto persistent = make();
    auto& persistentNode = persistent->createNode(std::make_unique<simpleNumberGenerator>(), 1, true);
    assert(persistent->savePersistent()); persistentNode.deleteSelf();
    assert(persistent->savePersistent()); assert(!fs::exists("Persistent/Number_1.json"));
    // Missing addon types are kept across repeated saves, regardless of registry availability later.
    fs::create_directory("Unknown");
    write("Unknown/modules.json", {{"Missing Addon", {{"01", {0,0}}}}});
    write("Unknown/Missing_Addon_1.json", {{"data", "keep"}});
    auto empty = make(); assert(empty->savePreset("Unknown")); assert(empty->savePreset("Unknown"));
    assert(fs::exists("Unknown/Missing_Addon_1.json"));
    // Never follow a leftover file or Macro directory symlink.
    auto links = make(); auto& linked = macro(*links); linked.getContainer()->setCanvasID("Linked");
    add(*linked.getContainer()); assert(links->savePreset("Links"));
    fs::rename("Links/Macro_1", "External"); fs::create_directory_symlink(fs::absolute("External"), "Links/Macro_1");
    links->getAllModules().front()->deleteSelf(); assert(links->savePreset("Links"));
    assert(fs::is_symlink("Links/Macro_1") && fs::exists("External/Number_1.json"));
    // Corrupt or escaping ownership data aborts the save without deleting anything.
    write("Unknown/.oceanode-node-files.json", {{"version",1}, {"nodes", {"../outside.json"}}, {"localMacros", ofJson::array()}, {"protectedNodes", ofJson::array()}});
    assert(!empty->savePreset("Unknown")); assert(fs::exists("Unknown/Missing_Addon_1.json"));
    // Legacy review is read-only until selected candidates are applied.
    using oceanodePresetSave::LegacyPreview;
    const std::set<string> known{"Number", "Macro"};
    auto review = LegacyPreview::scan("Preset", known);
    assert(review.candidates.count("Number_999.json") && fs::exists("Preset/Number_999.json"));
    assert(!LegacyPreview::scan("Preset", known, {"Number_999.json"}).candidates.count("Number_999.json"));
    // A newly created live node or a disk edit invalidates an earlier review.
    assert(!review.removeSelected({"Number_999.json"}, {"Number_999.json"}));
    write("Preset/Number_999.json", {{"Value", "Changed while reviewing"}});
    assert(!review.removeSelected({"Number_999.json"}));
    review = LegacyPreview::scan("Preset", known);
    assert(review.removeSelected({"Number_999.json"})); assert(!fs::exists("Preset/Number_999.json"));
    assert(fs::exists("Preset/notes.json") && fs::exists("Preset/samples/keep.wav"));
    fs::create_directories("Preset/Macro_9"); write("Preset/Macro_9/modules.json", nullptr);
    write("Preset/Macro_9.json", {{"LocalPreset", true}});
    review = LegacyPreview::scan("Preset", known);
    assert(review.candidates.count("Macro_9") && review.candidates.count("Macro_9.json"));
    assert(review.removeSelected({"Macro_9", "Macro_9.json"})); assert(!fs::exists("Preset/Macro_9"));
    assert(!LegacyPreview::scan("Links", known).candidates.count("Macro_1"));
    // Render the review in real ImGui; opening the modal performs no deletion.
    write("Copy/Number_999.json", {{"Value", 9}});
    auto* context = ImGui::CreateContext(); auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.DisplaySize = ImVec2(1400, 1000);
    unsigned char* pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    oceanodePresetSave::CleanupReview::instance().request(c);
    for(int frame = 0; frame < 2; ++frame){
        io.DeltaTime = 1.f/60; ImGui::NewFrame(); ImGui::Begin("CleanupHost");
        oceanodePresetSave::CleanupReview::instance().draw();
        assert(ImGui::IsPopupOpen("Review leftover node files"));
        ImGui::End(); ImGui::Render();
    }
    assert(fs::exists("Copy/Number_999.json")); ImGui::DestroyContext(context);
    assert(oceanodePresetSave::active == nullptr);
    std::cout << "Preset cleanup passed: save/delete, upgrade, nesting, failures/retry, ownership, library, media and symlinks.\n";
}
'''


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="preset-cleanup-access-") as tmp:
        access = Path(tmp) / "MacroCleanupAccess.h"
        access.write_text((native.ROOT / "src/Nodes/ofxOceanodeNodeMacro.h").read_text().replace("private:", "public:"))
        native.PRELUDE = '#include "' + str(access) + '"\n' + native.PRELUDE
        native.PRELUDE += '#include "Controls/PresetFileCleanupReview.h"\n'
        source = (native.ROOT / "src/Managers/ofxOceanodeContainer.cpp").read_text()
        native.SUPPORT += block(source, 'bool ofxOceanodeContainer::savePersistent(') + '\n'
        native.CASES = CASES
        native.main()
