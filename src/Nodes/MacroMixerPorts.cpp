#include "MacroMixerPorts.h"
#include <algorithm>
#include <limits>
#include <set>

namespace {
constexpr auto transient = ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_DisableSaveProject;
bool matchesType(const ofxOceanodeAbstractParameter& parameter,const std::string& type) {
    return std::visit([&](const auto& value) {
        if constexpr(std::is_same_v<std::decay_t<decltype(value)>,MacroMixerMixing::Object>)
            return parameter.valueType()==MacroMixerMixing::registry().adapter(type).description;
        else return parameter.valueType()==typeid(value).name();
    },MacroMixerMixing::neutral(type));
}
}

MacroMixerPorts::MacroMixerPorts(ofxOceanodeNodeModel& node, std::string separator) : node(node), separator(std::move(separator)) {}

bool MacroMixerPorts::matches(ofxOceanodeAbstractParameter& parameter,const MacroMixerContract::Router& router) {
    return matchesType(parameter,router.type)
        && bool(parameter.getFlags() & ofxOceanodeParameterFlags_DisableInConnection)==(router.direction=="Output");
}

ofxOceanodeAbstractParameter* MacroMixerPorts::parameter(const std::string& id) const {
    const auto found = installed.find(id);
    return found == installed.end() ? nullptr : found->second.get();
}

std::string MacroMixerPorts::separatorName() const {
    if(separator.empty()) return "";
    auto& group = node.getParameterGroup();
    for(int i = 0; i < int(group.size()); ++i)
        if(group.get(i).getName().rfind("SEPARATOR:|" + separator + "|", 0) == 0) return group.get(i).getName();
    return "";
}

void MacroMixerPorts::removeIf(const std::function<bool(const std::string& id, ofxOceanodeAbstractParameter& parameter)>& remove) {
    auto* host = node.getHostContainer();
    for(auto it = installed.begin(); it != installed.end();) {
        auto& parameter = *it->second;
        if(!remove(it->first, parameter)) { ++it; continue; }
        const auto id = it->first;
        triggerInputs.erase(id); lastInputs.erase(id); cabledInputs.erase(id);
        if(host && parameter.isTimelined()) {
            if(auto* extension = host->getAutomationExtension())
                extension->removeParameterBindings(parameter);
        }
        if(host && host->customGuiContainsParameterAnywhere(parameter)) {
            if(auto* extension = host->getCustomGuiExtension()) extension->removeParameterBindings(parameter);
        }
        parameter.removeAllConnections();
        node.removeParameter(parameter.getName());
        list.erase(std::remove_if(list.begin(), list.end(), [&](const Port& port) { return port.id == id; }), list.end());
        it = installed.erase(it);
    }
}

void MacroMixerPorts::install(const MacroMixerContract& contract, const Values* inputs) {
    auto& group = node.getParameterGroup();
    removeIf([&](const std::string& id, ofxOceanodeAbstractParameter& parameter) {
        return std::none_of(contract.routers.begin(), contract.routers.end(), [&](const auto& r) { return r.id == id && matches(parameter,r); });
    });
    if(!separator.empty() && separatorName().empty()) node.addSeparator(separator);
    // The node's other parameters keep their order; the separator and the routers follow them.
    const auto separatorParameter = separatorName();
    std::set<std::string> kept;
    for(const auto& port : installed) kept.insert(port.second->getName());
    std::vector<std::string> order;
    for(int i = 0; i < int(group.size()); ++i) {
        const auto name = group.get(i).getName();
        if(!kept.count(name) && (separatorParameter.empty() || name != separatorParameter)) order.push_back(name);
    }
    if(!separatorParameter.empty()) order.push_back(separatorParameter);
    for(const auto& router : contract.routers) {
        auto found = installed.find(router.id);
        if(found == installed.end()) {
            const bool input = router.direction=="Input";
            const auto initial = inputs ? inputs->find(router.id) : Values::const_iterator();
            const bool given = inputs && initial != inputs->end();
            std::visit([&](const auto& value) {
                using T=std::decay_t<decltype(value)>;
                if constexpr(std::is_same_v<T,MacroMixerMixing::Object>) {
                    const auto& adapter=MacroMixerMixing::registry().adapter(router.type);
                    auto p=adapter.makePort(router.name,input && given ? initial->second : MacroMixerMixing::Value(value));
                    const auto flags=input ? ofxOceanodeParameterFlags_DisableOutConnection
                        | ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_DisableSaveProject
                        : transient | ofxOceanodeParameterFlags_DisableInConnection | ofxOceanodeParameterFlags_DisplayMinimized;
                    p->setFlags(flags); p->setNodeModel(&node);
                    group.add(*p);
                    // The group holds its own reference; cables and bindings live on that one.
                    installed[router.id]=std::dynamic_pointer_cast<ofxOceanodeAbstractParameter>(*(group.end()-1));
                } else {
                auto p=std::make_shared<ofParameter<T>>();
                if constexpr(std::is_arithmetic_v<T>)
                    p->set(router.name,value,std::numeric_limits<T>::lowest(),std::numeric_limits<T>::max());
                else if constexpr(std::is_pointer_v<T>) p->set(router.name,value);
                else {
                    using Item=typename T::value_type;
                    p->set(router.name,value,T{std::numeric_limits<Item>::lowest()},T{std::numeric_limits<Item>::max()});
                }
                if(input) {
                    if(given && std::holds_alternative<T>(initial->second)) *p=std::get<T>(initial->second);
                    installed[router.id]=node.addParameter(*p,ofxOceanodeParameterFlags_DisableOutConnection
                        | ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_DisableSaveProject);
                } else installed[router.id]=node.addOutputParameter(*p,transient);
                }
            },MacroMixerMixing::neutral(router.type));
            lastInputs.erase(router.id); cabledInputs.erase(router.id);
        } else if(found->second->getName() != router.name) {
            found->second->setName(router.name);
        }
        order.push_back(router.name);
    }
    list.clear();
    for(const auto& router : contract.routers) list.push_back({router.id, router.name, router.type, router.direction});
    triggerInputs.clear();
    for(const auto& router : contract.routers) if(router.direction == "Input" && router.type == MacroMixerMixing::triggerType)
        triggerInputs[router.id] = installed.at(router.id)->cast<void>().getParameter().newListener([this, id = router.id] {
            if(onTriggerInput) onTriggerInput(id);
        });
    // OF reorders the existing shared parameter objects, preserving cable pointers.
    group.reorder(order);
    node.parameterGroupChanged.notify();
}

void MacroMixerPorts::clear() {
    if(installed.empty() && separatorName().empty()) return;
    removeIf([](const std::string&, ofxOceanodeAbstractParameter&) { return true; });
    if(!separator.empty()) node.removeSeparator(separator);
    node.parameterGroupChanged.notify();
}

bool MacroMixerPorts::syncInputs(MacroMixerRuntime& runtime, bool onlyChanged) {
    const auto& routers = runtime.contract().routers;
    if(!onlyChanged) {
        MacroMixerRuntime::Inputs values;
        for(const auto& r : routers) if(r.direction=="Input") {
            const auto found = installed.find(r.id);
            if(found != installed.end()) values[r.id] = MacroMixerMixing::readPort(r.type, *found->second);
        }
        return runtime.setInputs(values);
    }
    auto values = runtime.inputs();
    Values written;
    std::map<std::string, bool> cabled; // Uncomparable ports read in this pass.
    bool changed = false;
    for(const auto& port : list) {
        if(port.direction != "Input" || port.type == MacroMixerMixing::triggerType || !values.count(port.id)) continue;
        const auto router = std::find_if(routers.begin(), routers.end(), [&](const auto& r) { return r.id == port.id; });
        if(router == routers.end() || router->type != port.type || router->direction != "Input") continue;
        auto& parameter = *installed.at(port.id);
        auto value = MacroMixerMixing::readPort(port.type, parameter);
        if(std::holds_alternative<MacroMixerMixing::Object>(value)) {
            const bool has = parameter.hasInConnection();
            cabled[port.id] = has;
            if(!has && !cabledInputs.count(port.id)) continue;
        } else {
            const auto last = lastInputs.find(port.id);
            if(last != lastInputs.end() && last->second == value) continue;
            written[port.id] = value;
        }
        values[port.id] = std::move(value); changed = true;
    }
    if(!changed) return true;
    if(!runtime.setInputs(values)) return false;
    for(auto& entry : written) lastInputs[entry.first] = std::move(entry.second);
    for(const auto& entry : cabled) { if(entry.second) cabledInputs.insert(entry.first); else cabledInputs.erase(entry.first); }
    return true;
}

void MacroMixerPorts::forgetInputs() { lastInputs.clear(); cabledInputs.clear(); }

void MacroMixerPorts::write(const Port& port, const MacroMixerMixing::Value& value) {
    auto& parameter = *installed.at(port.id);
    std::visit([&](const auto& v) {
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,MacroMixerMixing::Object>) {
            const auto& adapter=MacroMixerMixing::registry().adapter(v.type);
            if(parameter.valueType()==adapter.description) adapter.write(parameter,v);
        } else if(parameter.valueType()==typeid(T).name()) parameter.cast<T>().getParameter()=v;
    },value);
}

void MacroMixerPorts::publish(MacroMixerRuntime& runtime) {
    const auto& routers = runtime.contract().routers;
    for(const auto& output : installed) {
        const auto port = std::find_if(list.begin(), list.end(), [&](const Port& p) { return p.id == output.first; });
        if(port == list.end() || port->direction != "Output") continue;
        if(std::none_of(routers.begin(), routers.end(), [&](const auto& r) { return r.id == port->id && r.direction == "Output"; })) continue;
        write(*port, runtime.takeOutput(port->id));
    }
}

void MacroMixerPorts::publish(const Values& outputs) {
    for(const auto& output : installed) {
        const auto port = std::find_if(list.begin(), list.end(), [&](const Port& p) { return p.id == output.first; });
        if(port == list.end() || port->direction != "Output") continue;
        const auto value = outputs.find(port->id);
        if(value != outputs.end()) write(*port, value->second);
    }
}

MacroMixerPorts::Values MacroMixerPorts::takeOutputs(MacroMixerRuntime& runtime) {
    Values values;
    for(const auto& router : runtime.contract().routers)
        if(router.direction == "Output") values[router.id] = runtime.takeOutput(router.id);
    return values;
}

ofJson MacroMixerPorts::savePorts() const {
    auto saved = ofJson::array();
    for(const auto& port : list)
        saved.push_back({{"id", port.id}, {"name", port.name}, {"type", port.type}, {"direction", port.direction}});
    return saved;
}

bool MacroMixerPorts::restorePorts(const ofJson& saved) {
    MacroMixerContract contract; contract.routers.clear();
    try {
        if(!saved.is_array()) return false;
        std::set<std::string> ids;
        for(const auto& item : saved) {
            MacroMixerContract::Router router;
            router.id = item.at("id").get<std::string>(); router.name = item.at("name").get<std::string>();
            router.type = item.at("type").get<std::string>(); router.direction = item.at("direction").get<std::string>();
            if(router.id.empty() || !ids.insert(router.id).second || (router.direction != "Input" && router.direction != "Output")) return false;
            try { MacroMixerMixing::neutral(router.type); } catch(const std::exception&) { continue; }
            contract.routers.push_back(std::move(router));
        }
    } catch(const std::exception&) { return false; }
    install(contract);
    return true;
}
