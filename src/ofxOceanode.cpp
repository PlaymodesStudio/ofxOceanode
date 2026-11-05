//
//  ofxOceanode.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/04/2020.
//

#include "ofxOceanode.h"
#include "ofxOceanodeTheme.h"
#include "ofxOceanodeColors.h"
#include "ofxOceanodeMenu.h"
#include "ofxOceanodeMenuLabel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeTimeController.h"
#include "imgui_internal.h"
#include "ofxOceanodeClipboard.h"
#include "ofxOceanodeWindowManager.h"
#include "ofxOceanodeHelp.h"
#include "ofxOceanodeNodePool.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeMacroMixer.h"
#include "ofxOceanodeLayouts.h"
#include "ofxOceanodeThemeManager.h"
#include "ofxOceanodeAppConfig.h"
#include "Controls/PresetFileCleanupReview.h"
#include <algorithm>
#include <climits>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#ifdef TARGET_WIN32
#include <shellapi.h>
#endif
#if defined(TARGET_OSX)
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace {
bool mainWindowIsFullscreen(){
    if(ofGetWindowMode() == OF_FULLSCREEN) return true;
#if defined(TARGET_OSX)
    // The green button enters native fullscreen, which ofGetWindowMode() does not report.
    auto window = static_cast<id>(ofGetCocoaWindow());
    if(window == nullptr) return false;
    constexpr unsigned long fullScreenMask = 1ul << 14; // NSWindowStyleMaskFullScreen
    const auto styleMask = reinterpret_cast<unsigned long(*)(id, SEL)>(objc_msgSend)(window, sel_registerName("styleMask"));
    return (styleMask & fullScreenMask) != 0;
#else
    return false;
#endif
}

void resetPresetCanvasViews(ofxOceanodeContainer* current, ofxOceanodeCanvas* currentCanvas){
    currentCanvas->resetViewToOrigin();
    for(auto* node : current->getAllModules()){
        if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())){
            resetPresetCanvasViews(macro->getContainer().get(), macro->getCanvas());
        }
    }
}
}

ofxOceanode::ofxOceanode(){
    windows = std::make_unique<ofxOceanodeWindowManager>();
    appConfig = std::make_unique<ofxOceanodeAppConfig>();
    theme = std::make_unique<ofxOceanodeThemeManager>();
    nodeRegistry = make_shared<ofxOceanodeNodeRegistry>();
    typesRegistry = ofxOceanodeTypesRegistry::getInstance();
    scope = ofxOceanodeScope::getInstance();
    oceanodeTime = ofxOceanodeTime::getInstance();
#ifdef OFXOCEANODE_USE_OSC
    receiverPortChanged = oscReceiverPort.newListener([this](int &i){
        oscReceiver.setup(oscReceiverPort);
    });
#endif
    
    registerDefaultTypes();      // ofxOceanodeDefaultTypes.cpp
    registerDefaultNodes();      // ofxOceanodeDefaultNodes.cpp
    registerDefaultRouting();    // ofxOceanodeDefaultRouting.cpp
}

ofxOceanode::~ofxOceanode(){
    // Hosts that do not call exit() must still release the time singleton's
    // graph references before static logger destruction.
    oceanodeTime->shutdown(container);
}


void ofxOceanode::setup(){
    ofSetEscapeQuitsApp(false);
    container = make_shared<ofxOceanodeContainer>(nodeRegistry, typesRegistry);
    canvas.setContainer(container);
    canvas.setup();
    ofxOceanodeShared::setRootContainer(container.get());
    ofxOceanodeShared::setRootCanvas(&canvas);
    scope->setup();
    //controls = make_unique<ofxOceanodeControls>(container);
    controls = make_unique<ofxOceanodeControls>(container,&canvas, oscReceiverPort);
    help = make_unique<ofxOceanodeHelp>(container);
    nodePool = make_unique<ofxOceanodeNodePool>(container, &canvas);
    //        timelines.emplace_back("Phasor_1/Beats_Div");
    //        timelines.emplace_back("Oscillator_1/Pow");
    //        timelines.emplace_back("Indexer_1/NumWaves");
    //        timelines.emplace_back("Mapper_1/Min_Input");
    OceanodeTheme* oceanodeTheme = new OceanodeTheme();
    gui.setup(oceanodeTheme, false, ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable, false);
    windows->setup(container, *controls);
    windows->installImGuiHandlers();
    ofxOceanodeClipboard::install();
    theme->setup(canvas);
    layouts = std::make_unique<ofxOceanodeLayouts>(*windows);
    layouts->setup();
    presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this](){
        if(ofxOceanodeShared::getPresetLoadType() != ofxOceanodePresetLoadType_FullPreset){
            return; // Don't reload layout for clipboard paste / duplicate
        }
        layouts->onPresetLoaded();
        resetPresetCanvasViews(container.get(), &canvas);
        // Re-arm after applying the layout at the next frame boundary.
        presetCanvasCenterPending = true;
        if(ofxOceanodeShared::getCurrentPresetPath().empty()){
            canvas.resetForNewPreset();
            ofxOceanodeShared::setActiveCanvasUniqueID(canvas.getUniqueID());
            canvas.requestFocus();
        }
    });

    presetSavedListener = ofxOceanodeShared::getPresetWasSavedEvent().newListener([this](){
        layouts->onPresetSaved();
    });

    ofxOceanodeShared::readMacros();
    oceanodeTime->setup(container, controls->get<ofxOceanodeTimeController>());
    // Last: config.json also applies spaceTogglesPlay to the Time controller.
    appConfig->setup(canvas, *controls, *windows);
#ifdef TARGET_OSX
    // Retain the host window identity even when output windows become current.
    titleWindow = std::dynamic_pointer_cast<ofAppGLFWWindow>(ofGetMainLoop()->getCurrentWindow());
#endif
}

void ofxOceanode::update(){
    container->getProfilingSession()->nextFrame();
    if(help) help->update();
    for(auto &item : configMenuItems)
        if(item.second.update) item.second.update();
    for(auto &item : utilityMenuItems)
        if(item.second.update) item.second.update();
#ifdef OFXOCEANODE_USE_OSC
    receiveOsc();
#endif
    oceanodeTime->update();
    container->update();
    controls->update();
#ifdef TARGET_OSX
    if(auto window = titleWindow.lock()){
        const std::string currentTitle = window->getSettings().title;
        if(!windowTitleCaptured){
            // Hosts may set their title after Oceanode's setup, before the first update.
            originalWindowTitle = currentTitle;
            windowTitleCaptured = true;
        }
        std::string title = originalWindowTitle;
        const std::string presetName = ofxOceanodeShared::getCurrentPresetName();
        if(!presetName.empty()){
            if(!title.empty()) title += " — ";
            title += presetName;
        }
        // Avoid asking macOS to redraw the title when its text has not changed.
        if(title != currentTitle) window->setWindowTitle(title);
    }
#endif
}

void ofxOceanode::draw(){
    theme->beforeFrame();
    layouts->beforeFrame();
    windows->beforeFrame(mainWindowIsFullscreen());
    if(presetCanvasCenterPending){
        resetPresetCanvasViews(container.get(), &canvas);
        presetCanvasCenterPending = false;
    }
    gui.begin();
    // ImGui uses PopupBg for menus and modals; derive it from WindowBg each frame.
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_PopupBg] = style.Colors[ImGuiCol_WindowBg];
    windows->beginHostWindow();
    drawMenuBar();
    // NOTE: windows->endHostWindow() follows all popup OpenPopup/BeginPopupModal
    // calls below, so the DockSpace window is still the active window context
    // when the popups are registered.

    layouts->drawPopups();
    controls->drawPopups();
    oceanodePresetSave::CleanupReview::instance().draw();
    drawUnusedNodesWindow();
    help->draw();
    nodePool->draw();

    if(openQuickTips){
        ImGui::OpenPopup("Here are some tips:");
        openQuickTips = false;
    }
    showHelpPopUp();

    theme->drawPopups();

    // Connection errors are queued while presets load. Render them from the
    // persistent DockSpace window so OpenPopup and BeginPopupModal use the
    // same stable ImGui ID stack on every frame.
    ofxOceanodeContainer::drawPendingConnectionErrorDialog();
    ofxOceanodeMacroMixer::drawPendingBlockedNodeDialog();
    windows->endHostWindow();
    if(windows->isShowMode()){
        windows->drawShowModeWindow();
		container->draw();
    }else{
        scope->draw();
        container->draw();
        controls->draw();
        canvas.draw();
    }
    //        for(auto &t : timelines){
    //            t.draw();
    //        }
    
    theme->drawWindows();

    for(auto &item : configMenuItems)
        if(item.second.draw) item.second.draw();
    for(auto &item : utilityMenuItems)
        if(item.second.draw) item.second.draw();

    windows->endFrame();
    gui.end();
    gui.draw();
    theme->afterRender();
}

void ofxOceanode::exit(){
    layouts->saveDocking(ofxOceanodeShared::getCurrentPresetPath());
    oceanodeTime->shutdown(container);
    container->clearContainer();
}

void ofxOceanode::drawUnusedNodesWindow(){
    if(!showUnusedNodesWindow) return;
    ImGui::SetNextWindowSize(ImVec2(620, 480), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(520, 320), ImVec2(FLT_MAX, FLT_MAX));
    if(focusUnusedNodesWindow){
        ImGui::SetNextWindowFocus();
        focusUnusedNodesWindow = false;
    }
    if(ImGui::Begin("Remove unused nodes", &showUnusedNodesWindow, ImGuiWindowFlags_NoDocking)){
        auto target = unusedNodeContainer.lock();
        const bool targetChanged = target && target->getNodesRevision() != unusedNodePlan.revision;
        // Selecting from here never takes focus: this window stays the one the arrow
        // keys go to, and the canvas does not start nudging the nodes just selected.
        // Resolve by name only while the reviewed graph still owns the same nodes.
        // Never retain node pointers across frames: edits and preset loads can delete them.
        auto selectNode = [&](const string& name){
            if(!target || targetChanged) return false;
            const auto& nodes = target->getParameterGroupNodesMap();
            auto found = nodes.find(name);
            if(found == nodes.end() || !target->showNodeInCanvas(*found->second, {}, false)) return false;
            unusedNodesSelectedNode = name;
            unusedNodesSelectedGroup = -1;
            return true;
        };
        auto selectGroup = [&](int index){
            if(!target || targetChanged || index < 0 || index >= static_cast<int>(unusedNodePlan.groups.size())) return false;
            vector<ofxOceanodeNode*> members;
            const auto& nodes = target->getParameterGroupNodesMap();
            for(const auto& name : unusedNodePlan.groups[index].nodes){
                auto found = nodes.find(name);
                if(found != nodes.end()) members.push_back(found->second);
            }
            if(!target->showNodesInCanvas(members, false)) return false;
            unusedNodesSelectedGroup = index;
            unusedNodesSelectedNode.clear();
            return true;
        };
        auto drawNode = [&](const string& name){
            ImGui::BeginDisabled(!target || targetChanged);
            if(ImGui::Selectable(name.c_str(), unusedNodesSelectedNode == name)) selectNode(name);
            if(unusedNodesScrollToSelection && unusedNodesSelectedNode == name) ImGui::SetScrollHereY(0.5f);
            ImGui::EndDisabled();
        };
        // Recompute the plan, keeping groups the user unticked while they stay the same.
        auto refreshPlan = [&](){
            std::set<std::vector<string>> unticked;
            for(const auto& group : unusedNodePlan.groups)
                if(!group.selected) unticked.insert(group.nodes);
            unusedNodePlan = target->findUnusedNodes();
            for(auto& group : unusedNodePlan.groups)
                if(unticked.count(group.nodes) != 0) group.selected = false;
            unusedNodesSelectedNode.clear();
            unusedNodesSelectedGroup = -1;
        };
        size_t nodeCount = 0, groupCount = 0, connectionCount = 0;
        for(const auto& group : unusedNodePlan.groups){
            if(!group.selected) continue;
            nodeCount += group.nodes.size();
            connectionCount += group.connections;
            ++groupCount;
        }
        ImGui::Text("Canvas: %s", target ? target->getCanvasID().c_str() : "(closed)");
        ImGui::Text("Remove %zu nodes in %zu groups and %zu connections. Keep %zu nodes.",
                    nodeCount, groupCount, connectionCount, unusedNodePlan.kept.size());
        ImGui::TextWrapped("Sinks, bound parameters and everything feeding them are kept. "
                           "Undo (Cmd/Ctrl+Z) restores a removal.");
        ImGui::TextWrapped("Click a node to select it on its canvas, or a group to select all its nodes. "
                           "Up and Down move through the list, Left on a group goes to its first node, "
                           "Right goes back to the group, Space ticks or unticks the selected group. "
                           "Untick a group to keep it.");
        if(unusedNodePlan.groups.empty()) ImGui::TextUnformatted("No removable unused nodes found.");
        // The rows in the order they are drawn: a group header (node empty), a node
        // inside a group, or a kept node (group -1). Arrow keys walk this list.
        struct NavItem { int group; string node; };
        vector<NavItem> navItems;
        if(!unusedNodePlan.groups.empty() || !unusedNodePlan.kept.empty()){
            ImGui::BeginChild("Unused node summary", ImVec2(0, std::max(100.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 4.0f)), true);
            for(size_t i = 0; i < unusedNodePlan.groups.size(); ++i){
                auto& group = unusedNodePlan.groups[i];
                const int groupIndex = static_cast<int>(i);
                ImGui::PushID(groupIndex);
                ImGui::Checkbox(("##group" + ofToString(i)).c_str(), &group.selected);
                ImGui::SameLine();
                const string title = "Group " + ofToString(i + 1) + ": " + group.nodes.front()
                    + " (" + ofToString(group.nodes.size()) + " nodes, " + ofToString(group.connections) + " connections)";
                // Groups start closed. OpenOnArrow: only the arrow folds a group, a click on the label selects it.
                const ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                    (unusedNodesSelectedGroup == groupIndex ? ImGuiTreeNodeFlags_Selected : 0);
                if(unusedNodesOpenGroup == groupIndex){
                    ImGui::SetNextItemOpen(true);
                    unusedNodesOpenGroup = -1;
                }
                const bool groupOpen = ImGui::TreeNodeEx(title.c_str(), headerFlags);
                navItems.push_back({groupIndex, ""});
                if(unusedNodesScrollToSelection && unusedNodesSelectedGroup == groupIndex) ImGui::SetScrollHereY(0.5f);
                // Clicking the header (not its arrow) selects the whole group on the canvas.
                if(ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) selectGroup(groupIndex);
                if(groupOpen){
                    for(const auto& name : group.nodes){
                        drawNode(name);
                        navItems.push_back({groupIndex, name});
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if(!unusedNodePlan.kept.empty()){
                ImGui::Separator();
                ImGui::TextUnformatted("Kept");
            }
            for(size_t i = 0; i < unusedNodePlan.kept.size(); ++i){
                const auto& kept = unusedNodePlan.kept[i];
                ImGui::PushID(static_cast<int>(i));
                const ImGuiTreeNodeFlags keptFlags = unusedNodesSelectedNode == kept.node ? ImGuiTreeNodeFlags_Selected : 0;
                const bool keptOpen = ImGui::TreeNodeEx(("Kept: " + kept.node).c_str(), keptFlags);
                navItems.push_back({-1, kept.node});
                if(unusedNodesScrollToSelection && unusedNodesSelectedNode == kept.node) ImGui::SetScrollHereY(0.5f);
                if(keptOpen){
                    drawNode(kept.node);
                    for(const auto& reason : kept.reasons) ImGui::TextWrapped("%s", reason.c_str());
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
        unusedNodesScrollToSelection = false;

        // Keyboard: only while this window has focus and nothing is being typed or dragged.
        const ImGuiIO& io = ImGui::GetIO();
        if(target && !targetChanged && !navItems.empty() &&
           ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
           !io.KeyCtrl && !io.KeySuper && !io.KeyAlt && !io.KeyShift &&
           !io.WantTextInput && !ImGui::IsAnyItemActive()){
            const int groupCountNow = static_cast<int>(unusedNodePlan.groups.size());
            // The group a selected node belongs to, or -1 for a kept node.
            int selectedNodeGroup = -1;
            for(int g = 0; g < groupCountNow && !unusedNodesSelectedNode.empty(); ++g){
                const auto& names = unusedNodePlan.groups[g].nodes;
                if(std::find(names.begin(), names.end(), unusedNodesSelectedNode) != names.end()) selectedNodeGroup = g;
            }
            // Where the selection sits in this frame's list. A node inside a folded
            // group is not listed, so stand on its group header instead.
            int current = -1;
            for(int i = 0; i < static_cast<int>(navItems.size()) && current < 0; ++i){
                const auto& item = navItems[i];
                if(unusedNodesSelectedGroup >= 0 && item.group == unusedNodesSelectedGroup && item.node.empty()) current = i;
                else if(unusedNodesSelectedGroup < 0 && !unusedNodesSelectedNode.empty() && item.node == unusedNodesSelectedNode) current = i;
            }
            if(current < 0 && selectedNodeGroup >= 0){
                for(int i = 0; i < static_cast<int>(navItems.size()) && current < 0; ++i)
                    if(navItems[i].group == selectedNodeGroup && navItems[i].node.empty()) current = i;
            }
            const int step = int(ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) - int(ImGui::IsKeyPressed(ImGuiKey_UpArrow, true));
            bool moved = false;
            if(step != 0){
                const int next = current < 0 ? 0 : std::clamp(current + step, 0, static_cast<int>(navItems.size()) - 1);
                if(next != current){
                    const auto& item = navItems[next];
                    moved = item.node.empty() ? selectGroup(item.group) : selectNode(item.node);
                }
            }else if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)){
                // On a group header: open the group and go to its first node.
                if(unusedNodesSelectedGroup >= 0 && unusedNodesSelectedGroup < groupCountNow &&
                   !unusedNodePlan.groups[unusedNodesSelectedGroup].nodes.empty()){
                    const int groupIndex = unusedNodesSelectedGroup;
                    moved = selectNode(unusedNodePlan.groups[groupIndex].nodes.front());
                    if(moved) unusedNodesOpenGroup = groupIndex;
                }
            }else if(ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)){
                // On a node inside a group: back to the group.
                if(unusedNodesSelectedGroup < 0 && selectedNodeGroup >= 0) moved = selectGroup(selectedNodeGroup);
            }else if(ImGui::IsKeyPressed(ImGuiKey_Space, false)){
                // On a group header: tick or untick the group, like its checkbox.
                if(unusedNodesSelectedGroup >= 0 && unusedNodesSelectedGroup < groupCountNow){
                    auto& selectedGroup = unusedNodePlan.groups[unusedNodesSelectedGroup];
                    selectedGroup.selected = !selectedGroup.selected;
                }
            }
            if(moved) unusedNodesScrollToSelection = true;
        }

        if(unusedNodesPlanChanged) ImGui::TextWrapped("Usage changed. The summary has been refreshed; review it before removing.");
        if(!target) ImGui::TextWrapped("The reviewed canvas is closed. Reopen this window from Edit to review another canvas.");
        if(targetChanged) ImGui::TextWrapped("The canvas changed. Refresh the summary before removing.");
        ImGui::BeginDisabled(!target || targetChanged || nodeCount == 0);
        if(ImGui::Button("Remove", ImVec2(120, 0))){
            // Success or not, show the current state: the window stays open for the next review.
            unusedNodesPlanChanged = !target->removeUnusedNodes(unusedNodePlan);
            refreshPlan();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!target);
        if(ImGui::Button("Refresh", ImVec2(120, 0))){
            refreshPlan();
            unusedNodesPlanChanged = false;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Close", ImVec2(120, 0))) showUnusedNodesWindow = false;
    }
    ImGui::End();
}

// The host window's menu bar. Each owner draws its own menu or section.
void ofxOceanode::drawMenuBar(){
    if(showMetrics) ImGui::ShowMetricsWindow(&showMetrics);

    static float globalMenuWidth = 0.0f;
    const ImRect menuBarRect = ImGui::GetCurrentWindow()->MenuBarRect();
    float menuTextY = menuBarRect.Min.y + (menuBarRect.GetHeight() - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(9.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));
    if (ImGui::BeginMenuBar())
    {
        menuTextY = ImGui::GetCursorScreenPos().y + ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset;
        std::vector<std::string> menuLabels = controls->menuControllerNames();
        for(const char* name : {"Edit", "Config", "Window", "Layouts", "Help"})
            menuLabels.emplace_back(name);
        const float available = std::max(0.0f, menuBarRect.Max.x - globalMenuWidth - ImGui::GetCursorScreenPos().x);
        const float labelWidth = ofxOceanodeMenuLabel::widthLimit(menuLabels, available, ImGui::GetStyle().ItemSpacing.x * 1.5f);
        auto beginTopLevelMenu = [labelWidth](const char* label){
            const ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing.x * 1.5f, spacing.y));
            const std::string visibleLabel = ofxOceanodeMenuLabel::abbreviated(label, labelWidth);
            const bool open = ImGui::BeginMenu(visibleLabel.c_str());
            ImGui::PopStyleVar();
            return open;
        };
        controls->drawMenu("Presets", labelWidth);

        if(beginTopLevelMenu("Edit"))
        {
            const string activeCanvasId = ofxOceanodeShared::getActiveCanvasUniqueID().empty()
                ? canvas.getUniqueID() : ofxOceanodeShared::getActiveCanvasUniqueID();
            ofxOceanodeContainer* activeContainer = container->getContainerForCanvasID(activeCanvasId);
            shared_ptr<ofxOceanodeContainer> activeContainerOwner = container;
            ofxOceanodeCanvas* activeCanvas = nullptr;
            std::function<void(shared_ptr<ofxOceanodeContainer>, ofxOceanodeCanvas*)> findActiveCanvas =
            [&](shared_ptr<ofxOceanodeContainer> currentContainer, ofxOceanodeCanvas* currentCanvas){
                if(activeCanvas != nullptr || currentContainer == nullptr || currentCanvas == nullptr) return;
                if(currentCanvas->getUniqueID() == activeCanvasId){
                    activeCanvas = currentCanvas;
                    activeContainerOwner = currentContainer;
                    return;
                }
                for(auto* node : currentContainer->getAllModules()){
                    if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())){
                        findActiveCanvas(macro->getContainer(), macro->getCanvas());
                    }
                }
            };
            findActiveCanvas(container, &canvas);
            if(activeContainer == nullptr) activeContainer = container.get();
            if(activeCanvas == nullptr) activeCanvas = &canvas;

            ofxOceanodeMenu::section("Selection");
            const bool hasSelection = activeContainer->getSelectedModules().size() > 1;
            if(ImGui::MenuItem("Portalize Selection", nullptr, false, hasSelection)){
                activeCanvas->requestPortalizeSelection();
            }
            if(ImGui::MenuItem("Auto Layout Selection", nullptr, false, hasSelection)){
                activeCanvas->requestAutoLayoutSelection();
            }
            ofxOceanodeMenu::section("Canvas");
            if(ImGui::MenuItem("Auto Layout Canvas", nullptr, false, activeContainer->getAllModules().size() > 1)){
                activeCanvas->requestAutoLayoutCanvas();
            }
            ofxOceanodeMenu::section("Cleanup");
            if(ImGui::MenuItem("Review leftover node files", nullptr, false,
                               !activeContainerOwner->getPresetStoragePath().empty())){
                oceanodePresetSave::CleanupReview::instance().request(activeContainerOwner);
            }
            if(ImGui::MenuItem("Remove unused nodes")){
                unusedNodeContainer = activeContainerOwner;
                unusedNodePlan = activeContainerOwner->findUnusedNodes();
                unusedNodesPlanChanged = false;
                showUnusedNodesWindow = true;
                focusUnusedNodesWindow = true;
                unusedNodesSelectedNode.clear();
                unusedNodesSelectedGroup = -1;
            }
            ImGui::EndMenu();
        }

        controls->drawMenus("Presets", labelWidth);
		
		if(beginTopLevelMenu("Config"))
		{
            if(!configMenuItems.empty()) ofxOceanodeMenu::section("Extensions");
            for(auto &item : configMenuItems){
                const bool enabled = !item.second.enabled || item.second.enabled();
                if(ImGui::MenuItem(item.first.c_str(), nullptr, false, enabled) && item.second.selected)
                    item.second.selected();
            }
            appConfig->drawMenuSections();
            theme->drawMenuSection();

            appConfig->drawConfigButtons();

			ImGui::EndMenu();
		}

        if(beginTopLevelMenu("Window"))
        {
            nodePool->drawMenu();
            ImGui::Separator();
            windows->drawWindowMenu();
            ImGui::EndMenu();
        }

        if(beginTopLevelMenu("Layouts")){
            layouts->drawMenu();
            ImGui::EndMenu();
        }

        if(beginTopLevelMenu("Help"))
        {
            ofxOceanodeMenu::section("Documentation");
            const std::string manual = userManualPath();
            if(ImGui::MenuItem("User Manual (PDF)", nullptr, false, !manual.empty())){
                openWithDefaultApp(manual);
            }
            if(ImGui::MenuItem("Quick tips", "CMD+H")){
                openQuickTips = true;
            }
            help->drawMenu();
            ImGui::Checkbox("Metrics", &showMetrics);
            if(ImGui::MenuItem("Reload Macros")){
                ofxOceanodeShared::updateMacrosStructure();
            }
            for(auto &item : utilityMenuItems){
                const bool enabled = !item.second.enabled || item.second.enabled();
                if(ImGui::MenuItem(item.first.c_str(), nullptr, false, enabled) && item.second.selected)
                    item.second.selected();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::PopStyleVar(2);

    // A separate window owns the whole area, including empty space between controls.
    // It remains above the menu bar for drawing and mouse hit testing.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float height = menuBarRect.GetHeight();
    const float spacing = 4.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 0));
    float digitWidth = 0.0f;
    for(char digit = '0'; digit <= '9'; ++digit)
        digitWidth = std::max(digitWidth, ImGui::CalcTextSize(&digit, &digit + 1).x);
    const float fps = ofGetFrameRate();
    char fpsText[32];
    std::snprintf(fpsText, sizeof(fpsText), "%d", static_cast<int>(fps));
    const float fpsWidth = std::max(3.0f * digitWidth, ImGui::CalcTextSize(fpsText).x);
    const float statusWidth = controls->menuBarStatusWidth();
    const float suffixWidth = controls->menuBarStatusSuffixWidth();
    const float rightGap = 8.0f;
    // Match the actual control widths and SameLine spacing so the FPS slot
    // stays close to the meters without changing the right edge alignment.
    const float transportWidth = 144.0f + spacing + 2.0f * (ImGui::GetFrameHeight() + 2.0f) + 4.0f;
    const float contentWidth = transportWidth + fpsWidth + spacing
                             + (statusWidth > 0 ? spacing + statusWidth : 0.0f)
                             + (suffixWidth > 0 ? spacing + suffixWidth : 0.0f);
    const float width = std::min(menuBarRect.GetWidth(), contentWidth + 4.0f + rightGap);
    globalMenuWidth = width;
    ImGui::SetNextWindowPos(ImVec2(menuBarRect.Max.x - width, menuBarRect.Min.y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_HorizontalScrollbar;
    if(ImGui::Begin("##OceanodeGlobalMenu", nullptr, flags)){
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        // Match the menu labels' text baseline while keeping the overlay and its
        // hit area confined to the menu bar.
        const float textOffset = std::max(0.0f, std::min(menuTextY - menuBarRect.Min.y,
            height - ImGui::GetTextLineHeight()));
        ImGui::SetCursorPosY(textOffset);

		// TRANSPORT: BPM, beat position, play/pause, stop (the shared global transport)
		if(auto transport = container->getTransport()) {
			const auto transportState = transport->getState();
			const bool externalClock = transport->hasExternalClock();
			const bool bpmLocked = externalClock || (container->getAutomationExtension() && container->getAutomationExtension()->isBpmAutomationEnabled());
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2,0));

			float globalBpm = transportState.bpm;
			ImGui::SetNextItemWidth(72);
			ImGui::BeginDisabled(bpmLocked);
			if(ImGui::DragFloat("##globalBpm", &globalBpm, 0.1f, 1.0f, 999.0f, "%.1f BPM") && !bpmLocked)
				container->setBpm(globalBpm);
			ImGui::EndDisabled();
			if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip(externalClock ? "Tempo from the external clock" :
				                  bpmLocked ? "Tempo from the tempo lane" : "Tempo (drag, or double-click to type)");

			ImGui::SameLine();
			double globalBeat = transportState.beatPosition;
			const double beatMin = 0.0;
			ImGui::SetNextItemWidth(72);
			ImGui::BeginDisabled(externalClock);
			if(ImGui::DragScalar("##globalBeat", ImGuiDataType_Double, &globalBeat, 0.05f, &beatMin, nullptr, "%.2f") && !externalClock)
				transport->seekToBeat(std::max(0.0, globalBeat));
			ImGui::EndDisabled();
			if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip(externalClock ? "Following an external clock" : "Transport position in beats (drag, or double-click to type)");

			// Square icon buttons drawn like the timeline toolbar's.
			const float iconSize = ImGui::GetFrameHeight();
			auto transportButton = [&](const char* id, int icon, bool on, const char* tooltip) {
				ImGui::SameLine(0.0f, 2.0f);
				const ImVec2 pos = ImGui::GetCursorScreenPos();
				const ImVec2 size(iconSize + 2.0f, iconSize);
				const bool pressed = ImGui::InvisibleButton(id, size) && !externalClock;
				const bool hovered = ImGui::IsItemHovered() && !externalClock;
				const bool held = ImGui::IsItemActive() && !externalClock;
				auto* dl = ImGui::GetWindowDrawList();
				const ImU32 accent = IM_COL32(90, 205, 125, 255);
				const ImU32 bg = on ? accent : ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
				dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, 3.0f);
				const ImU32 col = externalClock ? IM_COL32(120, 120, 120, 160) : on ? IM_COL32(15, 18, 24, 255) : IM_COL32(225, 228, 235, 255);
				const ImVec2 c(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
				const float s = size.y * 0.52f;
				if(icon == 0) { // play
					dl->AddTriangleFilled(ImVec2(c.x - s * 0.35f, c.y - s * 0.45f), ImVec2(c.x - s * 0.35f, c.y + s * 0.45f),
					                      ImVec2(c.x + s * 0.45f, c.y), col);
				} else if(icon == 1) { // pause
					dl->AddRectFilled(ImVec2(c.x - s * 0.38f, c.y - s * 0.42f), ImVec2(c.x - s * 0.1f, c.y + s * 0.42f), col, 1.0f);
					dl->AddRectFilled(ImVec2(c.x + s * 0.1f, c.y - s * 0.42f), ImVec2(c.x + s * 0.38f, c.y + s * 0.42f), col, 1.0f);
				} else { // stop
					dl->AddRectFilled(ImVec2(c.x - s * 0.36f, c.y - s * 0.36f), ImVec2(c.x + s * 0.36f, c.y + s * 0.36f), col, 1.5f);
				}
				if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", externalClock ? "Following an external clock" : tooltip);
				return pressed;
			};
			if(transportButton("##globalPlay", transportState.isPlaying ? 1 : 0, transportState.isPlaying,
			                   transportState.isPlaying ? "Pause" : "Play"))
				transport->setIsPlaying(!transportState.isPlaying);
			if(transportButton("##globalStop", 2, false, "Stop (back to the start)"))
				transport->stop();

			ImGui::PopStyleVar();
		}

        if(statusWidth > 0.0f){
            ImGui::SameLine();
            controls->drawMenuBarStatus();
        }
        if(suffixWidth > 0.0f){
            ImGui::SameLine();
            controls->drawMenuBarStatusSuffix();
        }
        ImGui::SameLine();
        // Keep the readout against the right padding even when the estimated
        // width of the preceding controls leaves unused space in the overlay.
        const ImVec2 fpsCursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(std::max(fpsCursor.x, menuBarRect.Max.x - rightGap - fpsWidth), fpsCursor.y));
        const ImVec4 fpsColor = fps >= 60.0f ? OceanodeColors::FpsGood
                             : fps > 30.0f ? OceanodeColors::FpsWarning
                                           : OceanodeColors::FpsBad;
        const ImVec2 fpsSize(fpsWidth, ImGui::GetTextLineHeight());
        const ImVec2 fpsPos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(fpsSize);
        ImGui::PushStyleColor(ImGuiCol_Text, fpsColor);
        ImGui::RenderTextClipped(fpsPos, ImVec2(fpsPos.x + fpsSize.x, fpsPos.y + fpsSize.y),
                                 fpsText, nullptr, nullptr, ImVec2(1.0f, 0.0f));
        ImGui::PopStyleColor();

    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(6);
}

// The PDF user manual (scripts/build_user_manual.py). A copy in the app's data
// folder wins, so a distributed app can ship it; otherwise the addon's docs/,
// found from where this file was compiled. Empty when there is neither.
std::string ofxOceanode::userManualPath(){
    const std::string name = "oceanode-user-manual.pdf";
    const std::string inData = ofToDataPath(name, true);
    if(ofFile::doesFileExist(inData, false)) return inData;
    std::filesystem::path source(__FILE__);
    // Makefile builds compile with paths relative to the project (bin/data/../..)
    if(source.is_relative()) source = std::filesystem::path(ofToDataPath("", true)) / ".." / ".." / source;
    std::error_code error;
    const auto inAddon = std::filesystem::weakly_canonical(source.parent_path().parent_path() / "docs" / name, error);
    return !error && std::filesystem::exists(inAddon, error) ? inAddon.string() : "";
}

// Opens a file with the system's default application for its type
// (ofLaunchBrowser only takes http and https URLs).
void ofxOceanode::openWithDefaultApp(const std::string& path){
#ifdef TARGET_WIN32
    ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
#ifdef TARGET_OSX
    std::string command = "open ";
#else
    std::string command = "xdg-open ";
#endif
    command += "'";
    for(char c : path) command += c == '\'' ? std::string("'\\''") : std::string(1, c);
    command += "'";
    if(std::system(command.c_str()) != 0){
        ofLogError("ofxOceanode") << "Couldn't open " << path;
    }
#endif
}


void ofxOceanode::showHelpPopUp()
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize;
    bool p_open = true;
    if(ImGui::BeginPopupModal("Here are some tips:", &p_open, flags)){
        ImGui::Text("%s", " - Press RIGHT CLICK to open new node popup");
        ImGui::Spacing();
        ImGui::Text("%s", " - Drag empty to move canvas");
        ImGui::Spacing();
        ImGui::Text("%s", " - Hold SHIFT to multiselect");
        ImGui::Spacing();
        ImGui::Text("%s", " - Press CMD and drag over canvas to select multiple nodes (SHIFT adds to current selection)");
        ImGui::Text("%s", "   * From Up to Down select all node");
        ImGui::Text("%s", "   * From Down to Up select region node");
        ImGui::Spacing();
        ImGui::Text("%s", " - Press in empty space or drag a node to clear selection");
        ImGui::Spacing();
        ImGui::Text("%s", " - Press BACKSPACE to delete selected nodes");
        ImGui::Spacing();
        ImGui::Text("%s", " - Press CMD+C to copy selection");
        ImGui::Spacing();
        ImGui::Text("%s", " - Press CMD+X to cut selection");
        ImGui::Spacing();
        ImGui::Text("%s", " - Press CMD+V to paste selection on mouse position (SHIFT to also paste in connections)");
        ImGui::Spacing();
        ImGui::Text("%s", " - Press CMD+D to duplicate selection on mouse position (SHIFT to also duplicate in connections)");
        ImGui::Spacing();
        ImGui::Text("%s", " - Press CMD+A to select are nodes");
        ImGui::EndPopup();
    }
}

#ifdef OFXOCEANODE_USE_OSC
void ofxOceanode::receiveOsc(){
    while(oscReceiver.hasWaitingMessages()){
        ofxOscMessage m;
        oscReceiver.getNextMessage(m);
        container->receiveOscMessage(m);
    }
}
#endif

void ofxOceanode::loadPreset(std::string presetPathRelativeToData){ //call preset via path
    ofxOceanodeShared::startedLoadingPreset();
    container->loadPreset(presetPathRelativeToData);
    ofxOceanodeShared::finishedLoadingPreset();
}

void ofxOceanode::loadPreset(std::string bank, std::string name){ //call preset via bank and name
    auto toSendPair = make_pair(name, bank);
    container->loadPresetEvent.notify(toSendPair);
}

void ofxOceanode::disableRenderAll(){
    ofxOceanodeShared::setConfigurationFlags(ofxOceanodeShared::getConfigurationFlags() | ofxOceanodeConfigurationFlags_DisableRenderAll);
}

void ofxOceanode::disableRenderHistograms(){
    ofxOceanodeShared::setConfigurationFlags(ofxOceanodeShared::getConfigurationFlags() | ofxOceanodeConfigurationFlags_DisableHistograms);
}
