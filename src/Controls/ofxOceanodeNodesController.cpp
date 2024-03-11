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

ofxOceanodeNodesController::ofxOceanodeNodesController(shared_ptr<ofxOceanodeContainer> _container,
                                                        ofxOceanodeCanvas* _canvas)
                                                        : container(_container),
                                                        canvas(_canvas),
                                                        ofxOceanodeBaseController("Nodes")
{

//     changedBpmListener = container->changedBpmEvent.newListener([this](float newBpm){
//    }
    
//    //Get node registry to update newly registered nodes
//    auto const &models = container->getRegistry()->getRegisteredModels();
//    auto const &categories = container->getRegistry()->getCategories();
//    auto const &categoriesModelsAssociation = container->getRegistry()->getRegisteredModelsCategoryAssociation();
//
//    // buil node categories vector for browsing on them
//    categoriesVector = vector<string>(categories.begin(), categories.end());
//
//    options = vector<vector<string>>(categories.size());
//    for(int i = 0; i < categories.size(); i++){
//        options.push_back(vector<string>());
//        for(auto &model : models){
//            if(categoriesModelsAssociation.at(model.first) == categoriesVector[i]){
//                options[i].push_back(model.first);
//            }
//        }
//        std::sort(options[i].begin(), options[i].end());
//    }
}

void ofxOceanodeNodesController::draw()
{
    // ADD NEW NODES
    
    //Get node registry to update newly registered nodes
    auto const &models = container->getRegistry()->getRegisteredModels();
    auto const &categories = container->getRegistry()->getCategories();
    auto const &categoriesModelsAssociation = container->getRegistry()->getRegisteredModelsCategoryAssociation();

    // build node categories vector for browsing on them
    categoriesVector = vector<string>(categories.begin(), categories.end());
    
    options = vector<vector<string>>(categories.size());
    for(int i = 0; i < categories.size(); i++){
        options.push_back(vector<string>());
        for(auto &model : models){
			if(categoriesModelsAssociation.at(model.first) == categoriesVector[i]){
                options[i].push_back(model.first);
            }
        }
        std::sort(options[i].begin(), options[i].end());
    }
    
    if(ImGui::TreeNode("+ Nodes"))
    {
        ImGui::Separator();
        char * cString = new char[256];
        strcpy(cString, searchField.c_str());
        ImVec2 offset = ImGui::GetCursorScreenPos() + canvas->getScrolling();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-40);
        if(ImGui::InputText("New?", cString,256)){
            searchField = cString;
        }
        ImGui::Separator();
        bool isEnterPressed = ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Enter)); //Select first option if enter is pressed
        bool isEnterReleased = ImGui::IsKeyReleased(ImGui::GetKeyIndex(ImGuiKey_Enter));

        for(int i = 0; i < categoriesVector.size(); i++)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f,0.45f,0.0f,0.5f));
            ImGui::Button("##colorTree",ImVec2(5,0));
            ImGui::PopStyleColor();
            ImGui::SameLine();
            
            if(searchField != "") ImGui::SetNextItemOpen(true);
            if(ImGui::TreeNode(categoriesVector[i].c_str()))
            {
                for(auto &op : options[i])
                {
                    bool showThis = false;
                    if(searchField != ""){
                        string lowercaseName = op;
                        std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), ::tolower);
                        if(ofStringTimesInString(op, searchField) || ofStringTimesInString(lowercaseName, searchField)){
                            showThis = true;
                        }
                    }else{
                        showThis = true;
                    }
                    if(showThis)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f,0.45f,0.0f,0.5f));
                        ImGui::Button("##colorBand",ImVec2(5,0));
                        ImGui::SameLine();
                        ImGui::PopStyleColor();
                        
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(0.65f, 0.65f, 0.65f,1.0f)));
                        
                        if( ImGui::Selectable(op.c_str()) || ( searchField!="" && isEnterReleased) )
                        {
                            if(true)
                            {
                                unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create(op);
                                if (type)
                                {
                                    auto &node = container->createNode(std::move(type));
                                    node.getNodeGui().setPosition(-canvas->getOffsetToCenter());
                                }
                                ImGui::PopStyleColor();
                                ImGui::CloseCurrentPopup();
                                isEnterPressed = false; //Next options we dont want to create them;
                                searchField="";
                                
                                break;
                            }
                        }
                        ImGui::PopStyleColor();
                    }
                }
                ImGui::TreePop();
            }
        }//for
        ImGui::TreePop();
    }// "+" Nodes
    ImGui::Separator();

    // MY NODES LIST
    
    if(ImGui::TreeNode("My Nodes"))
    {
        ImGui::Separator();
        
        vector<ofxOceanodeNode*> allNodes = container->getAllModules();
        char * cString = new char[256];
        strcpy(cString, searchFieldMyNodes.c_str());
        
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-40);
        if(ImGui::InputText("?", cString, 256)){
            searchFieldMyNodes = cString;
        }

        ImGui::Separator();
        
        
        bool isEnterPressed = ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Enter)); //Select first option if enter is pressed
        std::function<void(vector<ofxOceanodeNode*>, ofxOceanodeCanvas*)> listNodes = [this, &isEnterPressed, &listNodes](vector<ofxOceanodeNode*> nodes, ofxOceanodeCanvas* _canvas = nullptr){
            for(auto node : nodes)
            {
                bool showThis = false;
                string macroName = "";
                if(searchFieldMyNodes != ""){
                    string lowercaseName = node->getParameters().getName();
                    std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), ::tolower);
                    if(ofStringTimesInString(node->getParameters().getName(), searchFieldMyNodes) || ofStringTimesInString(lowercaseName, searchFieldMyNodes)){
                        showThis = true;
                    }
                }else{
                    showThis = true;
                }
                if (ofxOceanodeNodeMacro* m = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
                    macroName = (node->getParameters().getName() + " // " + m->getInspectorParameter<string>("Local Name").get());
                    if(ImGui::TreeNode((macroName + "_").c_str())){
                        listNodes(m->getContainer()->getAllModules(), m->getCanvas());
                        ImGui::TreePop();
                    }
                }
                if(showThis)
                {
                    if (macroName != "") {
                        if(ImGui::Selectable(macroName.c_str()) || isEnterPressed)
                        {
                            // get the size of the node to be able to center properly
                            glm::vec2 nodeSize = glm::vec2(node->getNodeGui().getRectangle().getWidth(),node->getNodeGui().getRectangle().getHeight());
                            
                            _canvas->bringOnTop();
                            _canvas->setScrolling(-(_canvas->getOffsetToCenter()-_canvas->getScrolling())-node->getNodeGui().getPosition()-nodeSize/2.0f);
                            ImGui::CloseCurrentPopup();
                            isEnterPressed = false; //Next options we dont want to create them;
                            break;
                        }
                    }
                    else if(ImGui::Selectable(node->getParameters().getName().c_str()) || isEnterPressed)
                    {
                        // get the size of the node to be able to center properly
                        glm::vec2 nodeSize = glm::vec2(node->getNodeGui().getRectangle().getWidth(),node->getNodeGui().getRectangle().getHeight());
                        
                        _canvas->bringOnTop();
                        _canvas->setScrolling(-(_canvas->getOffsetToCenter()-_canvas->getScrolling())-node->getNodeGui().getPosition()-nodeSize/2.0f);
                        ImGui::CloseCurrentPopup();
                        isEnterPressed = false; //Next options we dont want to create them;
                        //break;
                    }
                }
            }
        };
        
        listNodes(allNodes, canvas);

        ImGui::TreePop();
    }
}
#endif
