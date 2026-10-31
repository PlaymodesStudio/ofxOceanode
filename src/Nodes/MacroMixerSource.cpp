#include "MacroMixerSource.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeShared.h"
#include "router.h"
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <set>
#include <cmath>
#include <optional>

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
struct Analysis {
    const MacroMixerExclusions* excluded = nullptr;
    // All blocked nodes are collected, so each can be kept out before retrying.
    std::vector<MacroMixerDiagnostic> blocked;
    MacroMixerExclusions keptOut;
    bool skips(const fs::path& folder, const std::string& key) const {
        return excluded && excluded->count({folder.string(),key});
    }
    void throwBlocked() const {
        if(!blocked.empty()) throw Failure(blocked.front().file + "/modules.json", blocked.front().message);
    }
};
void analyse(ofxOceanodeNodeRegistry& registry, const fs::path& path,
             const MacroMixerContract* contract, std::set<fs::path>& ancestors, Analysis& analysis,
             ofJson* definition = nullptr) {
    const auto folder = fs::canonical(path);
    require(ancestors.size() < 64 && ancestors.insert(folder).second, folder.string(),
        "Recursive Macro file dependency (feedback cables inside a Macro are supported)");
    struct Leave { std::set<fs::path>& paths; fs::path path; ~Leave() { paths.erase(path); } } leave{ancestors,folder};
    auto modules = read(folder / "modules.json");
    if(modules.is_null()) modules=ofJson::object(); // Native saver uses null for an empty graph.
    if(definition) *definition = {{"folder",folder.string()},{"nodes",ofJson::object()}};
    require(modules.is_object(), folder.string(), "Expected a Macro modules object");
    struct Router { std::string key, model; };
    std::map<std::string, std::vector<Router>> routers;
    std::set<std::string> keys, skipped;
    for(auto model = modules.begin(); model != modules.end(); ++model) {
        const auto reason = MacroMixerSource::resourceBlocker(registry, model.key());
        if(reason.empty()) registry.checkCanCreate(model.key());
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
            if(analysis.skips(folder,key)) {
                skipped.insert(key);
                analysis.keptOut.insert({folder.string(),key});
                continue;
            }
            if(!reason.empty()) {
                // A global Macro used twice reaches the same saved node twice.
                if(std::none_of(analysis.blocked.begin(),analysis.blocked.end(),[&](const auto& b) { return b.file == folder.string() && b.node == key; }))
                    analysis.blocked.push_back({folder.string(), reason, key, model.key()});
                continue;
            }
            const auto file = folder / (key + ".json");
            const auto data = read(file);
            require(data.is_object(), file.string(), "Expected node settings object");
            ofJson* entry = definition ? &(*definition)["nodes"][key] : nullptr;
            if(entry) *entry = {{"model",model.key()},{"id",id},{"settings",data}};
            if(model.key().rfind("Router ", 0) == 0 && contract) {
                const auto name = data.at("Name").get<std::string>();
                routers[name].push_back({key,model.key()});
            }
            if(model.key() == "Macro") {
                const auto nested = data.value("LocalPreset", true) ? folder / key : globalMacro(data,file);
                analyse(registry, nested, nullptr, ancestors, analysis, entry ? &(*entry)["graph"] : nullptr);
            }
        }
    }
    // Validate references, never sort or rewrite cables. The native loader owns
    // parameter types, dynamic ports, event propagation and feedback behavior.
    auto cables = read(folder / "connections.json");
    if(cables.is_null()) cables=ofJson::object();
    require(cables.is_object(), folder.string(), "Expected Macro connections object");
    std::set<std::pair<std::string,std::string>> driven;
    auto kept = ofJson::object(); // Native loading leaves kept-out nodes' cables out too.
    for(auto from = cables.begin(); from != cables.end(); ++from) {
        require(keys.count(from.key()) && from.value().is_object(), folder.string(), "Unknown cable source: " + from.key());
        for(auto out = from.value().begin(); out != from.value().end(); ++out) {
            require(out.value().is_object(), folder.string(), "Invalid cable outputs");
            for(auto to = out.value().begin(); to != out.value().end(); ++to) {
                require(keys.count(to.key()) && to.value().is_object(), folder.string(), "Unknown cable destination: " + to.key());
                if(skipped.count(from.key()) || skipped.count(to.key())) continue;
                kept[from.key()][out.key()][to.key()] = to.value();
                for(auto in = to.value().begin(); in != to.value().end(); ++in)
                    require(driven.insert({to.key(),in.key()}).second, folder.string(), "Multiple sources for one input");
            }
        }
    }
    if(definition) (*definition)["connections"] = skipped.empty() ? cables : kept;
    if(contract) for(const auto& selected : contract->routers) {
        auto found = routers.find(selected.name);
        require(found != routers.end() && found->second.size() == 1, folder.string(),
            "Selected router must exist exactly once: " + selected.name);
        const auto& router = found->second.front();
        require(router.model == MacroMixerMixing::routerModel(selected.type), folder.string(),
            "Selected router has the wrong type: " + selected.name);
        if(selected.direction == "Input") require(!driven.count({router.key,MacroMixerMixing::routerPort(selected.type)}), folder.string(),
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
std::string MacroMixerSource::displayPath(const std::string& path) {
    const auto full = fs::path(path).lexically_normal();
    if(!full.is_absolute()) return path;
    auto root = fs::path(ofToDataPath("", true)).lexically_normal();
    if(!root.has_filename()) root = root.parent_path();
    const auto relative = full.lexically_relative(root);
    if(!relative.empty() && *relative.begin() != "..") return (root.filename() / relative).lexically_normal().generic_string();
    // Canonical sources may resolve links the data root does not; fall back to
    // the last "data" component.
    fs::path shown;
    for(const auto& part : full) shown = part == "data" ? fs::path(part) : shown.empty() ? shown : shown / part;
    return shown.empty() ? path : shown.generic_string();
}
MacroMixerSource::Revision MacroMixerSource::importFolder(ofxOceanodeContainer& parent,
    const std::string& folder, const MacroMixerContract& contract,
    std::vector<MacroMixerDiagnostic>& errors, const std::string& existingID, const MacroMixerExclusions* excluded) {
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
        ofJson definition;
        Analysis analysis{excluded};
        analyse(*parent.getRegistry(), source, &contract, ancestors, analysis, &definition);
        if(!analysis.blocked.empty()) { errors = analysis.blocked; return nullptr; }
        static std::atomic<uint64_t> sequence{0};
        auto revision = std::make_shared<MacroMixerRevision>();
        revision->macroID = existingID.empty() ? "macro-" + std::to_string(ofGetSystemTimeMicros()) + "-" + std::to_string(++sequence) : existingID;
        revision->displayName = source.filename().string(); revision->sourcePath = source.string();
        revision->interface = contract;
        revision->definition = std::move(definition);
        revision->keptOutNodes = std::move(analysis.keptOut);
        revision->wasAnalysed = true;
        return revision;
    } catch(const std::exception& e) { report(errors,e,folder); return nullptr; }
}

std::unique_ptr<MacroMixerSource::Graph> MacroMixerSource::instantiate(ofxOceanodeContainer& parent,
    Revision revision, std::vector<MacroMixerDiagnostic>& errors, const Inputs& inputs,
    std::shared_ptr<const MacroMixerExclusions> excluded) {
    errors.clear();
    try {
        require(bool(revision),"Macro","No Macro selected");
        // Recheck current files and registrations before native loading.
        revision = importFolder(parent,revision->source(),revision->contract(),errors,revision->id(),excluded.get());
        if(!revision) return nullptr;
        auto graph = std::make_unique<Graph>(); graph->revision = revision; graph->source = revision->source();
        auto upstream = parent.getRegistry();
        auto registry = std::make_shared<ofxOceanodeNodeRegistry>(*upstream);
        registry->setCreationGuard([upstream](const std::string& model) {
            upstream->checkCanCreate(model);
            const auto reason = MacroMixerSource::resourceBlocker(*upstream,model);
            if(!reason.empty()) throw std::runtime_error(reason);
        });
        registry->setFolderLoadGuard([upstream,excluded](const std::string& folder) {
            upstream->checkFolderLoad(folder);
            std::set<fs::path> ancestors;
            Analysis analysis{excluded.get()};
            analyse(*upstream,folder,nullptr,ancestors,analysis);
            analysis.throwBlocked();
        });
        if(excluded) registry->setNodeLoadFilter([excluded](const std::string& folder, const std::string& node) {
            std::error_code error;
            const auto path = fs::canonical(folder,error);
            return excluded->count({error ? folder : path.string(),node}) != 0;
        });
        graph->host = std::make_shared<ofxOceanodeContainer>(registry,parent.getTypesRegistry(),parent.getRootTransport());
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
        result = dynamic_cast<ofxOceanodeAbstractParameter*>(&router->getParameterGroup().get(MacroMixerMixing::routerPort(selected->type)));
    }
    require(result != nullptr,source,"Selected router is unavailable: " + selected->name);
    if(selected->direction == "Input") require(!result->hasInConnection(),source,"Input router is driven inside the Macro: " + selected->name);
    return *result;
}
MacroMixerMixing::Value MacroMixerSource::Graph::read(const std::string& id) const {
    auto& p = parameter(id);
    const auto& routers = revision->contract().routers;
    const auto selected = std::find_if(routers.begin(),routers.end(),[&](const auto& r) { return r.id == id; });
    if(selected->type == MacroMixerMixing::triggerType) {
        // A replaced router node is watched again from its first event.
        auto& port = p.cast<void>().getParameter();
        auto& latch = triggerLatches[id];
        if(!latch || !latch->port.isReferenceTo(port)) {
            latch = std::make_shared<TriggerLatch>(); latch->port = port;
            latch->listener = latch->port.newListener([counter = latch.get()] {
                counter->count = std::min(counter->count + 1, MacroMixerMixing::maxTriggers);
            });
        }
        return MacroMixerMixing::triggers(latch->count);
    }
    return std::visit([&](const auto& zero)->MacroMixerMixing::Value {
        using T = std::decay_t<decltype(zero)>;
        if constexpr(std::is_same_v<T,MacroMixerMixing::Object>) return MacroMixerMixing::registry().adapter(selected->type).read(p);
        else return p.cast<T>().getParameter().get();
    },MacroMixerMixing::neutral(selected->type));
}
void MacroMixerSource::Graph::clearTriggers() {
    for(auto& latch : triggerLatches) latch.second->count = 0;
}
void MacroMixerSource::Graph::trigger(const std::string& id) {
    if(!failure.empty()) return;
    try { parameter(id).cast<void>().getParameter().trigger(); }
    catch(const std::exception& e) { failure = e.what(); }
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
    try { ofEventArgs args; macro->draw(args); drawMorphConnections(); validateOutputs(); }
    catch(const std::exception& e) { failure = e.what(); }
    return failure.empty();
}


namespace {
using MorphValue = MacroMixerSource::MorphValue;
using MorphPath = MacroMixerSource::MorphPath;

bool morphEditable(ofxOceanodeNodeModel& model, ofxOceanodeAbstractParameter& p,
                   ofxOceanodeNodeMacro& owner, const MacroMixerSource::Graph& graph, bool root) {
    // Portal receivers have no incoming cable, but their values are driven by
    // the shared Portal service. Writing saved values here would feed them back
    // into their senders and compete with the graph's actual controls.
    if(model.nodeName().rfind("Portal ",0)==0) return false;
    // A node's own window is opened from MacroMixer, never by a source's save.
    if(auto* toggle=model.getWindowToggle()) if(toggle->getName()==p.getName()) return false;
    // A saved key owned by the node's sound morph must never also receive
    // generic parameter interpolation, even when it has a published port.
    const auto keys=model.morphKeys();
    if(std::find(keys.blended.begin(),keys.blended.end(),p.getEscapedName())!=keys.blended.end()) return false;
    const auto hint=model.morphHint(p);
    if(hint==ofxOceanodeNodeModel::MorphHint::Skip || hint==ofxOceanodeNodeModel::MorphHint::Structural) return false;
    // OF reports numeric vectors as non-serializable; the native Oceanode
    // preset saver explicitly handles them, so use that same exception here.
    const bool numericVector=p.valueType()==typeid(std::vector<float>).name() || p.valueType()==typeid(std::vector<int>).name();
    if(p.hasInConnection() || (!p.isSerializable() && !numericVector) || p.isReadOnly() ||
       (p.getFlags() & (ofxOceanodeParameterFlags_DisableInConnection |
                        ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_ReadOnly))) return false;
    if(auto* router = dynamic_cast<abstractRouter*>(&model)) {
        // Names, bounds and library/interface metadata must remain the target's.
        if(p.getName() != "Value" || router->isExcludeFromSnapshot()) return false;
        const auto& name = router->getNameParam().get();
        if(owner.getParameterGroup().contains(name)) {
            auto* pin = dynamic_cast<ofxOceanodeAbstractParameter*>(&owner.getParameterGroup().get(name));
            if(pin && pin->hasInConnection()) return false;
        }
        if(root) for(const auto& r : graph.revision->contract().routers)
            if(r.direction == "Input" && r.name == name) return false;
    }
    return true;
}

template<typename T> bool finiteMorph(const T& value) {
    if constexpr(std::is_same_v<T,float>) return std::isfinite(value);
    else if constexpr(std::is_same_v<T,std::vector<float>>) {
        return std::all_of(value.begin(),value.end(),[](float v) { return std::isfinite(v); });
    } else if constexpr(std::is_same_v<T,ofFloatColor>) {
        return std::isfinite(value.r) && std::isfinite(value.g) && std::isfinite(value.b) && std::isfinite(value.a);
    } else return true;
}

template<typename T> bool captureMorph(ofxOceanodeAbstractParameter& p, std::optional<MorphValue>& result) {
    if(p.valueType() != typeid(T).name()) return false;
    const auto& value = p.cast<T>().getParameter().get();
    if(finiteMorph(value)) {
        bool discrete = std::is_same_v<T,bool> || std::is_same_v<T,std::string>;
        if constexpr(std::is_arithmetic_v<T> || std::is_same_v<T,std::vector<int>> || std::is_same_v<T,std::vector<float>>)
            discrete = discrete || !p.cast<T>().getDropdownOptions().empty();
        result = MorphValue{value,discrete};
    }
    return true;
}
std::optional<MorphValue> captureMorph(ofxOceanodeAbstractParameter& p) {
    std::optional<MorphValue> result;
    (captureMorph<float>(p,result) || captureMorph<int>(p,result) || captureMorph<bool>(p,result) ||
     captureMorph<std::string>(p,result) || captureMorph<std::vector<float>>(p,result) ||
     captureMorph<std::vector<int>>(p,result) || captureMorph<ofColor>(p,result) || captureMorph<ofFloatColor>(p,result));
    return result;
}

void captureMorphContainer(const MacroMixerSource::Graph& graph, ofxOceanodeContainer& container,
                           ofxOceanodeNodeMacro& owner, const MorphPath& prefix,
                           MacroMixerSource::MorphSnapshot& values) {
    if(prefix.size() >= 128) return;
    for(auto* node : container.getAllModules()) {
        auto& model = node->getNodeModel();
        auto path = prefix;
        path.push_back(model.nodeName()); path.push_back(std::to_string(model.getNumIdentifier()));
        if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&model)) {
            if(auto child = macro->getContainer()) captureMorphContainer(graph,*child,*macro,path,values);
            continue; // Macro pins mirror child routers; never morph them twice.
        }
        auto& parameters = model.getParameterGroup();
        for(int i = 0; i < parameters.size(); ++i) {
            auto* p = dynamic_cast<ofxOceanodeAbstractParameter*>(&parameters.get(i));
            if(!p || !morphEditable(model,*p,owner,graph,prefix.empty())) continue;
            if(auto value = captureMorph(*p)) {
                value->discrete |= model.morphHint(*p)==ofxOceanodeNodeModel::MorphHint::Switch;
                auto key = path; key.push_back(p->getName()); values.emplace(std::move(key),std::move(*value));
            }
        }
    }
}

ofxOceanodeAbstractParameter* findMorphParameter(const MacroMixerSource::Graph& graph, const MorphPath& path) {
    auto* container = graph.container.get();
    auto* owner = graph.macro;
    for(size_t i = 0; i + 2 < path.size(); i += 2) {
        ofxOceanodeNodeModel* model = nullptr;
        const auto& nodes = container->getDynamicNodes();
        const auto type = nodes.find(path[i]);
        if(type != nodes.end()) {
            const auto node = type->second.find(std::stoi(path[i+1]));
            if(node != type->second.end()) model = &node->second->getNodeModel();
        }
        // Ordinary saved Macro nodes use the indexed path above. Preserve
        // native persistent nodes too, if a nested Macro supplies any.
        if(!model) for(auto* node : container->getAllModules()) {
            auto& candidate = node->getNodeModel();
            if(candidate.nodeName() == path[i] && std::to_string(candidate.getNumIdentifier()) == path[i+1]) {
                model = &candidate; break;
            }
        }
        if(!model) return nullptr;
        if(i + 3 == path.size()) {
            auto& group = model->getParameterGroup();
            if(!group.contains(path.back())) return nullptr;
            auto* p = dynamic_cast<ofxOceanodeAbstractParameter*>(&group.get(path.back()));
            return p && morphEditable(*model,*p,*owner,graph,i == 0) ? p : nullptr;
        }
        owner = dynamic_cast<ofxOceanodeNodeMacro*>(model);
        if(!owner || !owner->getContainer()) return nullptr;
        container = owner->getContainer().get();
    }
    return nullptr;
}

template<typename T> T morphNumber(T a, T b, float t) {
    const double value = (1.0-double(t))*double(a) + double(t)*double(b);
    if constexpr(std::is_integral_v<T>) return T(std::clamp(std::round(value),
        double(std::numeric_limits<T>::lowest()),double(std::numeric_limits<T>::max())));
    else return T(value);
}
template<typename T> T morphValue(const T& a, const T& b, float t, bool discrete) {
    if(t <= 0) return a;
    if(t >= 1) return b;
    if(discrete) return t < .5f ? a : b;
    if constexpr(std::is_same_v<T,float> || std::is_same_v<T,int>) return morphNumber(a,b,t);
    else if constexpr(std::is_same_v<T,std::vector<float>> || std::is_same_v<T,std::vector<int>>) {
        if(a.size() != b.size()) return t < .5f ? a : b;
        T value; value.reserve(b.size());
        for(size_t i = 0; i < b.size(); ++i) value.push_back(morphNumber(a[i],b[i],t));
        return value;
    } else if constexpr(std::is_same_v<T,ofColor> || std::is_same_v<T,ofFloatColor>) {
        T value;
        for(size_t i = 0; i < 4; ++i) value[i] = morphNumber(a[i],b[i],t);
        return value;
    } else return t < .5f ? a : b;
}

using MorphEdges = std::set<std::vector<std::string>>;
MorphEdges savedEdges(const ofJson& connections) {
    MorphEdges edges;
    for(auto from=connections.begin(); from!=connections.end(); ++from)
        for(auto out=from.value().begin(); out!=from.value().end(); ++out)
            for(auto to=out.value().begin(); to!=out.value().end(); ++to)
                for(auto in=to.value().begin(); in!=to.value().end(); ++in)
                    edges.insert({from.key(),out.key(),to.key(),in.key()});
    return edges;
}
std::string morphNodeKey(ofxOceanodeNodeModel& model) {
    return escaped(model.nodeName())+"_"+std::to_string(model.getNumIdentifier());
}
MorphEdges liveEdges(ofxOceanodeContainer& container) {
    MorphEdges edges;
    for(const auto& cable : container.getAllConnections()) {
        auto& from=cable->getSourceParameter(); auto& to=cable->getSinkParameter();
        auto* a=from.getNodeModel(); auto* b=to.getNodeModel();
        if(!a || !b || !cable->getActive()) throw std::runtime_error("Live Macro cables changed");
        edges.insert({morphNodeKey(*a),from.getName(),morphNodeKey(*b),to.getName()});
    }
    return edges;
}

MorphValue savedMorphValue(const MorphValue& prototype, const ofJson& json) {
    auto result=prototype;
    std::visit([&](auto& value) {
        using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,std::vector<float>> || std::is_same_v<T,std::vector<int>>) {
            using Number=typename T::value_type;
            auto scalar=[](const ofJson& item)->Number {
                if(item.is_number() || item.is_boolean()) return item.get<Number>();
                if(item.is_string()) return Number(ofToFloat(item.get<std::string>()));
                throw std::runtime_error("Invalid saved morph vector");
            };
            value.clear();
            if(json.is_array()) for(const auto& item:json) value.push_back(scalar(item));
            else value.push_back(scalar(json));
        } else if constexpr(std::is_same_v<T,std::string>) {
            value=json.is_string() ? json.get<std::string>() : json.dump();
        } else {
            // Use an independent native parameter so OF's string/color/Boolean
            // preset encoding is decoded without firing any graph listeners.
            ofParameter<T> decoded; decoded.set("Morph",value);
            if constexpr(std::is_arithmetic_v<T>) {
                if(json.is_number() || json.is_boolean()) decoded=json.get<T>();
                else decoded.fromString(json.get<std::string>());
            } else decoded.fromString(json.is_string() ? json.get<std::string>() : json.dump());
            value=decoded.get();
        }
        if(!finiteMorph(value)) throw std::runtime_error("Non-finite saved morph control");
    },result.value);
    return result;
}

// Match instances before comparing settings. Saved IDs are local labels, not
// cross-preset identities. Named interfaces and unique model types anchor the
// correspondence; port roles propagate it through repeated model types.
using MorphMatches = std::map<std::string,std::string>; // target -> running
MorphMatches matchMorphNodes(const ofJson& original, const ofJson& target) {
    const auto& before=original.at("nodes"); const auto& after=target.at("nodes");
    MorphMatches matches; std::set<std::string> used;
    auto match=[&](const std::string& oldKey,const std::string& newKey) {
        if(used.count(oldKey) || matches.count(newKey) || before.at(oldKey).at("model")!=after.at(newKey).at("model")) return false;
        used.insert(oldKey); matches.emplace(newKey,oldKey); return true;
    };
    std::map<std::string,std::vector<std::string>> oldTypes,newTypes,oldNames,newNames;
    auto index=[](const ofJson& nodes, auto& types, auto& names) {
        for(auto it=nodes.begin();it!=nodes.end();++it) {
            const auto model=it.value().at("model").template get<std::string>();
            types[model].push_back(it.key());
            if(model.rfind("Router ",0)==0 || model.rfind("Portal ",0)==0) {
                const auto& settings=it.value().at("settings");
                if(settings.contains("Name")) names[model+"/"+settings.at("Name").dump()+"/"+settings.value("Local",ofJson()).dump()].push_back(it.key());
            }
        }
    };
    index(before,oldTypes,oldNames); index(after,newTypes,newNames);
    for(const auto& entry:newNames) if(entry.second.size()==1 && oldNames[entry.first].size()==1)
        match(oldNames[entry.first].front(),entry.second.front());
    for(const auto& entry:newTypes) if(entry.second.size()==1 && oldTypes[entry.first].size()==1)
        match(oldTypes[entry.first].front(),entry.second.front());
    const auto a=savedEdges(original.at("connections")),b=savedEdges(target.at("connections"));
    auto propagate=[&]() {
        bool changed=false;
        // Work from a snapshot: discovering a match can add another anchor.
        const auto anchors=matches;
        for(const auto& anchor:anchors) {
            std::map<std::string,std::vector<std::string>> oldInputs,newInputs;
            for(const auto& e:a) if(e[2]==anchor.second) oldInputs[e[3]]=e;
            for(const auto& e:b) if(e[2]==anchor.first) newInputs[e[3]]=e;
            for(const auto& input:newInputs) {
                const auto old=oldInputs.find(input.first);
                if(old!=oldInputs.end() && old->second[1]==input.second[1])
                    changed=match(old->second[0],input.second[0]) || changed;
            }
            // A fan-out only identifies a counterpart if that port role is
            // unique on both sides. Never choose an arbitrary sibling.
            std::map<std::vector<std::string>,std::set<std::string>> oldOutputs,newOutputs;
            for(const auto& e:a) if(e[0]==anchor.second && !used.count(e[2]))
                oldOutputs[{e[1],before.at(e[2]).at("model").get<std::string>(),e[3]}].insert(e[2]);
            for(const auto& e:b) if(e[0]==anchor.first && !matches.count(e[2]))
                newOutputs[{e[1],after.at(e[2]).at("model").get<std::string>(),e[3]}].insert(e[2]);
            for(const auto& output:newOutputs) {
                const auto old=oldOutputs.find(output.first);
                if(output.second.size()==1 && old!=oldOutputs.end() && old->second.size()==1)
                    changed=match(*old->second.begin(),*output.second.begin()) || changed;
            }
        }
        return changed;
    };
    while(propagate()) {}
    // Preserve established identities for unanchored parts of the graph. Port
    // roles above take precedence when an ID was reused for a different role.
    for(auto it=after.begin();it!=after.end();++it) if(before.contains(it.key())) match(it.key(),it.key());
    while(propagate()) {}
    return matches;
}

// A matched node whose saved configuration cannot morph. Only that node is
// replaced: the running instance retires and the saved one is added, so the
// rest of the graph still morphs in place.
struct MorphNodeMismatch : std::runtime_error {
    MorphPath prefix; std::string key, reason;
    MorphNodeMismatch(const std::string& location,const std::string& reason,MorphPath prefix,std::string key)
        : std::runtime_error(location+": "+reason),prefix(std::move(prefix)),key(std::move(key)),reason(reason) {}
};
struct MorphSplit { std::string reason, replacement; };
using MorphSplits = std::map<std::pair<MorphPath,std::string>,MorphSplit>; // (container path, running key)

ofJson alignMorphDefinition(const ofJson& original, const ofJson& target, size_t depth=0,
                            MorphSplits* splits=nullptr, const MorphPath& prefix={}) {
    if(depth>=64) throw std::runtime_error("Macro matching nesting limit");
    auto matches=matchMorphNodes(original,target);
    const auto& before=original.at("nodes"); const auto& after=target.at("nodes");
    // Split nodes keep their role in matching above, so their neighbours still
    // pair up; only the pair itself becomes a removal and an addition.
    std::map<std::string,std::string> replaced; // target key -> running key
    if(splits) for(auto it=matches.begin();it!=matches.end();) {
        if(!splits->count({prefix,it->second})) { ++it; continue; }
        replaced.emplace(it->first,it->second); it=matches.erase(it);
    }
    std::set<std::string> occupied;
    for(auto it=before.begin();it!=before.end();++it) occupied.insert(it.key());
    for(const auto& match:matches) occupied.insert(match.second);
    // Unmatched target nodes are ordinary additions. A matched instance may
    // already occupy their saved ID, so give the addition a free runtime ID.
    for(auto it=after.begin();it!=after.end();++it) if(!matches.count(it.key())) {
        auto key=it.key(); int id=it.value().at("id").get<int>();
        const auto model=it.value().at("model").get<std::string>();
        while(occupied.count(key)) {
            if(id==std::numeric_limits<int>::max()) throw std::runtime_error("No free Macro node ID");
            key=escaped(model)+"_"+std::to_string(++id);
        }
        occupied.insert(key); matches.emplace(it.key(),key);
    }
    for(const auto& node:replaced) splits->at({prefix,node.second}).replacement=matches.at(node.first);
    auto result=target; result["nodes"]=ofJson::object(); result["connections"]=ofJson::object();
    for(auto it=after.begin();it!=after.end();++it) {
        const auto& key=matches.at(it.key()); auto entry=it.value();
        entry["id"]=std::stoi(key.substr(key.rfind('_')+1));
        if(before.contains(key) && before.at(key).contains("graph") && entry.contains("graph")) {
            auto path=prefix; path.push_back(before.at(key).at("model").get<std::string>());
            path.push_back(std::to_string(before.at(key).at("id").get<int>()));
            entry["graph"]=alignMorphDefinition(before.at(key).at("graph"),entry.at("graph"),depth+1,splits,path);
        }
        result["nodes"][key]=std::move(entry);
    }
    const auto& cables=target.at("connections");
    for(const auto& edge:savedEdges(cables)) result["connections"][matches.at(edge[0])][edge[1]][matches.at(edge[2])][edge[3]]=
        cables.at(edge[0]).at(edge[1]).at(edge[2]).at(edge[3]);
    return result;
}

void alignMorphValues(const ofJson& original,const ofJson& target,
                      const MacroMixerSource::MorphSnapshot& source, const MorphPath& oldPrefix,
                      const MorphPath& newPrefix,MacroMixerSource::MorphSnapshot& result) {
    if(oldPrefix.size()>=128) return;
    for(const auto& match:matchMorphNodes(original,target)) {
        const auto& a=original.at("nodes").at(match.second); const auto& b=target.at("nodes").at(match.first);
        auto oldPath=oldPrefix,newPath=newPrefix;
        oldPath.push_back(a.at("model").get<std::string>()); oldPath.push_back(std::to_string(a.at("id").get<int>()));
        newPath.push_back(b.at("model").get<std::string>()); newPath.push_back(std::to_string(b.at("id").get<int>()));
        if(a.contains("graph") && b.contains("graph")) {
            alignMorphValues(a.at("graph"),b.at("graph"),source,oldPath,newPath,result); continue;
        }
        for(auto value=source.lower_bound(oldPath);value!=source.end();++value) {
            const auto& path=value->first;
            if(path.size()<oldPath.size() || !std::equal(oldPath.begin(),oldPath.end(),path.begin())) break;
            if(path.size()!=oldPath.size()+1) continue;
            auto key=newPath; key.push_back(path.back()); result.emplace(std::move(key),value->second);
        }
    }
}

std::string morphLocation(const MorphPath& prefix,const std::string& node,const std::string& port="");
struct MorphContainerEdit {
    ofxOceanodeContainer* container;
    ofJson original, target;
    std::vector<std::string> added, removed;
    MorphPath path;
    // Matched nodes that blend their own saved state; changed: it differs
    // between the two saved patches.
    struct NodeMorph { std::string key; bool changed; };
    std::vector<NodeMorph> nodeMorphs;
    // Added Portals joining a live bus: they load without their saved value
    // and take the bus's, which the bus's own blend or driver then sets.
    std::set<std::string> joining;
};
// A whole nested Macro can be staged through native recall and retired as one
// unit. Edits to existing published interfaces/shared Portal services still need
// their own reconciliation strategy.
bool morphStageable(const ofJson& entry) {
    const auto model=entry.at("model").get<std::string>();
    return model.rfind("Router ",0)!=0 && model.rfind("Portal ",0)!=0;
}
bool morphPortal(const ofJson& entry) { return entry.at("model").get<std::string>().rfind("Portal ",0)==0; }
// A saved Portal's bus: its type (model), Name and scope.
struct PortalBus {
    std::string model,name; bool local;
    bool operator==(const PortalBus& o) const { return model==o.model && name==o.name && local==o.local; }
};
PortalBus portalBus(const ofJson& entry) {
    const auto& settings=entry.at("settings");
    const auto local=settings.value("Local",ofJson(true));
    const auto model=entry.at("model").get<std::string>();
    return {model,settings.contains("Name") && settings.at("Name").is_string() ? settings.at("Name").get<std::string>() : model.substr(7),
            local.is_string() ? local.get<std::string>()!="0" && local.get<std::string>()!="false" : local!=ofJson(false)};
}
// An unmatched Portal runs until completion or starts with the morph like any
// node. Added to a live bus, loading it would publish its saved value to the
// running patch's matching Portals at once.
bool morphPortalBusFree(ofxOceanodeContainer& container,const PortalBus& bus) {
    for(auto* portal:ofxOceanodeShared::getPortalsNamed(bus.name))
        if(portal->nodeName()==bus.model && portal->isLocal()==bus.local
           && (!bus.local || portal->getParents()==container.getCanvasID())) return false;
    return true;
}
bool morphPortalCabled(const MorphEdges& edges,const std::string& key) {
    return std::any_of(edges.begin(),edges.end(),[&](const auto& e) { return e[2]==key && e[3]=="Value"; });
}
// Why an added Portal on a live bus cannot join it, or empty when it can: with
// no cable of its own it takes the bus's value, which the target sets from
// another, cabled Portal anyway. Otherwise its value is the bus's.
std::string morphPortalJoin(const ofJson& target,const std::string& key) {
    const auto edges=savedEdges(target.at("connections"));
    if(morphPortalCabled(edges,key)) return "Added Portal drives a live Portal bus: "+key;
    const auto bus=portalBus(target.at("nodes").at(key));
    const auto& nodes=target.at("nodes");
    for(auto it=nodes.begin();it!=nodes.end();++it)
        if(it.key()!=key && morphPortal(it.value()) && portalBus(it.value())==bus && morphPortalCabled(edges,it.key())) return {};
    return "Added Portal sets a live Portal bus's value: "+key;
}

bool readMorphTarget(const MacroMixerSource::Graph& graph, ofxOceanodeContainer& container,
                     ofxOceanodeNodeMacro& owner, const ofJson& original, const ofJson& target,
                     const ofJson* overrides, const MorphPath& prefix,
                     MacroMixerSource::MorphSnapshot& values, std::vector<MorphContainerEdit>& edits) {
    if(prefix.size()>=128 || !original.is_object() || !target.is_object())
        throw std::runtime_error("Invalid or excessively nested Macro definition");
    const auto& before=original.at("nodes"); const auto& after=target.at("nodes");
    const auto nodes=container.getAllModules();
    std::set<std::string> liveNodes; for(auto* node:nodes) liveNodes.insert(morphNodeKey(node->getNodeModel()));
    for(const auto& key:liveNodes) if(!before.contains(key)) throw std::runtime_error(morphLocation(prefix,key)+": live node added since activation");
    for(auto it=before.begin();it!=before.end();++it) if(!liveNodes.count(it.key())) throw std::runtime_error(morphLocation(prefix,it.key())+": live node removed since activation");
    const auto expected=savedEdges(original.at("connections")),actual=liveEdges(container);
    if(expected!=actual) {
        auto all=expected; all.insert(actual.begin(),actual.end());
        for(const auto& edge:all) if(expected.count(edge)!=actual.count(edge))
            throw std::runtime_error(morphLocation(prefix,edge[2],edge[3])+": live cable differs ("+edge[0]+" / "+edge[1]+")");
    }
    MorphContainerEdit edit{&container,original,target,{},{},prefix,{}};
    for(auto it=before.begin();it!=before.end();++it) if(!after.contains(it.key())) {
        if(!morphStageable(it.value()) && !morphPortal(it.value())) throw std::runtime_error("Unmatched interface node removed: "+it.key());
        edit.removed.push_back(it.key());
    }
    for(auto it=after.begin();it!=after.end();++it) if(!before.contains(it.key())) {
        if(morphPortal(it.value())) {
            const auto bus=portalBus(it.value());
            if(!morphPortalBusFree(container,bus)) {
                // A Local bus is blended by its own plan (planPortalBuses).
                if(!bus.local) {
                    const auto reason=morphPortalJoin(target,it.key());
                    if(!reason.empty()) throw std::runtime_error(reason);
                }
                edit.joining.insert(it.key());
            }
        } else if(!morphStageable(it.value())) throw std::runtime_error("Unmatched interface node added: "+it.key());
        edit.added.push_back(it.key());
    }
    edits.push_back(std::move(edit));
    const size_t editIndex=edits.size()-1; // Nested Macros append their own edits.
    for(auto* node:nodes) {
        auto& model=node->getNodeModel(); const auto key=morphNodeKey(model);
        if(!before.contains(key)) return false;
        if(!after.contains(key)) continue;
        const auto& a=before.at(key); const auto& b=after.at(key);
        if(a.at("model")!=b.at("model") || a.at("id")!=b.at("id")) return false;
        auto oldConfig=a.at("settings"), newConfig=b.at("settings");
        const auto morphKeys=model.morphKeys();
        auto path=prefix; path.push_back(model.nodeName()); path.push_back(std::to_string(model.getNumIdentifier()));
        auto& params=model.getParameterGroup();
        for(int i=0;i<params.size();++i) {
            auto& p=params.get(i);
            // Retain node-owned registered keys for the saved-state comparison
            // below. Otherwise a count-only change disappears from the report.
            if(std::find(morphKeys.blended.begin(),morphKeys.blended.end(),p.getEscapedName())!=morphKeys.blended.end()) continue;
            // Router names and bounds define the Macro's interface. Inspectors
            // and unknown custom-save data also remain compatibility checks.
            if(dynamic_cast<abstractRouter*>(&model) && p.getName()!="Value") continue;
            if(model.nodeName().rfind("Portal ",0)==0 && p.getName()!="Value") continue;
            if(auto* macro=dynamic_cast<ofxOceanodeNodeMacro*>(&model)) {
                bool mirrored=false;
                if(auto child=macro->getContainer()) for(auto* n:child->getAllModules())
                    if(auto* router=dynamic_cast<abstractRouter*>(&n->getNodeModel())) mirrored|=router->getNameParam().get()==p.getName();
                // Active/Snapshot and other Macro lifecycle controls are not
                // router mirrors and cannot be silently discarded on reuse.
                if(!mirrored) continue;
            } else if(auto* parameter=dynamic_cast<ofxOceanodeAbstractParameter*>(&p)) {
                if(!parameter->hasInConnection() && !(parameter->getFlags()&ofxOceanodeParameterFlags_DisableInConnection)
                   && !captureMorph(*parameter)) continue; // Unsupported control/configuration.
                if(!parameter->hasInConnection() && model.morphHint(*parameter)==ofxOceanodeNodeModel::MorphHint::Structural) continue;
            }
            oldConfig.erase(p.getEscapedName()); newConfig.erase(p.getEscapedName());
        }
        // Ignore stale saved readouts when a node now correctly declares them
        // transient. Actual Inspector configuration still requires native recall.
        for(auto& inspector:model.getInspectorParameterGroup()) if(!inspector->isSerializable() || inspector->isReadOnly()) {
            oldConfig.erase(inspector->getEscapedName()); newConfig.erase(inspector->getEscapedName());
        }
        oldConfig.erase("expanded"); newConfig.erase("expanded");
        for(const auto& name:ofxOceanodeNode::nodeSettingsKeys()) { oldConfig.erase(name); newConfig.erase(name); }
        // Saved state the node blends itself, and saved keys without sound.
        for(const auto& name:morphKeys.ignored) { oldConfig.erase(name); newConfig.erase(name); }
        bool blendedChanged=false;
        for(const auto& name:morphKeys.blended) {
            blendedChanged|=oldConfig.value(name,ofJson())!=newConfig.value(name,ofJson());
            oldConfig.erase(name); newConfig.erase(name);
        }
        if(oldConfig!=newConfig) {
            std::set<std::string> fields;
            for(auto it=oldConfig.begin();it!=oldConfig.end();++it) fields.insert(it.key());
            for(auto it=newConfig.begin();it!=newConfig.end();++it) fields.insert(it.key());
            for(const auto& field:fields) if(!oldConfig.contains(field) || !newConfig.contains(field) || oldConfig.at(field)!=newConfig.at(field)) {
                const auto reason="saved configuration differs ("+field+")";
                // Routers and Portals publish interfaces and shared services;
                // they cannot run twice, so the whole graph falls back.
                if(morphStageable(a)) throw MorphNodeMismatch(morphLocation(prefix,key),reason,prefix,key);
                throw std::runtime_error(morphLocation(prefix,key)+": "+reason);
            }
        }
        if(!morphKeys.blended.empty()) {
            std::string reason;
            if(!model.morphCheck(b.at("settings"),reason)) {
                if(morphStageable(a)) throw MorphNodeMismatch(morphLocation(prefix,key),reason,prefix,key);
                throw std::runtime_error(morphLocation(prefix,key)+": "+reason);
            }
            edits[editIndex].nodeMorphs.push_back({key,blendedChanged});
        }
        if(auto* macro=dynamic_cast<ofxOceanodeNodeMacro*>(&model)) {
            if(!a.contains("graph") || !b.contains("graph") || !macro->getContainer()) return false;
            if(!readMorphTarget(graph,*macro->getContainer(),*macro,a.at("graph"),b.at("graph"),&b.at("settings"),path,values,edits)) return false;
            continue;
        }
        for(int i=0;i<params.size();++i) {
            auto* p=dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get(i));
            if(!p || !morphEditable(model,*p,owner,graph,prefix.empty())) continue;
            const ofJson* saved=&b.at("settings"); std::string name=p->getEscapedName();
            if(auto* router=dynamic_cast<abstractRouter*>(&model)) {
                const auto pinName=router->getNameParam().get();
                if(overrides && owner.getParameterGroup().contains(pinName)) {
                    const auto& pin=owner.getParameterGroup().get(pinName);
                    if(overrides->contains(pin.getEscapedName())) { saved=overrides; name=pin.getEscapedName(); }
                }
            }
            if(!saved->contains(name) || saved->at(name).is_null()) continue;
            if(auto prototype=captureMorph(*p)) {
                prototype->discrete|=model.morphHint(*p)==ofxOceanodeNodeModel::MorphHint::Switch;
                auto control=path; control.push_back(p->getName());
                values.emplace(std::move(control),savedMorphValue(*prototype,saved->at(name)));
            }
        }
    }
    return true;
}
// Aligns the target with the running graph and reads its controls. Each node
// whose configuration cannot morph is split off and the reading repeated.
ofJson readSplitMorphTarget(const MacroMixerSource::Graph& graph,const ofJson& original,const ofJson& target,
                            MacroMixerSource::MorphSnapshot& values,std::vector<MorphContainerEdit>& edits,MorphSplits& splits) {
    for(;;) {
        auto aligned=alignMorphDefinition(original,target,0,&splits);
        values.clear(); edits.clear();
        try {
            if(!readMorphTarget(graph,*graph.container,*graph.macro,original,aligned,nullptr,{},values,edits))
                throw std::runtime_error("Incompatible live graph or saved configuration");
            return aligned;
        } catch(const MorphNodeMismatch& mismatch) {
            // Each node is split once; a repeat means the split did not apply.
            if(!splits.emplace(std::make_pair(mismatch.prefix,mismatch.key),MorphSplit{mismatch.reason,""}).second) throw;
        }
    }
}
}

namespace {
ofxOceanodeNode* morphNode(ofxOceanodeContainer& container, const std::string& key) {
    for(auto* node:container.getAllModules()) if(morphNodeKey(node->getNodeModel())==key) return node;
    return nullptr;
}
ofxOceanodeAbstractParameter* morphPort(ofxOceanodeContainer& container, const std::string& key, const std::string& name) {
    auto* node=morphNode(container,key);
    if(!node || !node->getParameters().contains(name)) return nullptr;
    return dynamic_cast<ofxOceanodeAbstractParameter*>(&node->getParameters().get(name));
}
ofxOceanodeAbstractConnection* morphCable(ofxOceanodeContainer& container, const std::vector<std::string>& edge) {
    for(const auto& cable:container.getAllConnections()) {
        auto& a=cable->getSourceParameter(); auto& b=cable->getSinkParameter();
        if(a.getNodeModel() && b.getNodeModel() && morphNodeKey(*a.getNodeModel())==edge[0] &&
           a.getName()==edge[1] && morphNodeKey(*b.getNodeModel())==edge[2] && b.getName()==edge[3]) return cable.get();
    }
    return nullptr;
}
void removeMorphCable(ofxOceanodeAbstractConnection* cable) {
    if(cable) { cable->prepareForDestruction(); cable->deleteSelf(); }
}
void removeMorphNode(ofxOceanodeContainer& container, const std::string& key) {
    if(auto* node=morphNode(container,key)) {
        // Native cable destructors normally restore editable values. Structural
        // morph teardown must not write into nodes as they are being retired.
        for(size_t i=0;i<container.getAllConnections().size();) {
            auto* cable=container.getAllConnections()[i].get();
            if(cable->getSourceParameter().getNodeModel()==&node->getNodeModel() ||
               cable->getSinkParameter().getNodeModel()==&node->getNodeModel()) removeMorphCable(cable);
            else ++i;
        }
        node->deleteSelf();
    }
}
void verifyMorphGraph(ofxOceanodeContainer& container,const ofJson& definition,size_t depth=0) {
    if(depth>=64 || container.getAllModules().size()!=definition.at("nodes").size() ||
       liveEdges(container)!=savedEdges(definition.at("connections")))
        throw std::runtime_error("Added Macro graph did not load completely");
    for(auto* node:container.getAllModules()) {
        auto& model=node->getNodeModel(); const auto key=morphNodeKey(model);
        if(!definition.at("nodes").contains(key)) throw std::runtime_error("Added Macro node identities differ");
        if(auto* macro=dynamic_cast<ofxOceanodeNodeMacro*>(&model)) {
            const auto& entry=definition.at("nodes").at(key);
            if(!entry.contains("graph") || !macro->getContainer()) throw std::runtime_error("Added nested Macro is unavailable");
            verifyMorphGraph(*macro->getContainer(),entry.at("graph"),depth+1);
        }
    }
}
std::string morphLocation(const MorphPath& prefix,const std::string& node,const std::string& port) {
    std::string result;
    for(size_t i=0;i+1<prefix.size();i+=2) result+=prefix[i]+" "+prefix[i+1]+" / ";
    result+=node; if(!port.empty()) result+=" / "+port; return result;
}
MacroMixerMorphPolicy resolveMorphPolicy(const std::string& type,const MacroMixerMorphPolicies& policies,bool discrete=false) {
    const auto found=policies.find(type);
    const auto& policy=MacroMixerMixing::registry().policy(type,found!=policies.end() ? found->second.policy : discrete ? "switch" : type=="Texture" ? "crossfade" : "");
    return {policy.id,policy.options(found==policies.end() ? ofJson::object() : found->second.options)};
}
struct MorphCablePlan {
    ofxOceanodeAbstractParameter* sink=nullptr;
    std::optional<MorphValue> target,editable;
    std::string type;
    MacroMixerMorphPolicy policy;
};
// savedKey names the target node holding the saved value when it differs from
// the sink's: a Portal bus plan previews against a live Portal of the bus.
MorphCablePlan planMorphCable(ofxOceanodeContainer& c,const ofJson& target,const std::string& key,const std::string& name,
                             const std::vector<std::string>& a,const std::vector<std::string>& b,const MacroMixerMorphPolicies& policies,bool preview=false,
                             const std::string& savedKey={}) {
    MorphCablePlan plan; auto* sink=plan.sink=morphPort(c,key,name);
    if(!sink || sink->isReadOnly() || (sink->getFlags() & (ofxOceanodeParameterFlags_DisableInConnection | ofxOceanodeParameterFlags_ReadOnly)))
        throw std::runtime_error("Changed cable targets a protected/calculated parameter");
    if(auto* model=sink->getNodeModel()) {
        // Local Portal buses are planned whole (planPortalBuses).
        if(model->nodeName().rfind("Portal ",0)==0 && savedKey.empty()) throw std::runtime_error("Changed Portal service wiring");
        if(auto* router=dynamic_cast<abstractRouter*>(model)) if(router->isExcludeFromSnapshot()) throw std::runtime_error("Changed snapshot-excluded router wiring");
    }
    plan.type=MacroMixerMixing::portType(*sink);
    auto prototype=captureMorph(*sink);
    if(!prototype && plan.type.empty()) throw std::runtime_error("Changed cable uses an unsupported control type");
    if(!plan.type.empty()) {
        plan.policy=resolveMorphPolicy(plan.type,policies,prototype && prototype->discrete);
        const auto& policy=MacroMixerMixing::registry().policy(plan.type,plan.policy.policy);
        if(policy.createProcessor && (!policy.ready || !policy.retire))
            throw std::runtime_error("Changed "+plan.type+" connection policy lacks backend readiness/retirement support");
    }
    if(prototype) {
        const auto& settings=target.at("nodes").at(savedKey.empty() ? key : savedKey).at("settings");
        const bool saved=settings.contains(sink->getEscapedName()) && !settings.at(sink->getEscapedName()).is_null();
        if(!saved && (a.empty() || b.empty())) throw std::runtime_error("Changed cable lacks a fixed endpoint value");
        if((sink->getFlags()&ofxOceanodeParameterFlags_DisableSavePreset) && (a.empty() || b.empty()))
            throw std::runtime_error("Transient input needs live sources at both endpoints");
        plan.target=saved ? savedMorphValue(*prototype,settings.at(sink->getEscapedName())) : *prototype;
        if(!saved) std::visit([&](auto& value) { using T=std::decay_t<decltype(value)>; value=sink->cast<T>().getDefaultValue(); },plan.target->value);
        plan.editable=*prototype;
        if(!a.empty()) {
            auto* cable=morphCable(c,a); ofJson editable=ofJson::object();
            if(!cable || !cable->serializeBeforeConnectionValue(editable) || !editable.contains(sink->getEscapedName()))
                throw std::runtime_error("Cannot retain origin cable's editable value");
            plan.editable=savedMorphValue(*prototype,editable.at(sink->getEscapedName()));
        }
    } else if(plan.type!="Texture" && plan.type!=MacroMixerMixing::triggerType
              && !MacroMixerMixing::registry().policy(plan.type,plan.policy.policy).createProcessor && (a.empty() || b.empty()))
        throw std::runtime_error("Changed "+plan.type+" connection needs live sources at both endpoints");
    for(const auto& edge:{a,b}) if(!edge.empty()) {
        auto* source=morphPort(c,edge[0],edge[1]);
        if(!source && preview && target.at("nodes").contains(edge[0])) continue;
        if(!source) throw std::runtime_error("Changed cable source is unavailable");
        if(plan.target) {
            const auto value=captureMorph(*source);
            const auto numeric=[](size_t i) { return i==0 || i==1 || i==4 || i==5; };
            if(!value || (value->value.index()!=plan.target->value.index() && !(numeric(value->value.index())&&numeric(plan.target->value.index()))))
                throw std::runtime_error("Changed cable has incompatible signal types");
        } else if(MacroMixerMixing::portType(*source)!=plan.type) throw std::runtime_error("Changed cable has incompatible signal types");
    }
    return plan;
}
// A Local Portal bus the running graph uses and the target changes. One of its
// Portals (carrier) takes the blend like a changed cable into its Value, from
// the origin's driver (or the live bus value) to the target's driver (or its
// saved value); the bus spreads it to every matching Portal.
struct PortalBusPlan {
    std::string name, carrier, live; // live: a running Portal of the bus, for previews
    std::vector<std::string> a,b;    // driver edges; empty: the value itself
    std::string reason;              // why the bus cannot blend
};
struct PortalBusPlans {
    std::vector<PortalBusPlan> buses;
    std::set<std::pair<std::string,std::string>> owned; // Portal Value inputs the plans handle
};
PortalBusPlans planPortalBuses(ofxOceanodeContainer& c,const MorphContainerEdit& edit) {
    struct Bus { std::vector<std::string> before,after; };
    std::map<std::pair<std::string,std::string>,Bus> buses; // (model, Name)
    const auto& before=edit.original.at("nodes"); const auto& after=edit.target.at("nodes");
    for(auto it=before.begin();it!=before.end();++it) if(morphPortal(it.value())) {
        const auto bus=portalBus(it.value());
        if(bus.local) buses[{bus.model,bus.name}].before.push_back(it.key());
    }
    for(auto it=after.begin();it!=after.end();++it) if(morphPortal(it.value())) {
        const auto bus=portalBus(it.value());
        const auto found=buses.find({bus.model,bus.name});
        if(bus.local && found!=buses.end()) found->second.after.push_back(it.key());
    }
    PortalBusPlans result;
    if(buses.empty()) return result;
    // Cables into a Value, by sink node; parsed once per container.
    std::map<std::string,MorphEdges> oldValues,newValues;
    for(const auto& edge:savedEdges(edit.original.at("connections"))) if(edge[3]=="Value") oldValues[edge[2]].insert(edge);
    for(const auto& edge:savedEdges(edit.target.at("connections"))) if(edge[3]=="Value") newValues[edge[2]].insert(edge);
    const auto drivers=[](const std::map<std::string,MorphEdges>& values,const std::vector<std::string>& keys) {
        MorphEdges result;
        for(const auto& key:keys) if(const auto found=values.find(key);found!=values.end()) result.insert(found->second.begin(),found->second.end());
        return result;
    };
    for(const auto& entry:buses) {
        const auto& bus=entry.second; const auto& name=entry.first.second;
        for(const auto& key:bus.before) result.owned.insert({key,"Value"});
        for(const auto& key:bus.after) result.owned.insert({key,"Value"});
        if(bus.after.empty()) continue; // Removed Portals keep the bus until completion.
        const auto a=drivers(oldValues,bus.before),b=drivers(newValues,bus.after);
        if(a==b && !a.empty()) continue; // The same cables drive it.
        PortalBusPlan plan{name};
        if(a.size()>1 || b.size()>1) {
            plan.reason="Portal bus "+name+" changes drivers with several cabled Portals";
            result.buses.push_back(std::move(plan)); continue;
        }
        if(!b.empty()) plan.carrier=(*b.begin())[2];
        else for(const auto& key:bus.after) if(before.contains(key)) { plan.carrier=key; break; }
        if(plan.carrier.empty()) plan.carrier=bus.after.front();
        plan.live=before.contains(plan.carrier) ? plan.carrier : bus.before.front();
        if(!a.empty()) plan.a=*a.begin();
        if(!b.empty()) plan.b=*b.begin();
        if(a.empty() && b.empty()) {
            // A value bus: its Portals set it from their saved value.
            const auto saved=after.at(plan.carrier).at("settings").value("Value",ofJson());
            bool agree=true;
            for(const auto& key:bus.after) agree&=after.at(key).at("settings").value("Value",ofJson())==saved;
            if(!agree) { plan.reason="Portal bus "+name+" has different saved values"; result.buses.push_back(std::move(plan)); continue; }
            auto* port=morphPort(c,plan.live,"Value");
            const auto current=port ? captureMorph(*port) : std::nullopt;
            if(saved.is_null() || !current) continue; // Triggers and resources hold no value.
            try { if(savedMorphValue(*current,saved).value==current->value) continue; }
            catch(const std::exception& e) { plan.reason="Portal bus "+name+": "+e.what(); }
        }
        result.buses.push_back(std::move(plan));
    }
    return result;
}
void writeMorphControl(ofxOceanodeAbstractParameter& p, const MorphValue& control, bool silent) {
    std::visit([&](const auto& value) {
        using T=std::decay_t<decltype(value)>;
        if(p.valueType()!=typeid(T).name()) throw std::runtime_error("Changed morph sink type");
        auto parameter=p.cast<T>().getParameter();
        if(parameter.get()!=value) {
            if(silent) parameter.setWithoutEventNotifications(value);
            else parameter=value;
        }
    },control.value);
}
void detachMorphProxy(ofxOceanodeAbstractParameter& p) {
    if(p.hasInConnection()) p.getInConnection()->prepareForDestruction();
    for(auto* cable:p.getOutConnections()) cable->prepareForDestruction();
    p.removeAllConnections();
}
struct MorphSignal {
    virtual ~MorphSignal() = default;
    virtual void progress(float t)=0;
    virtual void draw() {}
    virtual bool ready() const { return true; }
    virtual void retire(const std::shared_ptr<void>&) {}
    virtual void disconnect()=0;
    virtual void constant(bool destination, bool silent=false)=0;
};
template<typename T> struct TypedMorphSignal : MorphSignal {
    ofParameter<T> a,b,out,sink;
    ofxOceanodeParameter<T> inputA,inputB,output;
    ofEventListeners listeners;
    T saved;
    float t=0;
    bool discrete=false, valid=true, publishing=false;
    TypedMorphSignal(ofxOceanodeAbstractParameter& p, const MorphValue& target)
    :sink(p.cast<T>().getParameter()),saved(std::get<T>(target.value)),discrete(target.discrete) {
        a.set("Origin",sink.get()); b.set("Destination",saved); out.set("Blend",sink.get());
        inputA.bindParameter(a); inputB.bindParameter(b); output.bindParameter(out);
        listeners.push(a.newListener([this](T&) { refresh(); }));
        listeners.push(b.newListener([this](T&) { refresh(); }));
    }
    ~TypedMorphSignal() override { disconnect(); }
    void refresh() {
        // Feedback cables can return to this input while it publishes. Native
        // ofParameter suppresses recursive events too; retain the same boundary.
        if(publishing) return;
        const auto value=morphValue(a.get(),b.get(),t,discrete);
        valid=finiteMorph(value);
        if(valid && out.get()!=value) {
            publishing=true;
            try { out=value; } catch(...) { publishing=false; throw; }
            publishing=false;
        }
    }
    void progress(float value) override {
        t=value; refresh();
        if(!valid) throw std::runtime_error("Non-finite changed-cable morph signal");
    }
    void disconnect() override {
        listeners.unsubscribeAll();
        detachMorphProxy(output); detachMorphProxy(inputA); detachMorphProxy(inputB);
    }
    void constant(bool destination, bool silent=false) override {
        const auto& value=destination ? saved : a.get();
        if(sink.get()!=value) {
            if(silent) sink.setWithoutEventNotifications(value);
            else sink=value;
        }
    }
};
// A changed cable into a void input. Events pass as they happen, inside the
// graph's own update; the policy decides from this frame's counts per side, so
// a missing side contributes nothing and the policy never invents an event.
struct TriggerMorphSignal : MorphSignal {
    MacroMixerMorphPolicy policy;
    ofParameter<void> a,b,out;
    ofxOceanodeParameter<void> inputA,inputB,output;
    ofEventListeners listeners;
    int firedA=0, firedB=0, passed=0;
    float t=0;
    explicit TriggerMorphSignal(const MacroMixerMorphPolicy& policy):policy(policy) {
        a.set("Origin"); b.set("Destination"); out.set("Blend");
        inputA.bindParameter(a); inputB.bindParameter(b); output.bindParameter(out);
        listeners.push(a.newListener([this] { fire(firedA); }));
        listeners.push(b.newListener([this] { fire(firedB); }));
    }
    ~TriggerMorphSignal() override { disconnect(); }
    void fire(int& side) {
        side=std::min(side+1,MacroMixerMixing::maxTriggers);
        const auto total=MacroMixerMixing::triggerCount(MacroMixerMixing::registry().mix(MacroMixerMixing::triggerType,policy.policy,
            MacroMixerMixing::triggers(firedA),MacroMixerMixing::triggers(firedB),t,policy.options));
        while(passed<total) { ++passed; out.trigger(); }
    }
    // Called once per frame before the graph updates; counts restart with it.
    void progress(float value) override { t=value; firedA=firedB=passed=0; }
    void disconnect() override {
        listeners.unsubscribeAll();
        detachMorphProxy(output); detachMorphProxy(inputA); detachMorphProxy(inputB);
    }
    void constant(bool,bool) override {} // Rewiring never fires the sink.
};
struct PolicyMorphSignal : MorphSignal {
    std::string type; MacroMixerMorphPolicy policy;
    std::shared_ptr<ofxOceanodeAbstractParameter> inputA,inputB,output;
    ofParameterGroup inputs;
    ofEventListener listener;
    MacroMixerMixing::Policy::Function renderer,processor;
    std::function<void(const MacroMixerMixing::Value&,bool)> writeSink;
    MacroMixerMixing::Value saved;
    float t=0; bool publishing=false;
    std::string failure;
    PolicyMorphSignal(ofxOceanodeContainer& container,const std::string& key,const std::string& name,const MorphCablePlan& plan)
    :type(plan.type),policy(plan.policy),saved(MacroMixerMixing::neutral(type)) {
        auto a=MacroMixerMixing::readPort(type,*plan.sink);
        if(plan.target) std::visit([&](const auto& value) {
            using T=std::decay_t<decltype(value)>;
            if constexpr(std::is_constructible_v<MacroMixerMixing::Value,T>) saved=value;
        },plan.target->value);
        inputA=MacroMixerMixing::makePort(type,"Origin",a);
        inputB=MacroMixerMixing::makePort(type,"Destination",saved);
        output=MacroMixerMixing::makePort(type,"Blend",a);
        inputs.add(*inputA); inputs.add(*inputB);
        writeSink=[&container,key,name,type=type](const MacroMixerMixing::Value& value,bool silent) {
            if(auto* p=morphPort(container,key,name)) if(MacroMixerMixing::portType(*p)==type) MacroMixerMixing::writePort(type,*p,value,silent);
        };
        const auto& definition=MacroMixerMixing::registry().policy(type,policy.policy);
        if(definition.createRenderer) renderer=definition.createRenderer();
        if(definition.createProcessor) processor=definition.createProcessor();
        listener=inputs.parameterChangedE().newListener([this](ofAbstractParameter&) { if(!renderer && !processor) refresh(); });
    }
    ~PolicyMorphSignal() override { disconnect(); }
    void refresh() {
        if(publishing) return;
        publishing=true;
        try {
            auto a=MacroMixerMixing::readPort(type,*inputA),b=MacroMixerMixing::readPort(type,*inputB);
            MacroMixerMixing::Registry::validate(type,a); MacroMixerMixing::Registry::validate(type,b);
            auto result=renderer ? renderer(a,b,t,policy.options) : processor ? processor(a,b,t,policy.options) : MacroMixerMixing::registry().mix(type,policy.policy,a,b,t,policy.options);
            MacroMixerMixing::Registry::validate(type,result);
            MacroMixerMixing::writePort(type,*output,result); failure.clear();
        } catch(const std::exception& e) { failure=e.what(); }
        publishing=false;
    }
    void progress(float value) override { t=value; if(!renderer) { refresh(); if(!failure.empty()) throw std::runtime_error(failure); } }
    void draw() override { if(renderer) { MacroMixerMixing::drawIsolated([&] { refresh(); }); if(!failure.empty()) throw std::runtime_error(failure); } }
    bool ready() const override {
        if(!processor) return true;
        const auto value=MacroMixerMixing::readPort(type,*output);
        MacroMixerMixing::Registry::validate(type,value);
        return MacroMixerMixing::registry().policy(type,policy.policy).ready(value);
    }
    void retire(const std::shared_ptr<void>& lease) override {
        if(processor) MacroMixerMixing::registry().policy(type,policy.policy).retire(MacroMixerMixing::readPort(type,*output),lease);
    }
    void disconnect() override {
        listener.unsubscribe();
        detachMorphProxy(*output); detachMorphProxy(*inputA); detachMorphProxy(*inputB);
    }
    void constant(bool destination,bool silent=false) override {
        // The new native cable immediately supplies a resource endpoint. Never
        // preserve a borrowed address as its editable disconnect value.
        writeSink(destination ? saved : MacroMixerMixing::readPort(type,*inputA),silent);
    }
};
}

// Owns temporary control cables outside the saved graph. Native connections
// perform scalar/vector conversions and event propagation into the blend inputs;
// the blend output drives the ordinary sink (including mirrored Macro pins).
// Destruction rolls back topology before Graph destroys its native containers.
struct MacroMixerMorphTopology {
    struct Signal {
        ofxOceanodeContainer* container;
        std::vector<std::string> before,after;
        std::shared_ptr<MorphSignal> blend;
        std::optional<MorphValue> originEditable;
        std::string type;
    };
    std::vector<MorphContainerEdit> edits;
    std::vector<Signal> signals;
    std::vector<std::pair<ofxOceanodeContainer*,std::string>> created;
    std::vector<std::pair<ofxOceanodeContainer*,std::vector<std::string>>> newCables;
    // Nodes blending their own saved state, once morphBegin accepted.
    std::vector<std::pair<ofxOceanodeContainer*,std::string>> nodeMorphs;
    // Resolved on each use: a listener may rebuild a node's ports meanwhile.
    void endNodeMorphs(bool completed) {
        for(auto it=nodeMorphs.rbegin();it!=nodeMorphs.rend();++it)
            if(auto* node=morphNode(*it->first,it->second)) node->getNodeModel().morphEnd(completed);
        nodeMorphs.clear();
    }
    ofJson targetDefinition;
    std::shared_ptr<ofxOceanodeContainer> hostLease;
    bool complete=false;
    // A container lease alone cannot retain nodes erased from its registry.
    // Keep native node owners as well, including whole departing Macros.
    std::shared_ptr<void> retirementLease() {
        struct Lease {
            std::shared_ptr<ofxOceanodeContainer> host;
            std::vector<std::shared_ptr<ofxOceanodeNode>> nodes;
            std::vector<std::shared_ptr<MorphSignal>> signals;
        };
        auto lease=std::make_shared<Lease>(); lease->host=hostLease;
        for(auto& edit:edits) for(auto& model:edit.container->getDynamicNodes()) for(auto& node:model.second) lease->nodes.push_back(node.second);
        for(auto& signal:signals) lease->signals.push_back(signal.blend);
        return lease;
    }
    ~MacroMixerMorphTopology() { if(!complete) { try { rollback(); } catch(...) {} } }
    void rollback() {
        endNodeMorphs(false);
        auto lease=retirementLease();
        for(auto& signal:signals) signal.blend->disconnect();
        for(auto& cable:newCables) removeMorphCable(morphCable(*cable.first,cable.second));
        for(auto& signal:signals) {
            if(!signal.before.empty()) {
                if(!morphCable(*signal.container,signal.before)) {
                    auto* source=morphPort(*signal.container,signal.before[0],signal.before[1]);
                    auto* sink=morphPort(*signal.container,signal.before[2],signal.before[3]);
                    if(source && sink) {
                        if(signal.originEditable) writeMorphControl(*sink,*signal.originEditable,true);
                        else MacroMixerMixing::writePort(signal.type,*sink,MacroMixerMixing::neutral(signal.type));
                        signal.container->createConnection(*source,*sink);
                    }
                }
            } else signal.blend->constant(false);
        }
        for(auto it=created.rbegin();it!=created.rend();++it) removeMorphNode(*it->first,it->second);
        for(auto& signal:signals) signal.blend->retire(lease);
        complete=true;
    }
    bool ready() const { for(auto& signal:signals) if(!signal.blend->ready()) return false; return true; }
    void progress(float t) {
        for(auto& signal:signals) signal.blend->progress(t);
        for(const auto& entry:nodeMorphs)
            if(auto* node=morphNode(*entry.first,entry.second)) node->getNodeModel().morphApply(t,t>=.5f);
    }
    void draw() { for(auto& signal:signals) signal.blend->draw(); }
    void commit() {
        endNodeMorphs(true);
        auto lease=retirementLease();
        for(auto& signal:signals) signal.blend->disconnect();
        for(auto& signal:signals) {
            signal.blend->constant(true,!signal.after.empty()); // Native cable retains the destination editable value.
            if(!signal.after.empty()) addCable(*signal.container,signal.after);
        }
        for(auto& edit:edits) for(const auto& key:edit.removed) removeMorphNode(*edit.container,key);
        for(auto& signal:signals) signal.blend->retire(lease);
        complete=true;
    }
    void addCable(ofxOceanodeContainer& container, const std::vector<std::string>& edge) {
        auto* from=morphPort(container,edge[0],edge[1]); auto* to=morphPort(container,edge[2],edge[3]);
        if(!from || !to || !container.createConnection(*from,*to)) throw std::runtime_error("Cannot create changed Macro cable");
        newCables.emplace_back(&container,edge);
    }
    void prepare(ofxOceanodeContainer& host,const MacroMixerMorphPolicies& policies) {
        // Added (and replaced) nodes load their saved settings as a preset
        // recall, like a native source load: nodes see isPresetLoading() and
        // skip recall-time side effects, such as a synth's saved Snapshot
        // number recalling that library slot over the saved sound. The scope
        // covers only the node loads; the cables and blends in between are
        // created outside it, so backends (the SC server graph) still react.
        {
            ofxOceanodeShared::ScopedNestedPresetLoad nestedLoad;
            for(auto& edit:edits) {
                auto& c=*edit.container;
                for(const auto& key:edit.added) {
                    const auto& entry=edit.target.at("nodes").at(key);
                    auto* node=c.createNodeFromName(entry.at("model").get<std::string>(),entry.at("id").get<int>());
                    if(!node) throw std::runtime_error("Cannot create added Macro node");
                    created.emplace_back(&c,key);
                    auto json=entry.at("settings"); auto& model=node->getNodeModel();
                    if(edit.joining.count(key)) json.erase("Value"); // Takes the bus's value.
                    node->presetWillBeLoaded(); model.loadBeforeConnections(json);
                    if(auto* macro=dynamic_cast<ofxOceanodeNodeMacro*>(&model)) {
                        if(!entry.contains("graph")) throw std::runtime_error("Added Macro definition is unavailable");
                        macro->macroLoadFromFolder(json,entry.at("graph").at("folder").get<std::string>());
                        verifyMorphGraph(*macro->getContainer(),entry.at("graph"));
                    } else model.macroLoad(json,edit.target.at("folder").get<std::string>());
                    node->loadParametersFromJson(json); node->loadInspectorParametersFromJson(json);
                    if(edit.joining.count(key)) if(auto* portal=dynamic_cast<abstractPortal*>(&model)) ofxOceanodeShared::requestPortalUpdate(portal);
                }
                if(!edit.added.empty()) c.allNodesCreated.notify(&c);
            }
        }
        // Blends one changed input: a and b are the origin's and target's
        // cables into it (empty: its value), sunk into plan.sink.
        const auto blendInput=[&](ofxOceanodeContainer& c,const std::string& key,const std::string& name,
                                  const std::vector<std::string>& a,const std::vector<std::string>& b,const MorphCablePlan& plan) {
            std::shared_ptr<MorphSignal> blend;
            ofxOceanodeAbstractParameter *inputA=nullptr,*inputB=nullptr,*output=nullptr;
            if(plan.type==MacroMixerMixing::triggerType) {
                auto mixer=std::make_shared<TriggerMorphSignal>(plan.policy);
                inputA=&mixer->inputA; inputB=&mixer->inputB; output=&mixer->output; blend=mixer;
            } else if(!plan.type.empty()) {
                auto mixer=std::make_shared<PolicyMorphSignal>(c,key,name,plan);
                inputA=mixer->inputA.get(); inputB=mixer->inputB.get(); output=mixer->output.get(); blend=mixer;
            } else std::visit([&](const auto& value) {
                using T=std::decay_t<decltype(value)>;
                auto mixer=std::make_shared<TypedMorphSignal<T>>(*plan.sink,*plan.target);
                inputA=&mixer->inputA; inputB=&mixer->inputB; output=&mixer->output; blend=mixer;
            },plan.target->value);
            signals.push_back({&c,a,b,blend,plan.editable,plan.type});
            if(!a.empty() && !host.createConnection(*morphPort(c,a[0],a[1]),*inputA)) throw std::runtime_error("Cannot connect origin morph signal");
            if(!b.empty() && !host.createConnection(*morphPort(c,b[0],b[1]),*inputB)) throw std::runtime_error("Cannot connect destination morph signal");
            if(!a.empty()) removeMorphCable(morphCable(c,a));
            if(!host.createConnection(*output,*plan.sink)) throw std::runtime_error("Cannot connect blended morph signal");
            blend->progress(0); // Publish resource bridges before evaluating backend readiness.
        };
        for(auto& edit:edits) {
            auto& c=*edit.container;
            const auto buses=planPortalBuses(c,edit);
            for(const auto& bus:buses.buses) {
                if(!bus.reason.empty()) throw std::runtime_error(morphLocation(edit.path,"Portal bus "+bus.name)+": "+bus.reason);
                try { blendInput(c,bus.carrier,"Value",bus.a,bus.b,planMorphCable(c,edit.target,bus.carrier,"Value",bus.a,bus.b,policies,false,bus.carrier)); }
                catch(const std::exception& e) { throw std::runtime_error(morphLocation(edit.path,"Portal bus "+bus.name)+": "+e.what()); }
            }
            const auto before=savedEdges(edit.original.at("connections")),after=savedEdges(edit.target.at("connections"));
            std::map<std::pair<std::string,std::string>,std::vector<std::string>> oldInputs,newInputs;
            for(const auto& edge:before) oldInputs[{edge[2],edge[3]}]=edge;
            for(const auto& edge:after) newInputs[{edge[2],edge[3]}]=edge;
            auto inputs=oldInputs; for(const auto& entry:newInputs) inputs.emplace(entry.first,entry.second);
            for(const auto& input:inputs) {
                const auto& key=input.first.first; const auto& name=input.first.second;
                const auto old=oldInputs.find(input.first),next=newInputs.find(input.first);
                const std::vector<std::string> a=old==oldInputs.end()?std::vector<std::string>{}:old->second;
                const std::vector<std::string> b=next==newInputs.end()?std::vector<std::string>{}:next->second;
                if(a==b || !edit.target.at("nodes").contains(key)) continue;
                if(buses.owned.count(input.first)) {
                    // A bus's driver moved to another Portal or left it. Only
                    // the blended driver (signals) and the cables kept as they
                    // are reach here without a plan; the rest is the bus's.
                    if(!a.empty() && !std::any_of(buses.buses.begin(),buses.buses.end(),[&](const auto& bus) { return bus.a==a; }))
                        throw std::runtime_error(morphLocation(edit.path,key,name)+": Portal bus driver changed without a plan");
                    continue;
                }
                if(!edit.original.at("nodes").contains(key)) { addCable(c,b); continue; }
                try { blendInput(c,key,name,a,b,planMorphCable(c,edit.target,key,name,a,b,policies)); }
                catch(const std::exception& e) { throw std::runtime_error(morphLocation(edit.path,key,name)+": "+e.what()); }
            }
        }
        {
            ofxOceanodeShared::ScopedNestedPresetLoad nestedLoad; // The recall itself, as above.
            for(auto& entry:created) if(auto* node=morphNode(*entry.first,entry.second)) {
                const auto edit=std::find_if(edits.begin(),edits.end(),[&](const auto& e) { return e.container==entry.first; });
                auto json=edit->target.at("nodes").at(entry.second).at("settings");
                if(edit->joining.count(entry.second)) json.erase("Value");
                auto& model=node->getNodeModel();
                model.presetRecallBeforeSettingParameters(json); node->loadParametersFromJson(json);
                node->loadInspectorParametersFromJson(json); model.presetRecallAfterSettingParameters(json);
                node->activateConnections(); node->presetHasLoaded();
            }
        }
        for(auto& edit:edits) for(const auto& entry:edit.nodeMorphs) {
            auto* node=morphNode(*edit.container,entry.key);
            std::string reason="node is unavailable";
            if(!node || !node->getNodeModel().morphBegin(edit.target.at("nodes").at(entry.key).at("settings"),reason))
                throw std::runtime_error(morphLocation(edit.path,entry.key)+": "+reason);
            nodeMorphs.emplace_back(edit.container,entry.key);
        }
    }
};

MacroMixerSource::MorphSnapshot MacroMixerSource::Graph::captureMorphValues() const {
    MorphSnapshot values;
    captureMorphContainer(*this,*container,*macro,{},values);
    return values;
}
void MacroMixerSource::Graph::prepareMorph(const MorphSnapshot& captured, const Graph* originGraph) {
    morphStart.clear(); morphTarget.clear();
    MorphSnapshot aligned;
    if(originGraph) alignMorphValues(originGraph->liveMorphDefinition.is_null() ? originGraph->revision->sourceDefinition() : originGraph->liveMorphDefinition,
        revision->sourceDefinition(),captured,{},{},aligned);
    const auto& origin=originGraph ? aligned : captured;
    const auto target = captureMorphValues();
    for(const auto& entry : target) {
        const auto start = origin.find(entry.first);
        if(start == origin.end() || start->second.value.index() != entry.second.value.index()) continue;
        if(start->second.value == entry.second.value) continue;
        morphStart.emplace(entry.first,start->second); morphTarget.emplace(entry.first,entry.second);
    }
}
bool MacroMixerSource::Graph::prepareMorphInPlace(Revision target,const MacroMixerMorphPolicies& policies) {
    MorphSnapshot destination;
    auto topology=std::make_shared<MacroMixerMorphTopology>();
    topology->hostLease=host;
    morphFallbackReason.clear();
    try {
        if(!target) throw std::runtime_error("No target Macro definition");
        const auto& original=liveMorphDefinition.is_null() ? revision->sourceDefinition() : liveMorphDefinition;
        MorphSplits splits;
        topology->targetDefinition=readSplitMorphTarget(*this,original,target->sourceDefinition(),destination,topology->edits,splits);
        topology->prepare(*host,policies);
    } catch(const std::exception& e) { morphFallbackReason=e.what(); return false; } // Staged edits roll back before fallback.
    const auto start=captureMorphValues();
    MorphSnapshot from,to;
    for(const auto& entry:destination) {
        const auto found=start.find(entry.first);
        if(found!=start.end() && found->second.value.index()==entry.second.value.index() && found->second.value!=entry.second.value) {
            from.emplace(entry.first,found->second); to.emplace(entry.first,entry.second);
        }
    }
    morphStart=std::move(from); morphTarget=std::move(to); morphTopology=std::move(topology);
    return true;
}
void MacroMixerSource::Graph::drawMorphConnections() {
    if(morphTopology) morphTopology->draw();
}
bool MacroMixerSource::Graph::morphConnectionsReady() const {
    return !morphTopology || morphTopology->ready();
}
std::vector<MacroMixerMorphDetail> MacroMixerSource::Graph::inspectMorph(Revision target,const MacroMixerMorphPolicies& policies) const {
    std::vector<MacroMixerMorphDetail> result;
    try {
        if(!target) throw std::runtime_error("No target Macro definition");
        const auto& original=liveMorphDefinition.is_null() ? revision->sourceDefinition() : liveMorphDefinition;
        MorphSnapshot values; std::vector<MorphContainerEdit> edits; MorphSplits splits;
        readSplitMorphTarget(*this,original,target->sourceDefinition(),values,edits,splits);
        for(const auto& edit:edits) {
            std::set<std::string> replacements;
            for(const auto& split:splits) if(split.first.first==edit.path) replacements.insert(split.second.replacement);
            for(const auto& key:edit.added) if(!replacements.count(key)) result.push_back({"added",morphLocation(edit.path,key),
                edit.joining.count(key) ? "Starts when the morph begins, with its Portal bus's value" : "Starts when the morph begins"});
            for(const auto& entry:edit.nodeMorphs) if(entry.changed) result.push_back({"node",morphLocation(edit.path,entry.key),
                "Blends its own saved state on the running node"});
            for(const auto& key:edit.removed) {
                const auto split=splits.find({edit.path,key});
                if(split==splits.end()) result.push_back({"removed",morphLocation(edit.path,key),"Runs until the morph completes"});
                else result.push_back({"replaced",morphLocation(edit.path,key),"The "+split->second.reason+", so it cannot morph. "
                    "The running node retires at completion, the saved one starts now and their connections blend."});
            }
            const auto buses=planPortalBuses(*edit.container,edit);
            for(const auto& bus:buses.buses) {
                const auto location=morphLocation(edit.path,"Portal bus "+bus.name);
                if(!bus.reason.empty()) { result.push_back({"fallback",location,bus.reason}); continue; }
                try {
                    const auto plan=planMorphCable(*edit.container,edit.target,bus.live,"Value",bus.a,bus.b,policies,true,bus.carrier);
                    const auto from=bus.a.empty() ? std::string("Bus value") : bus.a[0]+" / "+bus.a[1];
                    const auto to=bus.b.empty() ? std::string("Saved value") : bus.b[0]+" / "+bus.b[1];
                    result.push_back({"mix",location,from+" -> "+to,plan.type,plan.policy.policy,plan.policy.options});
                } catch(const std::exception& e) { result.push_back({"fallback",location,e.what()}); }
            }
            std::map<std::pair<std::string,std::string>,std::vector<std::string>> before,after;
            for(const auto& edge:savedEdges(edit.original.at("connections"))) before[{edge[2],edge[3]}]=edge;
            for(const auto& edge:savedEdges(edit.target.at("connections"))) after[{edge[2],edge[3]}]=edge;
            auto inputs=before; for(const auto& item:after) inputs.emplace(item.first,item.second);
            for(const auto& input:inputs) {
                const auto& key=input.first.first; const auto& name=input.first.second;
                const auto a=before.count(input.first)?before.at(input.first):std::vector<std::string>{};
                const auto b=after.count(input.first)?after.at(input.first):std::vector<std::string>{};
                if(a==b || !edit.original.at("nodes").contains(key) || !edit.target.at("nodes").contains(key) || buses.owned.count(input.first)) continue;
                const auto location=morphLocation(edit.path,key,name);
                try {
                    const auto plan=planMorphCable(*edit.container,edit.target,key,name,a,b,policies,true);
                    const auto endpoint=[&plan](const std::vector<std::string>& edge) { return edge.empty()?std::string(plan.type=="ScBus"?"Silence":"Fixed value"):edge[0]+" / "+edge[1]; };
                    result.push_back({"mix",location,endpoint(a)+" -> "+endpoint(b),plan.type,plan.policy.policy,plan.policy.options});
                } catch(const std::exception& e) { result.push_back({"fallback",location,e.what()}); }
            }
        }
        if(result.empty()) result.push_back({"live","Graph","Matching graph controls morph on the running nodes"});
    } catch(const std::exception& e) { result.push_back({"fallback","Graph",e.what()}); }
    return result;
}
void MacroMixerSource::Graph::cancelMorph() {
    morphTopology.reset(); morphStart.clear(); morphTarget.clear();
}
void MacroMixerSource::Graph::applyMorph(float progress) {
    // Resolve each path anew: assigning a control may rebuild ports or a nested
    // Macro. Do not retain node/parameter pointers across listener callbacks.
    for(const auto& entry : morphTarget) {
        auto* p = findMorphParameter(*this,entry.first);
        if(!p) continue;
        const auto& start = morphStart.at(entry.first);
        std::visit([&](const auto& target) {
            using T = std::decay_t<decltype(target)>;
            if(p->valueType() != typeid(T).name()) return;
            auto value = morphValue(std::get<T>(start.value),target,progress,start.discrete || entry.second.discrete);
            // Keep the underlying value alive even if its listener deletes the
            // port or reloads its owning nested Macro during this assignment.
            auto parameter = p->cast<T>().getParameter();
            if(parameter.get() != value) parameter = value;
        },entry.second.value);
    }
    if(morphTopology) morphTopology->progress(progress);
    if(progress >= 1) {
        if(morphTopology) {
            morphTopology->commit(); liveMorphDefinition=std::move(morphTopology->targetDefinition);
        }
        morphTopology.reset(); morphStart.clear(); morphTarget.clear();
    }
}
MacroMixerMorphPolicy MacroMixerSource::connectionPolicy(const std::string& type,const MacroMixerMorphPolicies& policies,bool discrete) {
    return resolveMorphPolicy(type,policies,discrete);
}
