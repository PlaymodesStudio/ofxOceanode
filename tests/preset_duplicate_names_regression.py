#!/usr/bin/env python3
"""Exercise production preset addressing with two presets of the same name.

Splices the Presets controller's folder scan, listeners, load, save, delete and
path helpers over a mocked openFrameworks and a real temporary Presets folder.
Covers names that contain "--", loads and saves that must hit the clicked
number rather than the first name match, renumbering after delete, and the
Save As replacement targeting and the unconfirmed duplicate guard. Does not draw ImGui or launch a host.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
from new_preset_regression import block

FIXTURE = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
using namespace std;
namespace fs = std::filesystem;

struct ofLog { template<class T> ofLog& operator<<(const T&){ return *this; } };
int warnings = 0;
ofLog ofLogWarning(const char*){ ++warnings; return {}; }
ofLog ofLogError(const char*){ return {}; }
template<class T> string ofToString(const T& v){ return to_string(v); }
int ofToInt(const string& s){ try{ return stoi(s); }catch(...){ return 0; } }
void ofStringReplace(string& s, const string& from, const string& to){
    for(size_t p = s.find(from); p != string::npos; p = s.find(from, p + to.size())) s.replace(p, from.size(), to);
}
struct ofFile { bool directory; bool isDirectory() const { return directory; } };
struct ofDirectory {
    string path; vector<fs::directory_entry> entries;
    ofDirectory() = default;
    explicit ofDirectory(string p) : path(std::move(p)) {}
    void open(string p){ path = std::move(p); }
    bool exists() const { return fs::exists(path); }
    void createDirectory(string p){ fs::create_directories(p); }
    size_t listDir(){ entries.assign(fs::directory_iterator(path), {}); sort(); return entries.size(); }
    void sort(){ std::sort(entries.begin(), entries.end(), [](auto& a, auto& b){ return a.path().filename() < b.path().filename(); }); }
    size_t size() const { return entries.size(); }
    string getName(size_t i) const { return entries[i].path().filename().string(); }
    ofFile getFile(size_t i) const { return {entries[i].is_directory()}; }
    string getAbsolutePath() const { return fs::absolute(path).lexically_normal().string(); }
    static bool doesDirectoryExist(const string& p){ return fs::is_directory(p); }
};
struct ofEventListener { int token = 0; };
template<class T> struct ofEvent {
    vector<function<void(T)>> listeners;
    ofEventListener newListener(function<void(T)> fn){ listeners.push_back(std::move(fn)); return {}; }
    void notify(T value){ for(auto& l : listeners) l(value); }
};
struct ofxOceanodeShared {
    inline static string path, bank, name; inline static int loads = 0, saves = 0;
    static void startedLoadingPreset(){ ++loads; } static void finishedLoadingPreset(){}
    static void setCurrentPresetPath(string p){ path = std::move(p); } static string getCurrentPresetPath(){ return path; }
    static void setCurrentBankName(string b){ bank = std::move(b); } static string getCurrentBankName(){ return bank; }
    static void setCurrentPresetName(string n){ name = std::move(n); } static string getCurrentPresetName(){ return name; }
    static void presetWasSaved(){ ++saves; }
    struct Event { ofEventListener newListener(function<void()>){return {};} };
    static Event& getPresetHasLoadedEvent(){static Event e;return e;}
    static Event& getPresetWasSavedEvent(){static Event e;return e;}
    static int getPresetLoadType(){return 0;}
    inline static vector<string> broadcasts; static void macroUpdated(string p){ broadcasts.push_back(std::move(p)); }
};
struct ofxOceanodeContainer {
    ofEvent<pair<string, string>> loadPresetEvent; ofEvent<pair<string, int>> loadPresetNumEvent;
    vector<string> loaded, saved;
    void loadPreset(string p){ loaded.push_back(std::move(p)); }
    bool savePreset(string p){ fs::create_directories(p); saved.push_back(std::move(p)); return true; }
};
struct MacroInstance { bool saveGlobalDefinition(bool){ return true; } };
struct GlobalMacroSaveReview { string name; string globalPath; MacroInstance* instance = nullptr; };
vector<GlobalMacroSaveReview> collectGlobalMacroSaveReviews(ofxOceanodeContainer&){ return {}; }
vector<GlobalMacroSaveReview> collapseEquivalentReviews(vector<GlobalMacroSaveReview> r){ return r; }
constexpr int ofxOceanodePresetLoadType_FullPreset=0;
struct ofxOceanodeBaseController { explicit ofxOceanodeBaseController(string){} };

PRODUCTION_HELPERS

struct ofxOceanodePresetsController : ofxOceanodeBaseController {
    ofxOceanodePresetsController(shared_ptr<ofxOceanodeContainer> _container);
    void reloadPresetList();
    void loadPreset(int index, const string& bank);
    void savePreset(int index, const string& bank);
    void beginSavePreset(string name, string bank, int index);
    void finishSavePreset();
    vector<int> saveAsReplacementTargets(const string& bank, const string& name) const;
    void deletePreset(int index, const string& bank);
    string presetFolderPath(int index, const string& bankName) const;
    string presetNameAt(int index, const string& bankName) const;
    map<string, vector<string>> bankPresets; map<string, int> currentPreset; vector<string> banks; int currentBank = -1;
    enum class PopupRequest { None, NewBank, SaveAs, Delete, ReviewSave, SaveResult }; PopupRequest popupRequest = PopupRequest::None;
    bool bypassSaveGuard=false,savingBeforeAction=false,actionApproved=false; function<void()> pendingAction;
    bool hasUnsavedChanges(){return false;}
    void requestDestructiveAction(function<void()> fn){pendingAction=fn;}
    void rememberSavedState(){} void cancelPendingAction(){pendingAction={};actionApproved=false;savingBeforeAction=false;}
    ofEventListener presetLoadedListener,presetSavedListener;
    string lastPresetName;
    enum class MacroSaveChoice { Skip, Save, SaveAs };
    struct PendingSave {
        string name, bank; bool createNew = false; int index = -1; bool writing = false;
        size_t reviewIndex = 0, nextWriteIndex = 0; string warning;
        vector<GlobalMacroSaveReview> reviews; vector<MacroSaveChoice> saveChoices;
        vector<string> saveAsPaths, savedMacros, savedAsMacros, skippedMacros;
    };
    unique_ptr<PendingSave> pendingSave; string saveResultText;
    // The spliced tail broadcasts the global macros written before the preset.
    void broadcastSavedMacros(PendingSave& request){
        for(size_t i = 0; i < request.nextWriteIndex && i < request.reviews.size(); ++i)
            if(request.saveChoices[i] == MacroSaveChoice::Save) ofxOceanodeShared::macroUpdated(request.reviews[i].globalPath);
        request.nextWriteIndex = request.reviews.size();
    }
    bool newPresetCreated = false, newPresetRequested = false; int loadPresetInNextUpdate = 0;
    ofEventListener presetListener, presetNumListener, saveCurrentPresetListener;
    shared_ptr<ofxOceanodeContainer> container;
};

PRODUCTION_METHODS

void ofxOceanodePresetsController::finishSavePreset(){
    if(!pendingSave) return;
    auto& request = *pendingSave;
    assert(request.reviews.empty());
PRODUCTION_FINISH_TAIL

static bool endsWith(const string& s, const string& tail){ return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0; }
static vector<string> folders(){
    vector<string> names; for(auto& e : fs::directory_iterator("Presets/Bank")) names.push_back(e.path().filename().string());
    std::sort(names.begin(), names.end()); return names;
}

int main(){
    for(const char* f : {"1--Foo", "2--Bar", "3--Foo", "4--My--Preset", "junk"}) fs::create_directories(string("Presets/Bank/") + f);
    auto container = make_shared<ofxOceanodeContainer>();
    ofxOceanodePresetsController presets(container);
    auto& list = presets.bankPresets["Bank"];
    assert(presets.banks == vector<string>{"Bank"} && presets.currentBank == -1 && presets.currentPreset["Bank"] == -1);
    assert((list == vector<string>{"Foo", "Bar", "Foo", "My--Preset"})); // junk skipped, "--" kept in a name
    assert(warnings == 2); // junk folder, duplicate name

    // The second "Foo" loads its own folder, not the first name match.
    presets.loadPreset(2, "Bank");
    assert(container->loaded.back() == "./Presets/Bank/3--Foo" && presets.currentPreset["Bank"] == 2 && presets.currentBank == 0);
    assert(ofxOceanodeShared::path == "./Presets/Bank/3--Foo" && ofxOceanodeShared::name == "Foo" && ofxOceanodeShared::bank == "Bank");
    presets.loadPreset(3, "Bank");
    assert(container->loaded.back() == "./Presets/Bank/4--My--Preset" && presets.currentPreset["Bank"] == 3);
    const size_t loadsBefore = container->loaded.size();
    presets.loadPreset(4, "Bank"); presets.loadPreset(-1, "Bank"); presets.loadPreset(0, "Nope");
    assert(container->loaded.size() == loadsBefore && presets.currentPreset["Bank"] == 3);

    // External loads: by number hits that number, by name hits the first match.
    container->loadPresetNumEvent.notify({"Bank", 3});
    assert(container->loaded.back() == "./Presets/Bank/3--Foo" && presets.currentPreset["Bank"] == 2);
    container->loadPresetEvent.notify({"Bank", "Foo"});
    assert(container->loaded.back() == "./Presets/Bank/1--Foo" && presets.currentPreset["Bank"] == 0);
    container->loadPresetEvent.notify({"Bank", "Missing"}); container->loadPresetNumEvent.notify({"Bank", 9});
    assert(container->loaded.back() == "./Presets/Bank/1--Foo");

    // Save writes the current number's folder, not the first "Foo".
    presets.loadPreset(2, "Bank");
    presets.savePreset(2, "Bank");
    assert(container->saved.back() == "./Presets/Bank/3--Foo" && presets.currentPreset["Bank"] == 2 && ofxOceanodeShared::saves == 1);
    assert(ofxOceanodeShared::path == "./Presets/Bank/3--Foo" && presets.lastPresetName == "Foo" && !presets.pendingSave);
    presets.savePreset(7, "Bank");
    assert(container->saved.size() == 1);

    // Resolve every matching numbered preset, including legacy names with spaces.
    assert((presets.saveAsReplacementTargets("Bank", "Foo") == vector<int>{0,2}));
    assert(presets.saveAsReplacementTargets("Missing", "Foo").empty());
    assert(presets.saveAsReplacementTargets("Bank", "").empty());
    presets.bankPresets["Legacy"] = {"My Preset", "My_Preset"};
    assert((presets.saveAsReplacementTargets("Legacy", "My Preset") == vector<int>{0,1}));
    assert((presets.saveAsReplacementTargets("Legacy", "My_Preset") == vector<int>{1}));
    presets.bankPresets.erase("Legacy");

    // Unconfirmed duplicate creation stays blocked. Confirmed Save As uses the
    // numbered overwrite path, so no extra preset/folder is created.
    presets.beginSavePreset("Foo", "Bank", -1);
    assert(container->saved.size() == 1 && !presets.pendingSave);
    presets.beginSavePreset("New One", "Bank", -1);
    assert(container->saved.back() == "./Presets/Bank/5--New_One" && list.size() == 5 && list.back() == "New_One");
    assert(presets.currentPreset["Bank"] == 4 && presets.newPresetCreated && fs::is_directory("Presets/Bank/5--New_One"));
    const auto foldersBeforeReplacement = folders();
    presets.beginSavePreset("", "Bank", 2);
    assert(container->saved.back() == "./Presets/Bank/3--Foo");
    assert(list.size() == 5 && folders() == foldersBeforeReplacement);

    assert(endsWith(presets.presetFolderPath(0, "Bank"), "/Presets/Bank/1--Foo"));
    assert(endsWith(presets.presetFolderPath(2, "Bank"), "/Presets/Bank/3--Foo"));
    assert(presets.presetFolderPath(9, "Bank").empty() && presets.presetFolderPath(0, "Nope").empty());
    assert(presets.presetNameAt(3, "Bank") == "My--Preset" && presets.presetNameAt(-1, "Bank").empty());

    // Deleting the first "Foo" renumbers the rest and keeps the current preset.
    presets.loadPreset(2, "Bank");
    presets.deletePreset(0, "Bank");
    assert((list == vector<string>{"Bar", "Foo", "My--Preset", "New_One"}));
    assert((folders() == vector<string>{"1--Bar", "2--Foo", "3--My--Preset", "4--New_One", "junk"}));
    assert(presets.currentPreset["Bank"] == 1 && ofxOceanodeShared::path == "./Presets/Bank/2--Foo");
    presets.loadPreset(1, "Bank");
    assert(container->loaded.back() == "./Presets/Bank/2--Foo");
    presets.deletePreset(1, "Bank");
    assert((folders() == vector<string>{"1--Bar", "2--My--Preset", "3--New_One", "junk"}) && presets.currentPreset["Bank"] == -1);
    presets.deletePreset(5, "Bank"); presets.deletePreset(0, "Nope");
    assert(list.size() == 3);

    // Disk refresh discovers new banks/presets and preserves a selected folder
    // even when sorting inserts a bank before it. It never loads the graph.
    presets.loadPreset(1, "Bank");
    const auto loadedBeforeRefresh = container->loaded;
    const string pathBeforeRefresh = ofxOceanodeShared::path;
    fs::create_directories("Presets/A_New_Bank/1--Added");
    fs::create_directories("Presets/Bank/4--External");
    presets.reloadPresetList();
    assert((presets.banks == vector<string>{"A_New_Bank", "Bank"}));
    assert(presets.currentBank == 1 && presets.currentPreset["Bank"] == 1);
    assert(presets.bankPresets["Bank"].back() == "External");
    assert(container->loaded == loadedBeforeRefresh && ofxOceanodeShared::path == pathBeforeRefresh);
    // A renamed selected folder must not leave Save pointing at another preset.
    fs::rename("Presets/Bank/2--My--Preset", "Presets/Bank/2--Renamed");
    presets.reloadPresetList();
    assert(presets.currentBank == 1 && presets.currentPreset["Bank"] == -1);
    assert(presets.bankPresets["Bank"][1] == "Renamed");
    fs::remove_all("Presets/Bank");
    presets.reloadPresetList();
    assert(presets.currentBank == -1 && !presets.bankPresets.count("Bank"));
    assert(container->loaded == loadedBeforeRefresh);
    assert(container->loadPresetEvent.listeners.size() == 1 && container->loadPresetNumEvent.listeners.size() == 1);

    puts("PASS: same-named presets load, save, delete and reveal by number; names keep '--'; duplicates warned.");
}
'''


def main():
    source = (ROOT / "src/Controls/ofxOceanodePresetsController.cpp").read_text()
    helpers = block(source, "string sanitizePresetName(string name)") + "\n" + block(source, "bool presetNameExists(")
    methods = ""
    for signature in ("ofxOceanodePresetsController::ofxOceanodePresetsController(",
                      "void ofxOceanodePresetsController::reloadPresetList(",
                      "void ofxOceanodePresetsController::loadPreset(",
                      "void ofxOceanodePresetsController::savePreset(",
                      "vector<int> ofxOceanodePresetsController::saveAsReplacementTargets(",
                      "void ofxOceanodePresetsController::beginSavePreset(",
                      "void ofxOceanodePresetsController::deletePreset(",
                      "string ofxOceanodePresetsController::presetFolderPath(",
                      "string ofxOceanodePresetsController::presetNameAt("):
        methods += block(source, signature) + "\n"
    finish = block(source, "void ofxOceanodePresetsController::finishSavePreset()")
    tail = finish[finish.index("    const size_t presetIndex = request.createNew"):]
    fixture = (FIXTURE.replace("PRODUCTION_HELPERS", helpers)
               .replace("PRODUCTION_METHODS", methods)
               .replace("PRODUCTION_FINISH_TAIL", tail))
    with tempfile.TemporaryDirectory(prefix="oceanode-preset-names-") as directory:
        work = Path(directory)
        cpp = work / "check.cpp"
        cpp.write_text(fixture)
        binary = work / "check"
        subprocess.run([os.environ.get("CXX", "clang++"), "-std=c++17", "-O0", "-g",
                        "-fsanitize=address,undefined", str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], cwd=work, check=True)


if __name__ == "__main__":
    main()
