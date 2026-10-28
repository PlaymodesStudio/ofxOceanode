//
//  ofxOceanodeGlobalPitch.h
//  ofxOceanode
//
//  The Global Pitch node's state (Default_Nodes/globalPitch.h): one scale
//  (pitch classes 0-11, and a low scale for the notes below a split note), a
//  shift along that scale (degrees, optionally kept inside each note's
//  octave), one transpose (semitones, optionally wrapped into each note's
//  octave) and one random detune per voice (cents), for every node in the
//  patch that plays or shows pitches. Each feature has its own list of destinations,
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

    struct Follower {
        ofxOceanodeNodeModel* node = nullptr;
        bool byDefault = true;
        std::function<void()> resend;
        std::function<void()> valueChanged;   // a node that applies the values itself, else none
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
    // degrees: pitch classes 0-11 (fractions allowed); on: false = bypassed
    static void setScale(const std::vector<float>& degrees, bool on) {
        if(state().scale == degrees && state().scaleOn == on) return;
        state().scale = degrees;
        state().scaleOn = on;
        state().offsets = offsetsFor(degrees, state().shift, state().shiftWrap);
        valuesChanged(Scale);
    }
    // shift: the quantized note moves this many degrees along its scale (the
    // scale's own notes, so a 5-note scale shifted 5 is an octave up). With
    // wrap, it stays in its octave instead: the degrees rotate.
    static int shift() { return state().shift; }
    static bool shiftWraps() { return state().shiftWrap; }
    static void setShift(int degrees, bool wrap) {
        if(state().shift == degrees && state().shiftWrap == wrap) return;
        state().shift = degrees;
        state().shiftWrap = wrap;
        state().offsets = offsetsFor(state().scale, degrees, wrap);
        state().lowOffsets = offsetsFor(state().lowScale, degrees, wrap);
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
        state().lowOffsets = offsetsFor(degrees, state().shift, state().shiftWrap);
        valuesChanged(Scale);
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
    // called after any value changes (an audio engine resends what its synths read)
    static void onValuesChanged(std::function<void()> f) { changeListeners().push_back(std::move(f)); }

    // ── the destinations ──
    static void follow(ofxOceanodeNodeModel* node, bool byDefault, std::function<void()> resend,
                       std::function<void()> valueChanged = {}) {
        unfollow(node);
        followers().push_back({node, byDefault, std::move(resend), std::move(valueChanged)});
    }
    static void unfollow(ofxOceanodeNodeModel* node) {
        auto& f = followers();
        f.erase(std::remove_if(f.begin(), f.end(), [node](const Follower& x){ return x.node == node; }), f.end());
        randoms().erase(node);
    }
    static const std::vector<Follower>& destinations() { return followers(); }

    // what a destination sends, plays or shows for a pitch v on a voice / channel;
    // withDetune false for one that cannot play a fraction of a semitone (a MIDI note)
    static float apply(ofxOceanodeNodeModel* node, float v, int channel = 0, bool withDetune = true) {
        if(follows(node, Scale)) {
            const float r = std::round(v);
            const int pc = ((int(r) % 12) + 12) % 12;
            const bool low = splitOn() && r < split();
            if(low ? lowScaleOn() : scaleOn()) v = r + (low ? state().lowOffsets : state().offsets)[pc];
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

    // each pitch class's offset to the nearest degree (the lower one on a tie),
    // then shift degrees further along the scale: into the next octaves, or
    // wrapped within the note's own octave
    static std::array<float, 12> offsetsFor(const std::vector<float>& degrees, int shift = 0, bool wrap = false) {
        std::array<float, 12> out{};
        if(degrees.empty()) return out;
        // the scale's degrees once each, in order, within one octave
        std::vector<float> sorted;
        for(float d : degrees) {
            const float degree = std::fmod(std::fmod(d, 12.0f) + 12.0f, 12.0f);
            bool known = false;
            for(float s : sorted) known = known || std::abs(s - degree) <= 1e-6f;
            if(!known) sorted.push_back(degree);
        }
        std::sort(sorted.begin(), sorted.end());
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
            const int d = nearest + shift;
            const int index = ((d % n) + n) % n;
            const int octaves = wrap ? 0 : (d - index) / n;
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
        bool shiftWrap = false;
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
