//
//  ofxOceanode.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/04/2020.
//

#include "ofxOceanode.h"
#include "ofxOceanodeTheme.h"
#include "ofxOceanodeMenu.h"
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
#ifdef TARGET_WIN32
#include <shellapi.h>
#endif

namespace {
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
}

void ofxOceanode::draw(){
    theme->beforeFrame();
    layouts->beforeFrame();
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
        auto drawNode = [&](const string& name){
            // Resolve by name only while the reviewed graph still owns the same nodes.
            // Never retain node pointers across frames: edits and preset loads can delete them.
            ImGui::BeginDisabled(!target || targetChanged);
            if(ImGui::Selectable(name.c_str(), unusedNodesSelectedNode == name)){
                const auto& nodes = target->getParameterGroupNodesMap();
                auto found = nodes.find(name);
                if(found != nodes.end() && target->showNodeInCanvas(*found->second))
                    unusedNodesSelectedNode = name;
            }
            ImGui::EndDisabled();
        };
        size_t nodeCount = 0;
        for(const auto& group : unusedNodePlan.groups) nodeCount += group.size();
        ImGui::Text("Canvas: %s", target ? target->getCanvasID().c_str() : "(closed)");
        ImGui::Text("Remove %zu nodes in %zu groups and %zu connections.",
                    nodeCount, unusedNodePlan.groups.size(), unusedNodePlan.connectionCount);
        ImGui::Text("Keep %zu nodes with usage or protected models.", unusedNodePlan.preservedNodes);
        ImGui::TextWrapped("Custom GUI, snapshots, Timeline, MIDI, Scope, persistent nodes, "
                           "and models with standalone or unknown effects are protected, together with their connected groups.");
        ImGui::TextWrapped("Click a node to open its canvas, select it and center the view.");
        if(nodeCount == 0) ImGui::TextUnformatted("No removable unused nodes found.");
        if(nodeCount != 0 || !unusedNodePlan.protectedGroups.empty()){
            ImGui::BeginChild("Unused node summary", ImVec2(0, std::max(100.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 4.0f)), true);
            for(size_t i = 0; i < unusedNodePlan.groups.size(); ++i){
                ImGui::Text("Remove group %zu", i + 1);
                for(const auto& name : unusedNodePlan.groups[i]) drawNode(name);
            }
            for(size_t i = 0; i < unusedNodePlan.protectedGroups.size(); ++i){
                const auto& group = unusedNodePlan.protectedGroups[i];
                ImGui::PushID(static_cast<int>(i));
                const string title = (group.possiblyUnused ? "Possibly unused, protected: " : "In use, protected: ")
                    + group.nodes.front() + " (" + ofToString(group.nodes.size()) + " nodes)";
                ImGui::SetNextItemOpen(group.possiblyUnused, ImGuiCond_Appearing);
                if(ImGui::TreeNode(title.c_str())){
                    for(const auto& name : group.nodes) drawNode(name);
                    for(const auto& reason : group.reasons) ImGui::TextWrapped("%s", reason.c_str());
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
        if(unusedNodesPlanChanged) ImGui::TextWrapped("Usage changed. The summary has been refreshed; review it before removing.");
        if(!target) ImGui::TextWrapped("The reviewed canvas is closed. Reopen this window from Edit to review another canvas.");
        if(targetChanged) ImGui::TextWrapped("The canvas changed. Refresh the summary before removing.");
        ImGui::BeginDisabled(!target || targetChanged || nodeCount == 0);
        if(ImGui::Button("Remove", ImVec2(120, 0))){
            if(target->removeUnusedNodes(unusedNodePlan)) showUnusedNodesWindow = false;
            else {
                unusedNodePlan = target->findUnusedNodes();
                unusedNodesSelectedNode.clear();
                unusedNodesPlanChanged = true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!target);
        if(ImGui::Button("Refresh", ImVec2(120, 0))){
            unusedNodePlan = target->findUnusedNodes();
            unusedNodesSelectedNode.clear();
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

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 6.0f));
    if (ImGui::BeginMenuBar())
    {
        auto beginTopLevelMenu = [](const char* label){
            const ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing.x * 1.5f, spacing.y));
            const bool open = ImGui::BeginMenu(label);
            ImGui::PopStyleVar();
            return open;
        };
        controls->drawMenu("Presets");

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
            }
            ImGui::EndMenu();
        }

        controls->drawMenus("Presets");
		
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
    ofxOceanodeShared::setConfigurationFlags(ofxOceanodeShared::getConfigurationFlags() | ofxOceanodeConfigurationFlags_DisableRenderAll);
}
