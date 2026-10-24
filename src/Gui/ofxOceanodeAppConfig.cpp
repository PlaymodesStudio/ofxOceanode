#include "ofxOceanodeAppConfig.h"
#include "ofxOceanodeWindowManager.h"
#include "ofxOceanodeCanvas.h"
#include "ofxOceanodeControls.h"
#include "ofxOceanodeTimeController.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeMenu.h"
#include "ofMain.h"
#include "imgui.h"
#include <cmath>

namespace {
const char* configFile = "config/config.json";
}

void ofxOceanodeAppConfig::setup(ofxOceanodeCanvas& canvasRef, ofxOceanodeControls& controlsRef, ofxOceanodeWindowManager& windowManager){
    canvas = &canvasRef;
    controls = &controlsRef;
    windows = &windowManager;
    load();
}

void ofxOceanodeAppConfig::save(){
    ofJson config;
    config["version"] = 2;
    config["desiredFPS"] = desiredFPS;
    config["vsync"] = vsync;
    config["configurationFlags"] = ofxOceanodeShared::getConfigurationFlags();
    config["showMode"] = windows->isShowMode();
    config["colorTexturePrecision"] = ofxOceanodeShared::getColorTexturePrecision();
    config["nodeTextWidth"] = canvas->getNodeWidthText();
    config["nodeWidgetWidth"] = canvas->getNodeWidthWidget();
    config["gridDivisions"] = canvas->getGridDivisions();
    config["snapToGrid"] = canvas->getSnapToGrid();
    config["autoInspectorShowHide"] = ofxOceanodeShared::getAutoInspectorShowHide();
    config["selectionCentersCanvas"] = ofxOceanodeShared::getSelectionCentersCanvas();
    config["connectionWidth"] = ofxOceanodeShared::getConnectionWidth();
    auto timeController = controls->get<ofxOceanodeTimeController>();
    config["spaceTogglesPlay"] = timeController && timeController->getSpaceTogglesPlay();

    ofDirectory::createDirectory(ofToDataPath("config", true), true, true);
    if(!ofSavePrettyJson(ofToDataPath(configFile, true), config))
        ofLogError("ofxOceanodeAppConfig") << "Could not save " << configFile;
}

void ofxOceanodeAppConfig::load(){
    const auto path = ofToDataPath(configFile, true);
    if(!ofFile(path).exists()) return; // Every target keeps its own default.
    const ofJson config = ofLoadJson(path);
    if(!config.is_object() || config.value("version", 0) != 2){
        ofLogError("ofxOceanodeAppConfig") << "Ignoring " << path << ": unsupported version. Run scripts/migrate_gui_files.py.";
        return;
    }
    if(config.contains("desiredFPS")){
        desiredFPS = config["desiredFPS"].get<int>();
        ofSetFrameRate(desiredFPS);
    }
    if(config.contains("vsync")){
        vsync = config["vsync"].get<bool>();
        ofSetVerticalSync(vsync);
    }
    if(config.contains("configurationFlags"))
        ofxOceanodeShared::setConfigurationFlags(config["configurationFlags"].get<ofxOceanodeConfigurationFlags>());
    if(config.contains("colorTexturePrecision"))
        ofxOceanodeShared::setColorTexturePrecision(config["colorTexturePrecision"].get<int>());
    if(config.contains("showMode")) windows->setShowMode(config["showMode"].get<bool>());
    if(config.contains("nodeTextWidth")) canvas->setNodeWidthText(config["nodeTextWidth"].get<int>());
    if(config.contains("nodeWidgetWidth")) canvas->setNodeWidthWidget(config["nodeWidgetWidth"].get<int>());
    if(config.contains("snapToGrid")){
        const bool snap = config["snapToGrid"].get<bool>();
        canvas->setSnapToGrid(snap);
        ofxOceanodeShared::setSnapToGrid(snap);
    }
    if(config.contains("gridDivisions")){
        const int divisions = config["gridDivisions"].get<int>();
        canvas->setGridDivisions(divisions);
        canvas->updateGridSize();
        ofxOceanodeShared::setSnapGridDiv(divisions);
    }
    if(config.contains("autoInspectorShowHide"))
        ofxOceanodeShared::setAutoInspectorShowHide(config["autoInspectorShowHide"].get<bool>());
    if(config.contains("selectionCentersCanvas"))
        ofxOceanodeShared::setSelectionCentersCanvas(config["selectionCentersCanvas"].get<bool>());
    if(config.contains("connectionWidth"))
        ofxOceanodeShared::setConnectionWidth(config["connectionWidth"].get<float>());
    if(config.contains("spaceTogglesPlay")){
        if(auto timeController = controls->get<ofxOceanodeTimeController>())
            timeController->setSpaceTogglesPlay(config["spaceTogglesPlay"].get<bool>());
    }
}

void ofxOceanodeAppConfig::drawMenuSections(){
    ofxOceanodeMenu::section("Rendering");
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
    
    if(ImGui::Checkbox("V Sync", &vsync)) ofSetVerticalSync(vsync);
    ofxOceanodeConfigurationFlags configurationFlags = ofxOceanodeShared::getConfigurationFlags();
    ImGui::CheckboxFlags("Disable Full Render", &configurationFlags, ofxOceanodeConfigurationFlags_DisableRenderAll);
    ImGui::CheckboxFlags("Disable Histograms", &configurationFlags, ofxOceanodeConfigurationFlags_DisableHistograms);
    ImGui::Checkbox("Show Mode", &windows->showModeRef());
    const int precision = ofxOceanodeShared::getColorTexturePrecision();
    int texturePrecision = precision == 16 ? 1 : precision == 8 ? 2 : 0;
    if(ImGui::Combo("Color texture precision", &texturePrecision, "RGBA32F\0RGBA16F\0RGBA8\0")){
        ofxOceanodeShared::setColorTexturePrecision(texturePrecision == 1 ? 16 : texturePrecision == 2 ? 8 : 32);
    }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Shared by compatible color nodes. Applied on their next render; history is reset.\nOscillators and numerical/index textures keep their precision.\n16F reduces precision; 8-bit clamps to [0,1]. Save Config to keep this choice.");
    
    ofxOceanodeMenu::section("Editor");
    ImGui::CheckboxFlags("Color Connections?", &configurationFlags, ofxOceanodeConfigurationFlags_ColorConnections);
    ImGui::CheckboxFlags("Dashed / Dotted lines", &configurationFlags, ofxOceanodeConfigurationFlags_DashedDottedLines);
    ofxOceanodeShared::setConfigurationFlags(configurationFlags);
    float connectionWidth = ofxOceanodeShared::getConnectionWidth();
    if(ImGui::SliderFloat("Connection Width", &connectionWidth, 1.0f, 8.0f, "%.1f")){
        ofxOceanodeShared::setConnectionWidth(connectionWidth);
    }
    bool autoInspector = ofxOceanodeShared::getAutoInspectorShowHide();
    if(ImGui::Checkbox("Inspector auto show/hide", &autoInspector)){
        ofxOceanodeShared::setAutoInspectorShowHide(autoInspector);
    }
    bool selectionCentersCanvas = ofxOceanodeShared::getSelectionCentersCanvas();
    if(ImGui::Checkbox("Selection centers canvas", &selectionCentersCanvas)){
        ofxOceanodeShared::setSelectionCentersCanvas(selectionCentersCanvas);
    }
    bool snap = canvas->getSnapToGrid();
    if(ImGui::Checkbox("Snap To Grid",&snap))
    {
        canvas->setSnapToGrid(snap);
        ofxOceanodeShared::setSnapToGrid(snap);
    }
    
    // Canvas zoom control
    float zoomLevel = canvas->getZoomLevel() * 100.0f;
    if(ImGui::SliderFloat("Canvas Zoom", &zoomLevel, 25.0f, 400.0f, "%.0f%%")) {
        canvas->setZoomLevel(zoomLevel / 100.0f);
    }
    ImGui::SameLine();
    if(ImGui::Button("Reset##zoom")) {
        canvas->setZoomLevel(1.0f);
    }
    
    int tw = canvas->getNodeWidthText();
    int ww = canvas->getNodeWidthWidget();
    int gd = canvas->getGridDivisions();
    
    if(ImGui::SliderInt("Node Text Width", &tw, 32, 128)) canvas->updateGridSize();
    if(ImGui::SliderInt("Node Widget Width", &ww, 64, 256)) canvas->updateGridSize();
    if(ImGui::SliderInt("Grid Divs",&gd,0,16))
    {
        canvas->setGridDivisions(gd);
        canvas->updateGridSize();
        ofxOceanodeShared::setSnapGridDiv(gd);
    }
    canvas->setNodeWidthText(tw);
    canvas->setNodeWidthWidget(ww);
}

void ofxOceanodeAppConfig::drawConfigButtons(){
    ofxOceanodeMenu::section("Configuration");
    if(ImGui::Button("Save Config")){
        save();
    }
    ImGui::SameLine();
    if(ImGui::Button("Load Config")){
        load();
    }
}
