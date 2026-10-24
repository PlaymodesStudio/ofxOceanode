#include "ofxOceanodeWindowManager.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeControls.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeScope.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeCanvas.h"
#include "ofxOceanodeCustomGuiExtension.h"
#include "ofxOceanodeProportionalDocking.h"
#include "ofxOceanodeFloatingWindowDecor.h"
#include "ofxOceanodeColors.h"
#include "ofxOceanodeMenu.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <vector>

namespace {
// Use model identity, never a user-editable node label or a retained pointer.
std::string nodeKey(ofxOceanodeNodeModel& model){
    return ofJson::array({model.nodeName(), model.getNumIdentifier()}).dump();
}

struct WindowToggle {
    std::string name;
    ofParameter<bool>* toggle;
    bool inspector;
};

// One node window as seen by the Window menu and by captured views.
struct WindowVisit {
    ofxOceanodeNode& node;
    ofxOceanodeNodeModel& model;
    ofxOceanodeNodeMacro* macro;   // with its child container, or nullptr
    std::string label;             // menu label path, "Macro_1 / Phasor_1"
    std::string key;               // nodeKey(model)
    std::vector<WindowToggle> toggles;
};

// Returns false to skip the node (and a Macro's children). A Macro's children
// are visited with the state the visitor stores in childState.
using NodeVisitor = std::function<bool(const WindowVisit&, ofJson* state, ofJson*& childState)>;
using OwnerVisitor = std::function<void(ofxOceanodeContainer& owner, const std::string& path, ofJson* state)>;

void collectToggles(ofParameterGroup& group, bool inspector, std::vector<WindowToggle>& out){
    for(auto& parameter : group){
        ofParameter<bool>* toggle = nullptr;
        if(auto* pin = dynamic_cast<ofxOceanodeParameter<bool>*>(parameter.get())) toggle = &pin->getParameter();
        else toggle = dynamic_cast<ofParameter<bool>*>(parameter.get());
        if(!toggle || !ofxOceanodeWindowManager::isWindowToggle(parameter->getName())) continue;
        out.push_back({parameter->getName(), toggle, inspector});
    }
}

// The one walker over every node window, including nodes inside nested Macros.
// Captured views skip deprecated persistent nodes; the Window menu lists them.
void walkWindows(ofxOceanodeContainer* owner, const std::string& path, ofJson* state, bool skipPersistent,
                 const NodeVisitor& visitNode, const OwnerVisitor& visitOwner){
    if(state && !state->is_object()) return;
    for(auto* node : owner->getAllModules()){
        if(skipPersistent && node->getIsPersistent()) continue;
        auto& model = node->getNodeModel();
        WindowVisit visit{*node, model, dynamic_cast<ofxOceanodeNodeMacro*>(&model),
            path + node->getParameters().getName(), nodeKey(model), {}};
        collectToggles(node->getParameters(), false, visit.toggles);
        collectToggles(node->getInspectorParameters(), true, visit.toggles);
        ofJson* childState = nullptr;
        if(!visitNode(visit, state, childState)) continue;
        if(visit.macro)
            walkWindows(visit.macro->getContainer().get(), visit.label + " / ", childState, skipPersistent, visitNode, visitOwner);
    }
    visitOwner(*owner, path, state);
}

// Captures (restore == false) or applies the node, Macro and Custom GUI part of a view.
void windowsState(ofxOceanodeContainer* owner, ofJson& state, bool restore){
    walkWindows(owner, "", &state, true,
        [restore](const WindowVisit& visit, ofJson* level, ofJson*& childState){
            if(restore && (!level->contains("nodes") || !(*level)["nodes"].is_object() ||
                           !(*level)["nodes"].contains(visit.key))) return false;
            auto& saved = (*level)["nodes"][visit.key];
            if(!restore) saved = ofJson::object();
            if(!saved.is_object()) return false;
            ofJson* groups[2] = {&saved["parameters"], &saved["inspector"]};
            for(const auto& entry : visit.toggles){
                ofJson& group = *groups[entry.inspector ? 1 : 0];
                if(!restore) group[entry.name] = entry.toggle->get();
                else if(group.contains(entry.name) && group[entry.name].is_boolean()) entry.toggle->set(group[entry.name].get<bool>());
            }
            if(visit.macro){
                if(!restore){
                    saved["open"] = visit.macro->isWindowVisible();
                    saved["children"] = ofJson::object();
                }else if(saved.contains("open") && saved["open"].is_boolean()){
                    // Restoring visibility must not force a canvas into a new dock.
                    visit.macro->setWindowVisible(saved["open"].get<bool>(), false);
                }
                childState = &saved["children"];
            }
            return true;
        },
        [restore](ofxOceanodeContainer& container, const std::string&, ofJson* level){
            if(auto* extension = container.getCustomGuiExtension()) extension->windowState(*level, restore);
        });
}
}

ofxOceanodeWindowManager::ofxOceanodeWindowManager(){
    proportionalDocking = std::make_unique<ofxOceanodeProportionalDocking>();
}

ofxOceanodeWindowManager::~ofxOceanodeWindowManager() = default;

void ofxOceanodeWindowManager::setup(std::shared_ptr<ofxOceanodeContainer> rootContainer, ofxOceanodeControls& controlsRef){
    root = std::move(rootContainer);
    controls = &controlsRef;
}

void ofxOceanodeWindowManager::installImGuiHandlers(){
    proportionalDocking->installSettings();
    ofxOceanodeFloatingWindowDecor::install();   // border + shadow on undocked windows and popups
}

bool ofxOceanodeWindowManager::isWindowToggle(const std::string& parameterName){
    std::string key;
    for(unsigned char c : parameterName) if(std::isalnum(c)) key += static_cast<char>(std::tolower(c));
    return key == "show" || key == "showwin" || key == "showwindow" ||
        key == "showgui" || key == "showeditor" || key == "showsnapshot" ||
        key == "showall" || key == "showsplit";
}

void ofxOceanodeWindowManager::beginHostWindow(){
    // DockBuilder operations are expected before the DockSpace is submitted.
    // This only affects registered controllers absent from the loaded file;
    // windows with an explicit saved DockId remain untouched.
    reconcileMissingControllerWindows();

    static bool opt_fullscreen_persistant = true;
    bool opt_fullscreen = opt_fullscreen_persistant;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }

    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
    // and handle the pass-thru hole, so we ask Begin() to not render a background.
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;

    // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
    // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
    // all active windows docked into it will lose their parent and become undocked.
    // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
    // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", &hostOpen, window_flags);
    ImGui::PopStyleVar();

    if (opt_fullscreen)
        ImGui::PopStyleVar(2);

    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        // ImGui paints idle dock splitters with WindowBg; override it so the
        // gaps between docked windows read as a distinct gutter. Docked windows
        // draw their own backgrounds later, after the pop.
        ImGui::PushStyleColor(ImGuiCol_WindowBg, OceanodeColors::DockGutter);
        proportionalDocking->dockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        ImGui::PopStyleColor();
        ofxOceanodeShared::setDockspaceID(dockspace_id);
        if(!ImGui::DockBuilderGetNode(dockspace_id)->IsSplitNode()){ //We dont have a split node;
            ImGuiID centralNode_id;
            ImGuiID leftNode_id;
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2, &leftNode_id, &centralNode_id);
            ofxOceanodeShared::setCentralNodeID(centralNode_id);
            ofxOceanodeShared::setLeftNodeID(leftNode_id);
        }
    }
    else
    {
        //TODO: Show assert, we need docking
        //ShowDockingDisabledMessage();
    }
    // NOTE: ImGui::End() is deferred to endHostWindow(), AFTER the caller's menu
    // bar and popup OpenPopup/BeginPopupModal calls, so the DockSpace window is
    // still the active window context when the popups are registered.
}

void ofxOceanodeWindowManager::endHostWindow(){
    ImGui::End(); // End DockSpace window — must come after all popup code
    // Refresh references every frame, including after preset/layout loads and
    // manual docking. This only updates IDs; it never rearranges saved windows.
    ofxOceanodeShared::setLeftNodeID(resolveControllerDockID());
    ImGuiDockNode* centralNode = ImGui::DockBuilderGetCentralNode(ofxOceanodeShared::getDockspaceID());
    ofxOceanodeShared::setCentralNodeID(centralNode ? centralNode->ID : 0);
}

void ofxOceanodeWindowManager::endFrame(){
    cycleDockedTabs();
    proportionalDocking->drawDockCollapseHandles();
}

void ofxOceanodeWindowManager::drawShowModeWindow(){
    float fps = ofGetFrameRate();
    if(fps>=60.0) ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 200, 0, 255));
    else if((fps>=50)&&(fps<60)) ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(150, 200, 0, 255));
    else if((fps>=30)&&(fps<50)) ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(255, 100, 0, 255));
    else ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(220, 0, 0, 255));

    if(ImGui::Begin("#FPS")){
        std::string text = ofToString(fps);
        auto windowWidth = ImGui::GetWindowSize().x;
        auto windowHeight = ImGui::GetWindowSize().y;
        auto textWidth   = ImGui::CalcTextSize(text.c_str()).x;

        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        ImGui::SetCursorPosY(windowHeight * 0.5f);
        ImGui::Text("%s", text.c_str());
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void ofxOceanodeWindowManager::applyIni(const std::string& data){
    proportionalDocking->reset();
    loadedGUILayoutLeftDockId = 0;
    loadedGUILayoutData = data;
    std::string currentWindow;
    std::map<std::string, ImGuiID> windowDockIds;
    const std::string windowPrefix = "[Window][";

    for(std::string line : ofSplitString(loadedGUILayoutData, "\n", false, false)){
        line = ofTrim(line);

        if(line.rfind(windowPrefix, 0) == 0 && line.size() > windowPrefix.size() && line.back() == ']'){
            currentWindow = line.substr(windowPrefix.size(), line.size() - windowPrefix.size() - 1);
            continue;
        }

        if(line.rfind("DockId=", 0) == 0){
            std::string dockIdText = line.substr(7);
            size_t comma = dockIdText.find(',');
            if(comma != std::string::npos) dockIdText.erase(comma);

            try{
                windowDockIds[currentWindow] = static_cast<ImGuiID>(std::stoul(dockIdText, nullptr, 0));
            }catch(const std::exception&){
                // Ignore malformed docking metadata and try another controller.
            }
        }
    }

    // Nodes is the reference for controllers missing from the layout.
    auto anchor = windowDockIds.find("Nodes");
    if(anchor != windowDockIds.end() && anchor->second != 0){
        loadedGUILayoutLeftDockId = anchor->second;
    }
    if(loadedGUILayoutLeftDockId == 0){
        for(const auto& controller : controls->getControllers()){
            if(controller->isMenuController()) continue;
            auto dock = windowDockIds.find(controller->getControllerName());
            if(dock != windowDockIds.end() && dock->second != 0){
                loadedGUILayoutLeftDockId = dock->second;
                break;
            }
        }
    }

    ImGui::LoadIniSettingsFromMemory(data.c_str(), data.size());
    // Docking IDs are local to this layout. Never reuse the previous layout's
    // default destinations while the new windows are being restored.
    ofxOceanodeShared::setLeftNodeID(0);
    ofxOceanodeShared::setCentralNodeID(0);
    pendingControllerDockReconciliation = true;
}

void ofxOceanodeWindowManager::clearIni(){
    ImGui::ClearIniSettings();
}

std::string ofxOceanodeWindowManager::captureIni() const{
    size_t size = 0;
    const char* ini = ImGui::SaveIniSettingsToMemory(&size);
    return std::string(ini, size);
}

ImGuiID ofxOceanodeWindowManager::resolveControllerDockID() const{
    const ImGuiID dockspaceID = ofxOceanodeShared::getDockspaceID();
    auto usableDockID = [dockspaceID](ImGuiID id) -> ImGuiID {
        ImGuiDockNode* node = id ? ImGui::DockBuilderGetNode(id) : nullptr;
        if(!node || node->IsSplitNode()) return 0;
        // A floating dock group or a nested Scope dockspace is not the main
        // controller pane, even if it happens to reuse an old numeric ID.
        if(dockspaceID && ImGui::DockNodeGetRootNode(node)->ID != dockspaceID) return 0;
        return id;
    };
    auto windowDockID = [&](const char* name) -> ImGuiID {
        if(ImGuiWindow* window = ImGui::FindWindowByName(name)){
            if(ImGuiID id = usableDockID(window->DockId)) return id;
        }
        // Hidden controllers may have saved settings without a live window.
        if(ImGuiWindowSettings* settings = ImGui::FindWindowSettingsByID(ImHashStr(name))){
            return usableDockID(settings->DockId);
        }
        return 0;
    };

    // During restoration the saved anchor takes precedence over windows that
    // have not yet been submitted. Afterwards, follow Nodes when it is moved.
    if(pendingControllerDockReconciliation){
        if(ImGuiID id = usableDockID(loadedGUILayoutLeftDockId)) return id;
    }
    if(ImGuiID id = windowDockID("Nodes")) return id;
    if(ImGuiID id = windowDockID("MiniMap")) return id;
    for(const auto& controller : controls->getControllers()){
        if(controller->isMenuController()) continue;
        if(ImGuiID id = windowDockID(controller->getControllerName().c_str())) return id;
    }
    return usableDockID(ofxOceanodeShared::getLeftNodeID());
}

void ofxOceanodeWindowManager::reconcileMissingControllerWindows(){
    if(!pendingControllerDockReconciliation) return;

    const ImGuiID leftDockId = resolveControllerDockID();

    // The DockSpace/anchor may not exist on the first frame of a layout without
    // docking data. Leave the request pending and retry next frame.
    if(leftDockId == 0 || ImGui::DockBuilderGetNode(leftDockId) == nullptr) return;

    ofxOceanodeShared::setLeftNodeID(leftDockId);

    auto layoutLines = ofSplitString(loadedGUILayoutData, "\n", false, false);
    for(std::string& line : layoutLines) line = ofTrim(line);

    for(const auto& controller : controls->getControllers()){
        if(controller->isMenuController()) continue;
        const std::string windowName = controller->getControllerName();
        const std::string windowHeader = "[Window][" + windowName + "]";
        bool windowWasSaved = false;
        for(const std::string& line : layoutLines){
            if(line == windowHeader){
                windowWasSaved = true;
                break;
            }
        }

        if(!windowWasSaved){
            ImGui::DockBuilderDockWindow(windowName.c_str(), leftDockId);
        }
    }

    loadedGUILayoutLeftDockId = leftDockId;
    loadedGUILayoutData.clear();
    pendingControllerDockReconciliation = false;
}

// Tab / Shift+Tab: the next / previous tab of the dock area holding the
// focused window. Not while typing or using a widget, in a popup, or with
// Ctrl / Cmd / Alt held (Ctrl+Tab is ImGui's own window switcher).
void ofxOceanodeWindowManager::cycleDockedTabs(){
    ImGuiIO& io = ImGui::GetIO();
    if(!ImGui::IsKeyPressed(ImGuiKey_Tab, false)) return;
    if(io.KeyCtrl || io.KeySuper || io.KeyAlt) return;
    if(io.WantTextInput || ImGui::IsAnyItemActive() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) return;
    ImGuiWindow* window = GImGui->NavWindow;
    if(window == nullptr) return;
    window = window->RootWindow;             // a child window: the window holding it
    ImGuiDockNode* node = window->DockNode;
    if(node == nullptr || node->TabBar == nullptr || node->TabBar->Tabs.Size < 2) return;
    ImGuiTabBar* bar = node->TabBar;
    const int count = bar->Tabs.Size;
    int current = -1;
    for(int i = 0; i < count; i++) if(bar->Tabs[i].Window == window) current = i;
    if(current < 0) return;
    // the next one with a window (skips tabs being closed)
    for(int step = 1; step < count; step++){
        const int i = ((current + (io.KeyShift ? -step : step)) % count + count) % count;
        ImGuiWindow* next = bar->Tabs[i].Window;
        if(next == nullptr) continue;
        bar->NextSelectedTabId = next->TabId;
        ImGui::FocusWindow(next);
        return;
    }
}

ofJson ofxOceanodeWindowManager::captureView(){
    ofJson state = {{"controllers", ofJson::object()}, {"windows", ofJson::object()}};
    const auto& visibility = controls->getControllersVisibility();
    for(const auto& controller : controls->getControllers()){
        if(controller->isMenuController()) continue;
        const auto& name = controller->getControllerName();
        const auto found = visibility.find(name);
        // Optional addon controllers can be drawn before the Window menu has
        // initialized their entry. Match Controls::draw's visible default.
        state["controllers"][name] = found == visibility.end() || found->second;
    }
    windowsState(root.get(), state["windows"], false);
    state["scopes"] = ofxOceanodeScope::getInstance()->isVisible();
    return state;
}

void ofxOceanodeWindowManager::restoreView(const ofJson& view){
    ofJson state = view;
    auto& visibility = controls->getControllersVisibility();
    for(const auto& controller : controls->getControllers()){
        const auto& key = controller->getControllerName();
        if(!controller->isMenuController() && state["controllers"].contains(key) && state["controllers"][key].is_boolean())
            visibility[key] = state["controllers"][key].get<bool>();
    }
    windowsState(root.get(), state["windows"], true);
    if(state.contains("scopes") && state["scopes"].is_boolean())
        ofxOceanodeScope::getInstance()->setVisible(state["scopes"].get<bool>());
}

void ofxOceanodeWindowManager::drawWindowMenu(){
    if(ImGui::MenuItem("Canvas"))
        if(auto* canvas = ofxOceanodeShared::getRootCanvas()) canvas->requestFocus();
    bool scopesVisible = ofxOceanodeScope::getInstance()->isVisible();
    if(ImGui::Checkbox("Scopes", &scopesVisible))
        ofxOceanodeScope::getInstance()->setVisible(scopesVisible);
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Show or hide Scopes without removing scoped parameters.");
    ImGui::TextDisabled("Visibility is saved with the preset.");
    ofxOceanodeMenu::section("Controllers");
    auto& visibility = controls->getControllersVisibility();
    std::vector<std::string> names;
    for(const auto& controller : controls->getControllers()) {
        const auto& name = controller->getControllerName();
        if(controller->isMenuController()) {
            visibility.erase(name);
            continue;
        }
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    for(const auto& name : names) {
        if(!visibility.count(name)) visibility[name] = true;
        ImGui::Checkbox(name.c_str(), &visibility[name]);
        if(name == "Inspector" && ImGui::IsItemHovered())
            ImGui::SetTooltip("Unchecked: always hidden. When enabled, automatic show/hide still follows node selection.");
    }
    using Entry = ofxOceanodeCustomGuiExtension::WindowEntry;
    std::vector<Entry> nodes, macros, panels;
    walkWindows(root.get(), "", nullptr, false,
        [&](const WindowVisit& visit, ofJson*, ofJson*&){
            if(auto* macro = visit.macro) {
                const auto label = visit.label;
                macros.push_back({label, [macro,label](){
                    bool show = macro->isSharedWindowVisible();
                    ImGui::BeginDisabled(macro->isWindowShownByOtherInstance());
                    if(ImGui::Checkbox(label.c_str(), &show)) macro->setWindowVisible(show);
                    ImGui::EndDisabled();
                }});
            }
            for(const auto& entry : visit.toggles) {
                auto* toggle = entry.toggle;
                const auto title = visit.label + " / " + entry.name;
                nodes.push_back({title, [toggle,title](){
                    bool show = toggle->get();
                    if(ImGui::Checkbox(title.c_str(), &show)) toggle->set(show);
                }});
            }
            return true;
        },
        [&](ofxOceanodeContainer& owner, const std::string& path, ofJson*){
            if(auto* extension = owner.getCustomGuiExtension()) {
                auto entries = extension->windowEntries(path);
                panels.insert(panels.end(), entries.begin(), entries.end());
            }
        });
    auto section = [](const char* name, std::vector<Entry>& entries){
        ofxOceanodeMenu::section(name);
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b){ return a.label < b.label; });
        if(entries.empty()) ImGui::TextDisabled("None in this patch");
        for(const auto& entry : entries) { ImGui::PushID(entry.label.c_str()); entry.draw(); ImGui::PopID(); }
    };
    section("Nodes", nodes);
    section("Macros", macros);
    if(root->getCustomGuiExtension()) section("Custom GUIs", panels);
    if(root->getCustomGuiExtension() && ImGui::MenuItem("New Custom GUI")) {
        auto* owner = root->getContainerForCanvasID(ofxOceanodeShared::getActiveCanvasUniqueID());
        if(!owner) owner = root.get();
        if(auto* extension = owner->getCustomGuiExtension()) extension->requestCreate();
    }
}
