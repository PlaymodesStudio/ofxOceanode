#include "MacroMixerRuntime.h"
#include <filesystem>
#include <cmath>
#include <set>
#include <algorithm>

namespace {
std::string sourceKey(const std::string& folder) {
    if(folder.empty()) return folder;
    return std::filesystem::weakly_canonical(std::filesystem::absolute(folder)).string();
}
MacroMixerMixing::Value inputFromJson(const std::string& type,const ofJson& json) {
    return std::visit([&](const auto& zero)->MacroMixerMixing::Value {
        using T=std::decay_t<decltype(zero)>;
        if constexpr(std::is_same_v<T,ofTexture*> || std::is_same_v<T,MacroMixerMixing::Object>) {
            if(!json.is_null()) throw std::runtime_error("Saved resource/shape inputs must be null");
            return MacroMixerMixing::neutral(type);
        } else {
            auto numeric=[](const ofJson& item,bool integer) {
                if(!item.is_number() || (integer && !item.is_number_integer())) throw std::runtime_error("Invalid saved input number");
                const auto v=item.get<double>();
                const auto low=integer ? double(std::numeric_limits<int>::lowest()) : -double(std::numeric_limits<float>::max());
                const auto high=integer ? double(std::numeric_limits<int>::max()) : double(std::numeric_limits<float>::max());
                if(!std::isfinite(v) || v<low || v>high) throw std::runtime_error("Saved input number is out of range");
            };
            if constexpr(std::is_same_v<T,bool>) {
                if(!json.is_boolean()) throw std::runtime_error("Invalid saved Boolean input");
            } else if constexpr(std::is_arithmetic_v<T>) numeric(json,std::is_same_v<T,int>);
            else {
                if(!json.is_array() || json.size()>MacroMixerMixing::maxVectorSize) throw std::runtime_error("Invalid saved input vector");
                for(const auto& item:json) numeric(item,std::is_same_v<typename T::value_type,int>);
            }
            return json.get<T>();
        }
    },MacroMixerMixing::neutral(type));
}
}

MacroMixerRuntime::Revision MacroMixerRuntime::importedMacro(const std::string& folder) const {
    const auto found = imports.find(sourceKey(folder));
    return found == imports.end() ? nullptr : found->second;
}
MacroMixerRuntime::Revision MacroMixerRuntime::macro(const std::string& id) const {
    for(const auto& entry : imports) if(entry.second->id() == id) return entry.second;
    return nullptr;
}
std::vector<MacroMixerRuntime::Revision> MacroMixerRuntime::macros() const {
    std::vector<Revision> result;
    for(const auto& id : order) if(auto revision = macro(id)) result.push_back(revision);
    return result;
}
bool MacroMixerRuntime::removeMacro(const std::string& id) {
    for(auto it = imports.begin(); it != imports.end(); ++it) if(it->second->id() == id) {
        const auto removedID = it->second->id();
        if(removedID == currentID() || removedID == destinationID() || removedID == queuedID()) clear();
        order.erase(std::remove(order.begin(), order.end(), removedID), order.end());
        sourcePolicies.erase(removedID); problems.erase(removedID);
        imports.erase(it);
        errors.clear(); return true;
    }
    errors = {{"macro", "Unknown Macro ID"}}; return false;
}
size_t MacroMixerRuntime::checkSources() {
    size_t passed = 0;
    const auto saved = errors;
    for(const auto& id : order) {
        auto entry = std::find_if(imports.begin(), imports.end(), [&](const auto& e) { return e.second->id() == id; });
        if(entry == imports.end()) continue;
        auto revision = MacroMixerSource::importFolder(parent, entry->second->source(), interface, errors, id, keptOut.get());
        if(revision) { entry->second = revision; ++passed; }
        noteProblem(id);
    }
    errors = saved; sourceDetailCache.clear();
    return passed;
}
void MacroMixerRuntime::noteProblem(const std::string& id) {
    // Records the diagnostics of the import just made for this source.
    if(errors.empty()) { problems.erase(id); return; }
    const auto blockedCount = std::count_if(errors.begin(), errors.end(), [](const auto& e) { return !e.node.empty(); });
    problems[id] = blockedCount ? std::to_string(blockedCount) + " blocked node(s); Transition asks whether to keep them out"
        : errors.front().message;
}
std::string MacroMixerRuntime::sourceProblem(const std::string& id) const {
    const auto found = problems.find(id);
    return found == problems.end() ? "" : found->second;
}
bool MacroMixerRuntime::moveMacro(const std::string& id, size_t index) {
    auto found = std::find(order.begin(), order.end(), id);
    if(found == order.end() || index >= order.size()) return false;
    order.erase(found); order.insert(order.begin() + index, id);
    return true;
}
bool MacroMixerRuntime::setContract(const MacroMixerContract& contract) {
    if(next) { errors = {{"routers", "Finish the transition before editing routers"}}; return false; }
    auto resolved=contract;
    try {
        for(auto& r:resolved.routers) {
            if(r.direction!="Input" && r.direction!="Output") throw std::runtime_error("Direction must be Input or Output");
            MacroMixerMixing::routerModel(r.type);
            if(r.direction=="Input") { r.policy.clear(); r.options=ofJson::object(); continue; }
            const auto& policy=MacroMixerMixing::registry().policy(r.type,r.policy);
            r.policy=policy.id; r.options=policy.options(r.options);
        }
    } catch(const std::exception& e) { errors={{"routers",e.what()}}; return false; }
    std::set<std::string> ids, names;
    if(contract.routers.size() > 64) {
        errors = {{"routers", "Declare at most 64 routers"}}; return false;
    }
    for(const auto& r : contract.routers) {
        ofParameter<float> probe; probe.setName(r.name);
        if(r.id.empty() || !ids.insert(r.id).second || ofTrim(r.name).empty()
           || !names.insert(probe.getEscapedName()).second) {
            errors = {{"routers", "Names and IDs must be nonempty and unique"}}; return false;
        }
    }
    // Removing selected routers only narrows the interface; the source graphs
    // retain their ordinary internal routers and cables. Other interface changes
    // still require reimporting the Macros.
    for(const auto& entry : imports) {
        const auto& old = entry.second->contract().routers;
        bool matches = true;
        for(const auto& r : contract.routers) matches = matches && std::any_of(old.begin(), old.end(),
            [&](const auto& candidate) { return r.id == candidate.id && r.name == candidate.name && r.type == candidate.type && r.direction==candidate.direction; });
        if(!matches) { errors = {{entry.second->name(), "Router edit conflicts with this imported Macro. Remove the Macro before changing its interface."}}; return false; }
    }
    Inputs inputs;
    for(const auto& r:resolved.routers) if(r.direction=="Input") {
        const auto found=inputValues.find(r.id);
        inputs[r.id]=found!=inputValues.end() && r.type==MacroMixerMixing::valueType(found->second)
            ? found->second : MacroMixerMixing::neutral(r.type);
    }
    if(resolved.routers.empty()) clear();
    const auto rebind = [&](Revision revision) -> Revision {
        auto updated = std::make_shared<MacroMixerRevision>(*revision);
        updated->interface = resolved;
        return updated;
    };
    for(auto& entry : imports) entry.second = rebind(entry.second);
    if(current) current->revision = rebind(current->revision);
    if(pending && pending->revision) pending->revision = rebind(pending->revision);
    for(auto it=processors.begin();it!=processors.end();) {
        const auto old=std::find_if(interface.routers.begin(),interface.routers.end(),[&](const auto& r){return r.id==it->first;});
        const auto retained=std::find_if(resolved.routers.begin(),resolved.routers.end(),[&](const auto& r){return r.id==it->first && old!=interface.routers.end() && r.type==old->type && r.policy==old->policy && r.direction=="Output";});
        if(retained!=resolved.routers.end()) { ++it; continue; }
        if(old!=interface.routers.end()) try { auto empty=MacroMixerMixing::neutral(old->type); it->second.function(empty,empty,1,MacroMixerMixing::registry().policy(old->type,old->policy).options(old->options)); } catch(...) {}
        it=processors.erase(it);
    }
    for(auto it=renderers.begin();it!=renderers.end();)
        if(!ids.count(it->first)) it=renderers.erase(it); else ++it;
    sourceDetailCache.clear(); transitionDetails.clear();
    inputValues=std::move(inputs); interface = resolved;
    for(auto it = mixedValues.begin(); it != mixedValues.end();)
        if(!ids.count(it->first)) it = mixedValues.erase(it); else ++it;
    const auto first=std::find_if(interface.routers.begin(),interface.routers.end(),[](const auto& r){return r.direction=="Output";});
    lastValue=first==interface.routers.end() ? 0.f : value(first->id);
    errors.clear(); return true;
}
bool MacroMixerRuntime::setMorphPolicy(const std::string& type,const std::string& id,const ofJson& options) {
    if(next) { errors={{"Morph connections","Finish the transition before editing connection policies"}}; return false; }
    try {
        const auto& policy=MacroMixerMixing::registry().policy(type,id);
        connectionPolicies[type]={policy.id,policy.options(options)};
        sourceDetailCache.clear(); errors.clear(); return true;
    } catch(const std::exception& e) { errors={{"Morph connections",e.what()}}; return false; }
}
bool MacroMixerRuntime::setSourceMorphPolicy(const std::string& id,const std::string& type,const std::string& policyID,const ofJson& options) {
    if(next) { errors={{"Morph connections","Finish the transition before editing connection policies"}}; return false; }
    if(!macro(id)) { errors={{"Morph connections","Unknown Macro ID"}}; return false; }
    try {
        const auto& policy=MacroMixerMixing::registry().policy(type,policyID);
        sourcePolicies[id][type]={policy.id,policy.options(options)};
        sourceDetailCache.clear(); errors.clear(); return true;
    } catch(const std::exception& e) { errors={{"Morph connections",e.what()}}; return false; }
}
bool MacroMixerRuntime::clearSourceMorphPolicy(const std::string& id,const std::string& type) {
    if(next) { errors={{"Morph connections","Finish the transition before editing connection policies"}}; return false; }
    const auto found=sourcePolicies.find(id);
    if(found!=sourcePolicies.end() && found->second.erase(type) && found->second.empty()) sourcePolicies.erase(found);
    sourceDetailCache.clear(); errors.clear(); return true;
}
const MacroMixerMorphPolicies& MacroMixerRuntime::sourceMorphPolicies(const std::string& id) const {
    static const MacroMixerMorphPolicies none;
    const auto found=sourcePolicies.find(id);
    return found==sourcePolicies.end() ? none : found->second;
}
MacroMixerMorphPolicies MacroMixerRuntime::morphPoliciesInto(const std::string& id) const {
    auto result=connectionPolicies;
    for(const auto& item:sourceMorphPolicies(id)) result[item.first]=item.second;
    return result;
}
const std::vector<MacroMixerMorphDetail>& MacroMixerRuntime::sourceDetails(const std::string& id) const {
    const auto recorded=transitionDetails.find(id);
    if(recorded!=transitionDetails.end()) return recorded->second;
    const auto found=sourceDetailCache.find(id);
    if(found!=sourceDetailCache.end()) return found->second;
    std::vector<MacroMixerMorphDetail> result;
    if(!current) result.push_back({"live","Graph","Start a source to inspect its morph into this source"});
    else if(inPlaceTarget || next) result.push_back({"live","Graph","Preview is available after the current transition"});
    else {
        auto target=macro(id); std::vector<Diagnostic> diagnostics;
        if(target) target=MacroMixerSource::importFolder(parent,target->source(),interface,diagnostics,id,keptOut.get());
        if(target) {
            result=current->inspectMorph(target,morphPoliciesInto(id));
            result.insert(result.begin(),{"context",target->name(),"From "+current->revision->name()+" to "+target->name()});
        }
        else for(const auto& error:diagnostics) result.push_back({"fallback",error.node.empty() ? error.file : error.file+" / "+error.node,
            error.node.empty() ? error.message : error.message+" Transition asks whether to keep it out of the graph."});
    }
    return sourceDetailCache.emplace(id,std::move(result)).first->second;
}
bool MacroMixerRuntime::setInputs(const Inputs& values) {
    try {
        if(values.size()!=inputValues.size()) throw std::runtime_error("Input values do not match the router contract");
        for(const auto& value:values) {
            const auto r=std::find_if(interface.routers.begin(),interface.routers.end(),[&](const auto& r){return r.id==value.first && r.direction=="Input";});
            if(r==interface.routers.end()) throw std::runtime_error("Unknown input router: "+value.first);
            MacroMixerMixing::Registry::validate(r->type,value.second);
        }
        inputValues=values; return true;
    } catch(const std::exception& e) { errors={{"inputs",e.what()}}; return false; }
}
float MacroMixerRuntime::value(const std::string& routerID) const {
    const auto found = mixedValues.find(routerID);
    return found == mixedValues.end() || !std::holds_alternative<float>(found->second) ? 0.f : std::get<float>(found->second);
}
const std::vector<float>& MacroMixerRuntime::vectorValue(const std::string& routerID) const {
    static const std::vector<float> empty{0.f};
    const auto found=mixedValues.find(routerID);
    return found==mixedValues.end() || !std::holds_alternative<std::vector<float>>(found->second)
        ? empty : std::get<std::vector<float>>(found->second);
}
MacroMixerMixing::Value MacroMixerRuntime::outputValue(const std::string& routerID) const {
    const auto router=std::find_if(interface.routers.begin(),interface.routers.end(),[&](const auto& r){return r.id==routerID;});
    if(router==interface.routers.end()) throw std::runtime_error("Unknown router ID: "+routerID);
    const auto found=mixedValues.find(routerID);
    if(found!=mixedValues.end()) {
        // An interface edit can change the type of an inactive retained value.
        if(router->type==MacroMixerMixing::valueType(found->second)) return found->second;
    }
    return MacroMixerMixing::neutral(router->type);
}
bool MacroMixerRuntime::importMacro(const std::string& folder, bool refresh) {
    if(next) {
        errors = {{folder,"Import/Refresh is unavailable during a transition; finish it first"}};
        return false;
    }
    if(refresh) { sourceDetailCache.clear(); transitionDetails.clear(); }
    auto previous = importedMacro(folder);
    if(previous && !refresh) { errors.clear(); return true; }
    if(!previous && order.size() >= 128) { errors = {{folder,"Maximum 128 imported macros"}}; return false; }
    preparing = true;
    auto revision = MacroMixerSource::importFolder(parent,folder,interface,errors,previous ? previous->id() : "",keptOut.get());
    preparing = false;
    if(previous) noteProblem(previous->id());
    if(!revision) { noteBlocked(folder,refresh); return false; }
    sourceDetailCache.clear(); transitionDetails.clear();
    imports[sourceKey(folder)] = revision;
    if(!previous) order.push_back(revision->id());
    return true;
}

void MacroMixerRuntime::noteBlocked(const std::string& folder, bool refresh) {
    std::vector<Diagnostic> nodes;
    for(const auto& error:errors) if(!error.node.empty()) nodes.push_back(error);
    if(nodes.empty()) return;
    blocked=std::move(nodes); blockedRequest.reset();
    blockedImport=folder; blockedRefresh=refresh;
}
bool MacroMixerRuntime::keepOutOfGraph(const std::string& folder, const std::string& node) {
    if(folder.empty() || node.empty()) return false;
    keptOut->insert({folder,node});
    blocked.erase(std::remove_if(blocked.begin(),blocked.end(),[&](const auto& b) { return b.file==folder && b.node==node; }),blocked.end());
    sourceDetailCache.clear();
    if(!blocked.empty()) return true;
    errors.clear();
    if(blockedRequest) {
        // Keep a newer request; otherwise retry the blocked activation on update.
        if(!pending) pending=std::move(blockedRequest);
        blockedRequest.reset(); blockedImport.clear();
        return true;
    }
    if(blockedImport.empty()) return true;
    const auto folderToImport=std::move(blockedImport); blockedImport.clear();
    return importMacro(folderToImport,blockedRefresh);
}
void MacroMixerRuntime::cancelBlocked() {
    blocked.clear(); blockedRequest.reset(); blockedImport.clear();
}
bool MacroMixerRuntime::restoreKeptOutNode(const std::string& folder, const std::string& node) {
    if(!keptOut->erase({folder,node})) return false;
    sourceDetailCache.clear(); return true;
}

ofJson MacroMixerRuntime::saveState() const {
    const auto policiesJson=[](const MacroMixerMorphPolicies& items) {
        auto result=ofJson::object();
        for(const auto& item:items) result[item.first]={{"policy",item.second.policy},{"options",item.second.options}};
        return result;
    };
    ofJson routers = ofJson::array(), entries = ofJson::array(), inputs=ofJson::object(), policies=policiesJson(connectionPolicies);
    for(const auto& r : interface.routers)
        routers.push_back({{"id", r.id}, {"name", r.name}, {"type", r.type}, {"policy",r.policy}, {"options",r.options}, {"direction",r.direction}});
    for(const auto& item:inputValues) inputs[item.first]=MacroMixerMixing::toJson(item.second);
    for(const auto& revision : macros()) {
        ofJson entry{{"id", revision->id()}, {"name", revision->name()}, {"source", revision->source()}};
        const auto& own=sourceMorphPolicies(revision->id());
        if(!own.empty()) entry["morphPolicies"]=policiesJson(own);
        entries.push_back(std::move(entry));
    }
    auto keptOutNodes=ofJson::array();
    for(const auto& node:*keptOut) keptOutNodes.push_back({{"folder",node.first},{"node",node.second}});
    const auto playback = inPlaceTarget ? inPlaceTarget : next ? next->revision : current ? current->revision
        : pending ? pending->revision : nullptr;
    return {{"version", 1}, {"routers", routers}, {"macros", entries}, {"inputs",inputs}, {"morphPolicies",policies},
        {"playback", playback ? ofJson(playback->id()) : ofJson(nullptr)},
        {"morph", next ? nextMorph : current ? currentMorph : pending ? pending->morph : false},
        {"keptOutNodes", keptOutNodes}};
}

bool MacroMixerRuntime::restoreState(const ofJson& saved) {
    if(next) {
        errors = {{"MacroMixer", "Clear playback before restoring during a fade"}};
        return false;
    }
    // Restore the reference list even when folders are unavailable, so users
    // can inspect/recover it. The requested playback is loaded on update and may
    // fail independently when the Macro source is unavailable.
    try {
        auto require = [](bool valid, const std::string& message) {
            if(!valid) throw std::runtime_error(message);
        };
        require(saved.is_object(), "Invalid MacroMixer state");
        const auto version = saved.at("version");
        require(version == 1, "Unsupported MacroMixer state version");
        const auto& routers = saved.at("routers");
        require(routers.is_array() && routers.size() <= 64, "Invalid saved router list");
        MacroMixerContract contract; contract.routers.clear();
        for(const auto& r : routers) {
            MacroMixerContract::Router item{r.at("id").get<std::string>(),r.at("name").get<std::string>(),r.at("type").get<std::string>()};
            item.direction=r.at("direction").get<std::string>();
            require(item.direction=="Input" || item.direction=="Output","Invalid saved router direction");
            item.policy=r.at("policy").get<std::string>(); item.options=r.at("options");
            if(item.direction=="Output") require(!item.policy.empty(),"Missing saved mixing policy");
            else require(item.policy.empty() && item.options.is_object() && item.options.empty(),"Input routers do not have mixing policies");
            contract.routers.push_back(std::move(item));
        }
        MacroMixerRuntime staged(parent);
        const bool savedMorph = saved.value("morph",false);
        if(saved.contains("morphPolicies")) {
            const auto& policies=saved.at("morphPolicies");
            require(policies.is_object() && policies.size()<=128,"Invalid saved morph connection policies");
            for(auto item=policies.begin();item!=policies.end();++item) {
                if(!staged.setMorphPolicy(item.key(),item.value().at("policy").get<std::string>(),item.value().at("options"))) {
                    errors=staged.diagnostics(); return false;
                }
            }
        }
        if(!staged.setContract(contract)) { errors = staged.diagnostics(); return false; }
        {
            const auto& inputs=saved.at("inputs");
            require(inputs.is_object() && inputs.size()==staged.inputValues.size(),"Invalid saved inputs");
            for(const auto& router:staged.interface.routers) if(router.direction=="Input")
                staged.inputValues[router.id]=inputFromJson(router.type,inputs.at(router.id));
        }
        const auto& entries = saved.at("macros");
        require(entries.is_array() && entries.size() <= 128, "Invalid saved Macro list");
        for(const auto& entry : entries) {
            auto reference = std::shared_ptr<MacroMixerRevision>(new MacroMixerRevision);
            reference->macroID = entry.at("id").get<std::string>();
            reference->displayName = entry.at("name").get<std::string>();
            const auto path = entry.at("source").get<std::string>();
            require(!reference->macroID.empty() && !reference->displayName.empty(), "Missing Macro ID or name");
            require(!path.empty() && std::filesystem::path(path).is_absolute(), "Missing or non-absolute source folder");
            reference->sourcePath = sourceKey(path);
            reference->interface = contract;
            require(!staged.macro(reference->id()) && !staged.imports.count(reference->source()), "Duplicate saved Macro ID or source");
            staged.imports.emplace(reference->source(), reference);
            staged.order.push_back(reference->id());
            if(entry.contains("morphPolicies")) { // Older states have none.
                const auto& policies=entry.at("morphPolicies");
                require(policies.is_object() && policies.size()<=128,"Invalid saved source morph policies");
                for(auto item=policies.begin();item!=policies.end();++item)
                    if(!staged.setSourceMorphPolicy(reference->id(),item.key(),item.value().at("policy").get<std::string>(),item.value().at("options"))) {
                        errors=staged.diagnostics(); return false;
                    }
            }
        }
        MacroMixerExclusions keptOutNodes; // Older states have none.
        if(saved.contains("keptOutNodes")) {
            const auto& nodes=saved.at("keptOutNodes");
            require(nodes.is_array() && nodes.size()<=4096,"Invalid saved kept-out nodes");
            for(const auto& node:nodes) {
                const auto folder=node.at("folder").get<std::string>(), key=node.at("node").get<std::string>();
                require(std::filesystem::path(folder).is_absolute() && !key.empty(),"Invalid saved kept-out node");
                keptOutNodes.insert({folder,key});
            }
        }
        const auto& playback = saved.at("playback");
        if(!playback.is_null()) {
            const auto id = playback.get<std::string>();
            auto reference = staged.macro(id);
            require(bool(reference), "Playback target does not belong to a saved Macro");
            staged.pending = std::make_unique<Request>(Request{reference->source(), reference, 0, 0, savedMorph});
        }
        clear();
        interface = std::move(staged.interface);
        inputValues=std::move(staged.inputValues);
        connectionPolicies=std::move(staged.connectionPolicies);
        sourcePolicies=std::move(staged.sourcePolicies); problems.clear();
        imports = std::move(staged.imports); order = std::move(staged.order);
        pending = std::move(staged.pending);
        *keptOut = std::move(keptOutNodes);
        return true;
    } catch(const std::exception& e) {
        errors = {{"MacroMixer", e.what()}}; return false;
    }
}

float MacroMixerRuntime::shape(float t, float power) {
    t = std::max(0.f, std::min(1.f, t));
    if(t == 0 || t == 1 || power == 0) return t;
    // Same bi-pow mapping as MacroMorphEngine::customPow.
    float v = t * 2.f - 1.f;
    const float k1 = 2.f * power * 0.99999f;
    const float k2 = k1 / ((-power * 0.999999f) + 1.f);
    return (v * (k2 + 1.f) / (k2 * std::abs(v) + 1.f) + 1.f) * 0.5f;
}

void MacroMixerRuntime::request(const std::string& folder, float ms, float power, bool morph) {
    pending = std::make_unique<Request>(Request{folder, importedMacro(folder),
        std::isfinite(ms) ? std::max(0.f, ms) : 0.f,
        std::isfinite(power) ? std::max(-1.f, std::min(1.f, power)) : 0.f, morph});
}
void MacroMixerRuntime::requestMacro(const std::string& id, float ms, float power, bool morph) {
    auto revision = macro(id);
    if(!revision) { errors = {{"macro", "Choose an imported Macro"}}; return; }
    request(revision->source(), ms, power, morph);
}

void MacroMixerRuntime::update() {
    updateWithClock([] { return double(ofGetElapsedTimeMillis()); });
}
void MacroMixerRuntime::update(double nowMs) {
    updateWithClock([nowMs] { return nowMs; });
}
void MacroMixerRuntime::updateWithClock(const std::function<double()>& clock) {
    bool canPrepare=true;
    if(!next && current && pending) for(const auto& r:interface.routers) {
        auto processor=processors.find(r.id);
        if(processor==processors.end()) continue;
        const auto& policy=MacroMixerMixing::registry().policy(r.type,r.policy);
        auto value=current->read(r.id);
        if(auto* object=std::get_if<MacroMixerMixing::Object>(&value)) object->owner=current;
        try {
            auto result=processor->second.function(value,value,1,policy.options(r.options));
            if(policy.ready && !policy.ready(result)) canPrepare=false;
        } catch(const std::exception& e) { errors={{"audio preparation",e.what()}}; canPrepare=false; }
    }
    if(!next && pending && canPrepare) {
        auto request = std::move(pending);
        // A queued request pins the entry identity, not its old file bytes.
        // Read the current source when preparation actually begins. A missing
        // or invalid folder fails this request and leaves the origin running.
        Revision revision;
        if(importMacro(request->folder, true)) revision = importedMacro(request->folder);
        else if(!blocked.empty() && blockedImport==request->folder) {
            // Retried once every blocked node has been kept out of the graph.
            blockedRequest=std::move(request); blockedImport.clear();
        }
        if(revision) {
            preparing = true;
            const auto morphStart = request->morph && current ? current->captureMorphValues() : MacroMixerSource::MorphSnapshot{};
            sourceDetailCache.clear();
            if(request->morph && current) {
                auto details=current->inspectMorph(revision,morphPoliciesInto(revision->id()));
                details.insert(details.begin(),{"context",revision->name(),"From "+current->revision->name()+" to "+revision->name()});
                transitionDetails[revision->id()]=std::move(details);
            }
            const bool reuse=request->morph && current && current->prepareMorphInPlace(revision,morphPoliciesInto(revision->id()));
            if(request->morph && current && !reuse) {
                auto& details=transitionDetails[revision->id()];
                if(std::none_of(details.begin(),details.end(),[](const auto& d) {return d.kind=="fallback";}))
                    details.push_back({"fallback","Graph",current->morphFallbackReason});
                for(auto& detail:details) if(detail.kind=="mix") { detail.kind="skipped"; detail.message="Connection mix skipped because the whole graph needs fallback. "+detail.message; }
            }
            std::shared_ptr<Graph> candidate = reuse ? current : MacroMixerSource::instantiate(parent, revision, errors, inputValues, keptOut);
            preparing = false;
            if(candidate) {
                if(!current) {
                    current = std::move(candidate);
                    currentMorph = request->morph;
                    mixedProgress = 1;
                } else {
                    if(request->morph && !reuse) candidate->prepareMorph(morphStart,current.get());
                    next = std::move(candidate);
                    inPlaceTarget=reuse ? revision : nullptr;
                    nextMorph = request->morph;
                    nextMorphReady = (reuse ? next->morphConnectionsReady() : !nextMorph || std::none_of(interface.routers.begin(),interface.routers.end(),[](const auto& r) {
                        return r.direction=="Output" && bool(MacroMixerMixing::registry().policy(r.type,r.policy).createProcessor);
                    }));
                    durationMs = request->durationMs;
                    biPow = request->biPow;
                    startMs = clock(); // Loading time is outside the fade.
                    mixedProgress = 0;
                }
            }
        }
    }
    if(morphing() && nextMorphReady) for(const auto& r:interface.routers) {
        const auto processor=processors.find(r.id);
        if(processor==processors.end()) continue;
        const auto& policy=MacroMixerMixing::registry().policy(r.type,r.policy);
        if(policy.ready && !policy.ready(outputValue(r.id))) nextMorphReady=false;
    }
    const float t=(finishRequested || durationMs<=0) ? 1.f : float(std::max(0.0,clock()-startMs)/durationMs);
    const float progress=morphing() && !nextMorphReady ? 0.f : next ? shape(t,biPow) : 1.f;
    if(morphing()) {
        try { next->applyMorph(progress); }
        catch(const std::exception& e) {
            errors={{"morph",e.what()}}; next->cancelMorph(); next.reset(); inPlaceTarget.reset(); finishRequested=false; return;
        }
    }
    const bool originValid = !current || morphing() || (current->setInputs(inputValues,false) && current->update());
    const bool destinationValid = !next || (next->setInputs(inputValues,false) && next->update());
    if(!originValid || !destinationValid) {
        errors = {{"runtime", !originValid ? current->failure : next->failure}};
        if(next) next->cancelMorph();
        next.reset(); inPlaceTarget.reset(); finishRequested = false;
        return; // Hold every last finite outer output on an invalid internal graph.
    }
    if(current) {
        try {
            // Validate every result before publishing any router or promoting B.
            std::map<std::string,MacroMixerMixing::Value> staged;
            bool resourcesReady=!morphing() || next->morphConnectionsReady();
            for(const auto& router:interface.routers) {
                if(router.direction=="Input") continue;
                if(router.type=="Texture") {
                    staged[router.id]=outputValue(router.id); // Owned previous frame, never a graph pointer.
                    continue;
                }
                const auto& policy=MacroMixerMixing::registry().policy(router.type,router.policy);
                const auto graph=morphing() ? next : current;
                if(morphMode() && !policy.createProcessor) {
                    auto value = graph->read(router.id);
                    MacroMixerMixing::Registry::validate(router.type,value);
                    if(auto* object=std::get_if<MacroMixerMixing::Object>(&value)) object->owner=graph;
                    staged[router.id]=std::move(value);
                    continue;
                }
                auto a=graph->read(router.id);
                auto b=next ? next->read(router.id) : a;
                if(policy.createProcessor) {
                    MacroMixerMixing::Registry::validate(router.type,a); MacroMixerMixing::Registry::validate(router.type,b);
                    if(auto* object=std::get_if<MacroMixerMixing::Object>(&a)) object->owner=graph;
                    if(auto* object=std::get_if<MacroMixerMixing::Object>(&b)) object->owner=next ? next : current;
                    auto& processor=processors[router.id];
                    if(processor.policy!=policy.id || !processor.function) processor={policy.id,policy.createProcessor()};
                    // Resource bridges retain graph leases and backend readiness.
                    // A graph morph feeds only the destination at the identity
                    // endpoint; no router-output interpolation takes place.
                    auto result=processor.function(a,b,morphMode() ? 1.f : progress,policy.options(router.options));
                    MacroMixerMixing::Registry::validate(router.type,result);
                    if(policy.ready) resourcesReady=policy.ready(result) && resourcesReady;
                    staged[router.id]=std::move(result);
                } else staged[router.id]=MacroMixerMixing::registry().mix(router.type,router.policy,a,b,progress,router.options);
            }
            mixedValues=std::move(staged); mixedProgress=progress;
            const auto first=std::find_if(interface.routers.begin(),interface.routers.end(),[](const auto& r){return r.direction=="Output";});
            lastValue=first==interface.routers.end() ? 0.f : value(first->id);
            if(next && !resourcesReady) { startMs=clock(); mixedProgress=0; if(morphing()) nextMorphReady=false; }
            else if(morphing() && !nextMorphReady) { nextMorphReady=true; startMs=clock(); mixedProgress=0; }
            else if(next && t>=1) {
                sourceDetailCache.clear();
                current=std::move(next); currentMorph=nextMorph;
                if(inPlaceTarget) {
                    current->revision=std::move(inPlaceTarget);
                    current->source=current->revision->source();
                }
            }
        } catch(const std::exception& e) {
            errors={{"runtime",std::string("Mix failed; holding all last finite router values: ")+e.what()}};
            if(next) next->cancelMorph();
            next.reset(); inPlaceTarget.reset();
        }
    }
    // A requested finish survives a resource preparation fence until the
    // destination can actually receive its final controls and be promoted.
    finishRequested = finishRequested && morphing();
}

void MacroMixerRuntime::draw() {
    if(!current) return;
    const bool textures=current->usesTextures || (next && next->usesTextures);
    try {
        const auto drawFrame=[&] {
            if(!morphing() && (!current->setInputs(inputValues) || !current->draw())) throw std::runtime_error(current->failure);
            if(next && (!next->setInputs(inputValues) || !next->draw())) throw std::runtime_error(next->failure);
            for(const auto& r:interface.routers) if(r.direction=="Output") {
                const auto graph=morphing() ? next : current;
                const auto a=graph->read(r.id), b=next ? next->read(r.id) : a;
                if(r.type!="Texture") {
                    if(MacroMixerMixing::registry().policy(r.type,r.policy).createProcessor) continue;
                    if(morphMode()) {
                        auto value=b;
                        MacroMixerMixing::Registry::validate(r.type,value);
                        if(auto* object=std::get_if<MacroMixerMixing::Object>(&value)) object->owner=next ? next : current;
                        mixedValues[r.id]=std::move(value);
                        continue;
                    }
                    mixedValues[r.id]=MacroMixerMixing::registry().mix(r.type,r.policy,a,b,next?mixedProgress:1.f,r.options);
                    continue;
                }
                // A private identity copy keeps texture outputs alive between
                // update/draw and bypasses all saved blend/strength options.
                const auto& policy=MacroMixerMixing::registry().policy(r.type,morphMode() ? "crossfade" : r.policy);
                const auto rendererID=morphMode() ? "morph-copy" : policy.id;
                auto& renderer=renderers[r.id];
                const auto evaluate=[&](const MacroMixerMixing::Policy::Function& function) {
                    auto result=function(a,b,morphMode() ? 1.f : next?mixedProgress:1.f,policy.options(morphMode() ? ofJson::object() : r.options));
                    MacroMixerMixing::Registry::validate(r.type,result);
                    return result;
                };
                if(renderer.policy!=rendererID || !renderer.function) {
                    if(!policy.createRenderer) throw std::runtime_error("Texture policy has no renderer");
                    Renderer candidate{rendererID,policy.createRenderer()};
                    const auto result=evaluate(candidate.function);
                    // Keep the previous renderer/output alive if the new policy fails.
                    renderer=std::move(candidate); mixedValues[r.id]=result;
                } else {
                    mixedValues[r.id]=evaluate(renderer.function);
                }
            }
            const auto first=std::find_if(interface.routers.begin(),interface.routers.end(),[](const auto& r){return r.direction=="Output";});
            lastValue=first==interface.routers.end() ? 0.f : value(first->id);
        };
        if(textures) MacroMixerMixing::drawIsolated(drawFrame); else drawFrame();
    } catch(const std::exception& e) { errors={{"graph draw",e.what()}}; }
}

void MacroMixerRuntime::clear() {
    // Restore temporary native routing while the graph owners still exist.
    // Backend retirement leases then outlive both the transition and playback.
    if(next) next->cancelMorph();
    if(current && current!=next) current->cancelMorph();
    for(auto& processor:processors) {
        const auto r=std::find_if(interface.routers.begin(),interface.routers.end(),[&](const auto& r){return r.id==processor.first;});
        if(r==interface.routers.end()) continue;
        try { auto empty=MacroMixerMixing::neutral(r->type); processor.second.function(empty,empty,1.f,MacroMixerMixing::registry().policy(r->type,r->policy).options(r->options)); }
        catch(...) {} // Destruction must still release the owned containers.
    }
    processors.clear();
    pending.reset(); next.reset(); current.reset(); inPlaceTarget.reset(); errors.clear();
    cancelBlocked();
    sourceDetailCache.clear(); transitionDetails.clear();
    finishRequested = false; lastValue = 0; mixedProgress = 0;
    currentMorph = false; nextMorph = false;
    nextMorphReady = true;
    mixedValues.clear();
    renderers.clear();
}
std::string MacroMixerRuntime::status() const {
    std::string label;
    switch(state()) {
        case State::Empty: label = "Empty"; break;
        case State::Preparing: label = "Preparing"; break;
        case State::Running: label = "Running"; break;
        case State::Transitioning: label = "Transitioning"; break;
    }
    if(morphingInPlace()) label = "Morphing live graph";
    else if(morphing()) label = "Morphing destination graph: " + (current && !current->morphFallbackReason.empty()
        ? current->morphFallbackReason : "incompatible structure or configuration");
    if(pending) label += " (request queued)";
    if(!blocked.empty()) label += " (" + std::to_string(blocked.size()) + " blocked node(s) await a decision)";
    if(!errors.empty()) label += ": " + errors.front().file + ": " + errors.front().message;
    return label;
}
