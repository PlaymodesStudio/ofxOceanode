#pragma once

#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <stdexcept>

// Preset-owned, explicit snapshots. Saving a preset never overwrites a named
// snapshot; the user chooses Update when a new arrangement should replace it.
class ofxOceanodeWorkspaces {
public:
    using Json = nlohmann::json;
    std::map<std::string, Json> entries;
    std::string active;
    std::string defaultLayout;

    std::string layoutOnLoad() const {
        return entries.count(defaultLayout) ? defaultLayout : active;
    }

    static std::string cleanName(const std::string& name){
        const auto first = name.find_first_not_of(" \t\r\n");
        if(first == std::string::npos) return {};
        return name.substr(first, name.find_last_not_of(" \t\r\n") - first + 1);
    }
    static bool validName(const std::string& name){
        for(unsigned char c : name) if(c < 32 || c == 127) return false;
        return !name.empty() && name.size() < 256 && cleanName(name) == name &&
            name.find("##") == std::string::npos;
    }
    void create(const std::string& name, Json snapshot){
        if(!validName(name)) throw std::runtime_error("Enter a name (1-255 bytes, without ## or line breaks).");
        if(entries.count(name)) throw std::runtime_error("A preset layout with that name already exists.");
        entries.emplace(name, std::move(snapshot));
        active = name;
    }
    void rename(const std::string& from, const std::string& to){
        if(from == to) return;
        if(!validName(to)) throw std::runtime_error("Enter a name (1-255 bytes, without ## or line breaks).");
        if(entries.count(to)) throw std::runtime_error("A preset layout with that name already exists.");
        entries.emplace(to, entries.at(from));
        entries.erase(from);
        if(active == from) active = to;
        if(defaultLayout == from) defaultLayout = to;
    }
    void erase(const std::string& name){
        entries.erase(name);
        if(active == name) active.clear();
        if(defaultLayout == name) defaultLayout.clear();
    }
    Json toJson() const { return {{"version", 1}, {"active", active}, {"default", defaultLayout}, {"workspaces", entries}}; }
    void fromJson(const Json& json){
        ofxOceanodeWorkspaces loaded;
        if(!json.is_object() || json.at("version") != 1 || !json.at("workspaces").is_object())
            throw std::runtime_error("Unsupported preset layout file.");
        for(auto it = json.at("workspaces").begin(); it != json.at("workspaces").end(); ++it){
            const auto& snapshot = it.value();
            if(!validName(it.key()) || !snapshot.is_object() ||
               !snapshot.at("layout").is_string() || snapshot.at("layout").get<std::string>().empty() ||
               !snapshot.at("controllers").is_object() || !snapshot.at("windows").is_object())
                throw std::runtime_error("Invalid preset layout snapshot.");
            loaded.entries.emplace(it.key(), snapshot);
        }
        loaded.active = json.value("active", std::string());
        if(!loaded.entries.count(loaded.active)) loaded.active.clear();
        loaded.defaultLayout = json.value("default", std::string());
        if(!loaded.entries.count(loaded.defaultLayout)) loaded.defaultLayout.clear();
        *this = std::move(loaded);
    }
};
