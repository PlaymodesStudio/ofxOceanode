#pragma once
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNodeMacro.h"
#include "MacroMixerMixing.h"
#include <cstdint>
#include <map>
#include <set>
class abstractRouter;

// A nonempty node names a saved node whose macroMixerBlocker() stops the load;
// file is then its canonical folder. Such nodes can be kept out of the graph.
struct MacroMixerDiagnostic { std::string file, message, node, model; };
// Saved nodes left out of every graph a MacroMixer analyses or loads, as
// canonical folder and saved node key (for example "ILDA_Debugger_3").
using MacroMixerExclusions = std::set<std::pair<std::string, std::string>>;
struct MacroMixerContract {
    struct Router {
        std::string id, name, type = "Float", policy;
        ofJson options = ofJson::object();
        std::string direction = "Output";
    };
    std::vector<Router> routers{{"value", "Value", "Float"}};
};

// Analysis metadata, never a substitute for the native Macro source on disk.
class MacroMixerRevision {
public:
    const std::string& id() const { return macroID; }
    const std::string& name() const { return displayName; }
    const std::string& source() const { return sourcePath; }
    const MacroMixerContract& contract() const { return interface; }
    // Kept-out nodes the last analysis skipped, as canonical folder and node key.
    const MacroMixerExclusions& keptOut() const { return keptOutNodes; }
    // False for references restored from a preset until they are next read.
    bool analysed() const { return wasAnalysed; }
    const ofJson& sourceDefinition() const { return definition; }
private:
    friend class MacroMixerSource;
    friend class MacroMixerRuntime;
    std::string macroID, displayName, sourcePath;
    MacroMixerContract interface;
    MacroMixerExclusions keptOutNodes;
    ofJson definition;
    bool wasAnalysed = false;
};

struct MacroMixerMorphPolicy { std::string policy; ofJson options = ofJson::object(); };
using MacroMixerMorphPolicies = std::map<std::string, MacroMixerMorphPolicy>;
struct MacroMixerMorphDetail {
    std::string kind, location, message, type, policy;
    ofJson options = ofJson::object();
};
struct MacroMixerMorphTopology;
struct MacroMixerMorphAutomation;

class MacroMixerSource {
public:
    using Revision = std::shared_ptr<const MacroMixerRevision>;
    using Inputs = std::map<std::string, MacroMixerMixing::Value>;
    struct MorphValue {
        using Value = std::variant<float, int, bool, std::string, std::vector<float>, std::vector<int>, ofColor, ofFloatColor>;
        Value value;
        bool discrete = false;    // Switches half way.
        bool logarithmic = false; // Glides in log space when both ends are positive.
    };
    // Alternating model/ID components, followed by the parameter name. Separate
    // components avoid collisions with names containing slashes or underscores.
    using MorphPath = std::vector<std::string>;
    using MorphSnapshot = std::map<MorphPath, MorphValue>;
    struct Graph {
        // Host owns an ordinary Macro node; container exposes its child for the
        // existing clock traversal. The private registry carries resource checks
        // through every nested Macro and subsequent native Macro reload.
        std::shared_ptr<ofxOceanodeContainer> host, container;
        ofxOceanodeNodeMacro* macro = nullptr;
        Revision revision;
        std::string source, failure, morphFallbackReason;
        bool usesTextures = false;
        // Where its resources run on a backend with several instances (the SC
        // server playing its audio). Stamped on the resource values read from
        // it and moved with it; an in-place morph moves `incoming` into
        // `instance` when it completes.
        std::shared_ptr<MacroMixerMixing::Placement> placement = std::make_shared<MacroMixerMixing::Placement>();
        ofxOceanodeAbstractParameter& parameter(const std::string& id) const;
        // A Trigger router reads as the events it fired since clearTriggers().
        // Watching starts at activation, so loading a graph fires nothing.
        MacroMixerMixing::Value read(const std::string& id) const;
        void clearTriggers();
        // Fires a Trigger Input router once, as an outer trigger arrives.
        void trigger(const std::string& id);
        bool setInputs(const Inputs& inputs, bool publishTextures = true);
        void validateOutputs() const;
        bool update();
        bool draw();
        MorphSnapshot captureMorphValues() const;
        // logBlend: the mixer's Log Blend, scaling every Log morph hint (1 geometric, 0 linear).
        void prepareMorph(const MorphSnapshot& origin, const Graph* originGraph = nullptr, const MacroMixerMorphPolicies& policies = {}, float logBlend = 1);
        // Read saved controls and reconcile compatible native nodes. Changed
        // connections mix live signals with registered router policies; added
        // and removed nodes coexist until completion.
        // details, when given, receives the transition's Details as inspectMorph
        // would read them, from the same alignment, so Start plans the target once.
        bool prepareMorphInPlace(Revision target, const MacroMixerMorphPolicies& policies = {},
                                 std::vector<MacroMixerMorphDetail>* details = nullptr, float logBlend = 1);
        std::vector<MacroMixerMorphDetail> inspectMorph(Revision target, const MacroMixerMorphPolicies& policies = {}) const;
        void drawMorphConnections();
        bool morphConnectionsReady() const;
        void cancelMorph();
        void applyMorph(float progress);
    private:
        struct TriggerLatch { ofParameter<void> port; ofEventListener listener; int count = 0; };
        mutable std::map<std::string, std::shared_ptr<TriggerLatch>> triggerLatches;
        MorphSnapshot morphStart, morphTarget;
        MacroMixerMorphPolicies morphPolicies; // Vector controls blend by their type's policy.
        float morphLogBlend = 1; // Log Blend of the running morph.
        ofJson liveMorphDefinition; // Saved target expressed in stable runtime node identities.
        std::shared_ptr<MacroMixerMorphTopology> morphTopology;
        // Controls the destination's timelines drive: each timeline write
        // blends from the control's start value by the morph's progress.
        std::shared_ptr<MacroMixerMorphAutomation> morphAutomation;
        void startMorphAutomation();
        void clearMorphAutomation();
        // Router id -> its port, kept while the container's nodes and the
        // router's name are unchanged. parameter() is asked several times per
        // router per frame.
        struct ResolvedRouter {
            std::uint64_t nodesRevision = 0;
            abstractRouter* router = nullptr;
            ofxOceanodeAbstractParameter* parameter = nullptr;
        };
        mutable std::map<std::string, ResolvedRouter> resolvedRouters;
    };
    // Analysis never constructs source nodes or prunes a user's graph. Every
    // blocked node is reported; excluded nodes and their cables are left out.
    static Revision importFolder(ofxOceanodeContainer& parent, const std::string& folder,
        const MacroMixerContract& contract, std::vector<MacroMixerDiagnostic>& errors,
        const std::string& existingID = "", const MacroMixerExclusions* excluded = nullptr);
    // The graph's registry keeps reading the shared exclusions, so later
    // native reloads and live morphs leave the same nodes out.
    static std::unique_ptr<Graph> instantiate(ofxOceanodeContainer& parent, Revision revision,
        std::vector<MacroMixerDiagnostic>& errors, const Inputs& inputs = {},
        std::shared_ptr<const MacroMixerExclusions> excluded = nullptr);
    static std::string resourceBlocker(ofxOceanodeNodeRegistry& registry, const std::string& model);
    // Policy a changed connection of this type uses, given configured policies.
    // Discrete controls default to Switch and textures to Crossfade.
    static MacroMixerMorphPolicy connectionPolicy(const std::string& type, const MacroMixerMorphPolicies& policies, bool discrete = false);
    // The first Global Portal in a source definition (nested Macros included),
    // or empty. Loading one publishes its saved value to the running patch.
    static std::string globalPortal(const ofJson& definition);
    // Display only: shortens a source path to start at the data folder
    // (data/Presets/...). Paths outside it are returned unchanged.
    static std::string displayPath(const std::string& path);
};
