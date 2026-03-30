//
//  ofxOceanodeNodesController.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/03/2018.
//

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodeNodesController.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeContainer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeCanvas.h"
#include "ofxOceanodeNodeMacro.h"
#include "portal.h"
#include "router.h"

ofxOceanodeNodesController::ofxOceanodeNodesController(shared_ptr<ofxOceanodeContainer> _container,
                                                        ofxOceanodeCanvas* _canvas)
                                                        : container(_container),
                                                        canvas(_canvas),
                                                        ofxOceanodeBaseController("Nodes")
{
    // Whenever a single node is clicked/selected in any canvas, sync the tree
    // view so that node becomes the selected (orange) item and scrolls into view.
    nodeSelectedListener = ofxOceanodeShared::getNodeSelectedInCanvasEvent().newListener(
        [this](ofxOceanodeNode* node){
            if(node != nullptr){
                selectedNode = node;
                scrollTreeToSelected = true;
                forceExpandAll = true;   // ensure parent macros are expanded
            }
        });
}

void ofxOceanodeNodesController::draw()
{
    // If a canvas was focused last frame (deferred via focusPending/onTop flags),
    // it may have stolen ImGui keyboard focus. Count down and then re-focus
    // the Nodes window so arrow-key navigation keeps working.
    if(refocusNodesDelay > 0) {
        refocusNodesDelay--;
        if(refocusNodesDelay == 0) {
            ImGui::SetWindowFocus("Nodes");
        }
    }

    // Rebuild navigable node list each frame
    navigableNodes.clear();

    // Apply deferred scroll — countdown so macro canvases get 2 frames to initialise
    if(scrollPendingFrames > 0) {
        scrollPendingFrames--;
        if(scrollPendingFrames == 0 && pendingScrollNode != nullptr && pendingScrollCanvas != nullptr) {
            glm::vec2 nodeSize = glm::vec2(pendingScrollNode->getNodeGui().getRectangle().getWidth(),
                                           pendingScrollNode->getNodeGui().getRectangle().getHeight());
            glm::vec2 nodePos  = pendingScrollNode->getNodeGui().getPosition();
            glm::vec2 center   = pendingScrollCanvas->getContentRegionSize() / 2.0f;
            pendingScrollCanvas->setScrolling(-nodePos - nodeSize / 2.0f + center);
            pendingScrollNode   = nullptr;
            pendingScrollCanvas = nullptr;
            // Re-activate macro canvas now that it has been drawn/docked at least once,
            // ensuring its tab becomes the visible/focused one on first visit.
            if(pendingScrollMacro != nullptr) {
                pendingScrollMacro->activateWindow();
                pendingScrollMacro = nullptr;
            }
        }
    }

    // MY NODES LIST
    
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if(ImGui::TreeNode("Project Nodes"))
    {
        ImGui::Separator();
        
        vector<ofxOceanodeNode*> allNodes = container->getAllModules();
        char * cString = new char[256];
        strcpy(cString, searchFieldMyNodes.c_str());
        
        if(ImGui::Button("x##clearSearch")) {
            searchFieldMyNodes = "";
            selectedNode = nullptr;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60);
        if(ImGui::InputText("?##searchMyNodes", cString, 256)){
            searchFieldMyNodes = cString;
        }
        ImGui::SameLine();
        if(ImGui::Button("<##collapseAll")) {
            forceExpandAll  = false;
            forceCollapseAll = true;
        }
        ImGui::SameLine();
        if(ImGui::Button(">##expandAll")) {
            forceExpandAll = true;
        }
        ImGui::Text("Filter:");
        ImGui::SameLine();
        ImGui::RadioButton("All",     &nodeTypeFilter, 0); ImGui::SameLine();
        ImGui::RadioButton("Macros",  &nodeTypeFilter, 1); ImGui::SameLine();
        ImGui::RadioButton("Portals", &nodeTypeFilter, 2); ImGui::SameLine();
        ImGui::RadioButton("Routers", &nodeTypeFilter, 3);
        ImGui::Separator();
        
        std::function<int(vector<ofxOceanodeNode*>)> countNodes = [this, &countNodes](vector<ofxOceanodeNode*> nodes) -> int{
            int count = 0;
            for(int i = 0; i < nodes.size(); i++){
                string nodeName = nodes[i]->getParameters().getName();
                if (ofxOceanodeNodeMacro* m = dynamic_cast<ofxOceanodeNodeMacro*>(&nodes[i]->getNodeModel())) {
                    if(m->isLocal()){
                        nodeName = (nodes[i]->getParameters().getName()
									+ " [" + m->getInspectorParameter<string>("Local Name").get()
									+ "]" );
                    }else{
                        nodeName = (nodes[i]->getParameters().getName()
									+ " [" + m->getCurrentMacroName()
									+ "]");
                    }
                    count += countNodes(m->getContainer()->getAllModules());
                }
				else if(abstractPortal* m = dynamic_cast<abstractPortal*>(&nodes[i]->getNodeModel()))
				{
                    nodeName = (nodes[i]->getNodeModel().nodeName()
								+ " " + ofToString(nodes[i]->getNodeModel().getNumIdentifier())
								+ "<" + m->getParameter<string>("Name").get()
								+ ">" );
				}
				else if(abstractRouter* m = dynamic_cast<abstractRouter*>(&nodes[i]->getNodeModel()))
				{
                    nodeName = (nodes[i]->getNodeModel().nodeName()
								+ " " + ofToString(nodes[i]->getNodeModel().getNumIdentifier())
								+ "<" + m->getNameParam().get()
								+ ">" );
				}
                
                if(searchFieldMyNodes != ""){
                    string lowercaseName = nodeName;
                    std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), ::tolower);
                    string lowercaseSearch = searchFieldMyNodes;
                    std::transform(lowercaseSearch.begin(), lowercaseSearch.end(), lowercaseSearch.begin(), ::tolower);
                    if(ofStringTimesInString(nodeName, searchFieldMyNodes) || ofStringTimesInString(lowercaseName, lowercaseSearch)){
                        count++;
                    }
                }else{
                    count++;
                }
            }
            return count;
        };
        
        
        std::function<void(vector<ofxOceanodeNode*>, ofxOceanodeCanvas*, ofxOceanodeNodeMacro*, int, bool)> listNodes = [this, &listNodes, countNodes](vector<ofxOceanodeNode*> nodes, ofxOceanodeCanvas* _canvas, ofxOceanodeNodeMacro* _macro, int depth, bool parentActive){
            vector<int> order(nodes.size());
            vector<string> displayNames(nodes.size());
            std::iota(order.begin(), order.end(), 0);
            for(int i = 0; i < nodes.size(); i++){
                string nodeName = nodes[i]->getParameters().getName();
                if (ofxOceanodeNodeMacro* m = dynamic_cast<ofxOceanodeNodeMacro*>(&nodes[i]->getNodeModel())) {
                    if(m->isLocal()){
                        nodeName = (nodes[i]->getParameters().getName()
									+ " [" + m->getInspectorParameter<string>("Local Name").get()
									+ "]");
                    }else{
                        nodeName = (nodes[i]->getParameters().getName()
									+ " [" + m->getCurrentMacroName()
									+"]");
                    }

                }
				else if(abstractPortal* m = dynamic_cast<abstractPortal*>(&nodes[i]->getNodeModel()))
				{
                    nodeName = (nodes[i]->getNodeModel().nodeName()
								+ " "
								+ ofToString(nodes[i]->getNodeModel().getNumIdentifier())
								+ " [" + m->getParameter<string>("Name").get()
								+ "]" );
                }
				else if(abstractRouter* m = dynamic_cast<abstractRouter*>(&nodes[i]->getNodeModel()))
				{
                    nodeName = (nodes[i]->getNodeModel().nodeName()
								+ " "
								+ ofToString(nodes[i]->getNodeModel().getNumIdentifier())
								+ " [" + m->getNameParam().get()
								+ "]" );
                }
                displayNames[i] = nodeName;
            }

            // Build sort keys: portals sort by Name param + numeric ID; others use display name
            vector<string> sortKeys(nodes.size());
            for(int i = 0; i < (int)nodes.size(); i++) {
                if(abstractPortal* m = dynamic_cast<abstractPortal*>(&nodes[i]->getNodeModel())) {
                    sortKeys[i] = m->getParameter<string>("Name").get() + " " + ofToString(nodes[i]->getNodeModel().getNumIdentifier());
                } else {
                    sortKeys[i] = displayNames[i];
                }
            }
            
            // Natural sort comparator for group-3 (other) nodes
            auto naturalCompare = [](const std::string& a, const std::string& b) -> bool {
                size_t i = 0, j = 0;
                while(i < a.size() && j < b.size()) {
                    if(std::isdigit((unsigned char)a[i]) && std::isdigit((unsigned char)b[j])) {
                        size_t ni = i, nj = j;
                        while(ni < a.size() && std::isdigit((unsigned char)a[ni])) ni++;
                        while(nj < b.size() && std::isdigit((unsigned char)b[nj])) nj++;
                        int numA = std::stoi(a.substr(i, ni - i));
                        int numB = std::stoi(b.substr(j, nj - j));
                        if(numA != numB) return numA < numB;
                        i = ni; j = nj;
                    } else {
                        if(std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[j]))
                            return std::tolower((unsigned char)a[i]) < std::tolower((unsigned char)b[j]);
                        i++; j++;
                    }
                }
                return a.size() < b.size();
            };

            // Group: 0=macro, 1=portal, 2=router, 3=other
            auto groupOf = [&nodes](int idx) -> int {
                if(dynamic_cast<ofxOceanodeNodeMacro*>(&nodes[idx]->getNodeModel()) != nullptr) return 0;
                if(dynamic_cast<abstractPortal*>(&nodes[idx]->getNodeModel()) != nullptr) return 1;
                if(dynamic_cast<abstractRouter*>(&nodes[idx]->getNodeModel()) != nullptr) return 2;
                return 3;
            };

            std::sort(order.begin(), order.end(), [&](const int & a, const int & b) -> bool {
                int ga = groupOf(a), gb = groupOf(b);
                if(ga != gb) return ga < gb;
                if(ga == 1) return naturalCompare(sortKeys[a], sortKeys[b]);  // portals: sort by Name param
                if(ga == 3) return naturalCompare(displayNames[a], displayNames[b]);  // others: natural sort
                return displayNames[a] < displayNames[b];  // macros, routers: alphabetical
            });

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 16.0f);

            constexpr float depthIndent = 12.0f;
            if(depth > 0) ImGui::Indent(depthIndent * depth);

            int rowIndex = 0;
            for(int orderPos = 0; orderPos < (int)order.size(); orderPos++)
            {
                int currentIdx = order[orderPos];
                bool showThis = false;
                bool nameMatches = false;

                string nodeName = displayNames[currentIdx];
                ofxOceanodeNode* node = nodes[currentIdx];

                // --- Name/search condition ---
                bool passesNameFilter = true;  // default: passes if no search active
                if(searchFieldMyNodes != "") {
                    string lowercaseName = nodeName;
                    std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), ::tolower);
                    string lowercaseSearch = searchFieldMyNodes;
                    std::transform(lowercaseSearch.begin(), lowercaseSearch.end(), lowercaseSearch.begin(), ::tolower);

                    passesNameFilter = ofStringTimesInString(nodeName, searchFieldMyNodes)
                                    || ofStringTimesInString(lowercaseName, lowercaseSearch);

                    // Also passes if it's a macro with matching children
                    if(!passesNameFilter) {
                        if(ofxOceanodeNodeMacro* mCheck = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
                            passesNameFilter = countNodes(mCheck->getContainer()->getAllModules()) > 0;
                        }
                    }
                }

                // --- Type condition ---
                bool passesTypeFilter = true;  // default: passes if filter=All
                if(this->nodeTypeFilter != 0) {
                    bool isMacro  = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel()) != nullptr;
                    bool isPortal = dynamic_cast<abstractPortal*>(&node->getNodeModel()) != nullptr;
                    bool isRouter = dynamic_cast<abstractRouter*>(&node->getNodeModel()) != nullptr;

                    switch(this->nodeTypeFilter) {
                        case 1: passesTypeFilter = isMacro;   break;
                        case 2: passesTypeFilter = isPortal;  break;
                        case 3: passesTypeFilter = isRouter;  break;
                    }

                    // Also passes if it's a macro with children matching the type filter
                    if(!passesTypeFilter) {
                        if(ofxOceanodeNodeMacro* mCheck = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
                            auto children = mCheck->getContainer()->getAllModules();
                            for(auto* child : children) {
                                bool childIsMacro  = dynamic_cast<ofxOceanodeNodeMacro*>(&child->getNodeModel()) != nullptr;
                                bool childIsPortal = dynamic_cast<abstractPortal*>(&child->getNodeModel()) != nullptr;
                                bool childIsRouter = dynamic_cast<abstractRouter*>(&child->getNodeModel()) != nullptr;
                                if((nodeTypeFilter==1 && childIsMacro) ||
                                   (nodeTypeFilter==2 && childIsPortal) ||
                                   (nodeTypeFilter==3 && childIsRouter)) {
                                    passesTypeFilter = true;
                                    break;
                                }
                            }
                        }
                    }
                }

                // Both conditions must be true (AND logic)
                showThis = passesNameFilter && passesTypeFilter;

                // nameMatches is true only when the node's own name directly matches the search
                if(searchFieldMyNodes != "" && showThis) {
                    string lowercaseName = nodeName;
                    std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), ::tolower);
                    string lowercaseSearch = searchFieldMyNodes;
                    std::transform(lowercaseSearch.begin(), lowercaseSearch.end(), lowercaseSearch.begin(), ::tolower);
                    nameMatches = ofStringTimesInString(nodeName, searchFieldMyNodes)
                               || ofStringTimesInString(lowercaseName, lowercaseSearch);
                }
                
                if(showThis)
                {
                    if (ofxOceanodeNodeMacro* m = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
                            // Record in navigable list (parent canvas, not macro interior)
                            this->navigableNodes.push_back({node, _canvas, _macro, nameMatches});
    
                            bool effectiveActive = parentActive && m->isActive();
                            if(!effectiveActive) {
                                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                            }

                            // Single tree node for macro: arrow toggles expand/collapse, label click navigates
                            ImGuiTreeNodeFlags macroFlags = ImGuiTreeNodeFlags_OpenOnArrow
                                                          | ImGuiTreeNodeFlags_SpanAvailWidth;
                            ImVec2 swatchPos = ImGui::GetCursorScreenPos();
                        ImGui::Dummy(ImVec2(9.0f, ImGui::GetTextLineHeight()));
                        ImGui::SameLine(0, 0);
                        // When search is active and children match, auto-expand this macro
                        if(searchFieldMyNodes != "" && countNodes(m->getContainer()->getAllModules()) > 0) {
                            ImGui::SetNextItemOpen(true);
                        }
                        if(forceExpandAll) {
                            ImGui::SetNextItemOpen(true);
                        }
                        bool isSelectedMacro = (node == this->selectedNode);
                        if(isSelectedMacro) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.0f, 1.0f));
                        ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
                        bool nodeOpen = ImGui::TreeNodeEx(nodeName.c_str(), macroFlags);
                        ImGui::PopItemFlag();
                        if(isSelectedMacro) ImGui::PopStyleColor();
                        if(this->scrollTreeToSelected && node == this->selectedNode) {
                            ImGui::SetScrollHereY(0.5f);
                            this->scrollTreeToSelected = false;
                        }
        
                        // Zebra stripe background
                        {
                            ImVec2 rowMin = ImVec2(ImGui::GetWindowPos().x, ImGui::GetItemRectMin().y);
                            ImVec2 rowMax = ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetItemRectMax().y);
                            ImU32 rowBg = (rowIndex % 2 == 0)
                                ? IM_COL32(255, 255, 255, 8)
                                : IM_COL32(0, 0, 0, 8);
                            drawList->AddRectFilled(rowMin, rowMax, rowBg);
                        }
        
                        // Yellow search match highlight
                        if(nameMatches) {
                            ImVec2 matchMin = ImVec2(ImGui::GetWindowPos().x, ImGui::GetItemRectMin().y);
                            ImVec2 matchMax = ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetItemRectMax().y);
                            drawList->AddRectFilled(matchMin, matchMax, IM_COL32(255, 220, 0, 45));
                        }

                        // Color swatch for macro node
                        {
                            ofColor c = node->getNodeGui().getColor();
                            ImVec4 nodeCol = ImVec4(c.r/255.0f, c.g/255.0f, c.b/255.0f, 1.0f);
                            ImU32 nodeColorU32 = ImGui::ColorConvertFloat4ToU32(nodeCol);
                            drawList->AddRectFilled(
                                swatchPos,
                                ImVec2(swatchPos.x + 4.0f, swatchPos.y + ImGui::GetTextLineHeight()),
                                nodeColorU32);
                        }
                        rowIndex++;

                        // Double-click on a macro label → open the macro's OWN canvas (enter the macro)
                        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && !ImGui::IsItemToggledOpen()) {
                            this->selectedNode = node;
                            m->activateWindow();  // opens & focuses the macro's interior canvas
                            ofxOceanodeShared::setActiveCanvasUniqueID(m->getCanvas()->getUniqueID());
                            ofxOceanodeShared::nodeSelectedInCanvas(nullptr);  // entering macro interior, no specific node selected
                            refocusNodesDelay = 4;
                            // Cancel any pending scroll that the first click of the double-click may have queued
                            this->pendingScrollNode   = nullptr;
                            this->pendingScrollCanvas = nullptr;
                            this->pendingScrollMacro  = nullptr;
                            this->scrollPendingFrames = 0;
                        }
                        // Single click on macro label (not the arrow, and not part of a double-click) → show parent canvas centered on this macro
                        else if(ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                            this->selectedNode = node;

                            // Select this node in its host canvas (deselect all others first)
                            {
                                auto hostContainer = (_macro != nullptr) ? _macro->getContainer() : this->container;
                                for(auto& pair : hostContainer->getParameterGroupNodesMap())
                                    pair.second->getNodeGui().setSelected(false);
                                node->getNodeGui().setSelected(true);
                            }

                            if(_macro != nullptr){
                                _macro->activateWindow(); // sets showWindow=true and calls canvas.requestFocus()
                                refocusNodesDelay = 4;
                            } else {
                                _canvas->requestFocus();
                                _canvas->bringOnTop();
                                refocusNodesDelay = 2;
                            }
                            ofxOceanodeShared::setActiveCanvasUniqueID(_canvas->getUniqueID());
                            ofxOceanodeShared::nodeSelectedInCanvas(node);
                            // Store for deferred application; macro canvases need 2 frames to initialise
                            this->pendingScrollNode   = node;
                            this->pendingScrollCanvas = _canvas;
                            this->pendingScrollMacro  = _macro;
                            this->scrollPendingFrames = (_macro != nullptr) ? 2 : 1;
                        }

                        if(nodeOpen) {
                            if(countNodes(m->getContainer()->getAllModules()) > 0){
                                listNodes(m->getContainer()->getAllModules(), m->getCanvas(), m, depth + 1, effectiveActive);
                            }
                            ImGui::TreePop();
                        }
                        if(!effectiveActive) {
                            ImGui::PopStyleVar();
                        }
                    } else {
                        // Record in navigable list
                        this->navigableNodes.push_back({node, _canvas, _macro, nameMatches});

                        if(!parentActive) {
                            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                        }

                        // Text color: orange if selected, dim otherwise
                        bool isSelectedLeaf = (node == this->selectedNode);
                        ImVec4 leafTextColor;
                        if(isSelectedLeaf) {
                            leafTextColor = ImVec4(1.0f, 0.55f, 0.0f, 1.0f);
                        } else {
                            leafTextColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                            leafTextColor.x *= 0.75f;
                            leafTextColor.y *= 0.75f;
                            leafTextColor.z *= 0.75f;
                        }
                        ImGui::PushStyleColor(ImGuiCol_Text, leafTextColor);

                        ImVec2 swatchPos = ImGui::GetCursorScreenPos();
                        ImGui::Dummy(ImVec2(9.0f, ImGui::GetTextLineHeight()));
                        ImGui::SameLine(0, 0);

                        ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
                        bool selected = ImGui::Selectable(nodeName.c_str(), false, ImGuiSelectableFlags_SpanAvailWidth);
                        ImGui::PopItemFlag();
                        if(this->scrollTreeToSelected && node == this->selectedNode) {
                            ImGui::SetScrollHereY(0.5f);
                            this->scrollTreeToSelected = false;
                        }

                        ImGui::PopStyleColor();

                        if(!parentActive) {
                            ImGui::PopStyleVar();
                        }

                        // Zebra stripe background
                        {
                            ImVec2 rowMin = ImVec2(ImGui::GetWindowPos().x, ImGui::GetItemRectMin().y);
                            ImVec2 rowMax = ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetItemRectMax().y);
                            ImU32 rowBg = (rowIndex % 2 == 0)
                                ? IM_COL32(255, 255, 255, 8)
                                : IM_COL32(0, 0, 0, 8);
                            drawList->AddRectFilled(rowMin, rowMax, rowBg);
                        }

                        // Yellow search match highlight
                        if(nameMatches) {
                            ImVec2 matchMin = ImVec2(ImGui::GetWindowPos().x, ImGui::GetItemRectMin().y);
                            ImVec2 matchMax = ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetItemRectMax().y);
                            drawList->AddRectFilled(matchMin, matchMax, IM_COL32(255, 220, 0, 45));
                        }

                        // Color swatch for leaf node
                        {
                            ofColor c = node->getNodeGui().getColor();
                            ImVec4 nodeCol = ImVec4(c.r/255.0f, c.g/255.0f, c.b/255.0f, 1.0f);
                            ImU32 nodeColorU32 = ImGui::ColorConvertFloat4ToU32(nodeCol);
                            drawList->AddRectFilled(
                                swatchPos,
                                ImVec2(swatchPos.x + 4.0f, swatchPos.y + ImGui::GetTextLineHeight()),
                                nodeColorU32);
                        }
                        rowIndex++;

                        if(selected)
                        {
                            this->selectedNode = node;

                            // Select this node in its host canvas (deselect all others first)
                            {
                                auto hostContainer = (_macro != nullptr) ? _macro->getContainer() : this->container;
                                for(auto& pair : hostContainer->getParameterGroupNodesMap())
                                    pair.second->getNodeGui().setSelected(false);
                                node->getNodeGui().setSelected(true);
                            }

                            if(_macro != nullptr){
                                _macro->activateWindow(); // sets showWindow=true and calls canvas.requestFocus()
                                // Macro windows may have an extra isFirstDraw frame before
                                // focusPending fires, so give an extra frame of margin.
                                refocusNodesDelay = 4;
                            } else {
                                _canvas->requestFocus();
                                _canvas->bringOnTop();
                                refocusNodesDelay = 2;
                            }
                            ofxOceanodeShared::setActiveCanvasUniqueID(_canvas->getUniqueID());
                            ofxOceanodeShared::nodeSelectedInCanvas(node);
                            // Store for deferred application; macro canvases need 2 frames to initialise
                            this->pendingScrollNode   = node;
                            this->pendingScrollCanvas = _canvas;
                            this->pendingScrollMacro  = _macro;
                            this->scrollPendingFrames = (_macro != nullptr) ? 2 : 1;
                        }
                    }
                }
            }
            if(depth > 0) ImGui::Unindent(depthIndent * depth);
            ImGui::PopStyleVar(); // IndentSpacing — always called exactly once
        };
        
        // Items use ImGuiItemFlags_NoNav so clicks never assign NavId and therefore
        // never enqueue ScrollToBringRectVisibleInWindow.  Arrow-key nav is handled
        // manually via scrollTreeToSelected + SetScrollHereY(0.5f) inside listNodes.
        ImGui::BeginChild("##nodesListChild", ImVec2(0, 0), false, ImGuiWindowFlags_NoNav);
        if(forceCollapseAll) {
            ImGui::GetStateStorage()->Clear();
        }
        listNodes(allNodes, canvas, nullptr, 0, true);
        ImGui::EndChild();

        // Arrow key navigation — only when this window (or its child) has focus
        bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        bool upPressed   = windowFocused && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow),   true);
        bool downPressed = windowFocused && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow), true);

        if((upPressed || downPressed) && !navigableNodes.empty()) {
            // Determine the candidate pool
            // If search is active AND there are matching nodes, iterate only matches
            // Otherwise iterate all visible nodes
            vector<NavigableNode*> pool;
            if(searchFieldMyNodes != "") {
                for(auto& n : navigableNodes) {
                    if(n.matchesSearch) pool.push_back(&n);
                }
            }
            if(pool.empty()) {
                // No search filter active, or search produced no matches — iterate all
                for(auto& n : navigableNodes) pool.push_back(&n);
            }

            if(!pool.empty()) {
                // Find current selectedNode in pool
                int currentIdx = -1;
                for(int i = 0; i < (int)pool.size(); i++) {
                    if(pool[i]->node == selectedNode) { currentIdx = i; break; }
                }

                int nextIdx = currentIdx;
                if(currentIdx != -1) {
                    if(downPressed) nextIdx = (currentIdx + 1) % (int)pool.size();
                    if(upPressed)   nextIdx = (currentIdx - 1 + (int)pool.size()) % (int)pool.size();
                }

                if(nextIdx != -1 && nextIdx != currentIdx && currentIdx != -1) {
                    NavigableNode& target = *pool[nextIdx];
                    selectedNode = target.node;

                    // Select this node in its host canvas (deselect all others first),
                    // mirroring the mouse-click behaviour.
                    {
                        auto hostContainer = (target.macro != nullptr) ? target.macro->getContainer() : this->container;
                        for(auto& pair : hostContainer->getParameterGroupNodesMap())
                            pair.second->getNodeGui().setSelected(false);
                        target.node->getNodeGui().setSelected(true);
                    }

                    // Trigger focus + deferred scroll (same as click)
                    if(target.macro != nullptr) {
                        target.macro->activateWindow();
                    } else {
                        target.canvas->requestFocus();
                        target.canvas->bringOnTop();
                    }
                    ofxOceanodeShared::setActiveCanvasUniqueID(target.canvas->getUniqueID());
                    ofxOceanodeShared::nodeSelectedInCanvas(target.node);
                    pendingScrollNode   = target.node;
                    pendingScrollCanvas = target.canvas;
                    pendingScrollMacro  = target.macro;
                    scrollPendingFrames = (target.macro != nullptr) ? 2 : 1;
                    this->scrollTreeToSelected = true;

                    // Re-focus Nodes window after the canvas has consumed its
                    // deferred SetNextWindowFocus (takes 2 frames).
                    refocusNodesDelay = (target.macro != nullptr) ? 4 : 2;
                }
            }
        }

        // Note: scrollTreeToSelected is NOT reset here so it can survive to the next
        // frame where listNodes will read it and call SetScrollHereY(0.5f).
        // listNodes itself clears the flag once it has scrolled to the selected item.
        // If the selected node is filtered out and listNodes never sees it, the flag
        // remains true and the restore is suppressed (neutral scroll — acceptable).

        ImGui::TreePop();
    }
    forceExpandAll   = false;
    forceCollapseAll = false;
}
#endif
