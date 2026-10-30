//
//  ofxOceanodeNodeSnapshots.h
//  ofxOceanode
//
//  A node's own snapshots: slots of saved parameter values (the parameters
//  the Inspector marks for snapshots), recalled with a glide of Morph ms
//  shaped by BiPow, as Macro snapshots are. Owned by ofxOceanodeNode, which
//  captures, applies and saves them with the node's preset.
//

#ifndef ofxOceanodeNodeSnapshots_h
#define ofxOceanodeNodeSnapshots_h

#include "ofJson.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct ofxOceanodeNodeSnapshots {
    static constexpr int numSlots = 16;

    struct Slot {
        std::string name;
        // Values keyed by escaped parameter name, as saved in presets.
        ofJson values;
    };
    std::map<int, Slot> slots;
    int currentSlot = -1;

    float morphMs = 0.f;
    float morphBiPow = 0.f;
    // Draws a snapshot selector at the top of the node.
    bool showSelectorInNode = false;

    // Glide in progress: start and target of each parameter that
    // interpolates, as floats (a color is r, g, b, a). Discrete values were
    // set when the recall began.
    struct GlideTrack {
        std::string parameter;  // escaped name
        std::vector<float> from, to;
    };
    std::vector<GlideTrack> glideTracks;
    bool gliding = false;
    uint64_t glideStartMs = 0;
    float glideMs = 0.f;
    float glideBiPow = 0.f;
    float progress = 0.f;

    bool hasSlot(int slot) const { return slots.count(slot) != 0; }
    std::string slotLabel(int slot) const {
        auto it = slots.find(slot);
        if(it == slots.end() || it->second.name.empty()) return std::to_string(slot);
        return it->second.name;
    }
    int firstFreeSlot() const {
        for(int i = 0; i < numSlots; i++) if(!hasSlot(i)) return i;
        return -1;
    }

    ofJson toJson() const {
        ofJson json;
        json["MorphMs"] = morphMs;
        json["BiPow"] = morphBiPow;
        json["ShowInNode"] = showSelectorInNode;
        json["Current"] = currentSlot;
        json["Slots"] = ofJson::object();
        for(const auto& slot : slots){
            json["Slots"][std::to_string(slot.first)] = {{"Name", slot.second.name}, {"Values", slot.second.values}};
        }
        return json;
    }

    void fromJson(const ofJson& json){
        *this = ofxOceanodeNodeSnapshots();
        if(!json.is_object()) return;
        morphMs = json.value("MorphMs", 0.f);
        morphBiPow = json.value("BiPow", 0.f);
        showSelectorInNode = json.value("ShowInNode", false);
        currentSlot = json.value("Current", -1);
        if(json.contains("Slots") && json["Slots"].is_object()){
            for(auto it = json["Slots"].begin(); it != json["Slots"].end(); ++it){
                int index = -1;
                try { index = std::stoi(it.key()); } catch(...) { continue; }
                if(index < 0 || index >= numSlots || !it.value().is_object()) continue;
                Slot slot;
                slot.name = it.value().value("Name", std::string());
                if(it.value().contains("Values")) slot.values = it.value()["Values"];
                slots[index] = slot;
            }
        }
        if(!hasSlot(currentSlot)) currentSlot = -1;
    }

    bool isDefault() const {
        return slots.empty() && morphMs == 0.f && morphBiPow == 0.f && !showSelectorInNode;
    }
};

#endif /* ofxOceanodeNodeSnapshots_h */
