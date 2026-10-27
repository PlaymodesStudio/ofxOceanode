//
//  globalPitch.h
//  ofxOceanode
//
//  Global Pitch: wireless pitch controls for every node in the patch that plays
//  or shows pitches (ofxOceanodeGlobalPitch.h): synths (with
//  ofxOceanodeSuperCollider: Boxes, generic SynthDefs, FM7, VST, Pitch cables
//  and piano-roll notes included), MIDI outputs and pitch displays:
//    Scale         the notes move to the nearest degree (pitch classes 0-11)
//    Scale Bypass  turns the scale off (at and above Split Note)
//    Split Note    the input notes below it (MIDI, 0 = off) move to the nearest
//    Low Scale     degree of Low Scale instead, e.g. to stay in tune with a
//                  neighbouring piece in the bass while the rest follows Scale
//    Low Scale Bypass  turns the low scale off (below Split Note)
//    Transpose     semitones, added after the scale (a transpose changes the key)
//    Mod 12        a transposed note wraps into its own octave, which starts at
//    Base Note     (C .. B): it never leaves that octave (cf. Fit Notes In Range)
//    Detune        each voice's own fixed random offset within +-cents, never quantized
//  The inspector lists the destinations with a tick per feature (SampleBox
//  and SlicerBox start unticked). Several Global Pitch nodes show one state.
//

#pragma once

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeGlobalPitch.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <functional>

class globalPitch : public ofxOceanodeNodeModel {
public:
    globalPitch() : ofxOceanodeNodeModel("Global Pitch") {
        description = "Scale, Transpose and Detune for every node in the patch that plays or shows pitches "
                      "(synths, MIDI outputs, piano rolls), without cables. Notes move to the "
                      "nearest degree of Scale (pitch classes 0-11), or of Low Scale "
                      "when below Split Note (MIDI, 0 = off), then Transpose (semitones) is added (Mod 12: wrapped "
                      "into the note's own octave from Base Note), then each "
                      "voice's own random Detune (within +-cents). The inspector chooses which destinations follow "
                      "each one.";
    }
    ~globalPitch() {
        auto& all = instances();
        all.erase(std::remove(all.begin(), all.end(), this), all.end());
        if(!all.empty()) return;   // the last one: the patch has no Global Pitch any more
        ofxOceanodeGlobalPitch::setTranspose(0.0f);
        ofxOceanodeGlobalPitch::setTransposeWrap(false, 0);
        ofxOceanodeGlobalPitch::setScale({}, false);
        ofxOceanodeGlobalPitch::setLowScale({}, 0.0f, false);
        ofxOceanodeGlobalPitch::setDetune(0.0f);
        for(int f = 0; f < ofxOceanodeGlobalPitch::NumFeatures; f++) ofxOceanodeGlobalPitch::setOverridesText(feature(f), "");
    }

    void setup() override {
        const auto& current = ofxOceanodeGlobalPitch::scale();
        addSeparator("Scale");
        addParameter(scale.set("Scale", current.empty() ? std::vector<float>{0, 2, 4, 5, 7, 9, 11} : current,
                               std::vector<float>(1, 0.0f), std::vector<float>(1, 11.0f)));
        addParameter(bypass.set("Scale Bypass", !current.empty() && !ofxOceanodeGlobalPitch::scaleOn()));
        const auto& low = ofxOceanodeGlobalPitch::lowScale();
        addSeparator("Low Scale");
        addParameter(split.set("Split Note", ofxOceanodeGlobalPitch::split(), 0.0f, 127.0f));
        addParameter(lowScale.set("Low Scale", low.empty() ? std::vector<float>{0} : low,
                                  std::vector<float>(1, 0.0f), std::vector<float>(1, 11.0f)));
        addParameter(lowBypass.set("Low Scale Bypass", !low.empty() && !ofxOceanodeGlobalPitch::lowScaleEnabled()));
        addSeparator("Transpose");
        addParameter(transpose.set("Transpose", ofxOceanodeGlobalPitch::transpose(), -48.0f, 48.0f));
        addParameter(wrap.set("Mod 12", ofxOceanodeGlobalPitch::transposeWraps()));
        addParameterDropdown(wrapBase, "Base Note", ofxOceanodeGlobalPitch::transposeBase(),
                             {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
        addSeparator("Detune");
        addParameter(detune.set("Detune", ofxOceanodeGlobalPitch::detune(), 0.0f, 100.0f));
        // the destinations that differ from their default, one string per feature (ofxOceanodeGlobalPitch::overridesText)
        static const char* names[ofxOceanodeGlobalPitch::NumFeatures] = {"Transpose Destinations", "Scale Destinations", "Detune Destinations"};
        for(int f = 0; f < ofxOceanodeGlobalPitch::NumFeatures; f++) {
            addInspectorParameter(destinations[f].set(names[f], ofxOceanodeGlobalPitch::overridesText(feature(f))));
            listeners.push(destinations[f].newListener([this, f](std::string& text){
                ofxOceanodeGlobalPitch::setOverridesText(feature(f), text);
                share([f, text](globalPitch* n){ n->destinations[f] = text; });
            }));
        }
        instances().push_back(this);

        auto sendScale = [this](){
            const std::vector<float> s = scale.get();
            const bool b = bypass.get();
            ofxOceanodeGlobalPitch::setScale(s, !b);
            share([s, b](globalPitch* n){ n->scale = s; n->bypass = b; });
        };
        listeners.push(scale.newListener([sendScale](std::vector<float>&){ sendScale(); }));
        listeners.push(bypass.newListener([sendScale](bool&){ sendScale(); }));
        auto sendLowScale = [this](){
            const std::vector<float> s = lowScale.get();
            const float at = split.get();
            const bool b = lowBypass.get();
            ofxOceanodeGlobalPitch::setLowScale(s, at, !b);
            share([s, at, b](globalPitch* n){ n->lowScale = s; n->split = at; n->lowBypass = b; });
        };
        listeners.push(lowScale.newListener([sendLowScale](std::vector<float>&){ sendLowScale(); }));
        listeners.push(lowBypass.newListener([sendLowScale](bool&){ sendLowScale(); }));
        listeners.push(split.newListener([sendLowScale](float&){ sendLowScale(); }));
        listeners.push(transpose.newListener([this](float& v){
            ofxOceanodeGlobalPitch::setTranspose(v);
            share([v](globalPitch* n){ n->transpose = v; });
        }));
        auto sendWrap = [this](){
            const bool on = wrap.get();
            const int base = wrapBase.get();
            ofxOceanodeGlobalPitch::setTransposeWrap(on, base);
            share([on, base](globalPitch* n){ n->wrap = on; n->wrapBase = base; });
        };
        listeners.push(wrap.newListener([sendWrap](bool&){ sendWrap(); }));
        listeners.push(wrapBase.newListener([sendWrap](int&){ sendWrap(); }));
        listeners.push(detune.newListener([this](float& v){
            ofxOceanodeGlobalPitch::setDetune(v);
            share([v](globalPitch* n){ n->detune = v; });
        }));
        sendLowScale();
        sendScale();
    }

    // presets from before Low Scale Bypass: Scale Bypass turned both scales off
    void presetRecallAfterSettingParameters(ofJson& json) override {
        if(!json.contains(lowBypass.getEscapedName())) lowBypass = bypass.get();
    }

    // one table for the three destination strings: a row per destination, a tick per feature
    bool drawInspectorParameter(ofAbstractParameter& parameter) override {
        if(parameter.isReferenceTo(destinations[1]) || parameter.isReferenceTo(destinations[2])) return true;
        if(!parameter.isReferenceTo(destinations[0])) return false;
        std::vector<ofxOceanodeGlobalPitch::Follower> list = ofxOceanodeGlobalPitch::destinations();
        std::sort(list.begin(), list.end(), [](auto& a, auto& b){
            return ofxOceanodeGlobalPitch::key(a.node) < ofxOceanodeGlobalPitch::key(b.node);
        });
        ImGui::TextUnformatted("Destinations");
        if(list.empty()) { ImGui::TextDisabled("Nothing pitched in the patch"); return true; }
        std::array<bool, ofxOceanodeGlobalPitch::NumFeatures> changed{};
        if(ImGui::BeginTable("##globalPitchDestinations", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Scale");
            ImGui::TableSetupColumn("Transp.");
            ImGui::TableSetupColumn("Detune");
            ImGui::TableHeadersRow();
            static const int order[3] = {ofxOceanodeGlobalPitch::Scale, ofxOceanodeGlobalPitch::Transpose, ofxOceanodeGlobalPitch::Detune};
            for(auto& d : list) {
                const auto k = ofxOceanodeGlobalPitch::key(d.node);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(k.c_str());
                if(!d.byDefault && ImGui::IsItemHovered()) ImGui::SetTooltip("Left out by default");
                for(int f : order) {
                    ImGui::TableNextColumn();
                    bool on = ofxOceanodeGlobalPitch::follows(d.node, feature(f));
                    ImGui::PushID((k + "/" + ofToString(f)).c_str());
                    if(ImGui::Checkbox("##on", &on)) { ofxOceanodeGlobalPitch::setFollows(d.node, feature(f), on); changed[f] = true; }
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
        if(ImGui::SmallButton("Defaults")) {
            for(int f = 0; f < ofxOceanodeGlobalPitch::NumFeatures; f++) { ofxOceanodeGlobalPitch::setOverridesText(feature(f), ""); changed[f] = true; }
        }
        for(int f = 0; f < ofxOceanodeGlobalPitch::NumFeatures; f++)
            if(changed[f]) destinations[f] = ofxOceanodeGlobalPitch::overridesText(feature(f));
        return true;
    }

private:
    static ofxOceanodeGlobalPitch::Feature feature(int f) { return static_cast<ofxOceanodeGlobalPitch::Feature>(f); }
    static std::vector<globalPitch*>& instances() { static std::vector<globalPitch*> v; return v; }
    // the other Global Pitch nodes show the same state
    void share(std::function<void(globalPitch*)> apply) {
        static bool sharing = false;
        if(sharing) return;
        sharing = true;
        for(auto* n : instances()) if(n != this) apply(n);
        sharing = false;
    }

    ofParameter<std::vector<float>> scale, lowScale;
    ofParameter<float> split;
    ofParameter<bool> bypass, lowBypass;
    ofParameter<float> transpose, detune;
    ofParameter<bool> wrap;
    ofParameter<int> wrapBase;
    std::array<ofParameter<std::string>, ofxOceanodeGlobalPitch::NumFeatures> destinations;
    ofEventListeners listeners;
};
