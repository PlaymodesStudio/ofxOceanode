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
#include "defaultNodes.h"

ofxOceanode::ofxOceanode(){
    nodeRegistry = make_shared<ofxOceanodeNodeRegistry>();
    typesRegistry = ofxOceanodeTypesRegistry::getInstance();
    scope = ofxOceanodeScope::getInstance();
    oceanodeTime = ofxOceanodeTime::getInstance();
    firstDraw = true;
    settingsLoaded = false;
    showMode = false;
    
#ifdef OFXOCEANODE_USE_OSC
    receiverPortChanged = oscReceiverPort.newListener([this](int &i){
        oscReceiver.setup(oscReceiverPort);
    });
#endif
    
    //Register default types
    typesRegistry->registerType<float>();
    typesRegistry->registerType<int>();
    typesRegistry->registerType<bool>();
    typesRegistry->registerType<void>();
    typesRegistry->registerType<string>();
    typesRegistry->registerType<char>();
    typesRegistry->registerType<vector<float>>();
    typesRegistry->registerType<vector<int>>();
    typesRegistry->registerType<ofColor>();
    typesRegistry->registerType<ofFloatColor>();
    typesRegistry->registerType<Timestamp>();
    
    //Register default nodes
    nodeRegistry->registerModel<oscillator>("Generators");
    nodeRegistry->registerModel<chaoticOscillator>("Generators");
    nodeRegistry->registerModel<phasor>("Generators");
    nodeRegistry->registerModel<simpleNumberGenerator>("Generators");
    nodeRegistry->registerModel<simpleNormalizedNumberGenerator>("Generators");
    nodeRegistry->registerModel<counter>("Generators");
    nodeRegistry->registerModel<ramp>("Generators");
    nodeRegistry->registerModel<mapper>("Modifiers");
    nodeRegistry->registerModel<ranger>("Modifiers");
    nodeRegistry->registerModel<indexer>("Generators");
    nodeRegistry->registerModel<reindexer>("Modifiers");
    nodeRegistry->registerModel<smoother>("Modifiers");
    nodeRegistry->registerModel<switcher>("Modifiers");
    nodeRegistry->registerModel<curve>("Modifiers");
	nodeRegistry->registerModel<curve2>("Modifiers");
    nodeRegistry->registerModel<ofxOceanodeNodeMacro>("MACRO");
    nodeRegistry->registerModel<noise>("Generators");
    nodeRegistry->registerModel<randomGenerator>("Generators");

    //Register default Routers
    nodeRegistry->registerModel<router<vector<float>>>("Router", "v_f", 0, 0, 1);
    nodeRegistry->registerModel<router<float>>("Router", "f", 0, 0, 1);
    nodeRegistry->registerModel<router<vector<int>>>("Router", "v_i", 0, 0, 1);
    nodeRegistry->registerModel<router<int>>("Router", "i", 0, 0, 1);
    nodeRegistry->registerModel<router<string>>("Router", "s", "string");
    nodeRegistry->registerModel<router<bool>>("Router", "b", false);
    nodeRegistry->registerModel<router<void>>("Router", "v");
    nodeRegistry->registerModel<router<char>>("Router", "c", ' ');
    nodeRegistry->registerModel<router<ofColor>>("Router", "color", ofColor::black);
    nodeRegistry->registerModel<router<ofFloatColor>>("Router", "color_f", ofFloatColor::black);
    nodeRegistry->registerModel<router<Timestamp>>("Router", "timestamp", Timestamp());
    
    //Register default Portals
    nodeRegistry->registerModel<portal<vector<float>>>("Portal", "v_f", 0, true);
    nodeRegistry->registerModel<portal<float>>("Portal", "f", 0, true);
    nodeRegistry->registerModel<portal<vector<int>>>("Portal", "v_i", 0, true);
    nodeRegistry->registerModel<portal<int>>("Portal", "i", 0, true);
    nodeRegistry->registerModel<portal<string>>("Portal", "s", "string");
    nodeRegistry->registerModel<portal<bool>>("Portal", "b", false);
    nodeRegistry->registerModel<portal<void>>("Portal", "v");
    nodeRegistry->registerModel<portal<char>>("Portal", "c", ' ');
    nodeRegistry->registerModel<portal<ofColor>>("Portal", "color", ofColor::black);
    nodeRegistry->registerModel<portal<ofFloatColor>>("Portal", "color_f", ofFloatColor::black);
    nodeRegistry->registerModel<portal<Timestamp>>("Router", "timestamp", Timestamp());
    
    //Register default BufferNodes
    nodeRegistry->registerModel<bufferNode<vector<float>>>("Portal", "v_f", 0, true);
    nodeRegistry->registerModel<bufferNode<float>>("Portal", "f", 0, true);
    nodeRegistry->registerModel<bufferNode<vector<int>>>("Portal", "v_i", 0, true);
    nodeRegistry->registerModel<bufferNode<int>>("Portal", "i", 0, true);
    nodeRegistry->registerModel<bufferNode<string>>("Portal", "s", "string");
    nodeRegistry->registerModel<bufferNode<bool>>("Portal", "b", false);
    nodeRegistry->registerModel<bufferNode<char>>("Portal", "c", ' ');
    nodeRegistry->registerModel<bufferNode<ofColor>>("Portal", "color", ofColor::black);
    nodeRegistry->registerModel<bufferNode<ofFloatColor>>("Portal", "color_f", ofFloatColor::black);
    
    //Register defalut BufferHeaders
    nodeRegistry->registerModel<bufferHeader<vector<float>>>("Portal", "v_f", vector<float>(1, 0), false);
    nodeRegistry->registerModel<bufferHeader<float>>("Portal", "f", 0, true);
    nodeRegistry->registerModel<bufferHeader<vector<int>>>("Portal", "v_i", vector<int>(1, 0), false);
    nodeRegistry->registerModel<bufferHeader<int>>("Portal", "i", 0, true);
    nodeRegistry->registerModel<bufferHeader<string>>("Portal", "s", "string");
    nodeRegistry->registerModel<bufferHeader<bool>>("Portal", "b", false);
    nodeRegistry->registerModel<bufferHeader<char>>("Portal", "c", ' ');
    nodeRegistry->registerModel<bufferHeader<ofColor>>("Portal", "color", ofColor::black);
    nodeRegistry->registerModel<bufferHeader<ofFloatColor>>("Portal", "color_f", ofFloatColor::black);
    
    //Register default BufferTpes
    registerType<buffer<float>*>("buffer_f");
    registerType<buffer<int>*>("buffer_i");
    registerType<buffer<bool>*>("buffer_b");
    registerType<buffer<string>*>("buffer_s");
    registerType<buffer<char>*>("buffer_c");
    registerType<buffer<vector<float>>*>("buffer_v_f");
    registerType<buffer<vector<int>>*>("buffer_v_i");
    registerType<buffer<ofColor>*>("buffer_color");
    registerType<buffer<ofFloatColor>*>("buffer_color_f");
}


void ofxOceanode::setup(){
    ofSetEscapeQuitsApp(false);
    container = make_shared<ofxOceanodeContainer>(nodeRegistry, typesRegistry);
    canvas.setContainer(container);
    canvas.setup();
    scope->setup();
    //controls = make_unique<ofxOceanodeControls>(container);
    controls = make_unique<ofxOceanodeControls>(container,&canvas, oscReceiverPort);
    //        timelines.emplace_back("Phasor_1/Beats_Div");
    //        timelines.emplace_back("Oscillator_1/Pow");
    //        timelines.emplace_back("Indexer_1/NumWaves");
    //        timelines.emplace_back("Mapper_1/Min_Input");
    gui.setup(nullptr);
	ofxOceanodeShared::readMacros();
    oceanodeTime->setup(container, controls->get<ofxOceanodeBPMController>());
    StyleColorsOceanode();
}

void ofxOceanode::update(){
#ifdef OFXOCEANODE_USE_OSC
    receiveOsc();
#endif
    oceanodeTime->update();
    container->update();
    controls->update();
}

void ofxOceanode::draw(){
    gui.begin();
    bool showDocker = true;
    ShowExampleAppDockSpace(&showDocker);
    if(showMode){
        drawShowModeWindow();
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
    
    //Make Presets the current active tab on the first frame
    if(firstDraw){
        if(settingsLoaded){
            ofxOceanodeShared::setCentralNodeID(ImGui::FindWindowByName("Canvas")->DockNode->ID);
            ofxOceanodeShared::setLeftNodeID(ImGui::FindWindowByName("Presets")->DockNode->ID);
        }
        ImGui::DockBuilderGetNode(ofxOceanodeShared::getLeftNodeID())->TabBar->NextSelectedTabId = ImGui::FindWindowByName("Presets")->ID;
        firstDraw = false;
    }
    
    gui.end();
    gui.draw();
}

void ofxOceanode::exit(){
    container->clearContainer();
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
        if(!ImGui::DockBuilderGetNode(dockspace_id)->IsSplitNode()){ //We dont have a split node;
            ImGuiID centralNode_id;
            ImGuiID leftNode_id;
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2, &leftNode_id, &centralNode_id);
            ofxOceanodeShared::setCentralNodeID(centralNode_id);
            ofxOceanodeShared::setLeftNodeID(leftNode_id);
        }else{
            settingsLoaded = true;
        }
    }
    else
    {
        //TODO: Show assert, we need docking
        //ShowDockingDisabledMessage();
    }
    static bool showManual = false;
    bool showHelp = false;
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
		
		if(ImGui::BeginMenu("Config"))
		       {
		           // Show actual running FPS (read-only) with color feedback
		           int currentFPS = (int)ofGetFrameRate();
		           
		           // Calculate percentage difference from desired FPS
		           float fpsDiff = abs(currentFPS - desiredFPS) / (float)desiredFPS * 100.0f;
		           
		           // Set colors based on difference
		           ImVec4 sliderColor;
		           if(fpsDiff <= 5.0f) {
		               sliderColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
		           } else if(fpsDiff <= 15.0f) {
		               sliderColor = ImVec4(1.0f, 0.6f, 0.0f, 1.0f); // Orange
		           } else {
		               sliderColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
		           }
		           
		           // Push the color styles for the slider
		           ImGui::PushStyleColor(ImGuiCol_SliderGrab, sliderColor);
		           ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(sliderColor.x*0.3f, sliderColor.y*0.3f, sliderColor.z*0.3f, 1.0f));
		           
		           // Draw the disabled FPS slider
		           ImGui::BeginDisabled();
		           ImGui::SliderInt("FPS", &currentFPS, 0, 240);
		           ImGui::EndDisabled();
		           
		           // Pop the color styles
		           ImGui::PopStyleColor(2);
		           
		           // Editable Desired FPS slider
		           if(ImGui::SliderInt("Desired FPS", &desiredFPS, 1, 240)){
		               ofSetFrameRate(desiredFPS);
		           }
            bool b = false;
            if(ImGui::Checkbox("V Sync", &b)){ofSetVerticalSync(b);};
            ofxOceanodeConfigurationFlags configurationFlags = ofxOceanodeShared::getConfigurationFlags();
            ImGui::CheckboxFlags("Disable Full Render", &configurationFlags, ofxOceanodeConfigurationFlags_DisableRenderAll);
            ImGui::CheckboxFlags("Disable Histograms", &configurationFlags, ofxOceanodeConfigurationFlags_DisableHistograms);
            ImGui::Checkbox("Show Mode", &showMode);
            ofxOceanodeShared::setConfigurationFlags(configurationFlags);
		    bool snap = canvas.getSnapToGrid();
			if(ImGui::Checkbox("Snap To Grid",&snap))
			{
				canvas.setSnapToGrid(snap);
			}
			
			int tw = canvas.getNodeWidthText();
			int ww = canvas.getNodeWidthWidget();
			int gd = canvas.getGridDivisions();
			
			if(ImGui::SliderInt("Node Text Width", &tw, 32, 128)) canvas.updateGridSize();
			if(ImGui::SliderInt("Node Widget Width", &ww, 64, 256)) canvas.updateGridSize();
			if(ImGui::SliderInt("Grid Divs",&gd,0,16))
			{
				canvas.setGridDivisions(gd);
				canvas.updateGridSize();
			}
			canvas.setNodeWidthText(tw);
			canvas.setNodeWidthWidget(ww);
			
			ImGui::Separator();
			
			if(ImGui::Button("Save Config")){
				saveConfig();
			}
			ImGui::SameLine();
			if(ImGui::Button("Load Config")){
				loadConfig();
			}
			
		          ImGui::EndMenu();
        }
		
        if(ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("Show User Manual", "CMD+L", &showManual);
            if(ImGui::MenuItem("Show Help", "CMD+H")){
                showHelp = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::End();
    
    if(showHelp){
        ImGui::OpenPopup("Here are some tips:");
    }
    showHelpPopUp();
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

void ofxOceanode::drawShowModeWindow(){
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

void ofxOceanode::saveConfig(){
    ofJson config;
    
    // Save Desired FPS
    config["desiredFPS"] = desiredFPS;
    
    // Save V Sync (we can't query current state, so we'll save false as default)
    config["vsync"] = false;
    
    // Save configuration flags
    config["configurationFlags"] = ofxOceanodeShared::getConfigurationFlags();
    
    // Save Show Mode
    config["showMode"] = showMode;
    
    // Save canvas settings
    config["nodeTextWidth"] = canvas.getNodeWidthText();
    config["nodeWidgetWidth"] = canvas.getNodeWidthWidget();
    config["gridDivisions"] = canvas.getGridDivisions();
	config["snapToGrid"] = canvas.getSnapToGrid();
	
    // Create config directory if it doesn't exist
    string configDir = ofToDataPath("config", true);
    ofDirectory dir(configDir);
    if(!dir.exists()){
        dir.create(true);
    }
    
    // Save to file
    string configPath = ofToDataPath("config/config.json", true);
    ofSavePrettyJson(configPath, config);
}

void ofxOceanode::loadConfig(){
    string configPath = ofToDataPath("config/config.json", true);
    
    // Check if config file exists
    ofFile configFile(configPath);
    if(!configFile.exists()){
        return; // Use defaults if file doesn't exist
    }
    
    // Load JSON
    ofJson config = ofLoadJson(configPath);
    
    // Load Desired FPS (with backward compatibility for old "fps" key)
    if(config.contains("desiredFPS")){
        desiredFPS = config["desiredFPS"].get<int>();
        ofSetFrameRate(desiredFPS);
    }
    else if(config.contains("fps")){
        // Backward compatibility: load old "fps" key
        desiredFPS = static_cast<int>(config["fps"].get<float>());
        ofSetFrameRate(desiredFPS);
    }
    
    // Load V Sync
    if(config.contains("vsync")){
        ofSetVerticalSync(config["vsync"].get<bool>());
    }
    
    // Load configuration flags
    if(config.contains("configurationFlags")){
        ofxOceanodeShared::setConfigurationFlags(config["configurationFlags"].get<ofxOceanodeConfigurationFlags>());
    }
    
    // Load Show Mode
    if(config.contains("showMode")){
        showMode = config["showMode"].get<bool>();
    }
    
    // Load canvas settings
    if(config.contains("nodeTextWidth")){
        canvas.setNodeWidthText(config["nodeTextWidth"].get<int>());
    }
    
    if(config.contains("nodeWidgetWidth")){
        canvas.setNodeWidthWidget(config["nodeWidgetWidth"].get<int>());
    }
    
    if(config.contains("gridDivisions")){
        canvas.setGridDivisions(config["gridDivisions"].get<int>());
        canvas.updateGridSize();
    }
	if(config.contains("snapToGrid")){
		canvas.setSnapToGrid(config["snapToGrid"].get<bool>());
	}
}
