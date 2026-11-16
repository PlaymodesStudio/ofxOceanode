//
//  globalPitch.h
//  ofxOceanode
//
//  Global Pitch: wireless pitch controls for every node in the patch that plays
//  or shows pitches (ofxOceanodeGlobalPitch.h): synths (with
//  ofxOceanodeSuperCollider: Boxes, generic SynthDefs, FM7, VST, Pitch cables
//  and piano-roll notes included), MIDI outputs and pitch displays:
//    Scope         which destinations it reaches: All Levels (the main canvas
//                  and every macro), This Level (the canvas it sits in), or
//                  This Level + Submacros. A destination takes the nearest
//                  one: its own level's This Level, then This Level +
//                  Submacros of its level or the nearest parent, then All Levels
//    Scale Root    (C .. B) the pitch class both scales' degrees count from
//    Scale         the notes move to the nearest degree (semitones 0-11 above
//                  Scale Root: E Phrygian dominant is root E, 0,1,4,5,7,8,10)
//    Scale Bypass  turns the scale off (at and above Split Note)
//    Shift         the quantized notes move this many degrees along Scale,
//                  into the next octave past the last degree (a diatonic
//                  transpose: a melody keeps its contour). -size .. +size
//    Split Note    the input notes below it (MIDI, 0 = off) move to the nearest
//    Low Scale     degree of Low Scale instead, e.g. to stay in tune with a
//                  neighbouring piece in the bass while the rest follows Scale
//    Low Bypass    turns the low scale off (below Split Note)
//    Low Shift     the same along Low Scale, below Split Note
//    Negative      negative harmony (Jacob Collier): after the scales, every
//    Negative Key  note mirrors around the axis between the key's minor and
//                  major third (C <-> G, E <-> Eb), staying in its register.
//                  Follows the Scale ticks
//    Transpose     semitones, added after the scale (a transpose changes the key)
//    Mod 12        a transposed note wraps into its own octave, which starts at
//    Base Note     (C .. B): it never leaves that octave (cf. Fit Notes In Range)
//    Detune        each voice's own fixed random offset within +-cents, never quantized
//    Glide         seconds a Transpose, Scale or Shift change takes to reach the
//                  notes still sounding (synths; a MIDI note takes the value of its instant)
//  The inspector lists the destinations with a tick per feature (SampleBox
//  and SlicerBox start unticked) for the destinations it reaches. Global Pitch
//  nodes of the same scope (on the same canvas, unless All Levels) show one
//  state; each scope in use has its own (ofxOceanodeGlobalPitch::MaxScopes at once).
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
                      "nearest degree of Scale (semitones 0-11 above Scale Root), or of Low Scale "
                      "when below Split Note (MIDI, 0 = off), move Shift (Low Shift below the split) degrees along it, "
                      "carried into the next octave, mirrored by Negative harmony around Negative Key "
                      "(the axis between its minor and major third), then Transpose (semitones) is added (Mod 12: wrapped "
                      "into the note's own octave from Base Note), then each "
                      "voice's own random Detune (within +-cents). Glide: seconds a change takes to reach the sounding "
                      "notes (synths; a MIDI note takes the value of its instant). The inspector chooses which destinations follow each one. "
                      "Scope: All Levels reaches the main canvas and every macro, This Level only the canvas the node sits in, "
                      "This Level + Submacros that canvas and the macros inside it; a destination follows the nearest Global Pitch reaching it.";
    }
    ~globalPitch() {
        auto& all = instances();
        all.erase(std::remove(all.begin(), all.end(), this), all.end());
        ofxOceanodeGlobalPitch::release(slot);   // the last one of its scope: that state returns to neutral
    }

    void setup() override {
        slot = ofxOceanodeGlobalPitch::claim(ofxOceanodeGlobalPitch::All, getParents());
        joined = 0;
        addParameterDropdown(scope, "Scope", 0, {"All Levels", "This Level", "This Level + Submacros"},
                             ofxOceanodeParameterFlags_DisableInConnection | ofxOceanodeParameterFlags_DisableOutConnection);
        const auto& current = ofxOceanodeGlobalPitch::scale(slot);
        addSeparator("Scale");
        addParameterDropdown(root, "Scale Root", ofxOceanodeGlobalPitch::scaleRoot(slot), noteNames());
        addParameter(scale.set("Scale", current.empty() ? std::vector<float>{0, 2, 4, 5, 7, 9, 11} : current,
                               std::vector<float>(1, 0.0f), std::vector<float>(1, 11.0f)));
        addParameter(bypass.set("Scale Bypass", !current.empty() && !ofxOceanodeGlobalPitch::scaleOn(slot)));
        addParameter(shift.set("Shift", ofxOceanodeGlobalPitch::shift(slot), -size(scale.get()), size(scale.get())));
        const auto& low = ofxOceanodeGlobalPitch::lowScale(slot);
        addSeparator("Low Scale");
        addParameter(split.set("Split Note", ofxOceanodeGlobalPitch::split(slot), 0.0f, 127.0f));
        addParameter(lowScale.set("Low Scale", low.empty() ? std::vector<float>{0} : low,
                                  std::vector<float>(1, 0.0f), std::vector<float>(1, 11.0f)));
        addParameter(lowBypass.set("Low Bypass", !low.empty() && !ofxOceanodeGlobalPitch::lowScaleEnabled(slot)));
        addParameter(lowShift.set("Low Shift", ofxOceanodeGlobalPitch::lowShift(slot), -size(lowScale.get()), size(lowScale.get())));
        addSeparator("Negative Harmony");
        addParameter(negative.set("Negative", ofxOceanodeGlobalPitch::negativeOn(slot)));
        addParameterDropdown(negativeKey, "Negative Key", ofxOceanodeGlobalPitch::negativeKey(slot), noteNames());
        addSeparator("Transpose");
        addParameter(transpose.set("Transpose", ofxOceanodeGlobalPitch::transpose(slot), -48.0f, 48.0f));
        addParameter(wrap.set("Mod 12", ofxOceanodeGlobalPitch::transposeWraps(slot)));
        addParameterDropdown(wrapBase, "Base Note", ofxOceanodeGlobalPitch::transposeBase(slot), noteNames());
        addSeparator("Detune");
        addParameter(detune.set("Detune", ofxOceanodeGlobalPitch::detune(slot), 0.0f, 100.0f));
        addSeparator("Glide");
        addParameter(glide.set("Glide", ofxOceanodeGlobalPitch::glide(slot), 0.0f, 10.0f));
        // the destinations that differ from their default, one string per feature (ofxOceanodeGlobalPitch::overridesText)
        static const char* names[ofxOceanodeGlobalPitch::NumFeatures] = {"Transpose Destinations", "Scale Destinations", "Detune Destinations"};
        for(int f = 0; f < ofxOceanodeGlobalPitch::NumFeatures; f++) {
            addInspectorParameter(destinations[f].set(names[f], ofxOceanodeGlobalPitch::overridesText(feature(f), slot)));
            listeners.push(destinations[f].newListener([this, f](std::string&){ sendDestinations(f); }));
        }
        instances().push_back(this);

        listeners.push(scope.newListener([this](int&){ join(); }));
        listeners.push(scale.newListener([this](std::vector<float>&){ sendScale(); }));
        listeners.push(root.newListener([this](int&){ sendRoot(); }));
        listeners.push(bypass.newListener([this](bool&){ sendScale(); }));
        listeners.push(lowScale.newListener([this](std::vector<float>&){ sendLowScale(); }));
        listeners.push(lowBypass.newListener([this](bool&){ sendLowScale(); }));
        listeners.push(split.newListener([this](float&){ sendLowScale(); }));
        listeners.push(shift.newListener([this](int&){ sendShift(); }));
        listeners.push(lowShift.newListener([this](int&){ sendLowShift(); }));
        listeners.push(negative.newListener([this](bool&){ sendNegative(); }));
        listeners.push(negativeKey.newListener([this](int&){ sendNegative(); }));
        listeners.push(transpose.newListener([this](float&){ sendTranspose(); }));
        listeners.push(wrap.newListener([this](bool&){ sendWrap(); }));
        listeners.push(wrapBase.newListener([this](int&){ sendWrap(); }));
        listeners.push(detune.newListener([this](float&){ sendDetune(); }));
        listeners.push(glide.newListener([this](float&){ sendGlide(); }));
        // Its values reach the state on the first update, not here: a preset
        // sets Scope first (presetRecallBeforeSettingParameters), and a node
        // of another scope must never write its default scale into All Levels
        unsent = true;
    }

    // the glide: the registry's lagged values move each frame while a Global
    // Pitch node exists (the last one of a scope leaving sets its Glide to 0)
    void update(ofEventArgs&) override {
        if(slot < 0) join();   // every scope was taken: one may be free now
        if(unsent) { unsent = false; send(); }
        ofxOceanodeGlobalPitch::advance();
    }

    // The scope first: the values that follow belong to its state, not to the
    // one the node held before the load. A preset from before Scope is All Levels.
    void presetRecallBeforeSettingParameters(ofJson& json) override {
        const auto key = scope.getEscapedName();
        int saved = 0;
        if(json.contains(key)) {
            if(json[key].is_number()) saved = json[key].get<int>();
            else if(json[key].is_string()) saved = ofToInt(json[key].get<std::string>());
        }
        scope = std::clamp(saved, 0, 2);
    }

    // every scope is in use: this node acts on nothing until one is free
    std::string getNodeTitleSuffix() const override { return slot < 0 ? "(!)" : ""; }
    std::string getNodeTitleTooltip() const override {
        return slot < 0 ? "Too many Global Pitch scopes at once (" + ofToString(ofxOceanodeGlobalPitch::MaxScopes - 1)
                          + " besides All Levels): this one does nothing" : "";
    }

    // presets saved as "Low Scale Bypass" (its earlier name), and from before
    // it existed, when Scale Bypass turned both scales off
    void presetRecallAfterSettingParameters(ofJson& json) override {
        if(!json.contains(root.getEscapedName())) root = 0;   // from before Scale Root: degrees counted from C
        if(!json.contains(negative.getEscapedName())) negative = false;   // from before Negative Harmony
        if(json.contains(lowBypass.getEscapedName())) return;
        ofParameter<bool> earlier;
        earlier.set("Low Scale Bypass", false);
        const auto key = earlier.getEscapedName();
        lowBypass = json.contains(key) && json[key].is_boolean() ? json[key].get<bool>() : bypass.get();
    }

    // one table for the three destination strings: a row per destination, a tick per feature
    bool drawInspectorParameter(ofAbstractParameter& parameter) override {
        if(parameter.isReferenceTo(destinations[1]) || parameter.isReferenceTo(destinations[2])) return true;
        if(!parameter.isReferenceTo(destinations[0])) return false;
        // the destinations this node's state reaches (another scope may be nearer to the rest)
        std::vector<ofxOceanodeGlobalPitch::Follower> list;
        for(auto& d : ofxOceanodeGlobalPitch::destinations())
            if(slot >= 0 && ofxOceanodeGlobalPitch::slotOf(d.node) == slot) list.push_back(d);
        std::sort(list.begin(), list.end(), [](auto& a, auto& b){
            return ofxOceanodeGlobalPitch::key(a.node) < ofxOceanodeGlobalPitch::key(b.node);
        });
        ImGui::TextUnformatted("Destinations");
        if(list.empty()) { ImGui::TextDisabled(scope.get() == 0 ? "Nothing pitched in the patch" : "Nothing pitched in this scope"); return true; }
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
                const auto parts = d.parts ? d.parts() : std::vector<ofxOceanodeGlobalPitch::Part>{};
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(k.c_str());
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s%s", d.byDefault ? "" : "Left out by default\n",
                                      parts.empty() ? "" : "Its ticks set every part below; each part can differ");
                for(int f : order) {
                    ImGui::TableNextColumn();
                    // a node with parts: ticked when every part follows; a click sets them all
                    bool on = ofxOceanodeGlobalPitch::follows(d.node, feature(f));
                    if(!parts.empty()) {
                        on = true;
                        for(auto& p : parts) on = on && ofxOceanodeGlobalPitch::follows(d.node, p.id, feature(f));
                    }
                    ImGui::PushID((k + "/" + ofToString(f)).c_str());
                    if(ImGui::Checkbox("##on", &on)) {
                        ofxOceanodeGlobalPitch::setFollows(d.node, feature(f), on);
                        for(auto& p : parts) ofxOceanodeGlobalPitch::setFollows(d.node, p.id, feature(f), on);
                        changed[f] = true;
                    }
                    ImGui::PopID();
                }
                for(auto& p : parts) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Indent();
                    ImGui::TextUnformatted(p.label.c_str());
                    ImGui::Unindent();
                    for(int f : order) {
                        ImGui::TableNextColumn();
                        bool on = ofxOceanodeGlobalPitch::follows(d.node, p.id, feature(f));
                        ImGui::PushID((ofxOceanodeGlobalPitch::key(d.node, p.id) + "/" + ofToString(f)).c_str());
                        if(ImGui::Checkbox("##on", &on)) { ofxOceanodeGlobalPitch::setFollows(d.node, p.id, feature(f), on); changed[f] = true; }
                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndTable();
        }
        if(ImGui::SmallButton("Defaults")) {
            for(int f = 0; f < ofxOceanodeGlobalPitch::NumFeatures; f++) { ofxOceanodeGlobalPitch::setOverridesText(feature(f), "", slot); changed[f] = true; }
        }
        for(int f = 0; f < ofxOceanodeGlobalPitch::NumFeatures; f++)
            if(changed[f]) destinations[f] = ofxOceanodeGlobalPitch::overridesText(feature(f), slot);
        return true;
    }

private:
    static ofxOceanodeGlobalPitch::Feature feature(int f) { return static_cast<ofxOceanodeGlobalPitch::Feature>(f); }
    static std::vector<std::string> noteNames() { return {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}; }
    // a shift's range: an octave of degrees down and up (shifting by the size is an octave)
    static int size(const std::vector<float>& degrees) {
        return std::max(1, (int)ofxOceanodeGlobalPitch::distinctDegrees(degrees).size());
    }
    // the scale changed size: the shift's range follows; a shift past it is clamped
    static void fitShift(ofParameter<int>& shift, const std::vector<float>& degrees) {
        const int n = size(degrees);
        if(shift.getMax() != n) { shift.setMin(-n); shift.setMax(n); }
        if(std::abs(shift.get()) > n) shift = std::clamp(shift.get(), -n, n);
    }
    static std::vector<globalPitch*>& instances() { static std::vector<globalPitch*> v; return v; }
    // the other Global Pitch nodes of this scope show the same state
    void share(std::function<void(globalPitch*)> apply) {
        static bool sharing = false;
        if(sharing) return;
        sharing = true;
        for(auto* n : instances()) if(n != this && n->slot == slot) apply(n);
        sharing = false;
    }
    // a parameter changed by hand, by a cable or by a preset (not while the
    // node takes over another state's values)
    bool live() const { return slot >= 0 && !adopting; }

    // Scope changed: the node leaves its state for the one of its new scope.
    // One that other nodes already hold: it shows their values. A new one
    // takes this node's values.
    void join() {
        if(slot >= 0 && scope.get() == joined) return;
        ofxOceanodeGlobalPitch::release(slot);
        slot = ofxOceanodeGlobalPitch::claim(static_cast<ofxOceanodeGlobalPitch::Scope>(scope.get()), getParents());
        joined = scope.get();
        if(slot < 0) return;
        if(ofxOceanodeGlobalPitch::holders(slot) > 1) { show(); return; }
        // the ticks stay with the state the node left: their names differ per scope
        for(int f = 0; f < ofxOceanodeGlobalPitch::NumFeatures; f++) destinations[f] = std::string();
        send();
    }
    void show() {
        adopting = true;
        root = ofxOceanodeGlobalPitch::scaleRoot(slot);
        const auto& current = ofxOceanodeGlobalPitch::scale(slot);
        if(!current.empty()) scale = current;
        bypass = !current.empty() && !ofxOceanodeGlobalPitch::scaleOn(slot);
        fitShift(shift, scale.get());
        shift = ofxOceanodeGlobalPitch::shift(slot);
        const auto& low = ofxOceanodeGlobalPitch::lowScale(slot);
        split = ofxOceanodeGlobalPitch::split(slot);
        if(!low.empty()) lowScale = low;
        lowBypass = !low.empty() && !ofxOceanodeGlobalPitch::lowScaleEnabled(slot);
        fitShift(lowShift, lowScale.get());
        lowShift = ofxOceanodeGlobalPitch::lowShift(slot);
        negative = ofxOceanodeGlobalPitch::negativeOn(slot);
        negativeKey = ofxOceanodeGlobalPitch::negativeKey(slot);
        transpose = ofxOceanodeGlobalPitch::transpose(slot);
        wrap = ofxOceanodeGlobalPitch::transposeWraps(slot);
        wrapBase = ofxOceanodeGlobalPitch::transposeBase(slot);
        detune = ofxOceanodeGlobalPitch::detune(slot);
        glide = ofxOceanodeGlobalPitch::glide(slot);
        for(int f = 0; f < ofxOceanodeGlobalPitch::NumFeatures; f++) destinations[f] = ofxOceanodeGlobalPitch::overridesText(feature(f), slot);
        adopting = false;
    }
    void send() {
        sendRoot();
        sendLowScale();
        sendScale();
        sendShift();
        sendLowShift();
        sendNegative();
        sendTranspose();
        sendWrap();
        sendDetune();
        sendGlide();
    }

    // each parameter to its scope's state and to the other nodes showing it
    void sendScale() {
        if(!live()) return;
        const std::vector<float> s = scale.get();
        fitShift(shift, s);
        const bool b = bypass.get();
        ofxOceanodeGlobalPitch::setScale(s, !b, slot);
        share([s, b](globalPitch* n){ n->scale = s; n->bypass = b; });
    }
    void sendRoot() {
        if(!live()) return;
        const int r = root.get();
        ofxOceanodeGlobalPitch::setScaleRoot(r, slot);
        share([r](globalPitch* n){ n->root = r; });
    }
    void sendLowScale() {
        if(!live()) return;
        const std::vector<float> s = lowScale.get();
        fitShift(lowShift, s);
        const float at = split.get();
        const bool b = lowBypass.get();
        ofxOceanodeGlobalPitch::setLowScale(s, at, !b, slot);
        share([s, at, b](globalPitch* n){ n->lowScale = s; n->split = at; n->lowBypass = b; });
    }
    void sendShift() {
        if(!live()) return;
        const int degrees = shift.get();
        ofxOceanodeGlobalPitch::setShift(degrees, slot);
        share([degrees](globalPitch* n){ n->shift = degrees; });
    }
    void sendLowShift() {
        if(!live()) return;
        const int degrees = lowShift.get();
        ofxOceanodeGlobalPitch::setLowShift(degrees, slot);
        share([degrees](globalPitch* n){ n->lowShift = degrees; });
    }
    void sendNegative() {
        if(!live()) return;
        const bool on = negative.get();
        const int key = negativeKey.get();
        ofxOceanodeGlobalPitch::setNegative(on, key, slot);
        share([on, key](globalPitch* n){ n->negative = on; n->negativeKey = key; });
    }
    void sendTranspose() {
        if(!live()) return;
        const float v = transpose.get();
        ofxOceanodeGlobalPitch::setTranspose(v, slot);
        share([v](globalPitch* n){ n->transpose = v; });
    }
    void sendWrap() {
        if(!live()) return;
        const bool on = wrap.get();
        const int base = wrapBase.get();
        ofxOceanodeGlobalPitch::setTransposeWrap(on, base, slot);
        share([on, base](globalPitch* n){ n->wrap = on; n->wrapBase = base; });
    }
    void sendDetune() {
        if(!live()) return;
        const float v = detune.get();
        ofxOceanodeGlobalPitch::setDetune(v, slot);
        share([v](globalPitch* n){ n->detune = v; });
    }
    void sendGlide() {
        if(!live()) return;
        const float v = glide.get();
        ofxOceanodeGlobalPitch::setGlide(v, slot);
        share([v](globalPitch* n){ n->glide = v; });
    }
    void sendDestinations(int f) {
        if(!live()) return;
        const std::string text = destinations[f].get();
        ofxOceanodeGlobalPitch::setOverridesText(feature(f), text, slot);
        share([f, text](globalPitch* n){ n->destinations[f] = text; });
    }

    int slot = -1;        // its scope's state in the registry; -1: every scope is taken
    int joined = 0;       // the Scope value that state was claimed for
    bool adopting = false;
    bool unsent = false;  // setup()'s values, sent on the first update
    ofParameter<int> scope;
    ofParameter<std::vector<float>> scale, lowScale;
    ofParameter<float> split;
    ofParameter<bool> bypass, lowBypass;
    ofParameter<float> transpose, detune, glide;
    ofParameter<bool> wrap, negative;
    ofParameter<int> root, wrapBase, negativeKey, shift, lowShift;
    std::array<ofParameter<std::string>, ofxOceanodeGlobalPitch::NumFeatures> destinations;
    ofEventListeners listeners;
};
