//
//  ofxOceanodeControls.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#include "ofxOceanodeControls.h"
#include "ofxOceanodePresetsController.h"
#include "ofxOceanodeTimeController.h"
#include "ofxOceanodeNodesController.h"
#include "ofxOceanodeInspectorController.h"
#include "ofxOceanodeMiniMapController.h"
#include "ofxOceanodeHierarchyController.h"
#include "ofxOceanodeLogController.h"
#include "ofxOceanodeGlobalVariablesController.h"
#include "ofxOceanodeTimelineController.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ofxOceanodeShared.h"

#ifdef OFXOCEANODE_USE_OSC
    #include "ofxOceanodeOSCController.h"
    #include "ofxOceanodeOSCVariablesController.h"
#endif

#ifdef OFXOCEANODE_USE_MIDI
    #include "ofxOceanodeMidiController.h"
#endif


ofxOceanodeControls::ofxOceanodeControls(shared_ptr<ofxOceanodeContainer> _container, ofxOceanodeCanvas* _canvas, ofParameter<int> & _receiverPort)
{
    container = _container;
    controllers.push_back(make_shared<ofxOceanodePresetsController>(container));
    controllers.push_back(make_shared<ofxOceanodeTimeController>(container));
    controllers.push_back(make_shared<ofxOceanodeNodesController>(container,_canvas));
    controllers.push_back(make_shared<ofxOceanodeInspectorController>(container, _canvas));
    controllers.push_back(make_shared<ofxOceanodeMiniMapController>(container, _canvas));
    controllers.push_back(make_shared<ofxOceanodeHierarchyController>(container, _canvas));
    auto logger = make_shared<ofxOceanodeLogController>();
    controllers.push_back(logger);
    controllers.push_back(make_shared<ofxOceanodeGlobalVariablesController>(container));
    controllers.push_back(make_shared<ofxOceanodeTimelineController>(container));
    
    ofSetLoggerChannel(logger);

#ifdef OFXOCEANODE_USE_OSC
    controllers.push_back(make_shared<ofxOceanodeOSCController>(_receiverPort));
    controllers.push_back(make_shared<ofxOceanodeOSCVariablesController>(container));
#endif
    
#ifdef OFXOCEANODE_USE_MIDI
    controllers.push_back(make_shared<ofxOceanodeMidiController>(get<ofxOceanodePresetsController>(), container));
#endif
    
    // Initialize all controllers as visible
    for(auto &c : controllers){
        controllerVisible[c->getControllerName()] = true;
    }
}


void ofxOceanodeControls::draw(){
    // Sync Inspector visibility with node selection state.
    auto inspector = get<ofxOceanodeInspectorController>();
    std::string inspectorName;
    bool activateInspectorTab = false;
    bool focusInspectorWindow = false;
    if(inspector){
        inspectorName = inspector->getControllerName();
        const bool wasVisible = controllerVisible[inspectorName];
        const bool autoShowHide = ofxOceanodeShared::getAutoInspectorShowHide();
        const bool hasSelection = inspector->hasAnySelectedNode();
        bool showInspector = !autoShowHide || hasSelection;
        if(!hasSelection)
            ofxOceanodeShared::consumeInspectorFocusRequest();

        // The canvas processes mouse release later in this frame. Keep its dock
        // size unchanged until that release has been handled, even if a node was
        // selected on mouse down in the previous frame.
        if(autoShowHide && showInspector && !wasVisible &&
           (ImGui::IsMouseDown(0) || ImGui::IsMouseReleased(0) ||
            ImGui::IsMouseDown(1) || ImGui::IsMouseReleased(1))){
            showInspector = false;
        }

        // Restore the tab that was active before the Inspector took focus when
        // selection ends, whether the Inspector will hide or remain open.
        if(!hasSelection && inspectorPreviousTabID != 0){
            ImGuiWindow* window = ImGui::FindWindowByName(inspectorName.c_str());
            ImGuiDockNode* dock = window ? window->DockNode : nullptr;
            if(dock && dock->ID == inspectorPreviousDockID && dock->TabBar &&
               inspectorPreviousTabID != 0 &&
               ImGui::TabBarFindTabByID(dock->TabBar, inspectorPreviousTabID) &&
               (dock->TabBar->SelectedTabId == window->TabId ||
                dock->TabBar->NextSelectedTabId == window->TabId)){
                dock->TabBar->NextSelectedTabId = inspectorPreviousTabID;
            }
            inspectorPreviousTabID = 0;
            inspectorPreviousDockID = 0;
        }

        // A header right-click requests activation even when auto show/hide is
        // disabled. Leave the request pending while a mouse interaction delays
        // the Inspector's appearance.
        focusInspectorWindow = showInspector && ofxOceanodeShared::consumeInspectorFocusRequest();
        activateInspectorTab = showInspector && (focusInspectorWindow || (autoShowHide && !wasVisible));
        if(activateInspectorTab){
            // Capture the active sibling before selecting the Inspector tab.
            ImGuiWindow* window = ImGui::FindWindowByName(inspectorName.c_str());
            ImGuiDockNode* dock = window ? window->DockNode : nullptr;
            if(!dock && window && window->DockId)
                dock = ImGui::DockBuilderGetNode(window->DockId);
            if(!dock && !window)
                dock = ImGui::DockBuilderGetNode(ofxOceanodeShared::getLeftNodeID());
            const ImGuiID selectedTabID = dock && dock->TabBar ? dock->TabBar->SelectedTabId : 0;
            if(selectedTabID && (!window || selectedTabID != window->TabId)){
                inspectorPreviousDockID = dock->ID;
                inspectorPreviousTabID = selectedTabID;
            }else if(!wasVisible){
                inspectorPreviousDockID = 0;
                inspectorPreviousTabID = 0;
            }
        }
        controllerVisible[inspectorName] = showInspector;
    }

    // Handles the "New Timeline Track" name dialog requested from a
    // node's right-click menu on the canvas. Called unconditionally, every
    // frame, regardless of whether the Timeline controller's own window is
    // visible below -- that window can be toggled off, or just not the
    // active docked tab, and the dialog still needs to show up (centered on
    // the main canvas, not wherever the Timeline window is) the moment it's
    // requested rather than whenever the user next happens to bring that
    // window forward.
    if(auto timelineController = get<ofxOceanodeTimelineController>()){
        timelineController->drawPendingTrackPopup();
    }

    for(auto &c : controllers){
        // Determine visibility
        auto it = controllerVisible.find(c->getControllerName());
        bool isVisible = (it == controllerVisible.end()) || it->second;

        // When a controller is hidden, skip Begin/End entirely so the window
        // truly disappears from the tab bar.
        if(!isVisible) continue;

        // Automatic appearance keeps keyboard focus on the canvas. A header
        // right-click explicitly focuses the Inspector, including when floating.
        ImGui::SetNextWindowDockID(ofxOceanodeShared::getLeftNodeID(), ImGuiCond_FirstUseEver);
        if(focusInspectorWindow && c->getControllerName() == inspectorName)
            ImGui::SetNextWindowFocus();
        if(ImGui::Begin(c->getControllerName().c_str(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing)){
            c->draw();
        }
        if(activateInspectorTab && c->getControllerName() == inspectorName){
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if(window->DockNode && window->DockNode->TabBar)
                window->DockNode->TabBar->NextSelectedTabId = window->TabId;
        }
        ImGui::End();
    }
}

void ofxOceanodeControls::update(){
    for(auto &c : controllers){
        c->update();
    }
}
