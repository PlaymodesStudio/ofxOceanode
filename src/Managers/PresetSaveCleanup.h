#pragma once

#include "ofMain.h"
#include <algorithm>
#include <climits>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#ifdef TARGET_WIN32
#include <windows.h>
#endif

// A save can span a preset and many embedded Macros. Commit their node lists
// and prune owned files only after every participating writer has succeeded.
namespace oceanodePresetSave {
namespace fs = std::filesystem;

struct Files {
    std::set<std::string> nodes, macros, protectedNodes;
};
struct Plan {
    fs::path folder;
    Files owned, current;
    ofJson modules;
};
struct State {
    bool ok = true;
    std::map<std::string, Plan> plans;
};
inline thread_local State* active = nullptr;

inline bool record(bool result){
    if(active && !result) active->ok = false;
    return result;
}

inline bool replaceFile(const fs::path& source, const fs::path& destination){
#ifdef TARGET_WIN32
    return MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
    std::error_code error;
    fs::rename(source, destination, error);
    return !error;
#endif
}

inline bool writeJson(const fs::path& path, const ofJson& json){
    const auto content = json.dump(4);
    const fs::path temporary = path.parent_path() / ("." + path.filename().string() + ".oceanode-save.tmp");
    std::error_code error;
    if(fs::is_symlink(fs::symlink_status(path, error)) ||
       fs::is_symlink(fs::symlink_status(temporary, error))) return false;
    std::ofstream output(temporary, std::ios::trunc);
    output << content << '\n';
    output.close();
    const bool saved = output && replaceFile(temporary, path);
    if(!saved && fs::is_regular_file(fs::symlink_status(temporary, error))) fs::remove(temporary, error);
    return saved;
}

inline bool saveJson(const std::string& path, const ofJson& json){
    bool saved = false;
    try {
        const fs::path absolute = ofToDataPath(path, true);
        fs::create_directories(absolute.parent_path());
        saved = writeJson(absolute, json);
    } catch(const std::exception& error){
        ofLogError("Preset save") << error.what();
    }
    if(!saved) ofLogError("Preset save") << "Could not write " << path;
    return record(saved);
}

inline bool safeStem(const std::string& name){
    return !name.empty() && name != "." && name != ".." &&
        name.find_first_of("/\\:") == std::string::npos;
}

inline std::string stem(const std::string& type, const std::string& id){
    if(!safeStem(type) || id.empty() || id.find_first_not_of("0123456789") != std::string::npos) return "";
    try {
        const auto number = std::stoll(id);
        if(number < 0 || number > INT_MAX) return "";
        std::string name = type;
        std::replace(name.begin(), name.end(), ' ', '_');
        return name + "_" + std::to_string(number);
    } catch(...) { return ""; }
}

inline bool nodeFile(const std::string& name){
    if(!safeStem(name) || fs::path(name).extension() != ".json") return false;
    const auto base = fs::path(name).stem().string();
    const auto split = base.rfind('_');
    return split != std::string::npos && stem(base.substr(0, split), base.substr(split + 1)) == base;
}

inline ofJson readJson(const fs::path& path){
    std::ifstream input(path);
    if(!input) throw std::runtime_error("Cannot read " + path.string());
    ofJson json;
    input >> json;
    return json;
}

inline ofJson manifest(const Files& files){
    return {{"version", 1}, {"nodes", files.nodes}, {"localMacros", files.macros},
            {"protectedNodes", files.protectedNodes}};
}

// Write the ownership journal atomically before touching existing graph files.
// Keeping the union across failed saves makes cleanup retryable even when a
// later node-list commit fails partway through a nested save.
inline bool writeManifest(const fs::path& folder, const Files& files){
    const auto path = folder / ".oceanode-node-files.json";
    return writeJson(path, manifest(files));
}

inline void merge(Files& into, const Files& from){
    into.nodes.insert(from.nodes.begin(), from.nodes.end());
    into.macros.insert(from.macros.begin(), from.macros.end());
    into.protectedNodes.insert(from.protectedNodes.begin(), from.protectedNodes.end());
}

class Scope {
public:
    Scope(): owner(active == nullptr){ if(owner) active = &state; }
    ~Scope(){ if(owner) active = nullptr; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    bool prepare(const std::string& path, const ofJson& modules,
                 const std::set<std::string>& registered){
        try {
            Plan plan;
            plan.folder = fs::absolute(ofToDataPath(path, true)).lexically_normal();
            plan.modules = modules;
            if(fs::is_symlink(fs::symlink_status(plan.folder))) throw std::runtime_error("Save folder is a symbolic link");
            fs::create_directories(plan.folder);
            const auto journal = plan.folder / ".oceanode-node-files.json";
            if(fs::exists(journal)){
                const auto json = readJson(journal);
                if(json.at("version") != 1) throw std::runtime_error("Unknown ownership journal version");
                for(const auto& item : json.at("nodes")){
                    const auto name = item.get<std::string>();
                    if(!nodeFile(name)) throw std::runtime_error("Invalid owned node filename");
                    plan.owned.nodes.insert(name);
                }
                for(const auto& item : json.at("localMacros")){
                    const auto name = item.get<std::string>();
                    if(name.rfind("Macro_", 0) != 0 || name != stem("Macro", name.substr(6)))
                        throw std::runtime_error("Invalid owned Macro directory");
                    plan.owned.macros.insert(name);
                }
                for(const auto& item : json.at("protectedNodes")){
                    const auto name = item.get<std::string>();
                    if(!nodeFile(name)) throw std::runtime_error("Invalid protected node filename");
                    plan.owned.protectedNodes.insert(name);
                }
            }
            const auto oldPath = plan.folder / "modules.json";
            if(fs::is_symlink(fs::symlink_status(oldPath))) throw std::runtime_error("Node list is a symbolic link");
            if(fs::exists(oldPath)){
                const auto previous = readJson(oldPath);
                if(!previous.is_null() && !previous.is_object()) throw std::runtime_error("Invalid modules.json");
                for(auto type = previous.begin(); type != previous.end(); ++type){
                    if(!type.value().is_object()) throw std::runtime_error("Invalid saved node list");
                    for(auto id = type.value().begin(); id != type.value().end(); ++id){
                        const auto name = stem(type.key(), id.key());
                        if(name.empty()) throw std::runtime_error("Invalid saved node identity");
                        plan.owned.nodes.insert(name + ".json");
                        if(!registered.count(type.key())) plan.owned.protectedNodes.insert(name + ".json");
                        if(type.key() == "Macro"){
                            const auto file = plan.folder / (name + ".json");
                            if(fs::is_regular_file(fs::symlink_status(file)) &&
                               readJson(file).value("LocalPreset", false)) plan.owned.macros.insert(name);
                        }
                    }
                }
            }
            for(auto type = modules.begin(); type != modules.end(); ++type){
                for(auto id = type.value().begin(); id != type.value().end(); ++id){
                    const auto name = stem(type.key(), id.key());
                    if(name.empty()) throw std::runtime_error("Invalid current node identity");
                    plan.current.nodes.insert(name + ".json");
                    plan.owned.protectedNodes.erase(name + ".json");
                }
            }
            const auto key = plan.folder.string();
            auto existing = active->plans.find(key);
            if(existing != active->plans.end()) merge(plan.owned, existing->second.owned);
            merge(plan.owned, plan.current);
            if(!writeManifest(plan.folder, plan.owned)) throw std::runtime_error("Cannot write ownership journal");
            active->plans[key] = std::move(plan);
            return true;
        } catch(const std::exception& error){
            ofLogError("Preset save") << path << ": " << error.what();
            return record(false);
        }
    }

    // Called by the node writer, where the final local/global selection is known.
    static void localMacro(const std::string& path, const std::string& name){
        if(!active) return;
        const auto key = fs::absolute(ofToDataPath(path, true)).lexically_normal().string();
        auto plan = active->plans.find(key);
        if(plan != active->plans.end()){
            plan->second.current.macros.insert(name);
            plan->second.owned.macros.insert(name);
            record(writeManifest(plan->second.folder, plan->second.owned));
        }
    }

    bool finish(){
        if(!owner) return active->ok;
        if(!state.ok) return false;
        // Persist ownership discovered while saving before publishing node lists.
        for(auto& entry : state.plans)
            if(!record(writeManifest(entry.second.folder, entry.second.owned))) return false;
        for(auto& entry : state.plans)
            if(!saveJson((entry.second.folder / "modules.json").string(), entry.second.modules)) return false;
        for(auto& entry : state.plans){
            auto& plan = entry.second;
            Files kept = plan.current;
            kept.protectedNodes = plan.owned.protectedNodes;
            for(const auto& name : plan.owned.nodes){
                if(plan.current.nodes.count(name)) continue;
                if(plan.owned.protectedNodes.count(name) || !remove(plan.folder / name, false)) kept.nodes.insert(name);
            }
            for(const auto& name : plan.owned.macros){
                if(plan.current.macros.count(name)) continue;
                if(plan.owned.protectedNodes.count(name + ".json") || !remove(plan.folder / name, true)) kept.macros.insert(name);
            }
            // The graph is committed. Failure to compact the journal is only
            // housekeeping: its previous union still owns every retained file.
            if(!writeManifest(plan.folder, kept))
                ofLogWarning("Preset cleanup") << "Keeping previous ownership journal in " << plan.folder;
        }
        return state.ok;
    }

private:
    static bool remove(const fs::path& path, bool directory){
        std::error_code error;
        const auto status = fs::symlink_status(path, error);
        if(status.type() == fs::file_type::not_found) return true;
        // Never traverse links or remove a file of an unexpected kind.
        if(error || fs::is_symlink(status) || (directory ? !fs::is_directory(status) : !fs::is_regular_file(status))){
            ofLogWarning("Preset cleanup") << "Keeping unexpected path " << path;
            return false;
        }
        if(directory) fs::remove_all(path, error);
        else fs::remove(path, error);
        if(error) ofLogWarning("Preset cleanup") << "Could not remove " << path << ": " << error.message();
        return !error;
    }
    bool owner;
    State state;
};
} // namespace oceanodePresetSave
