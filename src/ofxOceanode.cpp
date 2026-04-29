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
    typesRegistry->registerType<float>("f");
    typesRegistry->registerType<int>("i");
    typesRegistry->registerType<bool>("b");
    typesRegistry->registerType<void>("v");
    typesRegistry->registerType<string>("s");
    typesRegistry->registerType<char>("c");
    typesRegistry->registerType<vector<float>>("v_f");
    typesRegistry->registerType<vector<int>>("v_i");
    typesRegistry->registerType<vector<string>>("v_s");
    typesRegistry->registerType<ofColor>("color");
    typesRegistry->registerType<ofFloatColor>("color_f");
    typesRegistry->registerType<Timestamp>("timestamp");
    
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
	//nodeRegistry->registerModel<curve2>("Modifiers");
    nodeRegistry->registerModel<ofxOceanodeNodeMacro>("MACRO");
    nodeRegistry->registerModel<noise>("Generators");
    nodeRegistry->registerModel<randomGenerator>("Generators");

    //Register default Routers
    nodeRegistry->registerModel<router<vector<float>>>("Router", "v_f", 0, 0, 1);
    nodeRegistry->registerModel<router<float>>("Router", "f", 0, 0, 1);
    nodeRegistry->registerModel<router<vector<int>>>("Router", "v_i", 0, 0, 1);
    nodeRegistry->registerModel<router<int>>("Router", "i", 0, 0, 1);
    nodeRegistry->registerModel<router<string>>("Router", "s", "string");
    nodeRegistry->registerModel<router<vector<string>>>("Router", "v_s", "string");
    nodeRegistry->registerModel<routerDropdown>("Router");
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
    OceanodeTheme* oceanodeTheme = new OceanodeTheme();
    gui.setup(oceanodeTheme, false, ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable, false);
	canvas.setupFonts();
    loadDefaultGUILayout();
    // Auto-load default theme if pointer file exists
    {
        std::string defaultThemeFile = ofToDataPath("config/themes/defaultTheme", true);
        if(ofFile(defaultThemeFile).exists()){
            ofJson defaultJson = ofLoadJson(defaultThemeFile);
            std::string themeName = defaultJson.value("theme", "");
            if(!themeName.empty()){
                loadTheme(themeName);
            }
        }
    }

    presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this](){
        string iniPath = ofToDataPath(ofxOceanodeShared::getCurrentPresetPath() + "/ImGuiLayout.ini");
        if(ofFile(iniPath).exists()){
            pendingIniLoad = iniPath;   // defer — will be applied before next NewFrame
            pendingPresetsTabActivation = true; // force Presets tab active after layout restore
            // Cache the content for preservation during future saves
            ofBuffer buf = ofBufferFromFile(iniPath);
            ofxOceanodeShared::getLayoutContentCache()[iniPath] = buf.getText();
        }
        // Set main canvas layout path
        canvas.setLayoutIniPath(ofxOceanodeShared::getCurrentPresetPath() + "/ImGuiLayout.ini");
        ofxOceanodeShared::getActiveCanvasLayoutPath() = iniPath;
        hasActivePreset = true;
    });

    presetSavedListener = ofxOceanodeShared::getPresetWasSavedEvent().newListener([this](){
        string iniPath = ofToDataPath(ofxOceanodeShared::getCurrentPresetPath() + "/ImGuiLayout.ini");
        string& activeLayoutPath = ofxOceanodeShared::getActiveCanvasLayoutPath();
        
        // Only save from ImGui state if the root canvas is active
        // (activeLayoutPath matches the preset's layout path).
        if(activeLayoutPath == iniPath){
            ImGui::SaveIniSettingsToDisk(iniPath.c_str());
            // Update cache
            size_t sz = 0;
            const char* d = ImGui::SaveIniSettingsToMemory(&sz);
            if(d && sz > 0) ofxOceanodeShared::getLayoutContentCache()[iniPath] = string(d, sz);
        }
        // If a macro is active, save ImGui state to the macro's active path instead.
        // IMPORTANT: Read the root layout from disk FIRST — ImGui's in-memory state
        // currently holds the macro layout (loaded when the user focused the macro canvas),
        // and SaveIniSettingsToDisk writes that macro layout wherever we tell it.
        // The root layout file on disk is still correct (container->savePreset() never
        // touches ImGuiLayout.ini), so we snapshot it before any ImGui write, then
        // put it back afterwards to guarantee it isn't lost.
        else if(!activeLayoutPath.empty()){
            // Snapshot the root layout file from disk before any ImGui write
            string rootLayoutContent;
            if(ofFile(iniPath).exists()){
                ofBuffer buf = ofBufferFromFile(iniPath);
                rootLayoutContent = buf.getText();
            }
            
            // Save ImGui state (which is the macro's layout) to the macro's path
            ImGui::SaveIniSettingsToDisk(activeLayoutPath.c_str());
            
            // Restore the root layout from the disk snapshot we took above
            if(!rootLayoutContent.empty()){
                ofBuffer buf;
                buf.set(rootLayoutContent.c_str(), rootLayoutContent.size());
                ofBufferToFile(iniPath, buf);
                // Also update the in-memory cache to keep it in sync
                ofxOceanodeShared::getLayoutContentCache()[iniPath] = rootLayoutContent;
                ofLogNotice("ofxOceanode") << "Preserved root layout at " << iniPath;
            }
        }
        
        canvas.setLayoutIniPath(ofxOceanodeShared::getCurrentPresetPath() + "/ImGuiLayout.ini");
        hasActivePreset = true;
    });

 ofxOceanodeShared::readMacros();
    // Load and apply saved view preferences now that controls is initialized
    // and openFrameworks data path is fully set up
    loadViewConfig();
    if(!savedViewConfig.empty()){
        auto& visibility = controls->getControllersVisibility();
        for(auto& kv : savedViewConfig){
            visibility[kv.first] = kv.second;
        }
    }
    oceanodeTime->setup(container, controls->get<ofxOceanodeBPMController>());
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
    if(!pendingIniLoad.empty()){
        ImGui::LoadIniSettingsFromDisk(pendingIniLoad.c_str());
        pendingIniLoad.clear();
    }
    if(pendingPresetsTabActivation){
        ImGuiWindow* presetsWin = ImGui::FindWindowByName("Presets");
        if(presetsWin && presetsWin->DockNode && presetsWin->DockNode->TabBar){
            presetsWin->DockNode->TabBar->NextSelectedTabId = presetsWin->ID;
        }
        pendingPresetsTabActivation = false;
    }
    // Deferred canvas layout switching (save previous, load new)
    {
        string& pendingSave = ofxOceanodeShared::getPendingLayoutSavePath();
        string& pendingLoad = ofxOceanodeShared::getPendingLayoutLoadPath();
        
        if(!pendingLoad.empty()){
            // Save current ImGui state to the previous canvas's file
            if(!pendingSave.empty()){
                ImGui::SaveIniSettingsToDisk(pendingSave.c_str());
                // Cache the saved content in memory for preservation during preset save
                size_t iniSize = 0;
                const char* iniData = ImGui::SaveIniSettingsToMemory(&iniSize);
                if(iniData && iniSize > 0){
                    ofxOceanodeShared::getLayoutContentCache()[pendingSave] = string(iniData, iniSize);
                }
                pendingSave.clear();
            }
            
            // Load the new canvas's layout
            if(ofFile(pendingLoad).exists()){
                ImGui::LoadIniSettingsFromDisk(pendingLoad.c_str());
            }
            pendingLoad.clear();
        }
    }
    gui.begin();
    // Track active canvas for minimap
    {
        ImGuiContext* ctx = ImGui::GetCurrentContext();
        if(ctx && ctx->NavWindow) {
            ImGuiWindow* root = ctx->NavWindow->RootWindow;
            std::string name = root->Name;
            if(!name.empty() && name[0] == '(') {
                size_t closeParen = name.find(") ");
                if(closeParen != std::string::npos)
                    name = name.substr(closeParen + 2);
            }
            ofxOceanodeShared::setActiveCanvasUniqueID(name);
        }
    }
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
    
    // Draw theme editor window if open
    if(showThemeEditor){
        drawThemeEditorWindow();
    }

    //Make Presets the current active tab on the first frame
    if(firstDraw){
        if(settingsLoaded){
            ImGuiWindow* canvasWin = ImGui::FindWindowByName("Canvas");
            ImGuiWindow* presetsWin = ImGui::FindWindowByName("Presets");
            if(canvasWin && canvasWin->DockNode && presetsWin && presetsWin->DockNode){
                ofxOceanodeShared::setCentralNodeID(canvasWin->DockNode->ID);
                ofxOceanodeShared::setLeftNodeID(presetsWin->DockNode->ID);
            }
        }
        ImGuiDockNode* leftNode = ImGui::DockBuilderGetNode(ofxOceanodeShared::getLeftNodeID());
        ImGuiWindow* presetsWin = ImGui::FindWindowByName("Presets");
        if(leftNode && leftNode->TabBar && presetsWin){
            leftNode->TabBar->NextSelectedTabId = presetsWin->ID;
        }
        firstDraw = false;
    }
    
    gui.end();
    gui.draw();
}

void ofxOceanode::exit(){
    if(hasActivePreset){
        string iniPath = ofToDataPath(ofxOceanodeShared::getCurrentPresetPath() + "/ImGuiLayout.ini");
        string& activeLayout = ofxOceanodeShared::getActiveCanvasLayoutPath();
        
        // Save ImGui state to the currently active canvas layout path.
        // If root is active, activeLayout == iniPath so root gets saved.
        // If a macro is active, save to the macro's path — the root layout
        // on disk is already correct from the last focus-change save.
        if(!activeLayout.empty()){
            ImGui::SaveIniSettingsToDisk(activeLayout.c_str());
        } else {
            // Fallback: no active layout tracked, save to root preset path
            ImGui::SaveIniSettingsToDisk(iniPath.c_str());
        }
    }
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
	
	// make the bacground of the menus 25% darker to get better contrast with the main GUI,
	ImVec4 popupBg = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(popupBg.x * 0.75f, popupBg.y * 0.75f, popupBg.z * 0.75f, popupBg.w));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 6.0f));
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
			ImGui::MenuItem("Metrics", NULL, &show_app_metrics);
            ImGui::EndMenu();
        }
		
		if(ImGui::BeginMenu("View"))
		{
		    auto& controllers = controls->getControllers();
		    auto& visibility = controls->getControllersVisibility();
		    
		    // Build pending state on first open (static local map)
		    static std::map<std::string, bool> pendingVisibility;
		    static bool pendingInitialized = false;
		    if(!pendingInitialized){
		        pendingVisibility = visibility;
		        pendingInitialized = true;
		    }
		    // Sync any new controllers added after init
		    for(auto &c : controllers){
		        const std::string& name = c->getControllerName();
		        if(pendingVisibility.find(name) == pendingVisibility.end()){
		            pendingVisibility[name] = visibility.count(name) ? visibility.at(name) : true;
		        }
		    }
		    
		    // Iterate the sorted map so checkboxes appear alphabetically
		    for(auto& kv : pendingVisibility){
		        ImGui::Checkbox(kv.first.c_str(), &pendingVisibility[kv.first]);
		    }
		    
		    ImGui::Separator();
		    if(ImGui::Button("Apply")){
		        visibility = pendingVisibility;
		    }
		    ImGui::SameLine();
		    if(ImGui::Button("Reset")){
		        for(auto& kv : pendingVisibility){
		            pendingVisibility[kv.first] = true;
		        }
		    }
		    ImGui::SameLine();
		    if(ImGui::Button("Save")){
		        visibility = pendingVisibility;
		        saveViewConfig(pendingVisibility);
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
			
			ImGui::SeparatorText("GUI");
			bool autoInspector = ofxOceanodeShared::getAutoInspectorShowHide();
			if(ImGui::Checkbox("Inspector auto show/hide", &autoInspector)){
				ofxOceanodeShared::setAutoInspectorShowHide(autoInspector);
			}
			bool snap = canvas.getSnapToGrid();
			if(ImGui::Checkbox("Snap To Grid",&snap))
			{
				canvas.setSnapToGrid(snap);
				ofxOceanodeShared::setSnapToGrid(snap);
			}
			
			// Canvas zoom control
			float zoomLevel = canvas.getZoomLevel() * 100.0f;
			if(ImGui::SliderFloat("Canvas Zoom", &zoomLevel, 25.0f, 400.0f, "%.0f%%")) {
				canvas.setZoomLevel(zoomLevel / 100.0f);
			}
			ImGui::SameLine();
			if(ImGui::Button("Reset##zoom")) {
				canvas.setZoomLevel(1.0f);
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
				ofxOceanodeShared::setSnapGridDiv(gd);
			}
			canvas.setNodeWidthText(tw);
			canvas.setNodeWidthWidget(ww);
			
			ImGui::SeparatorText("LAYOUT");
			bool layoutWithCanvas = ofxOceanodeShared::getGuiLayoutChangesWithMacros();
			if(ImGui::Checkbox("GUI layout changes with macros", &layoutWithCanvas)){
				ofxOceanodeShared::getGuiLayoutChangesWithMacros() = layoutWithCanvas;
			}
			
			if(ImGui::BeginMenu("Load GUI layout")){
				string layoutsDir = ofToDataPath("config/guiLayouts/", true);
				ofDirectory dir(layoutsDir);
				dir.allowExt("ini");
				dir.listDir();
				if(dir.size() == 0){
					ImGui::BeginDisabled();
					ImGui::MenuItem("(no layouts saved)");
					ImGui::EndDisabled();
				} else {
					for(int i = 0; i < (int)dir.size(); i++){
						string name = dir.getName(i);
						if(ImGui::MenuItem(name.c_str())){
							pendingIniLoad = dir.getPath(i);
						}
					}
				}
				ImGui::EndMenu();
			}
			
			if(ImGui::MenuItem("Save current GUI layout")){
				memset(saveLayoutNameBuf, 0, sizeof(saveLayoutNameBuf));
				openSaveLayoutPopup = true;
			}
			
			if(ImGui::MenuItem("Set current GUI layout as default")){
				setDefaultLayoutSelected = "";
				openSetDefaultLayoutPopup = true;
			}
			
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

        if(ImGui::BeginMenu("Theme"))
        {
            if(ImGui::MenuItem("Edit Theme", nullptr, &showThemeEditor)){}
            ImGui::Separator();
            if(!currentThemeName.empty()){
                ImGui::TextDisabled("Current: %s", currentThemeName.c_str());
                ImGui::Separator();
            }
            if(ImGui::MenuItem("Save Theme...")){
                memset(themeNameBuf, 0, sizeof(themeNameBuf));
                if(!currentThemeName.empty()){
                    strncpy(themeNameBuf, currentThemeName.c_str(), sizeof(themeNameBuf) - 1);
                }
                openSaveThemePopup = true;
            }
            if(ImGui::BeginMenu("Load Theme...")){
                ofDirectory themeDir(ofToDataPath("config/themes", true));
                themeDir.allowExt("json");
                themeDir.listDir();
                if(themeDir.size() == 0){
                    ImGui::BeginDisabled();
                    ImGui::MenuItem("(no themes saved)");
                    ImGui::EndDisabled();
                } else {
                    for(int i = 0; i < (int)themeDir.size(); i++){
                        std::string name = themeDir.getFile(i).getBaseName();
                        bool isCurrent = (name == currentThemeName);
                        if(isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                        if(ImGui::MenuItem(name.c_str())){
                            loadTheme(name);
                        }
                        if(isCurrent) ImGui::PopStyleColor();
                    }
                }
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Set as Default")){
                ofDirectory dir(ofToDataPath("config/themes", true));
                dir.allowExt("json");
                dir.listDir();

                std::string currentDefault = "";
                ofFile defaultFile(ofToDataPath("config/themes/defaultTheme", true));
                if(defaultFile.exists()){
                    ofJson dj = ofLoadJson(ofToDataPath("config/themes/defaultTheme", true));
                    currentDefault = dj.value("theme", "");
                }

                if(dir.size() == 0){
                    ImGui::TextDisabled("No themes saved yet");
                }
                for(int i = 0; i < (int)dir.size(); i++){
                    std::string name = dir.getFile(i).getBaseName();
                    bool isDefault = (name == currentDefault);
                    if(isDefault) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
                    if(ImGui::MenuItem(name.c_str())){
                        ofDirectory::createDirectory(ofToDataPath("config/themes", true), true, true);
                        ofJson defaultJson;
                        defaultJson["theme"] = name;
                        ofSaveJson(ofToDataPath("config/themes/defaultTheme", true), defaultJson);
                    }
                    if(isDefault) ImGui::PopStyleColor();
                }
                ImGui::EndMenu();
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
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(1); // matches PushStyleColor(ImGuiCol_PopupBg)
    // NOTE: ImGui::End() is intentionally deferred until AFTER all popup
    // OpenPopup/BeginPopupModal calls below, so the DockSpace window is still
    // the active window context when the popups are registered.

    if(showHelp){
        ImGui::OpenPopup("Here are some tips:");
    }
    showHelpPopUp();

    // --- Save GUI Layout Modal ---
    // --- Save Theme Modal ---
    if(openSaveThemePopup){
        ImGui::OpenPopup("Save Theme##modal");
        openSaveThemePopup = false;
    }
    if(ImGui::BeginPopupModal("Save Theme##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::Text("Enter a name for the theme:");
        ImGui::SetNextItemWidth(260);
        ImGui::InputText("##themename", themeNameBuf, sizeof(themeNameBuf));
        ImGui::Spacing();
        bool canSave = (strlen(themeNameBuf) > 0);
        if(!canSave) ImGui::BeginDisabled();
        if(ImGui::Button("Save", ImVec2(120, 0))){
            saveTheme(std::string(themeNameBuf));
            ImGui::CloseCurrentPopup();
        }
        if(!canSave) ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(120, 0))){
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if(openSaveLayoutPopup){
        ImGui::OpenPopup("Save GUI Layout");
        openSaveLayoutPopup = false;
    }
    if(ImGui::BeginPopupModal("Save GUI Layout", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::Text("Enter a name for the layout:");
        ImGui::InputText("##layoutname", saveLayoutNameBuf, sizeof(saveLayoutNameBuf));
        ImGui::Spacing();
        if(ImGui::Button("Save", ImVec2(120, 0))){
            if(strlen(saveLayoutNameBuf) > 0){
                string dirPath = ofToDataPath("config/guiLayouts/", true);
                ofDirectory d(dirPath);
                if(!d.exists()) d.create(true);
                string fullPath = dirPath + string(saveLayoutNameBuf) + ".ini";
                ImGui::SaveIniSettingsToDisk(fullPath.c_str());
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(120, 0))){
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // --- Set Default GUI Layout Modal ---
    if(openSetDefaultLayoutPopup){
        ImGui::OpenPopup("Set Default GUI Layout");
        openSetDefaultLayoutPopup = false;
    }
    if(ImGui::BeginPopupModal("Set Default GUI Layout", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::Text("Choose a layout to use as default at startup:");
        ImGui::Spacing();
        string layoutsDir = ofToDataPath("config/guiLayouts/", true);
        ofDirectory dir(layoutsDir);
        dir.allowExt("ini");
        dir.listDir();
        if(dir.size() == 0){
            ImGui::TextDisabled("(no layouts saved)");
        } else {
            for(int i = 0; i < (int)dir.size(); i++){
                string name = dir.getName(i);
                bool selected = (setDefaultLayoutSelected == name);
                if(ImGui::Selectable(name.c_str(), selected, ImGuiSelectableFlags_DontClosePopups)){
                    setDefaultLayoutSelected = name;
                }
            }
        }
        ImGui::Spacing();
        bool canConfirm = !setDefaultLayoutSelected.empty();
        if(!canConfirm) ImGui::BeginDisabled();
        if(ImGui::Button("Set as Default", ImVec2(140, 0))){
            string configPath = ofToDataPath("config/config.json", true);
            ofJson j;
            ofFile existingCfg(configPath);
            if(existingCfg.exists()) j = ofLoadJson(configPath);
            j["defaultLayout"] = setDefaultLayoutSelected;
            string cfgDir = ofToDataPath("config/", true);
            ofDirectory cfgD(cfgDir);
            if(!cfgD.exists()) cfgD.create(true);
            ofSavePrettyJson(configPath, j);
            ImGui::CloseCurrentPopup();
        }
        if(!canConfirm) ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(120, 0))){
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End(); // End DockSpace window — must come after all popup code above
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
	config["autoInspectorShowHide"] = ofxOceanodeShared::getAutoInspectorShowHide();
	config["guiLayoutChangesWithCanvas"] = ofxOceanodeShared::getGuiLayoutChangesWithMacros();
	
	   // Create config directory if it doesn't exist
	   string configDir = ofToDataPath("config", true);
	   ofDirectory dir(configDir);
	   if(!dir.exists()){
	       dir.create(true);
	   }
	   
	   // Preserve defaultLayout if already set in the existing config
	   string configPath = ofToDataPath("config/config.json", true);
	   ofFile existingFile(configPath);
	   if(existingFile.exists()){
	       ofJson existing = ofLoadJson(configPath);
	       if(existing.contains("defaultLayout")){
	           config["defaultLayout"] = existing["defaultLayout"];
	       }
	   }
	   
	   // Save to file
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
	if(config.contains("snapToGrid")){
		bool b = config["snapToGrid"].get<bool>();
		canvas.setSnapToGrid(b);
		ofxOceanodeShared::setSnapToGrid(b);
	}
	if(config.contains("gridDivisions")){
		int i = config["gridDivisions"].get<int>();
		canvas.setGridDivisions(i);
		canvas.updateGridSize();
		ofxOceanodeShared::setSnapGridDiv(i);
	}
	if(config.contains("autoInspectorShowHide")){
		ofxOceanodeShared::setAutoInspectorShowHide(config["autoInspectorShowHide"].get<bool>());
	}
	if(config.contains("guiLayoutChangesWithMacros")){
		ofxOceanodeShared::getGuiLayoutChangesWithMacros() = config["guiLayoutChangesWithMacros"].get<bool>();
	}
}

void ofxOceanode::saveViewConfig(const std::map<std::string, bool>& visibilityMap){
    ofJson viewConfig;
    for(auto& kv : visibilityMap){
        viewConfig[kv.first] = kv.second;
    }
    
    // Create config directory if it doesn't exist
    string configDir = ofToDataPath("config", true);
    ofDirectory dir(configDir);
    if(!dir.exists()){
        dir.create(true);
    }
    
    string viewConfigPath = ofToDataPath("config/view.json", true);
    ofSavePrettyJson(viewConfigPath, viewConfig);
}

void ofxOceanode::loadViewConfig(){
    string viewConfigPath = ofToDataPath("config/view.json", true);
    
    ofFile viewConfigFile(viewConfigPath);
    if(!viewConfigFile.exists()){
        return; // No saved view config, use defaults
    }
    
    ofJson viewConfig = ofLoadJson(viewConfigPath);
    savedViewConfig.clear();
    for(auto it = viewConfig.begin(); it != viewConfig.end(); ++it){
        savedViewConfig[it.key()] = it.value().get<bool>();
    }
}

void ofxOceanode::loadDefaultGUILayout(){
    string configPath = ofToDataPath("config/config.json", true);
    ofFile f(configPath);
    if(!f.exists()) return;

    ofJson j = ofLoadJson(configPath);
    if(!j.contains("defaultLayout")) return;

    string filename = j["defaultLayout"].get<std::string>();
    if(filename.empty()) return;

    string fullPath = ofToDataPath("config/guiLayouts/" + filename, true);
    ofFile iniFile(fullPath);
    if(!iniFile.exists()) return;

    // Defer to draw() so ImGui context is fully active
    pendingIniLoad = fullPath;
}

void ofxOceanode::saveTheme(const std::string& name){
    ofDirectory::createDirectory(ofToDataPath("config/themes", true), true, true);
    ofJson j;
    ImVec4* colors = ImGui::GetStyle().Colors;
    for(int i = 0; i < ImGuiCol_COUNT; i++){
        string colorName = ImGui::GetStyleColorName(i);
        j["colors"][colorName] = { colors[i].x, colors[i].y, colors[i].z, colors[i].w };
    }
    string path = ofToDataPath("config/themes/" + name + ".json", true);
    ofSavePrettyJson(path, j);
    currentThemeName = name;
    ofLogNotice("ofxOceanode") << "Theme saved to " << path;
}

void ofxOceanode::loadTheme(const std::string& name){
    string path = ofToDataPath("config/themes/" + name + ".json", true);
    ofFile f(path);
    if(!f.exists()){
        ofLogWarning("ofxOceanode") << "Theme file not found: " << path;
        return;
    }
    ofJson j = ofLoadJson(path);
    if(!j.contains("colors")) return;

    ImVec4* colors = ImGui::GetStyle().Colors;
    for(int i = 0; i < ImGuiCol_COUNT; i++){
        string colorName = ImGui::GetStyleColorName(i);
        if(j["colors"].contains(colorName)){
            auto& arr = j["colors"][colorName];
            if(arr.is_array() && arr.size() == 4){
                colors[i] = ImVec4(arr[0].get<float>(), arr[1].get<float>(),
                                   arr[2].get<float>(), arr[3].get<float>());
            }
        }
    }
    currentThemeName = name;
    ofLogNotice("ofxOceanode") << "Theme loaded from " << path;
}

void ofxOceanode::drawThemeEditorWindow(){
    std::string editorTitle = "Theme Editor";
    if(!currentThemeName.empty()) editorTitle += " — " + currentThemeName;
    editorTitle += "###ThemeEditorWindow";

    ImGui::SetNextWindowSize(ImVec2(480, 620), ImGuiCond_FirstUseEver);
    if(ImGui::Begin(editorTitle.c_str(), &showThemeEditor)){
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        if(currentThemeName.empty()){
            ImGui::TextDisabled("No theme loaded");
        } else {
            ImGui::TextDisabled("Current theme: %s", currentThemeName.c_str());
        }
        ImGui::Spacing();

        if(ImGui::Button("Reset to Default")){
            OceanodeTheme defaultTheme;
            defaultTheme.setup();
            currentThemeName = "";
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginChild("##colorscroll", ImVec2(0, 0), false);
        for(int i = 0; i < ImGuiCol_COUNT; i++){
            const char* name = ImGui::GetStyleColorName(i);
            ImGui::PushID(i);
            ImGui::ColorEdit4(name, (float*)&colors[i],
                              ImGuiColorEditFlags_AlphaBar |
                              ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
