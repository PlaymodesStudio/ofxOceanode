//
//  ofxOceanodeGlobalPitch.h
//  ofxOceanode
//
//  The Global Pitch node's state (Default_Nodes/globalPitch.h): one scale
//  (degrees 0-11 above a root, and a low scale for the notes below a split
//  note), a shift along each scale (degrees, carried into the next octave), one
//  transpose (semitones, optionally wrapped into each note's
//  octave), one random detune per voice (cents) and a glide time, for every
//  node in the patch that plays or shows pitches. Each feature has its own list of destinations,
//  ticked in the Global Pitch node's inspector.
//
//  A destination registers itself and applies the values to the notes it
//  plays or shows:
//
//    setup():      ofxOceanodeGlobalPitch::follow(this, true, resend, valueChanged);
//    destructor:   ofxOceanodeGlobalPitch::unfollow(this);
//    a note:       note = ofxOceanodeGlobalPitch::apply(this, note, voice);
//
//  resend: the destination's choice of features changed; valueChanged: a value
//  it follows changed (a node that applies them itself resends its notes).
//  Nothing here depends on an audio engine: ofxOceanodeSuperCollider's
//  scGlobalPitch adds the SuperCollider buses its synths read
//  (onValuesChanged), and MIDI or display nodes call apply().
//

#pragma once

#include "ofxOceanodeNodeModel.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

class ofxOceanodeGlobalPitch {
public:
    enum Feature { Transpose = 0, Scale = 1, Detune = 2, NumFeatures = 3 };

    // A part of a destination with its own ticks (a drum kit's pad): id is
    // stable (the overrides are saved under "node/id"), label is what the table shows
    struct Part { std::string id, label; };
    struct Follower {
        ofxOceanodeNodeModel* node = nullptr;
        bool byDefault = true;
        std::function<void()> resend;
        std::function<void()> valueChanged;   // a node that applies the values itself, else none
        std::function<std::vector<Part>()> parts;   // none: the node is one destination
    };

    // ── the values ──
    static float transpose() { return state().transpose; }
    static void setTranspose(float v) {
        if(state().transpose == v) return;
        state().transpose = v;
        valuesChanged(Transpose);
    }
    static const std::vector<float>& scale() { return state().scale; }
    // the scale (at and above the split note) quantizes
    static bool scaleOn() { return state().scaleOn && !state().scale.empty(); }
    // degrees: semitones 0-11 above the root (fractions allowed); on: false = bypassed
    static void setScale(const std::vector<float>& degrees, bool on) {
        if(state().scale == degrees && state().scaleOn == on) return;
        state().scale = degrees;
        state().scaleOn = on;
        state().offsets = offsetsFor(degrees, state().shift, state().root);
        valuesChanged(Scale);
    }
    // root: the pitch class (0 = C .. 11 = B) the degrees of both scales count
    // from: E Phrygian dominant is root 4 with 0,1,4,5,7,8,10
    static int scaleRoot() { return state().root; }
    static void setScaleRoot(int root) {
        root = ((root % 12) + 12) % 12;
        if(state().root == root) return;
        state().root = root;
        state().offsets = offsetsFor(state().scale, state().shift, root);
        state().lowOffsets = offsetsFor(state().lowScale, state().lowShift, root);
        valuesChanged(Scale);
    }
    // shift: the quantized note moves this many degrees along its scale (negative
    // = down), into the next octave past the last degree: a diatonic transpose,
    // so a melody keeps its contour. The low scale has its own shift.
    static int shift() { return state().shift; }
    static void setShift(int degrees) {
        if(state().shift == degrees) return;
        state().shift = degrees;
        state().offsets = offsetsFor(state().scale, degrees, state().root);
        valuesChanged(Scale);
    }
    static int lowShift() { return state().lowShift; }
    static void setLowShift(int degrees) {
        if(state().lowShift == degrees) return;
        state().lowShift = degrees;
        state().lowOffsets = offsetsFor(state().lowScale, degrees, state().root);
        valuesChanged(Scale);
    }
    // each pitch class's offset to its nearest degree of the scale / low scale, shifted
    static const std::array<float, 12>& scaleOffsets() { return state().offsets; }
    static const std::array<float, 12>& lowScaleOffsets() { return state().lowOffsets; }
    // the input notes below split (MIDI) quantize to the low scale instead; split 0 = off
    static const std::vector<float>& lowScale() { return state().lowScale; }
    static float split() { return state().split; }
    static bool splitOn() { return state().split > 0.0f && !state().lowScale.empty(); }
    // the low scale quantizes the notes below the split note (its own bypass)
    static bool lowScaleOn() { return state().lowOn && splitOn(); }
    static bool lowScaleEnabled() { return state().lowOn; }
    static void setLowScale(const std::vector<float>& degrees, float split, bool on) {
        if(state().lowScale == degrees && state().split == split && state().lowOn == on) return;
        state().lowScale = degrees;
        state().split = split;
        state().lowOn = on;
        state().lowOffsets = offsetsFor(degrees, state().lowShift, state().root);
        valuesChanged(Scale);
    }
    // negative harmony (Jacob Collier): after the scale, each note is mirrored
    // around the axis between key's minor and major third (C: between Eb and E,
    // so C <-> G, E <-> Eb, D <-> F, B <-> Ab), to the mirror nearest the note
    // so it keeps its register: C major turns into C minor, G7 into Fm6.
    // Part of the Scale feature: its destinations follow it.
    static bool negativeOn() { return state().negative; }
    static int negativeKey() { return state().negativeKey; }
    static void setNegative(bool on, int key) {
        key = ((key % 12) + 12) % 12;
        if(state().negative == on && state().negativeKey == key) return;
        state().negative = on;
        state().negativeKey = key;
        valuesChanged(Scale);
    }
    // what a destination following Scale does to a note: the scale (or low
    // scale, below the split note) quantizes it, negative harmony mirrors it
    static bool harmonyOn(bool low) { return (low ? lowScaleOn() : scaleOn()) || negativeOn(); }
    // each pitch class's offset to its note after the scale and negative harmony
    static std::array<float, 12> harmonyOffsets(bool low) {
        std::array<float, 12> out{};
        if(low ? lowScaleOn() : scaleOn()) out = low ? state().lowOffsets : state().offsets;
        if(!negativeOn()) return out;
        const float axis2 = 2.0f * state().negativeKey + 7.0f;   // twice the axis: key + 3.5
        for(int pc = 0; pc < 12; pc++) {
            const float x = pc + out[pc], m = axis2 - x;
            out[pc] = m + 12.0f * std::round((x - m) / 12.0f) - pc;   // the mirror nearest the note
        }
        return out;
    }
    // modulo 12: a transposed note wraps into its own octave, which starts at base (pitch class 0-11)
    static bool transposeWraps() { return state().wrap; }
    static int transposeBase() { return state().wrapBase; }
    static void setTransposeWrap(bool on, int base) {
        base = ((base % 12) + 12) % 12;
        if(state().wrap == on && state().wrapBase == base) return;
        state().wrap = on;
        state().wrapBase = base;
        valuesChanged(Transpose);
    }
    static float detune() { return state().detune; }
    // cents: each voice's offset is a fixed random within +-cents
    static void setDetune(float cents) {
        if(state().detune == cents) return;
        state().detune = cents;
        valuesChanged(Detune);
    }
    // seconds a change of transpose, scale or shift takes to reach the notes
    // still sounding. Only an audio engine that glides its own values (the
    // SuperCollider synths: scGlobalPitch) can follow it; values applied once
    // (MIDI notes, apply()) jump.
    static float glide() { return state().glide; }
    static void setGlide(float seconds) {
        seconds = std::max(0.0f, seconds);
        if(state().glide == seconds) return;
        state().glide = seconds;
        for(auto& l : changeListeners()) l();
    }
    // called after any value changes (an audio engine resends what its synths read)
    static void onValuesChanged(std::function<void()> f) { changeListeners().push_back(std::move(f)); }

    // ── the destinations ──
    static void follow(ofxOceanodeNodeModel* node, bool byDefault, std::function<void()> resend,
                       std::function<void()> valueChanged = {}, std::function<std::vector<Part>()> parts = {}) {
        unfollow(node);
        followers().push_back({node, byDefault, std::move(resend), std::move(valueChanged), std::move(parts)});
    }
    static void unfollow(ofxOceanodeNodeModel* node) {
        auto& f = followers();
        f.erase(std::remove_if(f.begin(), f.end(), [node](const Follower& x){ return x.node == node; }), f.end());
        randoms().erase(node);
    }
    static const std::vector<Follower>& destinations() { return followers(); }

    // what a destination sends, plays or shows for a pitch v on a voice / channel;
    // withDetune false for one that cannot play a fraction of a semitone (a MIDI note)
    // The scale moves a note by its nearest semitone's offset, keeping the
    // note's fraction (a glide, a detune, a microtonal pitch): 61.37 with a C
    // major scale is 60.37.
    static float apply(ofxOceanodeNodeModel* node, float v, int channel = 0, bool withDetune = true) {
        if(follows(node, Scale)) {
            const float r = std::round(v);
            const int pc = ((int(r) % 12) + 12) % 12;
            const bool low = splitOn() && r < split();
            if(harmonyOn(low)) v += harmonyOffsets(low)[pc];
        }
        if(follows(node, Transpose)) v = transposed(v);
        if(withDetune && spread(node)) v += detune() * 0.01f * random(node, channel);
        return v;
    }
    // a note transposed: with modulo 12, wrapped into the octave from the base that holds it
    static float transposed(float v) {
        if(!state().wrap) return v + transpose();
        const float base = float(state().wrapBase);
        const float window = base + std::floor((v - base) / 12.0f) * 12.0f;
        return v + transpose() - 12.0f * std::floor((v + transpose() - window) / 12.0f);
    }
    // apply(), kept inside the control's declared range: a transpose must not push
    // a note past what the destination is built for
    static float applyInRange(ofxOceanodeNodeModel* node, float v, int channel, float lo, float hi, bool withDetune = true) {
        const float out = apply(node, v, channel, withDetune);
        return lo <= hi ? std::clamp(out, lo, hi) : out;
    }
    // true while a destination's channels each need their own value (Global Detune)
    static bool spread(ofxOceanodeNodeModel* node) { return detune() != 0.0f && follows(node, Detune); }
    // a destination's fixed random in -1..1 for a voice / channel (drawn once, while it follows)
    static float random(ofxOceanodeNodeModel* node, int index) {
        auto& r = randoms()[node];
        if(index < 0) index = 0;
        if(index >= (int)r.size()) {
            static std::mt19937 engine{std::random_device{}()};
            std::uniform_real_distribution<float> uniform(-1.0f, 1.0f);
            while((int)r.size() <= index) r.push_back(uniform(engine));
        }
        return r[index];
    }

    // a destination's patch-unique name ("AnalogBox 2", "Macro 1/PolyComb 1")
    static std::string key(ofxOceanodeNodeModel* node) {
        const std::string parents = node->getParents();
        return (parents.empty() || parents == "Canvas" ? "" : parents + "/") + node->nodeName() + " " + ofToString(node->getNumIdentifier());
    }
    static bool follows(ofxOceanodeNodeModel* node, Feature feature) {
        for(auto& f : followers()) if(f.node == node) {
            auto& o = state().overrides[feature];
            auto it = o.find(key(node));
            return it == o.end() ? f.byDefault : it->second;
        }
        return false;
    }
    static void setFollows(ofxOceanodeNodeModel* node, Feature feature, bool on) {
        for(auto& f : followers()) if(f.node == node) {
            const auto k = key(node);
            auto& o = state().overrides[feature];
            if(on == f.byDefault) o.erase(k); else o[k] = on;
            if(f.resend) f.resend();
        }
    }
    // a part of a node ("node/part"): its own tick, else the node's
    static std::string key(ofxOceanodeNodeModel* node, const std::string& part) { return key(node) + "/" + part; }
    static bool follows(ofxOceanodeNodeModel* node, const std::string& part, Feature feature) {
        auto& o = state().overrides[feature];
        auto it = o.find(key(node, part));
        return it == o.end() ? follows(node, feature) : it->second;
    }
    static void setFollows(ofxOceanodeNodeModel* node, const std::string& part, Feature feature, bool on) {
        for(auto& f : followers()) if(f.node == node) {
            auto& o = state().overrides[feature];
            if(on == follows(node, feature)) o.erase(key(node, part)); else o[key(node, part)] = on;
            if(f.resend) f.resend();
        }
    }

    // the destinations that differ from their default, as the Global Pitch node saves them:
    // one "key=0" or "key=1" per line
    static std::string overridesText(Feature feature) {
        std::string text;
        for(auto& [k, on] : state().overrides[feature]) text += k + (on ? "=1\n" : "=0\n");
        return text;
    }
    static void setOverridesText(Feature feature, const std::string& text) {
        std::map<std::string, bool> parsed;
        for(auto& line : ofSplitString(text, "\n", true, true)) {
            const auto eq = line.rfind('=');
            if(eq == std::string::npos || eq == 0) continue;
            parsed[line.substr(0, eq)] = line.substr(eq + 1) != "0";
        }
        if(parsed == state().overrides[feature]) return;
        state().overrides[feature] = std::move(parsed);
        for(auto& f : followers()) if(f.resend) f.resend();
    }

    // a scale's degrees once each, in order, within one octave (0-12)
    static std::vector<float> distinctDegrees(const std::vector<float>& degrees) {
        std::vector<float> sorted;
        for(float d : degrees) {
            const float degree = std::fmod(std::fmod(d, 12.0f) + 12.0f, 12.0f);
            bool known = false;
            for(float s : sorted) known = known || std::abs(s - degree) <= 1e-6f;
            if(!known) sorted.push_back(degree);
        }
        std::sort(sorted.begin(), sorted.end());
        return sorted;
    }
    // each pitch class's offset to the nearest degree (the lower one on a tie),
    // then shift degrees further along the scale, carried into the octave above
    // (or below) past its last (first) degree. The degrees count from root.
    static std::array<float, 12> offsetsFor(const std::vector<float>& degrees, int shift = 0, int root = 0) {
        std::array<float, 12> out{};
        if(degrees.empty()) return out;
        std::vector<float> rooted;
        for(float d : degrees) rooted.push_back(d + root);
        const std::vector<float> sorted = distinctDegrees(rooted);
        const int n = (int)sorted.size();
        for(int pc = 0; pc < 12; pc++) {
            float best = 0, bestDistance = 1e9f;
            int nearest = 0, nearestOctave = 0;
            for(int i = 0; i < n; i++) {
                for(int octave : {-1, 0, 1}) {
                    const float offset = sorted[i] + 12.0f * octave - pc, distance = std::abs(offset);
                    if(distance < bestDistance - 1e-6f || (std::abs(distance - bestDistance) <= 1e-6f && offset < best)) {
                        best = offset; bestDistance = distance; nearest = i; nearestOctave = octave;
                    }
                }
            }
            if(shift == 0) { out[pc] = best; continue; }
            const int step = nearest + shift;
            const int index = ((step % n) + n) % n;
            const int octaves = (step - index) / n;   // floor(step / n), negative shifts included
            out[pc] = sorted[index] + 12.0f * (nearestOctave + octaves) - pc;
        }
        return out;
    }

private:
    struct State {
        float transpose = 0.0f;
        std::vector<float> scale;
        bool scaleOn = false;
        std::array<float, 12> offsets{};
        std::vector<float> lowScale;
        float split = 0.0f;
        std::array<float, 12> lowOffsets{};
        bool lowOn = false;
        float detune = 0.0f;
        bool wrap = false;
        int wrapBase = 0;
        int shift = 0;
        int lowShift = 0;
        int root = 0;
        bool negative = false;
        int negativeKey = 0;
        float glide = 0.0f;
        std::array<std::map<std::string, bool>, NumFeatures> overrides;
    };
    static State& state() { static State s; return s; }
    static std::vector<Follower>& followers() { static std::vector<Follower> f; return f; }
    static std::vector<std::function<void()>>& changeListeners() { static std::vector<std::function<void()>> l; return l; }
    static std::map<ofxOceanodeNodeModel*, std::vector<float>>& randoms() { static std::map<ofxOceanodeNodeModel*, std::vector<float>> r; return r; }

    static void valuesChanged(Feature feature) {
        for(auto& l : changeListeners()) l();
        for(auto& f : followers()) if(f.valueChanged && follows(f.node, feature)) f.valueChanged();
    }
};
