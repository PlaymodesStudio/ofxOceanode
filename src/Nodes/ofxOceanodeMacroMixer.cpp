#include "ofxOceanodeMacroMixer.h"
#include "ofxOceanodeNodeRegistry.h"
#include "MacroDialogAppearance.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <set>
#if defined(TARGET_OSX)
#include <objc/message.h>
#include <objc/runtime.h>
#elif defined(TARGET_WIN32)
#include <commdlg.h>
#endif

namespace {
std::vector<ofxOceanodeMacroMixer*> liveMixers;
constexpr auto transient = ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_DisableSaveProject;
constexpr auto routerGroupFormat = "ofxOceanodeMacroMixer.routergroup";
#if defined(TARGET_OSX)
// Call Cocoa from this C++ translation unit so hosts need no extra .mm source.
template<typename Result=id, typename... Args>
Result panelMessage(id object, const char* selector, Args... args) {
    return reinterpret_cast<Result(*)(id,SEL,Args...)>(objc_msgSend)(object,sel_registerName(selector),args...);
}
id panelClass(const char* name) { return reinterpret_cast<id>(objc_getClass(name)); }
id panelString(const std::string& text) { return panelMessage(panelClass("NSString"),"stringWithUTF8String:",text.c_str()); }
#endif
ofFileDialogResult saveRouterGroupDialog(const std::string& directory) {
#if defined(TARGET_OSX)
    // ofSystemSaveDialog only sets the name field on macOS, not the directory.
    const auto pool=panelMessage(panelMessage(panelClass("NSAutoreleasePool"),"alloc"),"init");
    const auto context=panelMessage(panelClass("NSOpenGLContext"),"currentContext");
    const auto panel=panelMessage(panelClass("NSSavePanel"),"savePanel");
    panelMessage<void>(panel,"setDirectoryURL:",panelMessage(panelClass("NSURL"),"fileURLWithPath:",panelString(directory)));
    panelMessage<void>(panel,"setNameFieldStringValue:",panelString("routers.json"));
    panelMessage<void>(panel,"setMessage:",panelString("Save Router Group"));
    panelMessage<void>(panel,"setAllowedFileTypes:",panelMessage(panelClass("NSArray"),"arrayWithObject:",panelString("json")));
    panelMessage<void>(panel,"setAllowsOtherFileTypes:",false);
    ofFileDialogResult result;
    if(panelMessage<long>(panel,"runModal")==1) {
        const auto path=panelMessage(panelMessage(panel,"URL"),"path");
        result.filePath=panelMessage<const char*>(path,"UTF8String");
        result.fileName=ofFilePath::getFileName(result.filePath); result.bSuccess=true;
    }
    panelMessage<void>(context,"makeCurrentContext");
    panelMessage<void>(static_cast<id>(ofGetCocoaWindow()),"makeKeyAndOrderFront:",static_cast<id>(nullptr));
    panelMessage<void>(pool,"drain");
    return result;
#elif defined(TARGET_WIN32)
    wchar_t filename[32768]=L"routers.json";
    const auto folder=of::filesystem::path(directory).wstring();
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog);
    dialog.hwndOwner=WindowFromDC(wglGetCurrentDC());
    dialog.lpstrInitialDir=folder.c_str(); dialog.lpstrFile=filename; dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"JSON files (*.json)\0*.json\0"; dialog.lpstrDefExt=L"json";
    dialog.lpstrTitle=L"Save Router Group";
    dialog.Flags=OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    ofFileDialogResult result;
    if(GetSaveFileNameW(&dialog)) {
        result.filePath=of::filesystem::path(filename).string();
        result.fileName=ofFilePath::getFileName(result.filePath); result.bSuccess=true;
    }
    return result;
#else
    return ofSystemSaveDialog(ofFilePath::join(directory,"routers.json"),"Save Router Group");
#endif
}
std::string escapedName(const std::string& name) {
    ofParameter<float> probe; probe.setName(name); return probe.getEscapedName();
}
bool matchesType(const ofxOceanodeAbstractParameter& parameter,const std::string& type) {
    return std::visit([&](const auto& value) {
        if constexpr(std::is_same_v<std::decay_t<decltype(value)>,MacroMixerMixing::Object>)
            return parameter.valueType()==MacroMixerMixing::registry().adapter(type).description;
        else return parameter.valueType()==typeid(value).name();
    },MacroMixerMixing::neutral(type));
}
bool matchesRouter(ofxOceanodeAbstractParameter& parameter,const MacroMixerContract::Router& router) {
    return matchesType(parameter,router.type)
        && bool(parameter.getFlags() & ofxOceanodeParameterFlags_DisableInConnection)==(router.direction=="Output");
}
bool validOutputNames(const MacroMixerContract& contract, std::string& error) {
    const std::set<std::string> reserved{"Target", "Duration_Ms", "Bi_Pow", "Transition", "Finish_Transition", "Progress", "Status",
        "Output_Routers", "Routers", "Macros", "MacroMixerState", "Morph_macros"};
    std::set<std::string> names;
    for(const auto& router : contract.routers) {
        const auto name = escapedName(router.name);
        if(router.name.size() > 255 || ofTrim(router.name) != router.name || name.empty()
           || reserved.count(name) || !names.insert(name).second) {
            error = "Router names must be nonempty, unique, at most 255 bytes, and must not collide with control names.";
            return false;
        }
    }
    return true;
}

void drawMixOptions(const std::string& type,const ofJson& options,const std::function<void(const ofJson&)>& apply) {
    for(auto option=options.begin();option!=options.end();++option) {
        ImGui::PushID(option.key().c_str());
        auto value=option.value(); bool changed=false;
        const auto label=option.key()=="size" ? "Length (0 = larger)" : option.key()=="threshold" ? "Threshold"
            : type=="Texture" && option.key()=="strength" ? "Blend strength"
            : type=="Texture" && option.key()=="angle" ? "Direction (degrees)"
            : type=="Texture" && option.key()=="softness" ? "Edge softness"
            : type=="Texture" && option.key()=="cells" ? "Noise cells"
            : type=="Texture" && option.key()=="seed" ? "Noise seed"
            : type=="ScBus" && option.key()=="channels" ? "Channels"
            : type=="ScBus" && option.key()=="smoothing_ms" ? "Gain smoothing (ms)" : option.key().c_str();
        if(value.is_boolean()) { bool v=value.get<bool>(); changed=ImGui::Checkbox(label,&v); value=v; }
        else if(value.is_number()) {
            ImGui::TextUnformatted(label); ImGui::SetNextItemWidth(-1);
            if(value.is_number_integer()) { int v=value.get<int>(); changed=ImGui::InputInt("##option",&v); value=v; }
            else { float v=value.get<float>(); changed=ImGui::InputFloat("##option",&v,.01f,.1f); value=v; }
        }
        if(changed) { auto edit=options; edit[option.key()]=value; apply(edit); }
        ImGui::PopID();
    }
}

}

ofxOceanodeMacroMixer::~ofxOceanodeMacroMixer() {
    liveMixers.erase(std::remove(liveMixers.begin(),liveMixers.end(),this),liveMixers.end());
}

void ofxOceanodeMacroMixer::drawPendingBlockedNodeDialog() {
    constexpr const char* popupId = "Node blocks MacroMixer##ofxOceanodeMacroMixerBlocked";
    ofxOceanodeMacroMixer* owner = nullptr;
    std::vector<const MacroMixerDiagnostic*> waiting;
    for(auto* mixer : liveMixers) {
        if(!mixer->runtime) continue;
        for(const auto& node : mixer->runtime->blockedNodes())
            if(!mixer->answeredBlocked.count({node.file,node.node})) waiting.push_back(&node);
        if(!waiting.empty()) { owner = mixer; break; }
    }
    if(!owner) {
        if(ImGui::IsPopupOpen(popupId) && ImGui::BeginPopupModal(popupId)) { ImGui::CloseCurrentPopup(); ImGui::EndPopup(); }
        return;
    }
    if(!ImGui::IsPopupOpen(popupId)) {
        // Wait for other dialogs, such as connection errors from the same load.
        if(ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)) return;
        ImGui::OpenPopup(popupId);
    }
    MacroDialogAppearance::Style style;
    const float em = ImGui::GetFontSize();
    ImGui::SetNextWindowSizeConstraints(ImVec2(36*em,0),ImVec2(36*em,FLT_MAX));
    if(!ImGui::BeginPopupModal(popupId,nullptr,ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize)) return;
    const auto& node = *waiting.front();
    const auto answer = [&](const MacroMixerDiagnostic& blocked, ActionKind kind) {
        owner->answeredBlocked.insert({blocked.file,blocked.node});
        owner->actions.push_back({kind,blocked.file,0,{},{},blocked.node});
    };
    ImGui::TextUnformatted("A node prevents MacroMixer from loading this Macro");
    ImGui::Separator();
    ImGui::TextDisabled("NODE");
    ImGui::TextWrapped("%s %s",node.model.c_str(),node.node.substr(node.node.rfind('_')+1).c_str());
    ImGui::TextDisabled("MACRO FOLDER");
    ImGui::TextWrapped("%s",node.file.c_str());
    ImGui::TextDisabled("REASON");
    ImGui::TextWrapped("%s",node.message.c_str());
    ImGui::Spacing();
    ImGui::TextWrapped("Keep this node out of the MacroMixer graph so the load can continue? "
        "It is not created inside MacroMixer and its cables there are left out. "
        "The Macro folder is not changed; the choice is saved with this MacroMixer.");
    if(waiting.size() > 1) ImGui::TextDisabled("%zu blocked nodes remain, including this one.",waiting.size());
    ImGui::Spacing();
    if(ImGui::Button("Keep out of graph")) {
        answer(node,ActionKind::KeepOut);
        if(waiting.size() == 1) ImGui::CloseCurrentPopup();
    }
    if(waiting.size() > 1) {
        ImGui::SameLine();
        if(ImGui::Button(("Keep all " + std::to_string(waiting.size()) + " out").c_str())) {
            for(const auto* blocked : waiting) answer(*blocked,ActionKind::KeepOut);
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Cancel load")) {
        for(const auto* blocked : waiting) owner->answeredBlocked.insert({blocked->file,blocked->node});
        owner->actions.push_back({ActionKind::CancelBlocked,""});
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void ofxOceanodeMacroMixer::setup() {
    addInspectorParameter(morphMacros.set("Morph macros", false));
    routerEditor.set("Routers", [this] { drawRouters(); });
    macroEditor.set("Macros", [this] { drawMacros(); });
    routerEditor.setSerializable(false); macroEditor.setSerializable(false);
    addInspectorParameter(routerEditor); addInspectorParameter(macroEditor);
    state.set("Status", "Empty"); state.setSerializable(false); addInspectorParameter(state);
    addParameterDropdown(target, "Target", 0, {"No Macros"});
    addParameter(duration.set("Duration Ms", 1000, 0, FLT_MAX));
    addParameter(biPow.set("Bi Pow", 0, -1, 1));
    addParameter(transition.set("Transition"));
    addParameter(finish.set("Finish Transition"));
    addOutputParameter(progress.set("Progress", 0, 0, 1), transient);
    if(hostContainer) {
        runtime = std::make_unique<MacroMixerRuntime>(*hostContainer);
        MacroMixerContract empty; empty.routers.clear(); runtime->setContract(empty);
        if(std::find(liveMixers.begin(),liveMixers.end(),this) == liveMixers.end()) liveMixers.push_back(this);
    }
    listeners.push(target.newListener([this](int& index) {
        if(!runtime) return;
        const auto entries = runtime->macros();
        selectedID = index >= 0 && size_t(index) < entries.size() ? entries[index]->id() : "";
    }));
    listeners.push(transition.newListener([this] {
        if(!runtime || !getActive()) return;
        inspectorError.clear();
        runtime->requestMacro(selectedID, duration, biPow, morphMacros);
    }));
    listeners.push(finish.newListener([this] { if(runtime && getActive()) runtime->finish(); }));
}

void ofxOceanodeMacroMixer::setBpm(float bpm) {
    if(!runtime) return;
    // The owning container already applied the global tempo. Forward the
    // ordinary node notification to both live graphs without writing transport.
    const auto* destination=runtime->destination()==runtime->origin() ? nullptr : runtime->destination();
    for(const auto* graph : {runtime->origin(),destination}) if(graph)
        for(auto* node : graph->container->getAllModules()) node->setBpm(bpm);
}

void ofxOceanodeMacroMixer::resetPhase() {
    if(!runtime) return;
    // The root owns the transport reset/generation. Child clocks receive only
    // the normal phase-reset notification, once per live graph.
    const auto* destination=runtime->destination()==runtime->origin() ? nullptr : runtime->destination();
    for(const auto* graph : {runtime->origin(),destination}) if(graph)
        for(auto* node : graph->container->getAllModules()) node->resetPhase();
}

void ofxOceanodeMacroMixer::reportFailure() {
    inspectorError.clear();
    if(!runtime) { inspectorError = "No host container"; return; }
    for(const auto& error : runtime->diagnostics()) {
        if(!inspectorError.empty()) inspectorError += "\n";
        inspectorError += error.file + ": " + error.message;
    }
}

std::string ofxOceanodeMacroMixer::routerEditBlocker(const std::string& id) const {
    const auto found = outputs.find(id);
    if(found == outputs.end()) return "";
    auto& p = *found->second;
    if(p.hasInConnection() || p.hasOutConnections() || p.isScoped() || p.isTimelined()
       || (hostContainer && hostContainer->customGuiContainsParameterAnywhere(p)))
        return "Disconnect cables and remove scope, timeline and Custom GUI references before renaming/changing this router.";
    return "";
}

bool ofxOceanodeMacroMixer::setRouterContract(const MacroMixerContract& contract) {
    if(!runtime) return false;
    if(!validOutputNames(contract, inspectorError)) return false;
    for(const auto& router : contract.routers) {
        const auto name = escapedName(router.name);
        for(const auto& old : outputs) if(old.first != router.id && old.second->getEscapedName() == name) {
            inspectorError = "Router name is already in use: " + router.name; return false;
        }
        auto existing = outputs.find(router.id);
        if(existing != outputs.end() && existing->second->getName() != router.name
           && existing->second->getEscapedName() == name) {
            inspectorError = "This spelling uses the same saved parameter name. Choose a distinct name first."; return false;
        }
    }
    for(const auto& old : outputs) {
        auto found = std::find_if(contract.routers.begin(), contract.routers.end(), [&](const auto& r) { return r.id == old.first; });
        if(found != contract.routers.end() && (found->name != old.second->getName() || !matchesRouter(*old.second,*found))) {
            auto reason = routerEditBlocker(old.first);
            if(!reason.empty()) { inspectorError = reason; return false; }
        }
    }
    const auto previous=runtime->contract();
    if(!runtime->setContract(contract)) { reportFailure(); return false; }
    for(const auto& router:contract.routers) {
        const auto old=std::find_if(previous.routers.begin(),previous.routers.end(),[&](const auto& r){return r.id==router.id;});
        if(old!=previous.routers.end() && old->name!=router.name)
            std::snprintf(routerDrafts[router.id].data(),256,"%s",router.name.c_str());
    }
    installOutputs(contract);
    inspectorError.clear(); return true;
}

bool ofxOceanodeMacroMixer::setMorphConnectionPolicy(const std::string& type,const std::string& policy,const ofJson& options) {
    if(!runtime) return false;
    if(!runtime->setMorphPolicy(type,policy,options)) { reportFailure(); return false; }
    inspectorError.clear(); return true;
}

bool ofxOceanodeMacroMixer::saveRouterGroup(const std::string& path) {
    if(!runtime) return false;
    try {
        auto destination=path;
        if(destination.empty()) throw std::runtime_error("Choose a router group filename");
        if(ofToLower(ofFilePath::getFileExt(destination))!="json") destination+=".json";
        const ofJson json={{"format",routerGroupFormat},{"version",1},{"routers",runtime->saveState().at("routers")}};
        ofFile file(ofToDataPath(destination,true),ofFile::WriteOnly);
        if(!file.is_open()) throw std::runtime_error("Cannot open router group for writing: "+destination);
        file << json.dump(4) << '\n'; file.flush();
        if(!file) throw std::runtime_error("Cannot write router group: "+destination);
        inspectorError.clear(); publish(); return true;
    } catch(const std::exception& error) { inspectorError=error.what(); publish(); return false; }
}

bool ofxOceanodeMacroMixer::loadRouterGroup(const std::string& path) {
    if(!runtime) return false;
    try {
        ofFile file(ofToDataPath(path,true),ofFile::ReadOnly);
        if(!file.is_open()) throw std::runtime_error("Cannot open router group: "+path);
        ofJson json; file >> json;
        if(!json.is_object() || json.at("format")!=routerGroupFormat || json.at("version")!=1)
            throw std::runtime_error("Unsupported router group format or version");
        const auto& routers=json.at("routers");
        if(!routers.is_array() || routers.size()>64) throw std::runtime_error("Router groups must contain at most 64 routers");
        MacroMixerContract contract; contract.routers.clear();
        std::set<std::string> ids;
        for(const auto& item:routers) {
            MacroMixerContract::Router router;
            router.id=item.at("id").get<std::string>(); router.name=item.at("name").get<std::string>();
            router.type=item.at("type").get<std::string>(); router.direction=item.at("direction").get<std::string>();
            router.policy=item.at("policy").get<std::string>(); router.options=item.at("options");
            if(router.id.empty() || !ids.insert(router.id).second) throw std::runtime_error("Router IDs must be nonempty and unique");
            if(router.direction!="Input" && router.direction!="Output") throw std::runtime_error("Invalid router direction");
            if(!router.options.is_object() || (router.direction=="Input" && (!router.policy.empty() || !router.options.empty())))
                throw std::runtime_error("Invalid router mixing settings");
            if(!hostContainer || !hostContainer->getRegistry()->getRegisteredModels().count(MacroMixerMixing::routerModel(router.type)))
                throw std::runtime_error("Router type is unavailable in this host: "+router.type);
            contract.routers.push_back(std::move(router));
        }
        // A reusable group can come from another mixer. Keep matching local
        // parameter identities/cables, and avoid reusing an unrelated local ID.
        const auto& current=runtime->contract().routers;
        for(const auto& router:current) ids.insert(router.id);
        for(auto& router:contract.routers) {
            const auto match=std::find_if(current.begin(),current.end(),[&](const auto& old) {
                return old.name==router.name && old.type==router.type && old.direction==router.direction;
            });
            if(match!=current.end()) router.id=match->id;
            else if(outputs.count(router.id)) {
                auto id="loaded-"+router.id;
                while(ids.count(id)) id="loaded-"+id;
                ids.insert(id); router.id=std::move(id);
            }
        }
        const bool applied=setRouterContract(contract);
        publish(); return applied;
    } catch(const std::exception& error) { inspectorError=error.what(); publish(); return false; }
}

void ofxOceanodeMacroMixer::chooseRouterGroup(bool save) {
    try {
        const auto directory=ofToDataPath("routergroups",true);
        if(!ofDirectory(directory).create(true)) throw std::runtime_error("Cannot create router group folder: "+directory);
        const auto result=save ? saveRouterGroupDialog(directory) : ofSystemLoadDialog("Load Router Group",false,directory);
        if(result.bSuccess) {
            if(save) saveRouterGroup(result.filePath); else loadRouterGroup(result.filePath);
        }
    } catch(const std::exception& error) { inspectorError=error.what(); publish(); }
}

void ofxOceanodeMacroMixer::installOutputs(const MacroMixerContract& contract) {
    for(auto it = outputs.begin(); it != outputs.end();) {
        if(std::none_of(contract.routers.begin(), contract.routers.end(), [&](const auto& r) { return r.id == it->first && matchesRouter(*it->second,r); })) {
            auto& parameter = *it->second;
            if(hostContainer && parameter.isTimelined()) {
                if(auto* extension = hostContainer->getAutomationExtension())
                    extension->removeParameterBindings(parameter);
            }
            if(hostContainer && hostContainer->customGuiContainsParameterAnywhere(parameter)) {
                if(auto* extension = hostContainer->getCustomGuiExtension()) extension->removeParameterBindings(parameter);
            }
            parameter.removeAllConnections();
            removeParameter(it->second->getName());
            if(std::none_of(contract.routers.begin(),contract.routers.end(),[&](const auto& r){return r.id==it->first;}))
                routerDrafts.erase(it->first);
            it = outputs.erase(it);
        } else ++it;
    }
    std::vector<std::string> order{"Target", "Duration Ms", "Bi Pow", "Transition", "Finish Transition", "Progress"};
    for(const auto& router : contract.routers) {
        auto found = outputs.find(router.id);
        if(found == outputs.end()) {
            std::visit([&](const auto& value) {
                using T=std::decay_t<decltype(value)>;
                if constexpr(std::is_same_v<T,MacroMixerMixing::Object>) {
                    const auto& adapter=MacroMixerMixing::registry().adapter(router.type);
                    auto p=adapter.makePort(router.name,router.direction=="Input" ? runtime->inputs().at(router.id) : MacroMixerMixing::Value(value));
                    const auto flags=router.direction=="Input" ? ofxOceanodeParameterFlags_DisableOutConnection
                        | ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_DisableSaveProject
                        : transient | ofxOceanodeParameterFlags_DisableInConnection | ofxOceanodeParameterFlags_DisplayMinimized;
                    p->setFlags(flags); p->setNodeModel(this);
                    getParameterGroup().add(*p); outputs[router.id]=std::move(p);
                } else {
                auto p=std::make_shared<ofParameter<T>>();
                if constexpr(std::is_arithmetic_v<T>)
                    p->set(router.name,value,std::numeric_limits<T>::lowest(),std::numeric_limits<T>::max());
                else if constexpr(std::is_pointer_v<T>) p->set(router.name,value);
                else {
                    using Item=typename T::value_type;
                    p->set(router.name,value,T{std::numeric_limits<Item>::lowest()},T{std::numeric_limits<Item>::max()});
                }
                if(router.direction=="Input") {
                    *p=std::get<T>(runtime->inputs().at(router.id));
                    outputs[router.id]=addParameter(*p,ofxOceanodeParameterFlags_DisableOutConnection
                        | ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_DisableSaveProject);
                } else outputs[router.id]=addOutputParameter(*p,transient);
                }
            },MacroMixerMixing::neutral(router.type));
        } else if(found->second->getName() != router.name) {
            found->second->setName(router.name);
        }
        auto draft=routerDrafts.emplace(router.id,std::array<char,256>{});
        if(draft.second) std::snprintf(draft.first->second.data(),256,"%s",router.name.c_str());
        order.push_back(router.name);
    }
    // OF reorders the existing shared parameter objects, preserving cable pointers.
    getParameterGroup().reorder(order);
    parameterGroupChanged.notify();
}

void ofxOceanodeMacroMixer::syncSelector() {
    if(!runtime) return;
    const auto entries = runtime->macros();
    std::vector<std::string> labels;
    int selected = -1;
    for(size_t i = 0; i < entries.size(); ++i) {
        labels.push_back(std::to_string(i) + ": " + entries[i]->name());
        if(entries[i]->id() == selectedID) selected = int(i);
    }
    if(selected < 0 && !entries.empty()) { selected = 0; selectedID = entries[0]->id(); }
    if(entries.empty()) { labels.push_back("No Macros"); selectedID.clear(); selected = 0; }
    target.setMin(0); target.setMax(std::max(0, int(entries.size()) - 1));
    target.setWithoutEventNotifications(selected);
    getOceanodeParameter(target).setDropdownOptions(labels);
    std::string name = target.getName(); dropdownChanged.notify(name);
}

bool ofxOceanodeMacroMixer::importMacroFolder(const std::string& folder, bool refresh) {
    if(!runtime) return false;
    if(!syncInputs()) return false;
    if(!runtime->importMacro(folder, refresh)) { reportFailure(); return false; }
    inspectorError.clear(); syncSelector(); return true;
}
bool ofxOceanodeMacroMixer::removeImportedMacro(const std::string& id) {
    if(!runtime) return false;
    if(!runtime->removeMacro(id)) { reportFailure(); return false; }
    inspectorError.clear(); syncSelector(); publish(); return true;
}
bool ofxOceanodeMacroMixer::moveImportedMacro(const std::string& id, size_t index) {
    if(!runtime || !runtime->moveMacro(id, index)) return false;
    syncSelector(); return true;
}

bool ofxOceanodeMacroMixer::drawInspectorParameter(ofAbstractParameter& parameter) {
    if(!parameter.isReferenceTo(state)) return false;
    ImGui::SeparatorText("Status");
    ImGui::TextWrapped("%s", state.get().c_str());
    return true;
}

void ofxOceanodeMacroMixer::drawRouters() {
    if(!runtime) return;
    ImGui::SeparatorText("Routers");
    if(morphMacros) ImGui::TextWrapped("Morph macros: matching node types and connection roles keep live buffers and clocks even when IDs differ. Changed control cables blend live signals; added/removed nodes coexist until completion. Changed texture cables use GPU mixing. Source details show connection policies and reasons for destination-graph fallback. Router output mix policies are bypassed. Mode changes apply on the next Transition.");
    const auto contract=runtime->contract();
    auto types=MacroMixerMixing::supportedTypes();
    types.erase(std::remove_if(types.begin(),types.end(),[&](const auto& type) {
        return !hostContainer || !hostContainer->getRegistry()->getRegisteredModels().count(MacroMixerMixing::routerModel(type));
    }),types.end());
    if(types.empty()) return;
    if(size_t(newRouterType)>=types.size()) newRouterType=0;
    const bool fading=runtime->transitioning();
    const float em=ImGui::GetFontSize();
    const auto& style=ImGui::GetStyle();
    const float innerWidth=std::max(ImGui::GetContentRegionAvail().x,52*em);
    // Scrolling tables need an explicit height or they consume the entire
    // Inspector and push Macros below it. Size to the actual controls instead.
    float height=ImGui::GetTextLineHeight()+ImGui::GetFrameHeight()+4*style.CellPadding.y+2;
    for(const auto& router:contract.routers) {
        height+=ImGui::GetFrameHeight()+2*style.CellPadding.y;
        for(const auto& option:router.options) {
            if(option.is_boolean()) height+=ImGui::GetFrameHeight()+style.ItemSpacing.y;
            else if(option.is_number()) height+=ImGui::GetTextLineHeight()+ImGui::GetFrameHeight()+2*style.ItemSpacing.y;
        }
    }
    if(innerWidth>ImGui::GetContentRegionAvail().x) height+=style.ScrollbarSize;
    const auto flags=ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH
        | ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    auto headerColor=ofFloatColor(getColor());
    headerColor.setBrightness(headerColor.getBrightness()*.5f);
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,ImVec4(headerColor.r,headerColor.g,headerColor.b,headerColor.a));
    ImGui::BeginDisabled(fading);
    if(ImGui::BeginTable("##routers",5,flags,ImVec2(0,height),innerWidth)) {
        ImGui::TableSetupColumn("Name",ImGuiTableColumnFlags_WidthStretch,1);
        ImGui::TableSetupColumn("Direction",ImGuiTableColumnFlags_WidthFixed,7*em);
        ImGui::TableSetupColumn("Type",ImGuiTableColumnFlags_WidthStretch,1);
        ImGui::TableSetupColumn("Mix policy",ImGuiTableColumnFlags_WidthStretch,1.6f);
        ImGui::TableSetupColumn("",ImGuiTableColumnFlags_WidthFixed,11*em);
        ImGui::TableHeadersRow();
        for(size_t i=0;i<contract.routers.size();++i) {
            const auto& router=contract.routers[i];
            ImGui::PushID(router.id.c_str());
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
            auto draft=routerDrafts.emplace(router.id,std::array<char,256>{});
            if(draft.second) std::snprintf(draft.first->second.data(),256,"%s",router.name.c_str());
            ImGui::SetNextItemWidth(-1);
            bool apply=ImGui::InputText("##name",draft.first->second.data(),256,ImGuiInputTextFlags_EnterReturnsTrue);
            const bool pending=router.name!=draft.first->second.data();
            if(pending && ImGui::IsItemHovered()) ImGui::SetTooltip("Current name: %s",router.name.c_str());

            ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
            if(ImGui::BeginCombo("##direction",router.direction.c_str())) {
                for(const auto& direction:{"Input","Output"}) if(ImGui::Selectable(direction,router.direction==direction) && router.direction!=direction) {
                    auto edit=contract; edit.routers[i].direction=direction; edit.routers[i].policy.clear(); edit.routers[i].options=ofJson::object();
                    actions.push_back({ActionKind::Routers,"",0,std::move(edit)});
                }
                ImGui::EndCombo();
            }
            ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-1);
            if(ImGui::BeginCombo("##type",router.type.c_str())) {
                for(const auto& type:types) if(ImGui::Selectable(type.c_str(),router.type==type) && router.type!=type) {
                    auto edit=contract; edit.routers[i].type=type; edit.routers[i].policy.clear(); edit.routers[i].options=ofJson::object();
                    actions.push_back({ActionKind::Routers,"",0,std::move(edit)});
                }
                ImGui::EndCombo();
            }
            ImGui::TableSetColumnIndex(3);
            if(router.direction=="Output") {
                const auto& selected=MacroMixerMixing::registry().policy(router.type,router.policy);
                ImGui::SetNextItemWidth(-1);
                if(ImGui::BeginCombo("##policy",selected.label.c_str())) {
                    for(const auto& policy:MacroMixerMixing::registry().policies(router.type))
                        if(ImGui::Selectable(policy.label.c_str(),selected.id==policy.id)) {
                            auto edit=contract; edit.routers[i].policy=policy.id; edit.routers[i].options=ofJson::object();
                            actions.push_back({ActionKind::Routers,"",0,std::move(edit)});
                        }
                    ImGui::EndCombo();
                }
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",selected.label.c_str());
                drawMixOptions(router.type,router.options,[&](const ofJson& options) {
                    auto edit=contract; edit.routers[i].options=options;
                    actions.push_back({ActionKind::Routers,"",0,std::move(edit)});
                });
            } else ImGui::TextDisabled("-");
            ImGui::TableSetColumnIndex(4);
            ImGui::BeginDisabled(!pending);
            if(pending) {
                ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.68f,.34f,.06f,1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.72f,.38f,.08f,1));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(.56f,.28f,.06f,1));
                ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,1,1,1));
            }
            if(ImGui::Button("Apply")) apply=true;
            if(pending) ImGui::PopStyleColor(4);
            ImGui::EndDisabled();
            if(apply && pending) {
                auto edit=contract; edit.routers[i].name=draft.first->second.data();
                actions.push_back({ActionKind::Routers,"",0,std::move(edit)});
            }
            ImGui::SameLine(0,em*.25f); ImGui::BeginDisabled(i==0);
            if(ImGui::ArrowButton("##up",ImGuiDir_Up)) {
                auto edit=contract; std::swap(edit.routers[i],edit.routers[i-1]);
                actions.push_back({ActionKind::Routers,"",0,std::move(edit)});
            }
            ImGui::EndDisabled();
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Move router up");
            ImGui::SameLine(0,em*.25f); ImGui::BeginDisabled(i+1==contract.routers.size());
            if(ImGui::ArrowButton("##down",ImGuiDir_Down)) {
                auto edit=contract; std::swap(edit.routers[i],edit.routers[i+1]);
                actions.push_back({ActionKind::Routers,"",0,std::move(edit)});
            }
            ImGui::EndDisabled();
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Move router down");
            ImGui::SameLine(0,em*.25f);
            if(ImGui::Button("-")) {
                auto edit=contract; edit.routers.erase(edit.routers.begin()+i);
                actions.push_back({ActionKind::Routers,"",0,std::move(edit)});
            }
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Remove router");
            ImGui::PopID();
        }
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##newRouter","Router name",newRouterName.data(),newRouterName.size());
        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##newDirection",&newRouterDirection,"Input\0Output\0");
        ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##newRouterType",types.at(newRouterType).c_str())) {
            for(size_t i=0;i<types.size();++i)
                if(ImGui::Selectable(types[i].c_str(),size_t(newRouterType)==i)) newRouterType=int(i);
            ImGui::EndCombo();
        }
        ImGui::TableSetColumnIndex(3);
        ImGui::BeginDisabled(ofTrim(std::string(newRouterName.data())).empty());
        if(ImGui::Button("+ Add Router")) {
            auto edit=contract; std::string id;
            do { id="router-"+std::to_string(++routerSequence); } while(outputs.count(id));
            edit.routers.push_back({id,newRouterName.data(),types.at(newRouterType),"",ofJson::object(),newRouterDirection==0 ? "Input" : "Output"});
            actions.push_back({ActionKind::Routers,id,0,std::move(edit)});
        }
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    ImGui::PopStyleColor();
    ImGui::BeginDisabled(fading);
    if(ImGui::Button("Load##routergroup")) actions.push_back({ActionKind::LoadRouters,""});
    ImGui::EndDisabled(); ImGui::SameLine();
    if(ImGui::Button("Save##routergroup")) actions.push_back({ActionKind::SaveRouters,""});
}

void ofxOceanodeMacroMixer::drawMacros() {
    if(!runtime) return;
    ImGui::SeparatorText("Macros");
    const bool fading = runtime->transitioning();
    ImGui::BeginDisabled(fading || runtime->contract().routers.empty());
    if(ImGui::Button("+ Add Macro Folder")) {
        auto result = ofSystemLoadDialog("Choose Macro Folder", true);
        if(result.bSuccess) {
            actions.push_back({ActionKind::Import, result.getPath()});
            state = "Preparing: " + result.getPath();
        }
    }
    ImGui::EndDisabled();
    const auto entries = runtime->macros();
    for(size_t i = 0; i < entries.size(); ++i) {
        const auto& macro = entries[i];
        ImGui::PushID(macro->id().c_str());
        ImGui::Separator();
        ImGui::TextWrapped("%zu: %s", i, macro->name().c_str());
        std::string badges;
        if(macro->id() == runtime->currentID()) badges += "Running  ";
        if(macro->id() == runtime->destinationID()) badges += "Destination  ";
        if(macro->id() == runtime->queuedID()) badges += "Queued  ";
        if(macro->id() == selectedID) badges += "Selected";
        if(!badges.empty()) ImGui::TextWrapped("%s", badges.c_str());
        if(ImGui::SmallButton("Select")) target = int(i);
        ImGui::SameLine(); ImGui::BeginDisabled(fading);
        if(ImGui::SmallButton("Refresh")) actions.push_back({ActionKind::Refresh, macro->source()});
        ImGui::EndDisabled(); ImGui::SameLine(); ImGui::BeginDisabled(i == 0);
        if(ImGui::ArrowButton("##up",ImGuiDir_Up)) actions.push_back({ActionKind::MoveMacro, macro->id(), i-1});
        ImGui::EndDisabled(); ImGui::SameLine(); ImGui::BeginDisabled(i + 1 == entries.size());
        if(ImGui::ArrowButton("##down",ImGuiDir_Down)) actions.push_back({ActionKind::MoveMacro, macro->id(), i+1});
        ImGui::EndDisabled(); ImGui::SameLine();
        if(ImGui::SmallButton("-")) actions.push_back({ActionKind::RemoveMacro, macro->id()});
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Remove Macro; clears playback if it is running, a destination or queued.");
        if(ImGui::TreeNode("Source")) {
            ImGui::TextWrapped("%s", macro->source().c_str());
            for(const auto& notice : macro->notices()) ImGui::TextWrapped("%s", notice.c_str());
            if(morphMacros || runtime->morphMode()) {
                ImGui::SeparatorText(runtime->hasTransitionDetails(macro->id()) ? "Last requested morph" : "Morph from running source");
                std::set<std::string> editedTypes;
                const auto details=runtime->sourceDetails(macro->id());
                for(size_t n=0;n<details.size();++n) {
                    const auto& detail=details[n]; ImGui::PushID(int(n));
                    const auto kind=detail.kind=="context" ? "Transition" : detail.kind=="fallback" ? "Graph fallback" : detail.kind=="mix" ? "Connection mix"
                        : detail.kind=="added" ? "Added node" : detail.kind=="removed" ? "Removed node"
                        : detail.kind=="skipped" ? "Skipped connection mix" : "Live graph";
                    ImGui::TextWrapped("%s: %s",kind,detail.location.c_str());
                    ImGui::TextWrapped("%s",detail.message.c_str());
                    if(detail.kind=="mix" && detail.type.empty()) ImGui::TextDisabled("Native control interpolation");
                    if(detail.kind=="mix" && !detail.type.empty()) {
                        const auto& selected=MacroMixerMixing::registry().policy(detail.type,detail.policy);
                        ImGui::TextWrapped("%s: %s",detail.type.c_str(),selected.label.c_str());
                        if(editedTypes.insert(detail.type).second) {
                            ImGui::TextWrapped("Policy for changed %s connections (next transition)",detail.type.c_str());
                            auto settings=MacroMixerMorphPolicy{selected.id,detail.options};
                            const auto configured=runtime->morphPolicies().find(detail.type);
                            if(configured!=runtime->morphPolicies().end()) settings=configured->second;
                            const auto& choice=MacroMixerMixing::registry().policy(detail.type,settings.policy);
                            ImGui::BeginDisabled(fading); ImGui::SetNextItemWidth(-1);
                            if(ImGui::BeginCombo("##connectionPolicy",choice.label.c_str())) {
                                for(const auto& policy:MacroMixerMixing::registry().policies(detail.type)) if(ImGui::Selectable(policy.label.c_str(),policy.id==choice.id))
                                    actions.push_back({ActionKind::MorphPolicy,detail.type,0,{}, {policy.id,ofJson::object()}});
                                ImGui::EndCombo();
                            }
                            drawMixOptions(detail.type,settings.options,[&](const ofJson& options) {
                                actions.push_back({ActionKind::MorphPolicy,detail.type,0,{}, {choice.id,options}});
                            });
                            ImGui::EndDisabled();
                        }
                    }
                    ImGui::PopID();
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    const auto& keptOut = runtime->keptOutNodes();
    if(!keptOut.empty() && ImGui::TreeNode("##keptOut","Kept out of graph (%zu)",keptOut.size())) {
        for(const auto& node : keptOut) {
            ImGui::PushID((node.first + "/" + node.second).c_str());
            ImGui::TextWrapped("%s", node.second.c_str());
            ImGui::TextDisabled("%s", node.first.c_str());
            if(ImGui::SmallButton("Restore")) actions.push_back({ActionKind::RestoreNode,node.first,0,{},{},node.second});
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Load this node again from the next activation. While it blocks MacroMixer, you are asked again.");
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    if(ImGui::Button("Clear Playback")) actions.push_back({ActionKind::ClearPlayback, ""});
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Release running graphs and reset router values. Imported macros remain in the list.");
}

bool ofxOceanodeMacroMixer::syncInputs() {
    if(!runtime) return false;
    MacroMixerRuntime::Inputs values;
    for(const auto& r:runtime->contract().routers) if(r.direction=="Input") {
        auto& p=*outputs.at(r.id);
        values[r.id]=std::visit([&](const auto& zero)->MacroMixerMixing::Value {
            using T=std::decay_t<decltype(zero)>;
            if constexpr(std::is_same_v<T,MacroMixerMixing::Object>) return MacroMixerMixing::registry().adapter(r.type).read(p);
            else return p.cast<T>().getParameter().get();
        },MacroMixerMixing::neutral(r.type));
    }
    if(!runtime->setInputs(values)) { reportFailure(); return false; }
    return true;
}
void ofxOceanodeMacroMixer::publish() {
    for(auto& output:outputs) {
        if(runtime && runtime->inputs().count(output.first)) continue;
        std::visit([&](const auto& value) {
            using T=std::decay_t<decltype(value)>;
            if constexpr(std::is_same_v<T,MacroMixerMixing::Object>) MacroMixerMixing::registry().adapter(value.type).write(*output.second,value);
            else output.second->cast<T>().getParameter()=value;
        },runtime ? runtime->outputValue(output.first) : MacroMixerMixing::Value(0.f));
    }
    progress = runtime ? runtime->progress() : 0.f;
    state = runtime ? runtime->status() : "Empty";
    if(runtime) {
        if(auto entry = runtime->macro(runtime->currentID())) state = state.get() + " | Running: " + entry->name();
        if(auto entry = runtime->macro(runtime->destinationID())) state = state.get() + " | Destination: " + entry->name();
        if(auto entry = runtime->macro(runtime->queuedID())) state = state.get() + " | Queued: " + entry->name();
    }
    if(!inspectorError.empty()) state = state.get() + "\n" + inspectorError;
}

void ofxOceanodeMacroMixer::update(ofEventArgs&) {
    if(!runtime || !syncInputs()) return;
    if(!actions.empty()) {
        auto action = std::move(actions.front()); actions.pop_front();
        switch(action.kind) {
            case ActionKind::Import: importMacroFolder(action.text); break;
            case ActionKind::Refresh: importMacroFolder(action.text, true); break;
            case ActionKind::RemoveMacro: removeImportedMacro(action.text); break;
            case ActionKind::MoveMacro: moveImportedMacro(action.text, action.index); break;
            case ActionKind::Routers: if(setRouterContract(action.contract) && !action.text.empty()) newRouterName.fill(0); break;
            case ActionKind::LoadRouters: chooseRouterGroup(false); break;
            case ActionKind::SaveRouters: chooseRouterGroup(true); break;
            case ActionKind::MorphPolicy: setMorphConnectionPolicy(action.text,action.policy.policy,action.policy.options); break;
            case ActionKind::ClearPlayback: runtime->clear(); inspectorError.clear(); break;
            case ActionKind::KeepOut:
                answeredBlocked.erase({action.text,action.node});
                if(runtime->keepOutOfGraph(action.text,action.node)) inspectorError.clear(); else reportFailure();
                syncSelector(); break;
            case ActionKind::CancelBlocked: runtime->cancelBlocked(); answeredBlocked.clear(); break;
            case ActionKind::RestoreNode: runtime->restoreKeptOutNode(action.text,action.node); break;
        }
    }
    if(syncInputs()) runtime->update();
    publish();
}
void ofxOceanodeMacroMixer::deactivate() {
    actions.clear();
    answeredBlocked.clear();
    if(runtime) runtime->clear();
    inspectorError.clear(); publish();
    state = "Empty (deactivated; graphs released)";
}
void ofxOceanodeMacroMixer::draw(ofEventArgs&) {
    if(!runtime || !syncInputs()) return;
    runtime->draw(); publish();
}
void ofxOceanodeMacroMixer::presetSave(ofJson& json) {
    if(!runtime) return;
    syncInputs();
    auto saved = runtime->saveState();
    saved["selected"] = selectedID;
    json["MacroMixerState"] = std::move(saved);
}

void ofxOceanodeMacroMixer::loadBeforeConnections(ofJson& json) {
    recallPrepared = true; recallValid = false;
    actions.clear(); recalledSelection.clear();
    if(!json.contains(morphMacros.getEscapedName())) morphMacros = false;
    if(!runtime) return;
    if(!json.contains("MacroMixerState")) return;
    try {
        const auto& saved = json.at("MacroMixerState");
        // A containing preset reload replaces live playback, including a fade.
        // Release both old graphs before staging to retain the two-graph cap.
        if(runtime->transitioning()) runtime->clear();
        auto candidate = std::make_unique<MacroMixerRuntime>(*hostContainer);
        if(!candidate->restoreState(saved)) {
            inspectorError = "Cannot restore MacroMixer";
            for(const auto& e : candidate->diagnostics()) inspectorError += "\n" + e.file + ": " + e.message;
            return;
        }
        const auto selection = saved.at("selected").get<std::string>();
        if((!selection.empty() && !candidate->macro(selection)) ||
           (selection.empty() && !candidate->macros().empty()))
            throw std::runtime_error("Invalid saved selection");
        const auto& contract = candidate->contract();
        if(!validOutputNames(contract, inspectorError)) return;
        // The container reuses nodes with the same model/ID across macros.
        // Keep matching wrappers; changed outputs release old cables before the
        // parent loads its saved cable table. Non-cable UI references need their
        // own migration policy, so reject those interface changes explicitly.
        for(const auto& old : outputs) {
            const auto match = std::find_if(contract.routers.begin(), contract.routers.end(),
                [&](const auto& r) { return r.id == old.first && r.name == old.second->getName() && matchesRouter(*old.second,r); });
            if(match == contract.routers.end()) {
                auto& parameter = *old.second;
                if(parameter.isScoped() || parameter.isTimelined()
                   || hostContainer->customGuiContainsParameterAnywhere(parameter))
                    throw std::runtime_error("Remove scope, timeline and Custom GUI references before loading a different router interface.");
            }
        }
        // Remove changed names/IDs first, allowing a restored ID to reuse Value.
        for(auto it = outputs.begin(); it != outputs.end();) {
            if(std::none_of(contract.routers.begin(), contract.routers.end(),
                [&](const auto& r) { return r.id == it->first && r.name == it->second->getName() && matchesRouter(*it->second,r); })) {
                it->second->removeAllConnections();
                removeParameter(it->second->getName()); it = outputs.erase(it);
            } else ++it;
        }
        runtime = std::move(candidate);
        answeredBlocked.clear();
        routerDrafts.clear(); installOutputs(runtime->contract());
        for(const auto& input:runtime->inputs()) {
            auto& p=*outputs.at(input.first);
            if(!p.hasInConnection()) std::visit([&](const auto& value) {
                using T=std::decay_t<decltype(value)>;
                if constexpr(std::is_same_v<T,MacroMixerMixing::Object>) MacroMixerMixing::registry().adapter(value.type).write(p,value);
                else p.cast<T>().getParameter()=value;
            },input.second);
        }
        recalledSelection = selection; selectedID = selection; syncSelector();
        inspectorError.clear(); recallValid = true;
        publish();
    } catch(const std::exception& e) {
        inspectorError = std::string("Cannot restore MacroMixer: ") + e.what();
    }
}

void ofxOceanodeMacroMixer::presetRecallBeforeSettingParameters(ofJson& json) {
    // loadPreset/loadConfig may also be called directly, without the parent
    // preconnection pass. Do not rebuild outputs twice in the normal load path.
    if(!recallPrepared) loadBeforeConnections(json);

}
void ofxOceanodeMacroMixer::presetRecallAfterSettingParameters(ofJson& json) {
    if(json.contains("MacroMixerState")) {
        if(recallValid) selectedID = recalledSelection;
        syncSelector(); // Stable selection wins over a stale serialized row index.
    }
    recallPrepared = false;
    publish();
}
