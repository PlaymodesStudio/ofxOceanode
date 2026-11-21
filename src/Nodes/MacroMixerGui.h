#pragma once
#include "MacroMixerRuntime.h"
#include <array>
#include <cstdint>
#include <deque>
struct ImVec4;

// The MacroMixer inspector panels, usable by any owner of a MacroMixerRuntime
// (the MacroMixer node, a timeline track). Panels read the runtime and queue
// their edits in State::actions; the owner applies them outside GUI iteration
// (the node: one per update) with apply() or its own handling of a kind.
namespace MacroMixerGui {

// Ok, or the worst fallback of a transition. Ordered from cold to warm.
enum class Health { Neutral, Ok, NodeFallback, GraphFallback, Error };
ImVec4 healthColor(Health health);
// The warmest of one source's Details: Ok, or a node/graph fallback. Details
// only exist in Morph mode (morph, or while the runtime morphs).
Health detailsHealth(const MacroMixerRuntime& runtime, const std::string& id, bool morph);
// The runtime's diagnostics as one text ("folder: message" per line); empty without any.
std::string failureText(const MacroMixerRuntime& runtime);

enum class ActionKind { Import, Refresh, RemoveMacro, MoveMacro, Routers, LoadRouters, SaveRouters, ClearPlayback, MorphPolicy,
    KeepOut, CancelBlocked, RestoreNode, CheckStatus };
// Import/Refresh: text = folder. RemoveMacro/MoveMacro: text = source ID (index = new place).
// Routers: contract = the edited contract; text = the new router's ID when one was added.
// MorphPolicy: text = type, policy = the choice, node = source ID (empty: the default;
// with a source, an empty policy returns the type to the default).
// KeepOut/RestoreNode: text = folder, node = saved node key.
struct Action {
    ActionKind kind;
    std::string text;
    size_t index = 0;
    MacroMixerContract contract;
    MacroMixerMorphPolicy policy;
    std::string node;
};

// Draft and dialog state of one owner's panels, plus its queued edits.
struct State {
    std::deque<Action> actions;
    std::map<std::string, std::array<char, 256>> routerDrafts; // Name fields, by router ID.
    std::array<char, 256> newRouterName{};
    uint64_t routerSequence = 0;
    int newRouterType = 0, newRouterDirection = 1;
    MacroMixerExclusions answeredBlocked; // Answered in the blocked-node dialog, not applied yet.
};

// What the source panels show beside the runtime's own state.
struct SourceContext {
    std::string selectedID;                 // Badged "Selected"; empty: none.
    ofFloatColor color{.8f, .8f, .8f, 1.f}; // Header tint (the owner node's color).
    bool morph = false;                     // The owner is in Morph mode: Details are shown.
};

// How an owner replaces the contract; empty: runtime.setContract. The MacroMixer
// node validates names against its ports and reinstalls them. It reports its
// own failure text.
using SetContract = std::function<bool(const MacroMixerContract&)>;
// Applies one queued action to the runtime, as the MacroMixer node does.
// False when it was refused or cancelled; error then holds failureText (it is
// cleared where the node clears its own). LoadRouters/SaveRouters open the
// file dialog. host: the container whose registry says which router types exist.
bool apply(MacroMixerRuntime& runtime, const Action& action, State& state, std::string& error,
           ofxOceanodeContainer* host = nullptr, const SetContract& setContract = {});

// ---- ROUTERS ----
// The routers table with its add row and Load/Save Routers.
void drawRouters(const MacroMixerRuntime& runtime, State& state, ofxOceanodeContainer* host);
// The option fields of one mix policy; apply receives the edited options.
void drawMixOptions(const std::string& type, const ofJson& options, const std::function<void(const ofJson&)>& apply);
// After a contract was applied: name drafts follow renamed routers (against
// previous, when given), removed routers lose theirs, new ones get one.
void syncRouterDrafts(State& state, const MacroMixerContract& contract, const MacroMixerContract* previous = nullptr);
// Router group files. save writes the runtime's routers and clears error.
// read builds a contract for this runtime: a router matching a current one by
// name, type and direction takes its ID, and a foreign ID equal to a current
// one is renamed. choose opens the dialog in data/routergroups; path stays
// empty when it is cancelled. All return false with error set on failure.
bool saveRouterGroup(const MacroMixerRuntime& runtime, const std::string& path, std::string& error);
bool readRouterGroup(const MacroMixerRuntime& runtime, ofxOceanodeContainer* host, const std::string& path,
                     MacroMixerContract& contract, std::string& error);
bool chooseRouterGroupFile(bool save, std::string& path, std::string& error);

// ---- MACROS ----
// Add Folder, Clear Playback, Check Status and every source (header + panel).
void drawSources(MacroMixerRuntime& runtime, State& state, const SourceContext& context);
// A collapsible bar in context.color with the source's buttons on the right.
// Returns whether it is open; then draw the panel and call ImGui::TreePop().
// Push an ID per source around it.
bool drawSourceHeader(const MacroMixerRuntime& runtime, State& state, const SourceContext& context,
                      const MacroMixerRevision& macro, size_t index, size_t count);
// One source's panel: path, problems, policies, windows, Details, checks and
// kept-out nodes. Window ticks are applied to the runtime at once.
void drawSource(MacroMixerRuntime& runtime, State& state, const SourceContext& context, const MacroMixerRevision& macro);
void drawSourceWindows(MacroMixerRuntime& runtime, const MacroMixerRevision& macro);
// The source check's morphs into this source, one per origin.
void drawSourceChecks(const MacroMixerRuntime& runtime, const MacroMixerRevision& macro);

// ---- POLICIES ----
// The "Default Mix/Morph policies" tree of the runtime's defaults.
void drawDefaultPolicies(const MacroMixerRuntime& runtime, State& state);
// The runtime's stored policies, one row per type. An empty ID edits the
// default; a source ID edits that source's overrides. Disabled during a transition.
void drawMorphPolicies(const MacroMixerRuntime& runtime, State& state, const std::string& id);
// Router ID -> mix policy and options, overriding the contract's for one transition.
using RouterPolicies = std::map<std::string, MacroMixerMorphPolicy>;
// The same rows over caller-owned values (one timeline overlap): policies
// overrides defaults per type, routers overrides each Output router's own mix
// policy. A missing entry means "not overridden". Edits are applied at once
// with canonical options; returns whether anything changed. Entries of routers
// the contract no longer has are left alone.
bool drawTransitionPolicies(const MacroMixerContract& contract, const MacroMixerMorphPolicies& defaults,
                            MacroMixerMorphPolicies& policies, RouterPolicies& routers, bool disabled = false);

// ---- BLOCKED NODES ----
// Owners whose blocked nodes the application-level dialog asks about. The
// runtime is read when the dialog draws; null skips the owner. Answers are
// queued in the owner's State (KeepOut, CancelBlocked).
void addBlockedOwner(State& state, std::function<const MacroMixerRuntime*()> runtime);
void removeBlockedOwner(State& state);
// Asks, one node at a time, whether to keep each blocked node out of the
// graph. Drawn once per frame by the host with the other application dialogs.
void drawPendingBlockedNodeDialog();

}
