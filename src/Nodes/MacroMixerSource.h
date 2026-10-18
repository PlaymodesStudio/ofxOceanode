#pragma once
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNodeMacro.h"
#include "MacroMixerMixing.h"

struct MacroMixerDiagnostic { std::string file, message; };
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
    const std::vector<std::string>& notices() const { return importNotices; }
private:
    friend class MacroMixerSource;
    friend class MacroMixerRuntime;
    std::string macroID, displayName, sourcePath;
    MacroMixerContract interface;
    std::vector<std::string> importNotices;
};

class MacroMixerSource {
public:
    using Revision = std::shared_ptr<const MacroMixerRevision>;
    using Inputs = std::map<std::string, MacroMixerMixing::Value>;
    struct Graph {
        // Host owns an ordinary Macro node; container exposes its child for the
        // existing clock traversal. The private registry carries resource checks
        // through every nested Macro and subsequent native Macro reload.
        std::shared_ptr<ofxOceanodeContainer> host, container;
        ofxOceanodeNodeMacro* macro = nullptr;
        Revision revision;
        std::string source, failure;
        bool usesTextures = false;
        ofxOceanodeAbstractParameter& parameter(const std::string& id) const;
        MacroMixerMixing::Value read(const std::string& id) const;
        bool setInputs(const Inputs& inputs, bool publishTextures = true);
        void validateOutputs() const;
        bool update();
        bool draw();
    };
    // Analysis never constructs source nodes or prunes a user's graph.
    static Revision importFolder(ofxOceanodeContainer& parent, const std::string& folder,
        const MacroMixerContract& contract, std::vector<MacroMixerDiagnostic>& errors,
        const std::string& existingID = "");
    static std::unique_ptr<Graph> instantiate(ofxOceanodeContainer& parent, Revision revision,
        std::vector<MacroMixerDiagnostic>& errors, const Inputs& inputs = {});
    static std::string resourceBlocker(ofxOceanodeNodeRegistry& registry, const std::string& model);
};
