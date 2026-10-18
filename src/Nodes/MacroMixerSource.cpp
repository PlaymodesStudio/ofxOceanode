#include "MacroMixerSource.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeShared.h"
#include "router.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <set>

namespace {
namespace fs = std::filesystem;
struct Failure : std::runtime_error {
    std::string file;
    Failure(const std::string& file, const std::string& message) : std::runtime_error(message), file(file) {}
};
void require(bool valid, const std::string& file, const std::string& message) {
    if(!valid) throw Failure(file, message);
}
ofJson read(const fs::path& file) {
    std::ifstream stream(file);
    require(bool(stream), file.string(), "Missing or unreadable Macro file");
    try { ofJson value; stream >> value; return value; }
    catch(const std::exception& e) { throw Failure(file.string(), e.what()); }
}
std::string escaped(std::string name) { ofStringReplace(name, " ", "_"); return name; }
fs::path globalMacro(const ofJson& json, const fs::path& file) {
    auto category = ofxOceanodeShared::getMacroDirectoryStructure();
    for(const auto& name : json.at("CategoryStruct")) {
        require(bool(category), file.string(), "Global Macro library is unavailable");
        auto found = std::find_if(category->categories.begin(), category->categories.end(),
            [&](const auto& c) { return c->name == name.get<std::string>(); });
        require(found != category->categories.end(), file.string(), "Global Macro category is unavailable");
        category = *found;
    }
    require(bool(category), file.string(), "Global Macro library is unavailable");
    const auto name = json.at("Macro").get<std::string>();
    const auto found = std::find_if(category->macros.begin(), category->macros.end(),
        [&](const auto& entry) { return entry.first == name; });
    require(found != category->macros.end(), file.string(), "Global Macro is unavailable: " + name);
    return found->second;
}
void analyse(ofxOceanodeNodeRegistry& registry, const fs::path& path,
             const MacroMixerContract* contract, std::set<fs::path>& ancestors) {
    const auto folder = fs::canonical(path);
    require(ancestors.size() < 64 && ancestors.insert(folder).second, folder.string(),
        "Recursive Macro file dependency (feedback cables inside a Macro are supported)");
    struct Leave { std::set<fs::path>& paths; fs::path path; ~Leave() { paths.erase(path); } } leave{ancestors,folder};
    const auto modules = read(folder / "modules.json");
    require(modules.is_object(), folder.string(), "Expected a Macro modules object");
    struct Router { std::string key, model; };
    std::map<std::string, std::vector<Router>> routers;
    std::set<std::string> keys;
    for(auto model = modules.begin(); model != modules.end(); ++model) {
        const auto reason = MacroMixerSource::resourceBlocker(registry, model.key());
        require(reason.empty(), (folder / "modules.json").string(), reason);
        registry.checkCanCreate(model.key());
        require(registry.getRegisteredModels().count(model.key()), folder.string(), "Unregistered node: " + model.key());
        require(model.value().is_object(), folder.string(), "Expected Macro node identifiers");
        for(auto node = model.value().begin(); node != model.value().end(); ++node) {
            require(!node.key().empty() && node.key().find_first_not_of("0123456789") == std::string::npos,
                folder.string(), "Invalid node identifier");
            const int id = std::stoi(node.key());
            require(id > 0 && node.value().is_array() && node.value().size() == 2,
                folder.string(), "Macro must contain saved local nodes, with positions");
            const auto key = escaped(model.key()) + "_" + std::to_string(id);
            require(keys.insert(key).second, folder.string(), "Duplicate node identifier");
            const auto file = folder / (key + ".json");
            const auto data = read(file);
            require(data.is_object(), file.string(), "Expected node settings object");
            if(model.key().rfind("Router ", 0) == 0 && contract) {
                const auto name = data.at("Name").get<std::string>();
                routers[name].push_back({key,model.key()});
            }
            if(model.key() == "Macro") {
                const auto nested = data.value("LocalPreset", true) ? folder / key : globalMacro(data,file);
                analyse(registry, nested, nullptr, ancestors);
            }
        }
    }
    // Validate references, never sort or rewrite cables. The native loader owns
    // parameter types, dynamic ports, event propagation and feedback behavior.
    const auto cables = read(folder / "connections.json");
    require(cables.is_object(), folder.string(), "Expected Macro connections object");
    std::set<std::pair<std::string,std::string>> driven;
    for(auto from = cables.begin(); from != cables.end(); ++from) {
        require(keys.count(from.key()) && from.value().is_object(), folder.string(), "Unknown cable source: " + from.key());
        for(auto out = from.value().begin(); out != from.value().end(); ++out) {
            require(out.value().is_object(), folder.string(), "Invalid cable outputs");
            for(auto to = out.value().begin(); to != out.value().end(); ++to) {
                require(keys.count(to.key()) && to.value().is_object(), folder.string(), "Unknown cable destination: " + to.key());
                for(auto in = to.value().begin(); in != to.value().end(); ++in)
                    require(driven.insert({to.key(),in.key()}).second, folder.string(), "Multiple sources for one input");
            }
        }
    }
    if(contract) for(const auto& selected : contract->routers) {
        auto found = routers.find(selected.name);
        require(found != routers.end() && found->second.size() == 1, folder.string(),
            "Selected router must exist exactly once: " + selected.name);
        const auto& router = found->second.front();
        require(router.model == MacroMixerMixing::routerModel(selected.type), folder.string(),
            "Selected router has the wrong type: " + selected.name);
        if(selected.direction == "Input") require(!driven.count({router.key,"Value"}), folder.string(),
            "Selected Input router is driven inside the Macro: " + selected.name);
    }
}
void report(std::vector<MacroMixerDiagnostic>& errors, const std::exception& e, const std::string& fallback) {
    auto* failure = dynamic_cast<const Failure*>(&e);
    errors.push_back({failure ? failure->file : fallback, e.what()});
}
}

std::string MacroMixerSource::resourceBlocker(ofxOceanodeNodeRegistry& registry, const std::string& model) {
    const auto declared = registry.macroMixerBlocker(model);
    if(!declared.empty()) return declared;
    // Legacy physical/network outputs lack capability declarations. No category
    // bans: receivers, computation, portals, nested Macros and feedback are native.
    static const std::set<std::string> externalOutputs{
        "ILDA Projector", "ILDA Shape", "Polylines OSC Sender", "Laser Simulator",
        "SC Output", "SC NRT Recorder", "SC MultiTrack Rec",
        "Texture Sender", "Display Output", "Texture Recorder", "Texture Recorder FFmpeg",
        "Note Out", "Control Out", "Control Change", "Program Change", "Note Gate", "MIDI Clock Out",
        "Artnet Sender", "Artnode Sender", "E131 Sender", "NDI Sender"
    };
    if(externalOutputs.count(model) || model.rfind("OSC Sender ",0) == 0)
        return "Keep external outputs outside MacroMixer: " + model;
    return {};
}
MacroMixerSource::Revision MacroMixerSource::importFolder(ofxOceanodeContainer& parent,
    const std::string& folder, const MacroMixerContract& contract,
    std::vector<MacroMixerDiagnostic>& errors, const std::string& existingID) {
    errors.clear();
    try {
        require(!folder.empty(), folder, "Choose a Macro folder");
        require(!contract.routers.empty(), folder, "Select at least one router");
        std::set<std::string> ids,names;
        for(const auto& r : contract.routers) {
            require(!r.id.empty() && ids.insert(r.id).second && !r.name.empty() && names.insert(r.name).second,
                folder,"Selected router names and IDs must be nonempty and unique");
            require(r.direction == "Input" || r.direction == "Output", folder,"Invalid router direction");
            MacroMixerMixing::routerModel(r.type);
            if(r.direction == "Output") MacroMixerMixing::registry().policy(r.type,r.policy).options(r.options);
        }
        const auto source = fs::canonical(folder);
        std::set<fs::path> ancestors;
        analyse(*parent.getRegistry(), source, &contract, ancestors);
        static std::atomic<uint64_t> sequence{0};
        auto revision = std::make_shared<MacroMixerRevision>();
        revision->macroID = existingID.empty() ? "macro-" + std::to_string(ofGetSystemTimeMicros()) + "-" + std::to_string(++sequence) : existingID;
        revision->displayName = source.filename().string(); revision->sourcePath = source.string();
        revision->interface = contract;
        revision->importNotices = {"Runs as an ordinary Macro, including nested Macros, native connections and clocks.",
            "Only selected routers participate in mixing; additional routers retain their native behavior.",
            "External outputs are rejected recursively. Analysis and Refresh never execute source nodes.",
            "Each activation reads the Macro source. Save/clipboard store references; source folders must remain available."};
        return revision;
    } catch(const std::exception& e) { report(errors,e,folder); return nullptr; }
}

std::unique_ptr<MacroMixerSource::Graph> MacroMixerSource::instantiate(ofxOceanodeContainer& parent,
    Revision revision, std::vector<MacroMixerDiagnostic>& errors, const Inputs& inputs) {
    errors.clear();
    try {
        require(bool(revision),"Macro","No Macro selected");
        // Recheck current files and registrations before native loading.
        revision = importFolder(parent,revision->source(),revision->contract(),errors,revision->id());
        if(!revision) return nullptr;
        auto graph = std::make_unique<Graph>(); graph->revision = revision; graph->source = revision->source();
        auto upstream = parent.getRegistry();
        auto registry = std::make_shared<ofxOceanodeNodeRegistry>(*upstream);
        registry->setCreationGuard([upstream](const std::string& model) {
            upstream->checkCanCreate(model);
            const auto reason = MacroMixerSource::resourceBlocker(*upstream,model);
            if(!reason.empty()) throw std::runtime_error(reason);
        });
        registry->setFolderLoadGuard([upstream](const std::string& folder) {
            upstream->checkFolderLoad(folder);
            std::set<fs::path> ancestors;
            analyse(*upstream,folder,nullptr,ancestors);
        });
        graph->host = std::make_shared<ofxOceanodeContainer>(registry,parent.getTypesRegistry(),parent.getTransport());
        static std::atomic<uint64_t> hosts{0};
        graph->host->setCanvasID("MacroMixer/" + std::to_string(++hosts));
        graph->host->setTimelineHost(&parent);
        auto& node = graph->host->createNode(std::make_unique<ofxOceanodeNodeMacro>(),1);
        graph->macro = static_cast<ofxOceanodeNodeMacro*>(&node.getNodeModel());
        require(graph->macro->loadMacroSource(revision->source()),revision->source(),"Native Macro load failed");
        graph->container = graph->macro->getContainer();
        for(const auto& router : revision->contract().routers) graph->usesTextures |= router.type == "Texture";
        require(graph->setInputs(inputs),graph->source,graph->failure);
        graph->validateOutputs();
        return graph;
    } catch(const std::exception& e) { report(errors,e,"Macro activation"); return nullptr; }
}

ofxOceanodeAbstractParameter& MacroMixerSource::Graph::parameter(const std::string& id) const {
    const auto& routers = revision->contract().routers;
    const auto selected = std::find_if(routers.begin(),routers.end(),[&](const auto& r) { return r.id == id; });
    require(selected != routers.end(),source,"Unknown selected router: " + id);
    ofxOceanodeAbstractParameter* result = nullptr;
    for(auto* node : container->getAllModules()) {
        auto* router = dynamic_cast<abstractRouter*>(&node->getNodeModel());
        if(!router || router->getNameParam().get() != selected->name) continue;
        require(!result && router->nodeName() == MacroMixerMixing::routerModel(selected->type), source,
            "Selected router changed name/type or became ambiguous: " + selected->name);
        result = dynamic_cast<ofxOceanodeAbstractParameter*>(&router->getParameterGroup().get("Value"));
    }
    require(result != nullptr,source,"Selected router is unavailable: " + selected->name);
    if(selected->direction == "Input") require(!result->hasInConnection(),source,"Input router is driven inside the Macro: " + selected->name);
    return *result;
}
MacroMixerMixing::Value MacroMixerSource::Graph::read(const std::string& id) const {
    auto& p = parameter(id);
    const auto& routers = revision->contract().routers;
    const auto selected = std::find_if(routers.begin(),routers.end(),[&](const auto& r) { return r.id == id; });
    return std::visit([&](const auto& zero)->MacroMixerMixing::Value {
        using T = std::decay_t<decltype(zero)>;
        if constexpr(std::is_same_v<T,MacroMixerMixing::Object>) return MacroMixerMixing::registry().adapter(selected->type).read(p);
        else return p.cast<T>().getParameter().get();
    },MacroMixerMixing::neutral(selected->type));
}
bool MacroMixerSource::Graph::setInputs(const Inputs& inputs, bool publishTextures) {
    if(!failure.empty()) return false;
    try {
        for(const auto& router : revision->contract().routers) if(router.direction == "Input") {
            auto value = inputs.find(router.id); if(value == inputs.end()) continue;
            MacroMixerMixing::Registry::validate(router.type,value->second);
            auto& p = parameter(router.id);
            std::visit([&](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr(std::is_same_v<T,MacroMixerMixing::Object>) MacroMixerMixing::registry().adapter(router.type).write(p,value->second);
                else if constexpr(std::is_same_v<T,ofTexture*>) { if(publishTextures) p.cast<T>().getParameter() = v; }
                else if(p.cast<T>().getParameter().get() != v) p.cast<T>().getParameter() = v;
            },value->second);
        }
    } catch(const std::exception& e) { failure = e.what(); }
    return failure.empty();
}
void MacroMixerSource::Graph::validateOutputs() const {
    for(const auto& router : revision->contract().routers)
        MacroMixerMixing::Registry::validate(router.type,read(router.id));
}
bool MacroMixerSource::Graph::update() {
    if(!failure.empty()) return false;
    try { ofEventArgs args; macro->update(args); validateOutputs(); }
    catch(const std::exception& e) { failure = e.what(); }
    return failure.empty();
}
bool MacroMixerSource::Graph::draw() {
    if(!failure.empty()) return false;
    try { ofEventArgs args; macro->draw(args); validateOutputs(); }
    catch(const std::exception& e) { failure = e.what(); }
    return failure.empty();
}
