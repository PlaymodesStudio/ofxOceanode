//
//  ofxOceanode.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/04/2020.
//

#include "ofxOceanode.h"
#include "ofxOceanodeTheme.h"
#include "ofxOceanodeShared.h"
#include "imgui_internal.h"

ofxOceanode::ofxOceanode(){
    nodeRegistry = make_shared<ofxOceanodeNodeRegistry>();
    typesRegistry = make_shared<ofxOceanodeTypesRegistry>();
}


void ofxOceanode::setup(){
    ofSetEscapeQuitsApp(false);
    container = make_shared<ofxOceanodeContainer>(nodeRegistry, typesRegistry);
    canvas.setContainer(container);
    canvas.setup();
    
    controls = make_unique<ofxOceanodeControls>(container);
    //        timelines.emplace_back("Phasor_1/Beats_Div");
    //        timelines.emplace_back("Oscillator_1/Pow");
    //        timelines.emplace_back("Indexer_1/NumWaves");
    //        timelines.emplace_back("Mapper_1/Min_Input");
    gui.setup(nullptr);
    
    StyleColorsOceanode();
}

void ofxOceanode::update(){
    container->update();
}

void ofxOceanode::draw(){
    gui.begin();
    bool showDocker = true;
    ShowExampleAppDockSpace(&showDocker);
    container->draw();
    controls->draw();
    canvas.draw();
    //        for(auto &t : timelines){
    //            t.draw();
    //        }
    
    //Make Presets the current active tab on the first frame
    ImGui::DockBuilderGetNode(ofxOceanodeShared::getLeftNodeID())->TabBar->SelectedTabId = ImGui::FindWindowByName("Presets")->ID;
    
    gui.end();
    gui.draw();
}

void ofxOceanode::ShowExampleAppDockSpace(bool* p_open)
{
    static bool opt_fullscreen_persistant = true;
    bool opt_fullscreen = opt_fullscreen_persistant;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetWorkPos());
        ImGui::SetNextWindowSize(viewport->GetWorkSize());
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
    ImGui::Begin("DockSpace", p_open, window_flags);
    ImGui::PopStyleVar();
    
    if (opt_fullscreen)
        ImGui::PopStyleVar(2);
    
    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        ofxOceanodeShared::setDockspaceID(dockspace_id);
        ImGuiID centralNode_id;
        ImGuiID leftNode_id;
        if(!ImGui::DockBuilderGetNode(dockspace_id)->IsSplitNode()){ //We dont have a split node;
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2, &leftNode_id, &centralNode_id);
        }else{
            centralNode_id = ImGui::DockBuilderGetNode(dockspace_id)->ChildNodes[1]->ID;
            leftNode_id = ImGui::DockBuilderGetNode(dockspace_id)->ChildNodes[0]->ID;
        }
        ofxOceanodeShared::setCentralNodeID(centralNode_id);
        ofxOceanodeShared::setLeftNodeID(leftNode_id);
    }
    else
    {
        //TODO: Show assert, we need docking
        //ShowDockingDisabledMessage();
    }
    static bool showManual = false;
    static bool show_app_metrics = false;
    if (show_app_metrics){ImGui::ShowMetricsWindow(&show_app_metrics);}
    if (showManual) showManualWindow(&showManual);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New")) {}
            if (ImGui::MenuItem("Open")) {}
            if (ImGui::BeginMenu("Open Recent")) {
                if(ImGui::MenuItem("Recent1.oceanode")){}
                if(ImGui::MenuItem("Recent2.oceanode")){}
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
            if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "CTRL+X")) {}
            if (ImGui::MenuItem("Copy", "CTRL+C")) {}
            if (ImGui::MenuItem("Paste", "CTRL+V")) {}
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Windows"))
        {
            ImGui::MenuItem("Metrics", NULL, &show_app_metrics);
            if (ImGui::MenuItem("Show Presets", "CMD+P")) {}
            if (ImGui::MenuItem("Show BPM", "CMD+B")) {}
            if (ImGui::MenuItem("Show Timeline", "CMD+T")) {}
            //Maybe better to use a modal? Need to open windows in layout?
            if (ImGui::BeginMenu("Saved Layouts")){
                //                    ofDirectory dir("Layouts");
                //                    for(auto layout : dir.get)
                //TODO: Get layouts from disk
                //for layout in layouts
                //  if(ImGui::MenuItem(layout)){
                //      LoadIniSettingsFromDisk(layout);
                //  }
                if(ImGui::MenuItem("Dummy Layout 1")){}
                if(ImGui::MenuItem("Dummy Layout 2")){}
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("Show User Manual", "CMD+L", &showManual);
            if(ImGui::Button("Show Help"))
            {
                ImGui::OpenPopup("Help?");
            }
            if(ImGui::BeginPopupModal("Help?", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
                showHelpPopUp();
                ImGui::EndPopup();
            }
            
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    ImGui::End();
}

void ofxOceanode::showManualWindow(bool *b)
{
    ImGui::Begin("Show User Manual");
    ImGui::Button("This is a user manual !");
    ImGui::Text("Framerate is %f", ofGetFrameRate());
//
//    ofImage*  i = new ofImage();
//    i->allocate(270,70,OF_IMAGE_COLOR);
//    i->setUseTexture(true);
//    i->load("./../../../ofxaddons_thumbnail.png");
//    i->setUseTexture(true);
//    i->update();
//
//    if(i->isAllocated())
//    {
//        ImTextureID MtextureID = (ImTextureID*)(intptr_t) i->getTexture().texData.textureID;
//        glm::vec2 size;
//        size.x = 270;
//        size.y = 70;//(tempCast.get()->getWidth() / tempCast.get()->getHeight()) * size.x;
//        ImGui::Image(MtextureID, size, ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255), ImColor(0,0,0,0));
//
//
////        ofTexture* t = &i->getTexture();
////        ImGui::Image(MtextureID,size);
////        ImDrawList* draw_list = ImGui::GetWindowDrawList();
////
////        void ImDrawList::AddImage(ImTextureID user_texture_id, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col)
////        draw_list->PushTextureID(MtextureID);
////        draw_list->AddImage(MtextureID, ImVec2(0,0), ImVec2(100,100),ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255,255));
////        draw_list->PopTextureID();
//    }
 

    ImGui::End();
        
}


void ofxOceanode::showHelpPopUp()
{    
    ImGui::Text("This is very helpful!");
    if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsItemActive()){
        ImGui::CloseCurrentPopup();
    }
}


