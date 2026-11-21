#include "MacroMixerGui.h"
#include "ofMain.h"
#include "ofxOceanodeNodeRegistry.h"
#include "MacroDialogAppearance.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <set>
#if defined(TARGET_OSX)
#include <objc/message.h>
#include <objc/runtime.h>
#elif defined(TARGET_WIN32)
#include <commdlg.h>
#endif

namespace {
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

struct BlockedOwner { MacroMixerGui::State* state; std::function<const MacroMixerRuntime*()> runtime; };
// Never destroyed: owners may unregister during static destruction.
std::vector<BlockedOwner>& blockedOwners() { static auto* owners = new std::vector<BlockedOwner>; return *owners; }

using ApplyPolicy = std::function<void(const std::string& type, const std::string& policy, const ofJson& options)>;
// One row per type. own null: the defaults themselves are edited. Otherwise
// own overrides defaults per type and a "Default" entry removes the override
// (apply with an empty policy). apply may change the edited map at once.
void drawPolicyRows(const MacroMixerMorphPolicies& defaults, const MacroMixerMorphPolicies* own, bool disabled, const ApplyPolicy& applyPolicy) {
    ImGui::BeginDisabled(disabled);
    for(const auto& type : MacroMixerMixing::supportedTypes()) {
        const auto policies = MacroMixerMixing::registry().policies(type);
        if(policies.empty()) continue;
        ImGui::PushID(type.c_str());
        ImGui::Spacing();
        const auto inherited = MacroMixerSource::connectionPolicy(type, defaults);
        const auto inheritedLabel = MacroMixerMixing::registry().policy(type, inherited.policy).label;
        const auto& edited = own ? *own : defaults;
        const auto set = edited.find(type);
        const bool overridden = set != edited.end();
        const auto current = overridden ? set->second : inherited;
        const auto apply = [&](const std::string& policy, const ofJson& options) { applyPolicy(type, policy, options); };
        ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(type.c_str());
        ImGui::SameLine(ImGui::GetFontSize() * 8); ImGui::SetNextItemWidth(-1);
        const auto preview = overridden || !own ? MacroMixerMixing::registry().policy(type, current.policy).label : "Default (" + inheritedLabel + ")";
        if(ImGui::BeginCombo("##policy", preview.c_str())) {
            if(own && ImGui::Selectable(("Default (" + inheritedLabel + ")").c_str(), !overridden)) apply("", ofJson::object());
            for(const auto& policy : policies)
                if(ImGui::Selectable(policy.label.c_str(), overridden && policy.id == current.policy)) apply(policy.id, ofJson::object());
            ImGui::EndCombo();
        }
        if(overridden || !own) {
            ImGui::Indent(ImGui::GetFontSize() * 8);
            MacroMixerGui::drawMixOptions(type, current.options, [&](const ofJson& options) { apply(current.policy, options); });
            ImGui::Unindent(ImGui::GetFontSize() * 8);
        }
        ImGui::PopID();
    }
    ImGui::EndDisabled();
}
}

namespace MacroMixerGui {

ImVec4 healthColor(Health health) {
    switch(health) {
        case Health::Ok: return ImVec4(.4f, .85f, .45f, 1);
        case Health::NodeFallback: return ImVec4(1.0f, .62f, .22f, 1);
        case Health::GraphFallback:
        case Health::Error: return ImVec4(1.0f, .38f, .32f, 1);
        default: return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    }
}

Health detailsHealth(const MacroMixerRuntime& runtime, const std::string& id, bool morph) {
    // Details only exist in Morph mode. The running source is read only once a
    // transition recorded its Details: a preview from itself says nothing.
    if(id.empty() || !(morph || runtime.morphMode())) return Health::Ok;
    if(id == runtime.currentID() && !runtime.hasTransitionDetails(id)) return Health::Ok;
    auto result = Health::Ok;
    for(const auto& detail : runtime.sourceDetails(id)) {
        if(detail.kind == "fallback" || detail.kind == "skipped") return Health::GraphFallback;
        if(detail.kind == "replaced") result = Health::NodeFallback;
    }
    return result;
}

std::string failureText(const MacroMixerRuntime& runtime) {
    std::string text;
    for(const auto& error : runtime.diagnostics()) {
        if(!text.empty()) text += "\n";
        text += MacroMixerSource::displayPath(error.file) + ": " + error.message;
    }
    return text;
}

bool apply(MacroMixerRuntime& runtime, const Action& action, State& state, std::string& error,
           ofxOceanodeContainer* host, const SetContract& setContract) {
    const auto failed = [&] { error = failureText(runtime); return false; };
    const auto done = [&] { error.clear(); return true; };
    const auto replaceContract = [&](const MacroMixerContract& contract) {
        if(setContract) return setContract(contract);
        const auto previous = runtime.contract();
        if(!runtime.setContract(contract)) return failed();
        syncRouterDrafts(state, runtime.contract(), &previous);
        return done();
    };
    switch(action.kind) {
        case ActionKind::Import: return runtime.importMacro(action.text) ? done() : failed();
        case ActionKind::Refresh: return runtime.importMacro(action.text, true) ? done() : failed();
        case ActionKind::RemoveMacro: return runtime.removeMacro(action.text) ? done() : failed();
        case ActionKind::MoveMacro: return runtime.moveMacro(action.text, action.index);
        case ActionKind::Routers:
            if(!replaceContract(action.contract)) return false;
            if(!action.text.empty()) state.newRouterName.fill(0);
            return true;
        case ActionKind::LoadRouters: {
            std::string path;
            if(!chooseRouterGroupFile(false, path, error) || path.empty()) return false;
            MacroMixerContract contract;
            return readRouterGroup(runtime, host, path, contract, error) && replaceContract(contract);
        }
        case ActionKind::SaveRouters: {
            std::string path;
            if(!chooseRouterGroupFile(true, path, error) || path.empty()) return false;
            return saveRouterGroup(runtime, path, error);
        }
        case ActionKind::MorphPolicy: {
            const bool applied = action.node.empty() ? runtime.setMorphPolicy(action.text, action.policy.policy, action.policy.options)
                : action.policy.policy.empty() ? runtime.clearSourceMorphPolicy(action.node, action.text)
                : runtime.setSourceMorphPolicy(action.node, action.text, action.policy.policy, action.policy.options);
            return applied ? done() : failed();
        }
        case ActionKind::ClearPlayback: runtime.clear(); return done();
        case ActionKind::KeepOut:
            state.answeredBlocked.erase({action.text, action.node});
            return runtime.keepOutOfGraph(action.text, action.node) ? done() : failed();
        case ActionKind::CancelBlocked: runtime.cancelBlocked(); state.answeredBlocked.clear(); return true;
        case ActionKind::RestoreNode: return runtime.restoreKeptOutNode(action.text, action.node);
        case ActionKind::CheckStatus: runtime.requestChecks(); return true;
    }
    return false;
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
            : type==MacroMixerMixing::triggerType && option.key()=="seed" ? "Seed"
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

void syncRouterDrafts(State& state, const MacroMixerContract& contract, const MacroMixerContract* previous) {
    for(auto it = state.routerDrafts.begin(); it != state.routerDrafts.end();) {
        if(std::none_of(contract.routers.begin(), contract.routers.end(), [&](const auto& r) { return r.id == it->first; }))
            it = state.routerDrafts.erase(it);
        else ++it;
    }
    for(const auto& router : contract.routers) {
        auto draft = state.routerDrafts.emplace(router.id, std::array<char,256>{});
        bool write = draft.second;
        if(!write && previous) {
            const auto old = std::find_if(previous->routers.begin(), previous->routers.end(), [&](const auto& r) { return r.id == router.id; });
            write = old != previous->routers.end() && old->name != router.name;
        }
        if(write) std::snprintf(draft.first->second.data(), 256, "%s", router.name.c_str());
    }
}

bool saveRouterGroup(const MacroMixerRuntime& runtime, const std::string& path, std::string& error) {
    try {
        auto destination=path;
        if(destination.empty()) throw std::runtime_error("Choose a router group filename");
        if(ofToLower(ofFilePath::getFileExt(destination))!="json") destination+=".json";
        const ofJson json={{"format",routerGroupFormat},{"version",1},{"routers",runtime.saveState().at("routers")}};
        ofFile file(ofToDataPath(destination,true),ofFile::WriteOnly);
        if(!file.is_open()) throw std::runtime_error("Cannot open router group for writing: "+destination);
        file << json.dump(4) << '\n'; file.flush();
        if(!file) throw std::runtime_error("Cannot write router group: "+destination);
        error.clear(); return true;
    } catch(const std::exception& e) { error=e.what(); return false; }
}

bool readRouterGroup(const MacroMixerRuntime& runtime, ofxOceanodeContainer* host, const std::string& path,
                     MacroMixerContract& contract, std::string& error) {
    try {
        ofFile file(ofToDataPath(path,true),ofFile::ReadOnly);
        if(!file.is_open()) throw std::runtime_error("Cannot open router group: "+path);
        ofJson json; file >> json;
        if(!json.is_object() || json.at("format")!=routerGroupFormat || json.at("version")!=1)
            throw std::runtime_error("Unsupported router group format or version");
        const auto& routers=json.at("routers");
        if(!routers.is_array() || routers.size()>64) throw std::runtime_error("Router groups must contain at most 64 routers");
        MacroMixerContract loaded; loaded.routers.clear();
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
            if(!host || !host->getRegistry()->getRegisteredModels().count(MacroMixerMixing::routerModel(router.type)))
                throw std::runtime_error("Router type is unavailable in this host: "+router.type);
            loaded.routers.push_back(std::move(router));
        }
        // A reusable group can come from another mixer. Keep matching local
        // parameter identities/cables, and avoid reusing an unrelated local ID.
        const auto& current=runtime.contract().routers;
        std::set<std::string> local;
        for(const auto& router:current) { ids.insert(router.id); local.insert(router.id); }
        for(auto& router:loaded.routers) {
            const auto match=std::find_if(current.begin(),current.end(),[&](const auto& old) {
                return old.name==router.name && old.type==router.type && old.direction==router.direction;
            });
            if(match!=current.end()) router.id=match->id;
            else if(local.count(router.id)) {
                auto id="loaded-"+router.id;
                while(ids.count(id)) id="loaded-"+id;
                ids.insert(id); router.id=std::move(id);
            }
        }
        contract=std::move(loaded); return true;
    } catch(const std::exception& e) { error=e.what(); return false; }
}

bool chooseRouterGroupFile(bool save, std::string& path, std::string& error) {
    path.clear();
    try {
        const auto directory=ofToDataPath("routergroups",true);
        if(!ofDirectory(directory).create(true)) throw std::runtime_error("Cannot create router group folder: "+directory);
        const auto result=save ? saveRouterGroupDialog(directory) : ofSystemLoadDialog("Load Router Group",false,directory);
        if(result.bSuccess) path=result.filePath;
        return true;
    } catch(const std::exception& e) { error=e.what(); return false; }
}

void drawRouters(const MacroMixerRuntime& runtime, State& state, ofxOceanodeContainer* host) {
    auto& actions=state.actions;
    const auto contract=runtime.contract();
    auto types=MacroMixerMixing::supportedTypes();
    types.erase(std::remove_if(types.begin(),types.end(),[&](const auto& type) {
        return !host || !host->getRegistry()->getRegisteredModels().count(MacroMixerMixing::routerModel(type));
    }),types.end());
    if(types.empty()) return;
    if(size_t(state.newRouterType)>=types.size()) state.newRouterType=0;
    const bool fading=runtime.transitioning();
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
    // Header: a gray a little darker than the darkest field/button inside the table.
    float darkest=1;
    for(const auto item:{ImGuiCol_FrameBg,ImGuiCol_Button}) {
        const auto& c=style.Colors[item];
        darkest=std::min(darkest,(.299f*c.x+.587f*c.y+.114f*c.z)*c.w);
    }
    const float gray=std::max(0.f,darkest-.05f);
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,ImVec4(gray,gray,gray,1));
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
            auto draft=state.routerDrafts.emplace(router.id,std::array<char,256>{});
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
        ImGui::InputTextWithHint("##newRouter","Router name",state.newRouterName.data(),state.newRouterName.size());
        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##newDirection",&state.newRouterDirection,"Input\0Output\0");
        ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##newRouterType",types.at(state.newRouterType).c_str())) {
            for(size_t i=0;i<types.size();++i)
                if(ImGui::Selectable(types[i].c_str(),size_t(state.newRouterType)==i)) state.newRouterType=int(i);
            ImGui::EndCombo();
        }
        ImGui::TableSetColumnIndex(3);
        ImGui::BeginDisabled(ofTrim(std::string(state.newRouterName.data())).empty());
        if(ImGui::Button("+ Add Router")) {
            auto edit=contract; std::string id;
            const auto taken=[&](const std::string& candidate) {
                return std::any_of(contract.routers.begin(),contract.routers.end(),[&](const auto& r){return r.id==candidate;});
            };
            do { id="router-"+std::to_string(++state.routerSequence); } while(taken(id));
            edit.routers.push_back({id,state.newRouterName.data(),types.at(state.newRouterType),"",ofJson::object(),state.newRouterDirection==0 ? "Input" : "Output"});
            actions.push_back({ActionKind::Routers,id,0,std::move(edit)});
        }
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    ImGui::PopStyleColor();
    ImGui::BeginDisabled(fading);
    if(ImGui::Button("Load Routers")) actions.push_back({ActionKind::LoadRouters,""});
    ImGui::EndDisabled(); ImGui::SameLine();
    if(ImGui::Button("Save Routers")) actions.push_back({ActionKind::SaveRouters,""});
}

void drawDefaultPolicies(const MacroMixerRuntime& runtime, State& state) {
    if(ImGui::TreeNode("Default Mix/Morph policies")) {
        ImGui::TextWrapped("How changed connections blend in a morph. Each source can override them for transitions into it.");
        drawMorphPolicies(runtime, state, "");
        ImGui::TreePop();
    }
}

void drawMorphPolicies(const MacroMixerRuntime& runtime, State& state, const std::string& id) {
    drawPolicyRows(runtime.morphPolicies(), id.empty() ? nullptr : &runtime.sourceMorphPolicies(id), runtime.transitioning(),
        [&](const std::string& type, const std::string& policy, const ofJson& options) {
            state.actions.push_back({ActionKind::MorphPolicy, type, 0, {}, {policy, options}, id});
        });
}

bool drawTransitionPolicies(const MacroMixerContract& contract, const MacroMixerMorphPolicies& defaults,
                            MacroMixerMorphPolicies& policies, RouterPolicies& routers, bool disabled) {
    bool changed = false;
    // The policy with its options validated and completed, as the runtime stores them.
    const auto canonical = [](const std::string& type, const std::string& policy, const ofJson& options, MacroMixerMorphPolicy& out) {
        try {
            const auto& found = MacroMixerMixing::registry().policy(type, policy);
            out = {found.id, found.options(options)};
            return true;
        } catch(const std::exception&) { return false; }
    };
    drawPolicyRows(defaults, &policies, disabled, [&](const std::string& type, const std::string& policy, const ofJson& options) {
        MacroMixerMorphPolicy edit;
        if(policy.empty()) changed = policies.erase(type) != 0 || changed;
        else if(canonical(type, policy, options, edit)) { policies[type] = std::move(edit); changed = true; }
    });
    bool titled = false;
    ImGui::BeginDisabled(disabled);
    for(const auto& router : contract.routers) {
        if(router.direction != "Output") continue;
        const auto choices = MacroMixerMixing::registry().policies(router.type);
        if(choices.empty()) continue;
        MacroMixerMorphPolicy own;
        if(!canonical(router.type, router.policy, router.options, own)) continue;
        if(!titled) { ImGui::Spacing(); ImGui::TextDisabled("Router mix policies"); titled = true; }
        ImGui::PushID(router.id.c_str());
        ImGui::Spacing();
        const auto ownLabel = "Router (" + MacroMixerMixing::registry().policy(router.type, own.policy).label + ")";
        const auto set = routers.find(router.id);
        const bool overridden = set != routers.end();
        const auto current = overridden ? set->second : own;
        const auto apply = [&](const std::string& policy, const ofJson& options) {
            MacroMixerMorphPolicy edit;
            if(policy.empty()) changed = routers.erase(router.id) != 0 || changed;
            else if(canonical(router.type, policy, options, edit)) { routers[router.id] = std::move(edit); changed = true; }
        };
        std::string currentLabel = ownLabel;
        if(overridden) {
            MacroMixerMorphPolicy known;
            currentLabel = canonical(router.type, current.policy, current.options, known)
                ? MacroMixerMixing::registry().policy(router.type, known.policy).label : current.policy;
        }
        ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(router.name.c_str());
        ImGui::SameLine(ImGui::GetFontSize() * 8); ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##policy", currentLabel.c_str())) {
            if(ImGui::Selectable(ownLabel.c_str(), !overridden)) apply("", ofJson::object());
            for(const auto& policy : choices)
                if(ImGui::Selectable(policy.label.c_str(), overridden && policy.id == current.policy)) apply(policy.id, ofJson::object());
            ImGui::EndCombo();
        }
        if(overridden) {
            ImGui::Indent(ImGui::GetFontSize() * 8);
            drawMixOptions(router.type, current.options, [&](const ofJson& options) { apply(current.policy, options); });
            ImGui::Unindent(ImGui::GetFontSize() * 8);
        }
        ImGui::PopID();
    }
    ImGui::EndDisabled();
    return changed;
}

void drawSources(MacroMixerRuntime& runtime, State& state, const SourceContext& context) {
    auto& actions = state.actions;
    const bool fading = runtime.transitioning();
    ImGui::BeginDisabled(fading || runtime.contract().routers.empty());
    if(ImGui::Button("+ Add Folder")) {
        auto result = ofSystemLoadDialog("Choose Macro Folder", true);
        if(result.bSuccess) actions.push_back({ActionKind::Import, result.getPath()});
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if(ImGui::Button("Clear Playback")) actions.push_back({ActionKind::ClearPlayback, ""});
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Release running graphs and reset router values. Imported macros remain in the list.");
    // Loading every source stalls frames, so the check never runs while one plays.
    const bool playing = !runtime.currentID().empty() || fading;
    ImGui::SameLine(); ImGui::BeginDisabled(playing || runtime.macros().size() < 2);
    if(ImGui::Button("Check Status")) actions.push_back({ActionKind::CheckStatus, ""});
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(playing ? "Clear Playback first: the check loads every source and would stall the running show."
                                  : "Load each source once and preview it morphing into every other one.");
    if(const auto left = runtime.originsToCheck()) {
        ImGui::SameLine();
        if(playing) ImGui::TextDisabled("Check waits for Clear Playback (%zu to go)", left);
        else ImGui::TextDisabled("Checking sources: %zu to go", left);
    }
    const auto entries = runtime.macros();
    for(size_t i = 0; i < entries.size(); ++i) {
        const auto& macro = entries[i];
        ImGui::PushID(macro->id().c_str());
        ImGui::Spacing();
        if(drawSourceHeader(runtime, state, context, *macro, i, entries.size())) {
            drawSource(runtime, state, context, *macro);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

bool drawSourceHeader(const MacroMixerRuntime& runtime, State& state, const SourceContext& context,
                      const MacroMixerRevision& macro, size_t index, size_t count) {
    auto& actions = state.actions;
    const bool fading = runtime.transitioning();
    // A header in the node color, like parameter separators; the running source is stronger.
    std::string badges;
    for(const auto& badge : {std::make_pair(runtime.currentID(), "Running"), std::make_pair(runtime.destinationID(), "Destination"),
                             std::make_pair(runtime.queuedID(), "Queued"), std::make_pair(context.selectedID, "Selected")})
        if(macro.id() == badge.first) badges += (badges.empty() ? "" : ", ") + std::string(badge.second);
    const bool running = macro.id() == runtime.currentID();
    const auto color = context.color;
    const auto& style = ImGui::GetStyle();
    const float em = ImGui::GetFontSize(), left = ImGui::GetCursorPosX(), right = left + ImGui::GetContentRegionAvail().x;
    const float alpha = running ? .4f : .15f;
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(color.r, color.g, color.b, alpha));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(color.r, color.g, color.b, alpha + .1f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(color.r, color.g, color.b, alpha + .2f));
    const bool open = ImGui::TreeNodeEx("##source", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth);
    ImGui::PopStyleColor(3);
    const auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRectFilled(min, ImVec2(min.x + em * .25f, max.y),
        ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, 1)));
    ImGui::SameLine(left + em + style.FramePadding.x * 3);
    // The name takes the colour of the worst morph into this source found by
    // the source check, as its Details do; red too when the source fails to read.
    const auto worst = runtime.worstInto(macro.id());
    const ImVec4* tint = nullptr;
    static const ImVec4 graphFallback = healthColor(Health::GraphFallback), nodeFallback = healthColor(Health::NodeFallback);
    if(!runtime.sourceProblem(macro.id()).empty() || worst == MacroMixerRuntime::Compatibility::GraphFallback) tint = &graphFallback;
    else if(worst == MacroMixerRuntime::Compatibility::NodeFallback) tint = &nodeFallback;
    if(tint) ImGui::PushStyleColor(ImGuiCol_Text, *tint);
    ImGui::TextUnformatted((std::to_string(index) + ": " + macro.name()).c_str());
    if(tint) ImGui::PopStyleColor();
    if(tint && ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        if(!runtime.sourceProblem(macro.id()).empty()) ImGui::TextUnformatted(runtime.sourceProblem(macro.id()).c_str());
        for(const auto& check : runtime.checksInto(macro.id())) {
            if(check.second.level != MacroMixerRuntime::Compatibility::GraphFallback && check.second.level != MacroMixerRuntime::Compatibility::NodeFallback) continue;
            const auto from = runtime.macro(check.first);
            ImGui::Text("From %s: %s", from ? from->name().c_str() : check.first.c_str(),
                check.second.level == MacroMixerRuntime::Compatibility::GraphFallback ? "graph fallback" : "node replaced");
        }
        ImGui::EndTooltip();
    }
    if(!badges.empty()) {
        // Badges take the warmest colour of this source's Details, green when none is orange or red.
        const auto badgeHealth = detailsHealth(runtime, macro.id(), context.morph);
        ImGui::SameLine();
        ImGui::TextColored(badgeHealth == Health::Ok ? ImVec4(0, 1, 0, 1) : healthColor(badgeHealth), "%s", badges.c_str());
    }
    // Refresh, up, down and remove, right-aligned on the header row.
    const float gap = em * .25f, square = ImGui::GetFrameHeight();
    const float buttons = 3 * square + ImGui::CalcTextSize("-").x + style.FramePadding.x * 2 + 3 * gap;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), right - buttons - style.FramePadding.x));
    // Buttons sit on the header without their own background; hover still shows.
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, .15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, .25f));
    ImGui::BeginDisabled(fading);
    if(ImGui::Button("##refresh", ImVec2(square, square))) actions.push_back({ActionKind::Refresh, macro.source()});
    {
        // A circular arrow: an open arc with an arrowhead at its end.
        const auto box = ImGui::GetItemRectMin();
        const ImVec2 centre(box.x + square * .5f, box.y + square * .5f);
        const float radius = square * .27f, thickness = std::max(1.f, square * .09f);
        const float from = -PI * .35f, to = PI * 1.35f;
        const auto ink = ImGui::GetColorU32(ImGuiCol_Text);
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->PathArcTo(centre, radius, from, to, 20);
        drawList->PathStroke(ink, 0, thickness);
        const ImVec2 tip(centre.x + std::cos(from) * radius, centre.y + std::sin(from) * radius);
        const float head = square * .16f;
        drawList->AddTriangleFilled(ImVec2(tip.x - head, tip.y - head * .9f), ImVec2(tip.x + head * .9f, tip.y - head * .1f),
            ImVec2(tip.x - head * .3f, tip.y + head), ink);
    }
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Refresh: read the source folder again");
    ImGui::EndDisabled(); ImGui::SameLine(0, gap); ImGui::BeginDisabled(index == 0);
    if(ImGui::ArrowButton("##up", ImGuiDir_Up)) actions.push_back({ActionKind::MoveMacro, macro.id(), index - 1});
    ImGui::EndDisabled(); ImGui::SameLine(0, gap); ImGui::BeginDisabled(index + 1 == count);
    if(ImGui::ArrowButton("##down", ImGuiDir_Down)) actions.push_back({ActionKind::MoveMacro, macro.id(), index + 1});
    ImGui::EndDisabled(); ImGui::SameLine(0, gap);
    if(ImGui::Button("-")) actions.push_back({ActionKind::RemoveMacro, macro.id()});
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Remove Macro; clears playback if it is running, a destination or queued.");
    ImGui::PopStyleColor(3);
    return open;
}

void drawSource(MacroMixerRuntime& runtime, State& state, const SourceContext& context, const MacroMixerRevision& macro) {
    namespace fs = std::filesystem;
    auto& actions = state.actions;
    const auto& source = macro.source();
    ImGui::TextWrapped("%s", MacroMixerSource::displayPath(source).c_str());
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", source.c_str());
    std::error_code missing;
    const bool available = fs::exists(fs::path(source) / "modules.json", missing);
    const auto problem = runtime.sourceProblem(macro.id());
    if(!available) ImGui::TextColored(ImVec4(1,.7f,.3f,1), "Missing (folder or modules.json unavailable)");
    else if(!problem.empty()) ImGui::TextColored(ImVec4(1,.7f,.3f,1), "%s", problem.c_str());
    ImGui::Spacing();
    const auto own = runtime.sourceMorphPolicies(macro.id()).size();
    if(ImGui::TreeNode("##policies", own ? "Mix/Morph Policies (%zu set)" : "Mix/Morph Policies", own)) {
        drawMorphPolicies(runtime, state, macro.id());
        ImGui::TreePop();
    }
    drawSourceWindows(runtime, macro);
    if(context.morph || runtime.morphMode()) {
        ImGui::Spacing();
        // Red: the whole graph cannot morph and falls back to the router crossfade.
        // Orange: one node cannot morph and is replaced, crossfading on its cables.
        static const ImVec4 graphFallback = healthColor(Health::GraphFallback), nodeFallback = healthColor(Health::NodeFallback);
        const auto colorOf = [](const std::string& kind) -> const ImVec4* {
            return kind == "fallback" || kind == "skipped" ? &graphFallback : kind == "replaced" ? &nodeFallback : nullptr;
        };
        const auto& details = runtime.sourceDetails(macro.id());
        const ImVec4* worst = nullptr;
        for(const auto& detail : details) if(const auto* color = colorOf(detail.kind)) if(!worst || color == &graphFallback) worst = color;
        if(worst) ImGui::PushStyleColor(ImGuiCol_Text, *worst);
        const bool open = ImGui::TreeNode("Details");
        if(worst) ImGui::PopStyleColor();
        if(open) {
            // Issues first: graph fallbacks, then replaced nodes, then the rest in
            // plan order. The context line stays on top as the list's title.
            std::vector<size_t> order;
            for(size_t n = 0; n < details.size(); ++n) order.push_back(n);
            const auto rank = [&](size_t n) {
                const auto& kind = details[n].kind;
                return kind == "context" ? 0 : kind == "fallback" || kind == "skipped" ? 1 : kind == "replaced" ? 2 : 3;
            };
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return rank(a) < rank(b); });
            for(const auto n : order) {
                const auto& detail = details[n];
                ImGui::PushID(int(n));
                if(detail.kind == "context") {
                    ImGui::TextWrapped("%s (%s)", detail.message.c_str(), runtime.hasTransitionDetails(macro.id()) ? "last transition" : "preview");
                    ImGui::PopID(); continue;
                }
                const auto kind = detail.kind == "fallback" ? "Graph fallback" : detail.kind == "mix" ? "Connection mix"
                    : detail.kind == "added" ? "Added node" : detail.kind == "removed" ? "Removed node"
                    : detail.kind == "replaced" ? "Replaced node" : detail.kind == "node" ? "Node morph"
                    : detail.kind == "skipped" ? "Skipped connection mix" : detail.kind == "audio" ? "Audio servers" : "Live graph";
                ImGui::Spacing();
                if(const auto* color = colorOf(detail.kind)) {
                    ImGui::PushStyleColor(ImGuiCol_Text, *color);
                    ImGui::TextWrapped("%s: %s", kind, detail.location.c_str());
                    ImGui::TextWrapped("%s", detail.message.c_str());
                    ImGui::PopStyleColor();
                } else {
                    ImGui::TextWrapped("%s: %s", kind, detail.location.c_str());
                    ImGui::TextDisabled("%s", detail.message.c_str());
                }
                if(detail.kind == "mix") {
                    if(detail.type.empty()) ImGui::TextDisabled("Native control interpolation");
                    else ImGui::TextDisabled("%s: %s", detail.type.c_str(), MacroMixerMixing::registry().policy(detail.type, detail.policy).label.c_str());
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
    drawSourceChecks(runtime, macro);

    // Nodes the last analysis left out, plus kept-out nodes inside this folder
    // when it has not been read since a preset was restored.
    std::vector<std::pair<std::string, std::string>> keptOut;
    for(const auto& node : runtime.keptOutNodes()) {
        const auto relative = fs::path(node.first).lexically_relative(source);
        const bool inside = !relative.empty() && *relative.begin() != "..";
        if(macro.keptOut().count(node) || inside) keptOut.push_back(node);
    }
    if(!keptOut.empty()) {
        ImGui::Spacing();
        if(ImGui::TreeNode("##keptOut", "Kept out (%zu)", keptOut.size())) {
            for(const auto& node : keptOut) {
                ImGui::PushID((node.first + "/" + node.second).c_str());
                auto name = node.second; std::replace(name.begin(), name.end(), '_', ' ');
                ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(name.c_str());
                ImGui::SameLine();
                if(ImGui::SmallButton("Restore")) actions.push_back({ActionKind::RestoreNode, node.first, 0, {}, {}, node.second});
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("Load this node again from the next activation. While it blocks MacroMixer, you are asked again.");
                if(node.first != source) {
                    ImGui::TextDisabled("%s", MacroMixerSource::displayPath(node.first).c_str());
                    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", node.first.c_str());
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
}

void drawSourceChecks(const MacroMixerRuntime& runtime, const MacroMixerRevision& macro) {
    using Compatibility = MacroMixerRuntime::Compatibility;
    const auto& checks = runtime.checksInto(macro.id());
    if(checks.empty()) return;
    static const ImVec4 graphFallback = healthColor(Health::GraphFallback), nodeFallback = healthColor(Health::NodeFallback);
    const auto colorOf = [](Compatibility level) -> const ImVec4* {
        return level == Compatibility::GraphFallback ? &graphFallback : level == Compatibility::NodeFallback ? &nodeFallback : nullptr;
    };
    const auto* worst = colorOf(runtime.worstInto(macro.id()));
    ImGui::Spacing();
    if(worst) ImGui::PushStyleColor(ImGuiCol_Text, *worst);
    const bool open = ImGui::TreeNode("##checks", "Morphs from other sources");
    if(worst) ImGui::PopStyleColor();
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Each other source's saved state previewed morphing into this one.\nRed: the whole graph falls back to a crossfade. Orange: a node is replaced.");
    if(!open) return;
    for(const auto& check : checks) {
        ImGui::PushID(check.first.c_str());
        const auto from = runtime.macro(check.first);
        const auto name = from ? from->name() : check.first;
        const auto level = check.second.level;
        const auto* color = colorOf(level);
        const auto label = level == Compatibility::GraphFallback ? "graph fallback" : level == Compatibility::NodeFallback ? "node replaced"
            : level == Compatibility::Ok ? "morphs in place" : "not checked";
        if(color) ImGui::TextColored(*color, "From %s: %s", name.c_str(), label);
        else ImGui::TextDisabled("From %s: %s", name.c_str(), label);
        ImGui::Indent();
        for(const auto& problem : check.second.problems) {
            if(color) ImGui::PushStyleColor(ImGuiCol_Text, *color);
            ImGui::TextWrapped("%s: %s", problem.location.c_str(), problem.message.c_str());
            if(color) ImGui::PopStyleColor();
        }
        ImGui::Unindent();
        ImGui::PopID();
    }
    ImGui::TreePop();
}

void drawSourceWindows(MacroMixerRuntime& runtime, const MacroMixerRevision& macro) {
    // A playing source lists its windows (both during a transition): its
    // nodes' own, its Custom GUI panels and its timelines. Others keep their
    // choice until they play. Windows never open from what a source saved.
    const auto windows = runtime.sourceWindows(macro.id());
    const auto remembered = runtime.openWindows(macro.id());
    if(windows.empty() && remembered.empty()) return;
    ImGui::Spacing();
    const bool showing = runtime.showsWindows(macro.id());
    const bool expanded = ImGui::TreeNodeEx("##windows", showing ? ImGuiTreeNodeFlags_DefaultOpen : 0, "Windows (%zu open)", remembered.size());
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("Node windows, Custom GUI panels and timelines of this source.\nTicked ones open while it plays.");
    if(!expanded) return;
    for(const auto& window : windows) {
        ImGui::PushID(window.key.c_str());
        bool open = window.open;
        if(ImGui::Checkbox(window.title.c_str(), &open)) runtime.setSourceWindow(macro.id(), window.key, open);
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", window.key.c_str());
        ImGui::PopID();
    }
    if(!showing) {
        ImGui::TextDisabled("Open while this source plays:");
        for(const auto& key : remembered) {
            ImGui::PushID(key.c_str());
            bool open = true;
            if(ImGui::Checkbox(key.c_str(), &open)) runtime.setSourceWindow(macro.id(), key, open);
            ImGui::PopID();
        }
    }
    ImGui::TreePop();
}

void addBlockedOwner(State& state, std::function<const MacroMixerRuntime*()> runtime) {
    auto& owners = blockedOwners();
    const auto found = std::find_if(owners.begin(), owners.end(), [&](const auto& owner) { return owner.state == &state; });
    if(found != owners.end()) found->runtime = std::move(runtime);
    else owners.push_back({&state, std::move(runtime)});
}

void removeBlockedOwner(State& state) {
    auto& owners = blockedOwners();
    owners.erase(std::remove_if(owners.begin(), owners.end(), [&](const auto& owner) { return owner.state == &state; }), owners.end());
}

void drawPendingBlockedNodeDialog() {
    constexpr const char* popupId = "Node blocks MacroMixer##ofxOceanodeMacroMixerBlocked";
    State* owner = nullptr;
    std::vector<const MacroMixerDiagnostic*> waiting;
    for(const auto& candidate : blockedOwners()) {
        const auto* runtime = candidate.runtime ? candidate.runtime() : nullptr;
        if(!runtime) continue;
        for(const auto& node : runtime->blockedNodes())
            if(!candidate.state->answeredBlocked.count({node.file,node.node})) waiting.push_back(&node);
        if(!waiting.empty()) { owner = candidate.state; break; }
    }
    if(!owner) {
        if(ImGui::IsPopupOpen(popupId)) {
            ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if(ImGui::BeginPopupModal(popupId)) { ImGui::CloseCurrentPopup(); ImGui::EndPopup(); }
        }
        return;
    }
    if(!ImGui::IsPopupOpen(popupId)) {
        // Wait for other dialogs, such as connection errors from the same load.
        if(ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)) return;
        ImGui::OpenPopup(popupId);
    }
    MacroDialogAppearance::Style style;
    const float em = ImGui::GetFontSize();
    // Resizable: 36 em wide and fitted to its content when it opens.
    ImGui::SetNextWindowSize(ImVec2(36*em,0),ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(20*em,8*em),ImVec2(FLT_MAX,FLT_MAX));
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(!ImGui::BeginPopupModal(popupId,nullptr,ImGuiWindowFlags_None)) return;
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
    ImGui::TextWrapped("%s",MacroMixerSource::displayPath(node.file).c_str());
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",node.file.c_str());
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

}
