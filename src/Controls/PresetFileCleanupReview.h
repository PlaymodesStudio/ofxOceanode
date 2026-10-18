#pragma once

#include "Managers/PresetSaveCleanup.h"
#include "Managers/ofxOceanodeNodeRegistry.h"
#include "Nodes/ofxOceanodeNodeMacro.h"
#include "imgui.h"

namespace oceanodePresetSave {

// Legacy files have no ownership journal. Offer only registered node filenames
// and recognizable embedded Macro directories, and require a reviewed removal.
struct LegacyPreview {
    fs::path root;
    std::set<std::string> registered;
    std::set<std::string> live;
    std::map<std::string, bool> candidates; // relative path -> directory
    std::map<std::string, std::string> fingerprint;

    void stamp(const fs::path& path){
        const auto status = fs::symlink_status(path);
        if(fs::is_symlink(status)) throw std::runtime_error("Symbolic link in cleanup candidate: " + path.string());
        fingerprint[path.string()] = std::to_string(static_cast<long long>(fs::last_write_time(path).time_since_epoch().count())) +
            (fs::is_regular_file(status) ? ":" + std::to_string(fs::file_size(path)) : ":directory");
    }
    ofJson dependency(const fs::path& path){
        if(fs::is_symlink(fs::symlink_status(path))) throw std::runtime_error("Saved metadata is a symbolic link");
        const auto json = readJson(path);
        fingerprint[path.string()] = json.dump();
        return json;
    }
    void candidate(const fs::path& path, bool directory){
        // Do not offer a directory containing links or unreadable entries.
        const auto saved = fingerprint;
        try {
            stamp(path);
            if(directory) for(const auto& entry : fs::recursive_directory_iterator(path)) stamp(entry.path());
            candidates[path.lexically_relative(root).generic_string()] = directory;
        } catch(const std::exception&){ fingerprint = saved; }
    }
    void scanFolder(const fs::path& folder){
        if(fs::is_symlink(fs::symlink_status(folder))) throw std::runtime_error("Canvas folder is a symbolic link");
        const auto modules = dependency(folder / "modules.json");
        if(!modules.is_null() && !modules.is_object()) throw std::runtime_error("Invalid saved node list");
        std::set<std::string> listed, local;
        for(auto type = modules.begin(); type != modules.end(); ++type){
            if(!type.value().is_object()) throw std::runtime_error("Invalid saved node list");
            for(auto id = type.value().begin(); id != type.value().end(); ++id){
                const auto name = stem(type.key(), id.key());
                if(name.empty()) throw std::runtime_error("Invalid saved node identity");
                listed.insert(name + ".json");
                const auto file = folder / (name + ".json");
                if(type.key() == "Macro" && fs::is_regular_file(fs::symlink_status(file)) &&
                   dependency(file).value("LocalPreset", false)){
                    local.insert(name);
                    scanFolder(folder / name);
                }
            }
        }
        std::set<std::string> protectedFiles;
        const auto journal = folder / ".oceanode-node-files.json";
        if(fs::exists(journal)){
            const auto json = dependency(journal);
            for(const auto& name : json.at("protectedNodes")) protectedFiles.insert(name.get<std::string>());
        }
        for(const auto& entry : fs::directory_iterator(folder)){
            const auto name = entry.path().filename().string();
            const auto status = entry.symlink_status();
            if(fs::is_symlink(status)) continue;
            const auto relative = entry.path().lexically_relative(root).generic_string();
            if(fs::is_regular_file(status) && nodeFile(name) && !listed.count(name) && !protectedFiles.count(name) && !live.count(relative)){
                const auto base = entry.path().stem().string();
                bool known = false;
                for(const auto& type : registered){
                    if(stem(type, base.substr(base.rfind('_') + 1)) + ".json" == name){ known = true; break; }
                }
                if(known && readJson(entry.path()).is_object()) candidate(entry.path(), false);
            }else if(fs::is_directory(status) && registered.count("Macro") && name.rfind("Macro_", 0) == 0 &&
                     name == stem("Macro", name.substr(6)) && !local.count(name) &&
                     !live.count(relative) &&
                     !protectedFiles.count(name + ".json") && fs::is_regular_file(entry.path() / "modules.json")){
                const auto graph = readJson(entry.path() / "modules.json");
                if(graph.is_null() || graph.is_object()) candidate(entry.path(), true);
            }
        }
    }
    static LegacyPreview scan(const std::string& path, const std::set<std::string>& types,
                              const std::set<std::string>& liveFiles = {}){
        LegacyPreview result;
        result.root = fs::absolute(ofToDataPath(path, true)).lexically_normal();
        result.registered = types;
        result.live = liveFiles;
        result.scanFolder(result.root);
        return result;
    }
    bool removeSelected(const std::set<std::string>& selected, const std::set<std::string>& liveFiles = {}) const {
        const auto now = scan(root.string(), registered, liveFiles);
        if(now.candidates != candidates || now.fingerprint != fingerprint) return false;
        for(const auto& name : selected){
            const auto found = candidates.find(name);
            if(found == candidates.end()) return false;
            std::error_code error;
            if(found->second) fs::remove_all(root / name, error);
            else fs::remove(root / name, error);
            if(error) throw std::runtime_error("Could not remove " + name + ": " + error.message());
        }
        return true;
    }
};

class CleanupReview {
public:
    static CleanupReview& instance(){ static CleanupReview review; return review; }
    void request(const std::shared_ptr<ofxOceanodeContainer>& container){
        target = container;
        pathToScan = container->getPresetStoragePath();
        registered.clear();
        for(const auto& model : container->getRegistry()->getRegisteredModels()) registered.insert(model.first);
        refresh(); open = true;
    }
    void draw(){
        if(!open && !ImGui::IsPopupOpen("Review leftover node files")) return;
        if(open){ ImGui::OpenPopup("Review leftover node files"); open = false; }
        ImGui::SetNextWindowSizeConstraints(ImVec2(650, 0), ImVec2(900, 700));
        if(!ImGui::BeginPopupModal("Review leftover node files", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
        ImGui::TextWrapped("Saved canvas: %s", pathToScan.c_str());
        ImGui::TextWrapped("These files are absent from the saved node lists. Review them before removal. Save your patch first to keep any current edits.");
        const auto container = target.lock();
        const bool sameCanvas = container && container->getPresetStoragePath() == pathToScan;
        if(!sameCanvas) ImGui::TextWrapped("The canvas changed. Close this review and open it again for the current canvas.");
        if(!message.empty()) ImGui::TextWrapped("%s", message.c_str());
        ImGui::Separator();
        if(preview.candidates.empty() && message.empty()) ImGui::TextUnformatted("No leftover node files found.");
        ImGui::BeginChild("LeftoverFiles", ImVec2(650, 260), true);
        for(const auto& item : preview.candidates){
            bool checked = selected.count(item.first) != 0;
            const auto label = item.first + (item.second ? "/ (entire embedded Macro)" : "");
            if(ImGui::Checkbox(label.c_str(), &checked)){
                if(checked) selected.insert(item.first); else selected.erase(item.first);
            }
        }
        ImGui::EndChild();
        ImGui::BeginDisabled(selected.empty() || !message.empty() || !sameCanvas);
        if(ImGui::Button("Remove selected files")){
            try {
                if(preview.removeSelected(selected, liveFiles())) refresh();
                else message = "Files changed since this review. Refresh and review the updated list.";
            } catch(const std::exception& error){ message = error.what(); }
        }
        ImGui::EndDisabled(); ImGui::SameLine();
        if(ImGui::Button("Select all")) for(const auto& item : preview.candidates) selected.insert(item.first);
        ImGui::SameLine();
        if(ImGui::Button("Refresh")) refresh();
        ImGui::SameLine(); if(ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
private:
    std::set<std::string> liveFiles() const {
        std::set<std::string> result;
        std::function<void(ofxOceanodeContainer&, const std::string&)> collect =
        [&](ofxOceanodeContainer& container, const std::string& prefix){
            for(auto* node : container.getAllModules()){
                const auto name = stem(node->getNodeModel().nodeName(), std::to_string(node->getNodeModel().getNumIdentifier()));
                result.insert(prefix + name + ".json");
                auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel());
                if(macro && macro->isLocal()){
                    result.insert(prefix + name);
                    collect(*macro->getContainer(), prefix + name + "/");
                }
            }
        };
        if(auto container = target.lock()) collect(*container, "");
        return result;
    }
    void refresh(){
        selected.clear(); message.clear(); preview = {};
        try { preview = LegacyPreview::scan(pathToScan, registered, liveFiles()); }
        catch(const std::exception& error){ message = error.what(); }
    }
    bool open = false;
    std::string pathToScan, message;
    std::set<std::string> registered, selected;
    LegacyPreview preview;
    std::weak_ptr<ofxOceanodeContainer> target;
};
} // namespace oceanodePresetSave
