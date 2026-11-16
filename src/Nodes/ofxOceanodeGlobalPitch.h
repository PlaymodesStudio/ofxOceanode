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
//  Scopes: a Global Pitch node acts on every level of the patch (All), on the
//  canvas it sits in (Level), or on that canvas and the macros inside it
//  (Subtree). Each scope in use is one state in a slot (0 = All, always there;
//  claim() / release() for the others, MaxScopes in all), and a destination
//  takes the most specific state covering its canvas (slotOf): its own level's
//  Level, then that level's Subtree, then the nearest parent's Subtree, then
//  All. Every value and setter takes the slot (default 0, the All state).
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

    // ── the scopes ──
    enum Scope { All = 0, Level = 1, Subtree = 2 };
    static constexpr int MaxScopes = 8;   // states at once: slot 0 (All) and seven scoped ones
    static bool valid(int slot) { return slot >= 0 && slot < MaxScopes && states()[slot].used; }
    // A Global Pitch node takes the state of its scope: the one other nodes of
    // the same scope on the same canvas (path: getParents()) already hold, or a
    // new, neutral one. -1: every slot is taken. release() it once.
    static int claim(Scope scope, const std::string& path) {
        auto& all = states();
        if(scope == All) { all[0].holders++; return 0; }
        for(int slot = 1; slot < MaxScopes; slot++) {
            auto& s = all[slot];
            if(s.used && s.scope == scope && samePath(s.path, path)) { s.holders++; return slot; }
        }
        for(int slot = 1; slot < MaxScopes; slot++) {
            if(all[slot].used) continue;
            all[slot] = State{};
            all[slot].used = true;
            all[slot].scope = scope;
            all[slot].path = path;
            all[slot].holders = 1;
            scopesChanged(slot);
            return slot;
        }
        return -1;
    }
    // The last node leaving a state returns its values to neutral (and frees the slot)
    static void release(int slot) {
        if(!valid(slot)) return;
        auto& s = states()[slot];
        if(--s.holders > 0) return;
        // All with nothing set: no destination hears a difference
        const bool silent = slot == 0 && neutral(s);
        s = State{};
        s.used = slot == 0;
        if(!silent) scopesChanged(slot);
    }
    static int holders(int slot) { return valid(slot) ? states()[slot].holders : 0; }
    // the slot of the state a node on this canvas follows: the most specific one covering it
    static int slotOf(ofxOceanodeNodeModel* node) {
        bool scoped = false;
        for(int slot = 1; slot < MaxScopes; slot++) scoped = scoped || states()[slot].used;
        return scoped ? slotAt(node->getParents()) : 0;   // only All: no path to look at
    }
    static int slotAt(const std::string& path) {
        int best = 0, bestRank = 0;
        for(int slot = 1; slot < MaxScopes; slot++) {
            const auto& s = states()[slot];
            if(!s.used) continue;
            if(!(s.scope == Level ? samePath(path, s.path) : within(path, s.path))) continue;
            const int rank = 2 * (depth(s.path) + 1) + (s.scope == Level ? 1 : 0);
            if(rank > bestRank) { best = slot; bestRank = rank; }
        }
        return best;
    }
    // canvas paths (getParents()): "Canvas" at the root, "Macro 1 / Macro 2" inside macros
    static bool isRoot(const std::string& path) { return path.empty() || path == "Canvas" || path == "0"; }
    static bool samePath(const std::string& a, const std::string& b) { return isRoot(a) ? isRoot(b) : a == b; }
    // path is the canvas at parent, or inside it at any depth ("Macro 1" is not inside "Macro 10")
    static bool within(const std::string& path, const std::string& parent) {
        if(isRoot(parent)) return true;
        if(isRoot(path)) return false;
        return path == parent || path.compare(0, parent.size() + 3, parent + " / ") == 0;
    }
    static int depth(const std::string& path) {
        if(isRoot(path)) return 0;
        int levels = 1;
        for(size_t at = path.find(" / "); at != std::string::npos; at = path.find(" / ", at + 3)) levels++;
        return levels;
    }

    // ── the values ──
    static float transpose(int slot = 0) { return state(slot).transpose; }
    static void setTranspose(float v, int slot = 0) {
        if(!valid(slot) || state(slot).transpose == v) return;
        state(slot).transpose = v;
        valuesChanged(Transpose, slot);
    }
    static const std::vector<float>& scale(int slot = 0) { return state(slot).scale; }
    // the scale (at and above the split note) quantizes
    static bool scaleOn(int slot = 0) { return state(slot).scaleOn && !state(slot).scale.empty(); }
    // degrees: semitones 0-11 above the root (fractions allowed); on: false = bypassed
    static void setScale(const std::vector<float>& degrees, bool on, int slot = 0) {
        if(!valid(slot)) return;
        auto& s = state(slot);
        if(s.scale == degrees && s.scaleOn == on) return;
        s.scale = degrees;
        s.scaleOn = on;
        s.offsets = offsetsFor(degrees, s.shift, s.root);
        valuesChanged(Scale, slot);
    }
    // root: the pitch class (0 = C .. 11 = B) the degrees of both scales count
    // from: E Phrygian dominant is root 4 with 0,1,4,5,7,8,10
    static int scaleRoot(int slot = 0) { return state(slot).root; }
    static void setScaleRoot(int root, int slot = 0) {
        if(!valid(slot)) return;
        auto& s = state(slot);
        root = ((root % 12) + 12) % 12;
        if(s.root == root) return;
        s.root = root;
        s.offsets = offsetsFor(s.scale, s.shift, root);
        s.lowOffsets = offsetsFor(s.lowScale, s.lowShift, root);
        valuesChanged(Scale, slot);
    }
    // shift: the quantized note moves this many degrees along its scale (negative
    // = down), into the next octave past the last degree: a diatonic transpose,
    // so a melody keeps its contour. The low scale has its own shift.
    static int shift(int slot = 0) { return state(slot).shift; }
    static void setShift(int degrees, int slot = 0) {
        if(!valid(slot)) return;
        auto& s = state(slot);
        if(s.shift == degrees) return;
        s.shift = degrees;
        s.offsets = offsetsFor(s.scale, degrees, s.root);
        valuesChanged(Scale, slot);
    }
    static int lowShift(int slot = 0) { return state(slot).lowShift; }
    static void setLowShift(int degrees, int slot = 0) {
        if(!valid(slot)) return;
        auto& s = state(slot);
        if(s.lowShift == degrees) return;
        s.lowShift = degrees;
        s.lowOffsets = offsetsFor(s.lowScale, degrees, s.root);
        valuesChanged(Scale, slot);
    }
    // each pitch class's offset to its nearest degree of the scale / low scale, shifted
    static const std::array<float, 12>& scaleOffsets(int slot = 0) { return state(slot).offsets; }
    static const std::array<float, 12>& lowScaleOffsets(int slot = 0) { return state(slot).lowOffsets; }
    // the input notes below split (MIDI) quantize to the low scale instead; split 0 = off
    static const std::vector<float>& lowScale(int slot = 0) { return state(slot).lowScale; }
    static float split(int slot = 0) { return state(slot).split; }
    static bool splitOn(int slot = 0) { return state(slot).split > 0.0f && !state(slot).lowScale.empty(); }
    // the low scale quantizes the notes below the split note (its own bypass)
    static bool lowScaleOn(int slot = 0) { return state(slot).lowOn && splitOn(slot); }
    static bool lowScaleEnabled(int slot = 0) { return state(slot).lowOn; }
    static void setLowScale(const std::vector<float>& degrees, float split, bool on, int slot = 0) {
        if(!valid(slot)) return;
        auto& s = state(slot);
        if(s.lowScale == degrees && s.split == split && s.lowOn == on) return;
        s.lowScale = degrees;
        s.split = split;
        s.lowOn = on;
        s.lowOffsets = offsetsFor(degrees, s.lowShift, s.root);
        valuesChanged(Scale, slot);
    }
    // negative harmony (Jacob Collier): after the scale, each note is mirrored
    // around the axis between key's minor and major third (C: between Eb and E,
    // so C <-> G, E <-> Eb, D <-> F, B <-> Ab), to the mirror nearest the note
    // so it keeps its register: C major turns into C minor, G7 into Fm6.
    // Part of the Scale feature: its destinations follow it.
    static bool negativeOn(int slot = 0) { return state(slot).negative; }
    static int negativeKey(int slot = 0) { return state(slot).negativeKey; }
    static void setNegative(bool on, int key, int slot = 0) {
        if(!valid(slot)) return;
        auto& s = state(slot);
        key = ((key % 12) + 12) % 12;
        if(s.negative == on && s.negativeKey == key) return;
        s.negative = on;
        s.negativeKey = key;
        valuesChanged(Scale, slot);
    }
    // what a destination following Scale does to a note: the scale (or low
    // scale, below the split note) quantizes it, negative harmony mirrors it
    static bool harmonyOn(bool low, int slot = 0) { return (low ? lowScaleOn(slot) : scaleOn(slot)) || negativeOn(slot); }
    // each pitch class's offset to its note after the scale and negative harmony
    static std::array<float, 12> harmonyOffsets(bool low, int slot = 0) {
        std::array<float, 12> out{};
        if(low ? lowScaleOn(slot) : scaleOn(slot)) out = low ? state(slot).lowOffsets : state(slot).offsets;
        if(!negativeOn(slot)) return out;
        const float axis2 = 2.0f * state(slot).negativeKey + 7.0f;   // twice the axis: key + 3.5
        for(int pc = 0; pc < 12; pc++) {
            const float x = pc + out[pc], m = axis2 - x;
            out[pc] = m + 12.0f * std::round((x - m) / 12.0f) - pc;   // the mirror nearest the note
        }
        return out;
    }
    // modulo 12: a transposed note wraps into its own octave, which starts at base (pitch class 0-11)
    static bool transposeWraps(int slot = 0) { return state(slot).wrap; }
    static int transposeBase(int slot = 0) { return state(slot).wrapBase; }
    static void setTransposeWrap(bool on, int base, int slot = 0) {
        if(!valid(slot)) return;
        auto& s = state(slot);
        base = ((base % 12) + 12) % 12;
        if(s.wrap == on && s.wrapBase == base) return;
        s.wrap = on;
        s.wrapBase = base;
        valuesChanged(Transpose, slot);
    }
    static float detune(int slot = 0) { return state(slot).detune; }
    // cents: each voice's offset is a fixed random within +-cents
    static void setDetune(float cents, int slot = 0) {
        if(!valid(slot) || state(slot).detune == cents) return;
        state(slot).detune = cents;
        valuesChanged(Detune, slot);
    }
    // seconds a change of transpose, scale or shift takes to reach the notes
    // still sounding. The registry keeps a lagged copy of the transpose and of
    // both offset tables (lag(), moved by advance() each frame from the Global
    // Pitch node) that apply() reads, so a destination that resends its notes
    // on valueChanged glides along with the SuperCollider buses (scGlobalPitch).
    // A value applied once (a MIDI note) takes the lagged value of its instant.
    static float glide(int slot = 0) { return state(slot).glide; }
    static void setGlide(float seconds, int slot = 0) {
        seconds = std::max(0.0f, seconds);
        if(!valid(slot) || state(slot).glide == seconds) return;
        state(slot).glide = seconds;
        for(auto& l : changeListeners()) l(slot);
        if(seconds <= 0.0f) advance();   // the sounding notes jump to where they were heading
    }
    // Each frame: moves every state's lagged transpose and offset tables
    // towards their targets like SuperCollider's Lag (60 dB in glide seconds)
    // and tells the destinations that apply the values themselves to resend
    // their notes. Several callers per frame are harmless (wall-clock time).
    static void advance() {
        const double now = ofGetElapsedTimeMicros() * 1e-6;   // double: a float's steps grow coarse after days
        for(int slot = 0; slot < MaxScopes; slot++) {
            auto& s = states()[slot];
            if(!s.used) continue;
            Lag& l = s.lag;
            const double dt = std::max(0.0, now - l.lastTime);
            l.lastTime = now;
            const float transposeTarget = s.transpose;
            const std::array<float, 12> offsetsTarget = lagTarget(false, slot), lowTarget = lagTarget(true, slot);
            bool transposeMoved = false, scaleMoved = false;
            auto move = [&](float& value, float target) {
                if(value == target) return false;
                if(s.glide <= 0.0f || dt <= 0.0) { value = target; return true; }
                const float k = 1.0f - std::exp(float(std::log(0.001) * dt / s.glide));
                value += (target - value) * k;
                if(std::abs(target - value) < 1e-3f) value = target;
                return true;
            };
            transposeMoved = move(l.transpose, transposeTarget);
            for(int pc = 0; pc < 12; pc++) {
                scaleMoved = move(l.offsets[pc], offsetsTarget[pc]) || scaleMoved;
                scaleMoved = move(l.lowOffsets[pc], lowTarget[pc]) || scaleMoved;
            }
            if(!transposeMoved && !scaleMoved) continue;
            for(auto& f : followers()) if(f.valueChanged && slotOf(f.node) == slot) {
                if((transposeMoved && follows(f, slot, Transpose)) || (scaleMoved && follows(f, slot, Scale))) f.valueChanged();
            }
        }
    }
    // the lagged transpose and offset tables apply() reads (the targets while no glide runs)
    static float laggedTranspose(int slot = 0) { return state(slot).lag.transpose; }
    static const std::array<float, 12>& laggedHarmonyOffsets(bool low, int slot = 0) { return low ? state(slot).lag.lowOffsets : state(slot).lag.offsets; }
    // called with the slot of a state after any of its values change (an audio
    // engine resends what its synths read)
    static void onValuesChanged(std::function<void(int)> f) { changeListeners().push_back(std::move(f)); }

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
        const Follower* f = follower(node);
        if(!f) return v;
        const int slot = slotOf(node);
        if(follows(*f, slot, Scale)) {
            const float r = std::round(v);
            const int pc = ((int(r) % 12) + 12) % 12;
            const bool low = splitOn(slot) && r < split(slot);
            if(harmonyOn(low, slot)) v += laggedHarmonyOffsets(low, slot)[pc];
        }
        if(follows(*f, slot, Transpose)) v = transposed(v, slot);
        if(withDetune && detune(slot) != 0.0f && follows(*f, slot, Detune)) v += detune(slot) * 0.01f * random(node, channel);
        return v;
    }
    // a note transposed (by the lagged transpose while a glide runs): with
    // modulo 12, wrapped into the octave from the base that holds it
    static float transposed(float v, int slot = 0) {
        const float t = laggedTranspose(slot);
        if(!state(slot).wrap) return v + t;
        const float base = float(state(slot).wrapBase);
        const float window = base + std::floor((v - base) / 12.0f) * 12.0f;
        return v + t - 12.0f * std::floor((v + t - window) / 12.0f);
    }
    // apply(), kept inside the control's declared range: a transpose must not push
    // a note past what the destination is built for
    static float applyInRange(ofxOceanodeNodeModel* node, float v, int channel, float lo, float hi, bool withDetune = true) {
        const float out = apply(node, v, channel, withDetune);
        return lo <= hi ? std::clamp(out, lo, hi) : out;
    }
    // true while a destination's channels each need their own value (Global Detune)
    static bool spread(ofxOceanodeNodeModel* node) { return detune(slotOf(node)) != 0.0f && follows(node, Detune); }
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
    // the name its ticks are saved under in a state: key() in the All state,
    // relative to the state's canvas in a scoped one ("PolyComb 1", "Macro 2/PolyComb 1"),
    // so a macro keeps its ticks wherever it is loaded
    static std::string key(ofxOceanodeNodeModel* node, int slot) {
        const std::string& base = state(slot).path;
        const std::string parents = node->getParents();
        if(slot == 0 || isRoot(base) || !within(parents, base)) return key(node);
        const std::string inside = parents.size() > base.size() + 3 ? parents.substr(base.size() + 3) : "";
        return (inside.empty() ? "" : inside + "/") + node->nodeName() + " " + ofToString(node->getNumIdentifier());
    }
    // the ticks live in the state the destination follows (slotOf)
    static bool follows(ofxOceanodeNodeModel* node, Feature feature) {
        const Follower* f = follower(node);
        return f && follows(*f, slotOf(node), feature);
    }
    static void setFollows(ofxOceanodeNodeModel* node, Feature feature, bool on) {
        const Follower* f = follower(node);
        if(!f) return;
        const int slot = slotOf(node);
        const auto k = key(node, slot);
        auto& o = state(slot).overrides[feature];
        if(on == f->byDefault) o.erase(k); else o[k] = on;
        if(f->resend) f->resend();
    }
    // a part of a node ("node/part"): its own tick, else the node's
    static std::string key(ofxOceanodeNodeModel* node, const std::string& part) { return key(node) + "/" + part; }
    static bool follows(ofxOceanodeNodeModel* node, const std::string& part, Feature feature) {
        const int slot = slotOf(node);
        auto& o = state(slot).overrides[feature];
        auto it = o.find(key(node, slot) + "/" + part);
        return it == o.end() ? follows(node, feature) : it->second;
    }
    static void setFollows(ofxOceanodeNodeModel* node, const std::string& part, Feature feature, bool on) {
        const Follower* f = follower(node);
        if(!f) return;
        const int slot = slotOf(node);
        const auto k = key(node, slot) + "/" + part;
        auto& o = state(slot).overrides[feature];
        if(on == follows(*f, slot, feature)) o.erase(k); else o[k] = on;
        if(f->resend) f->resend();
    }

    // the destinations of a state that differ from their default, as the Global
    // Pitch node saves them: one "key=0" or "key=1" per line
    static std::string overridesText(Feature feature, int slot = 0) {
        std::string text;
        for(auto& [k, on] : state(slot).overrides[feature]) text += k + (on ? "=1\n" : "=0\n");
        return text;
    }
    static void setOverridesText(Feature feature, const std::string& text, int slot = 0) {
        if(!valid(slot)) return;
        std::map<std::string, bool> parsed;
        for(auto& line : ofSplitString(text, "\n", true, true)) {
            const auto eq = line.rfind('=');
            if(eq == std::string::npos || eq == 0) continue;
            parsed[line.substr(0, eq)] = line.substr(eq + 1) != "0";
        }
        if(parsed == state(slot).overrides[feature]) return;
        state(slot).overrides[feature] = std::move(parsed);
        for(auto& f : followers()) if(f.resend && slotOf(f.node) == slot) f.resend();
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
    // what the SuperCollider glide synth lags on its buses: the transpose and
    // each side's offset table as written (zeros while that side's harmony is off)
    struct Lag {
        float transpose = 0.0f;
        std::array<float, 12> offsets{}, lowOffsets{};
        double lastTime = 0.0;
    };
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
        Lag lag;
        // its scope: slot 0 is All and always used; the others while a node holds them
        bool used = false;
        Scope scope = All;
        std::string path;
        int holders = 0;
    };
    static std::array<State, MaxScopes>& states() {
        static std::array<State, MaxScopes> s = [](){ std::array<State, MaxScopes> a; a[0].used = true; return a; }();
        return s;
    }
    // a slot's state; a neutral one for a slot out of range (a node left without a scope)
    static State& state(int slot = 0) {
        static State none;
        if(slot >= 0 && slot < MaxScopes) return states()[slot];
        none = State{};
        return none;
    }
    static bool neutral(const State& s) {
        if(s.transpose != 0.0f || s.scaleOn || s.lowOn || s.negative || s.detune != 0.0f || s.wrap || s.glide != 0.0f) return false;
        for(auto& o : s.overrides) if(!o.empty()) return false;
        return true;
    }
    static std::array<float, 12> lagTarget(bool low, int slot) { return harmonyOn(low, slot) ? harmonyOffsets(low, slot) : std::array<float, 12>{}; }
    static void snapLag(int slot) {
        Lag& l = state(slot).lag;
        l.transpose = state(slot).transpose;
        l.offsets = lagTarget(false, slot);
        l.lowOffsets = lagTarget(true, slot);
    }
    static std::vector<Follower>& followers() { static std::vector<Follower> f; return f; }
    static const Follower* follower(ofxOceanodeNodeModel* node) {
        for(auto& f : followers()) if(f.node == node) return &f;
        return nullptr;
    }
    static bool follows(const Follower& f, int slot, Feature feature) {
        auto& o = state(slot).overrides[feature];
        if(o.empty()) return f.byDefault;
        auto it = o.find(key(f.node, slot));
        return it == o.end() ? f.byDefault : it->second;
    }
    static std::vector<std::function<void(int)>>& changeListeners() { static std::vector<std::function<void(int)>> l; return l; }
    static std::map<ofxOceanodeNodeModel*, std::vector<float>>& randoms() { static std::map<ofxOceanodeNodeModel*, std::vector<float>> r; return r; }

    static void valuesChanged(Feature feature, int slot) {
        if(state(slot).glide <= 0.0f) snapLag(slot);   // no glide: apply() reads the new values at once
        for(auto& l : changeListeners()) l(slot);
        for(auto& f : followers()) if(f.valueChanged && slotOf(f.node) == slot && follows(f, slot, feature)) f.valueChanged();
    }
    // a state appeared or left: its values are neutral, and every destination
    // may follow another state now (its buses, its notes)
    static void scopesChanged(int slot) {
        for(auto& l : changeListeners()) l(slot);
        for(auto& f : followers()) {
            if(f.resend) f.resend();
            if(f.valueChanged) f.valueChanged();
        }
    }
};
