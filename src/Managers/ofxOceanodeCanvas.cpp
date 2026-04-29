//
//  ofxOceanode.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#ifndef OFXOCEANODE_HEADLESS

#include "ofxOceanodeCanvas.h"
#include <cmath>
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNode.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <unordered_set>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_opengl2.h"
#include "ofGLUtils.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeParameter.h"
#include "ofAppGLFWWindow.h"

// Static member definition – shared across all canvas instances (ImGui uses a single font atlas)
ImFont* ofxOceanodeCanvas::zoomFonts[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
ImFont* ofxOceanodeCanvas::zoomFontsBold[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

int ofxOceanodeCanvas::getCurrentFontIndex() const {
    float desiredScreenSize = ZOOM_FONT_SIZES[2] * zoomLevel;  // index 2 = default (18px)
    int bestIdx = 2;
    float bestDist = FLT_MAX;
    for(int i = 0; i < 5; i++) {
        float dist = fabsf(ZOOM_FONT_SIZES[i] - desiredScreenSize);
        if(dist < bestDist) { bestDist = dist; bestIdx = i; }
    }
    return bestIdx;
}

int ofxOceanodeCanvas::getCeilingFontIndex() const {
    float targetSize = ZOOM_FONT_SIZES[2] * zoomLevel; // 14 * zoom
    // Find smallest loaded font whose nominal size >= targetSize (scale-down only)
    for(int i = 0; i < 5; i++){
        if(ZOOM_FONT_SIZES[i] >= targetSize)
            return i;
    }
    return 4; // fallback: largest font
}

bool ofxOceanodeCanvas::shouldRenderText() const {
    // Hide text exactly when the smallest pre-cached font (8pt) would exceed
    // the zoom-scaled target frame height (BASE_FRAME_HEIGHT = 14.0 + 2*1.0 = 16.0).
    // This matches the FramePadding compensation logic: below this zoom level,
    // targetPaddingY goes negative and row heights can no longer be maintained proportionally.
    static constexpr float BASE_FRAME_HEIGHT = ZOOM_FONT_SIZES[2] + 2.0f; // 16.0f
    return ZOOM_FONT_SIZES[0] < BASE_FRAME_HEIGHT * zoomLevel;
}

ImFont* ofxOceanodeCanvas::getZoomFont() const {
    int idx = getCurrentFontIndex();
    if(zoomFonts[idx] != nullptr) return zoomFonts[idx];
    return nullptr; // fallback to current font
}

void ofxOceanodeCanvas::setupFonts() {
    ImGuiIO& io = ImGui::GetIO();
    std::string fontPath = ofToDataPath("config/font/JetBrainsMono-2.304/fonts/ttf/JetBrainsMono-Medium.ttf", true);

    // Detect Retina / HiDPI pixel density.
    // Strategy 1: ask OpenGL for the actual framebuffer width and compare to the
    // logical window width.  This is reliable at any point after window creation
    // because it queries the real framebuffer rather than a cached OF value.
    // Strategy 2: fall back to ofAppGLFWWindow::getPixelScreenCoordScale().
    float scale = 1.0f;

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    float fbWidth = (float)viewport[2];
    float winWidth = (float)ofGetWindowWidth();
    if(winWidth > 0.0f && fbWidth > winWidth) {
        scale = fbWidth / winWidth;
    } else {
        // Fallback: ask GLFW window directly
        ofAppGLFWWindow* glfwWindow = dynamic_cast<ofAppGLFWWindow*>(ofGetWindowPtr());
        if(glfwWindow) {
            scale = (float)glfwWindow->getPixelScreenCoordScale();
        }
    }
    ofLogNotice("ofxOceanodeCanvas") << "Font pixel scale: " << scale
                                     << " (fbWidth=" << fbWidth << ", winWidth=" << winWidth << ")";

    // Load fonts at LOGICAL pixel size — do NOT multiply by scale and do NOT set
    // FontGlobalScale.  Loading at physical size (×scale) then compensating with
    // FontGlobalScale=1/scale caused ImFont::FontSize (stored at atlas size) to
    // diverge from g.FontSize (layout size after the global scale multiplier).
    // That divergence trips an assertion inside InputTextEx at line 4785 when
    // ImGui does cursor/click-position math on a multiline text widget.
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 2;   // 2 is sufficient at 2× physical — saves atlas memory
    fontCfg.OversampleV = 2;   // increase from default 1 for vertical sharpness

    ofFile fontFile(fontPath);
    if(!fontFile.exists()) {
        ofLogWarning("ofxOceanodeCanvas") << "JetBrainsMono font not found at: " << fontPath;
        ofLogWarning("ofxOceanodeCanvas") << "Using default ImGui font for zoom system";
        for(int i = 0; i < 5; i++) {
            zoomFonts[i] = io.Fonts->AddFontDefault();
        }
    } else {
        // Add default (index 2) FIRST — ImGui uses the first added font as default
        // This ensures the main UI font is 18px, not 10px
        zoomFonts[2] = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), ZOOM_FONT_SIZES[2], &fontCfg);

        // Add the remaining fonts
        zoomFonts[0] = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), ZOOM_FONT_SIZES[0], &fontCfg);
        zoomFonts[1] = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), ZOOM_FONT_SIZES[1], &fontCfg);
        zoomFonts[3] = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), ZOOM_FONT_SIZES[3], &fontCfg);
        zoomFonts[4] = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), ZOOM_FONT_SIZES[4], &fontCfg);

        // Validate and fall back any that failed
        for(int i = 0; i < 5; i++) {
            if(!zoomFonts[i]) {
                ofLogWarning("ofxOceanodeCanvas") << "Failed to load font size " << ZOOM_FONT_SIZES[i];
                zoomFonts[i] = io.Fonts->AddFontDefault();
            }
        }
    }

    // ── Bold fonts (ExtraBold) for node titles ──────────────────────────
    std::string boldFontPath = ofToDataPath("config/font/JetBrainsMono-2.304/fonts/ttf/JetBrainsMono-ExtraBold.ttf", true);
    ofFile boldFontFile(boldFontPath);
    if(!boldFontFile.exists()) {
        ofLogWarning("ofxOceanodeCanvas") << "JetBrainsMono ExtraBold font not found at: " << boldFontPath;
        ofLogWarning("ofxOceanodeCanvas") << "Bold node titles will fall back to regular font";
        for(int i = 0; i < 5; i++) {
            zoomFontsBold[i] = nullptr; // nullptr signals "no bold available"
        }
    } else {
        ImFontConfig boldCfg;
        boldCfg.OversampleH = 2;
        boldCfg.OversampleV = 2;
        boldCfg.MergeMode   = false;

        for(int i = 0; i < 5; i++) {
            zoomFontsBold[i] = io.Fonts->AddFontFromFileTTF(boldFontPath.c_str(), ZOOM_FONT_SIZES[i], &boldCfg);
            if(!zoomFontsBold[i]) {
                ofLogWarning("ofxOceanodeCanvas") << "Failed to load bold font size " << ZOOM_FONT_SIZES[i];
            }
        }
    }

    // Rebuild the font atlas after adding fonts (called after gui.setup() created the context)
    io.Fonts->Build();

    // ── DIAGNOSTIC: Font atlas state after build ───────────────────────────
    ofLogNotice("ofxOceanodeCanvas::setupFonts") << "--- Font Atlas Diagnostic ---";
    ofLogNotice("ofxOceanodeCanvas::setupFonts") << "Total fonts in atlas: " << io.Fonts->Fonts.Size;
    for(int _di = 0; _di < io.Fonts->Fonts.Size; _di++) {
        ImFont* _f = io.Fonts->Fonts[_di];
        bool isZoom2 = (_f == zoomFonts[2]);
        ofLogNotice("ofxOceanodeCanvas::setupFonts")
            << "  Fonts[" << _di << "] @ " << (void*)_f
            << (isZoom2 ? "  <-- zoomFonts[2] (JetBrainsMono 18px)" : "");
    }
    ofLogNotice("ofxOceanodeCanvas::setupFonts")
        << "io.FontDefault (before fix) = " << (void*)io.FontDefault
        << (io.FontDefault == nullptr ? "  (NULL => was using Fonts[0])" : "");
    ofLogNotice("ofxOceanodeCanvas::setupFonts")
        << "zoomFonts[2]   = " << (void*)zoomFonts[2];
    if(io.Fonts->Fonts.Size > 0) {
        bool defaultIsZoom2 = (io.Fonts->Fonts[0] == zoomFonts[2]);
        ofLogNotice("ofxOceanodeCanvas::setupFonts")
            << "Fonts[0] == zoomFonts[2]: " << (defaultIsZoom2 ? "YES (atlas order correct)" : "NO  (built-in font is Fonts[0]; io.FontDefault fix is required)");
    }
    // ── END DIAGNOSTIC ─────────────────────────────────────────────────────

    // ── FIX: Explicitly set the global default font to JetBrainsMono-Medium 18px.
    // Without this, ImGui falls back to Fonts->Fonts[0].  Because ofxImGui's
    // gui.setup() calls AddFontDefault() before setupFonts() runs, Fonts[0] is
    // the built-in ImGui font — not JetBrainsMono — so every window/menu/widget
    // that does NOT call PushFont() renders with the wrong font.
    if(zoomFonts[2]) {
        io.FontDefault = zoomFonts[2];
        ofLogNotice("ofxOceanodeCanvas::setupFonts")
            << "io.FontDefault set to zoomFonts[2] (JetBrainsMono-Medium 18px)";
    }

    // Publish the base (zoom=1.0) font size so that render-time code in other
    // translation units (e.g. macro GUI) can compute the current zoom factor via
    // ImGui::GetFontSize() / ofxOceanodeShared::getZoomBaseFontSize() without
    // hard-coding the canvas-internal ZOOM_FONT_SIZES constant.
    ofxOceanodeShared::setZoomBaseFontSize(ZOOM_FONT_SIZES[2]);

    // Publish the base frame height so NodeGui and other classes can derive the
    // correct row height without duplicating this formula.
    {
        static constexpr float BASE_FRAME_PADDING_Y = 1.0f;
        static constexpr float BASE_FRAME_HEIGHT = ZOOM_FONT_SIZES[2] + 2.0f * BASE_FRAME_PADDING_Y;
        ofxOceanodeShared::setBaseFrameHeight(BASE_FRAME_HEIGHT);
    }

    // FontGlobalScale is intentionally left at its default (1.0).  Fonts are now
    // loaded at logical pixel size so no global scaling compensation is needed.
    // Setting FontGlobalScale < 1 caused ImFont::FontSize (atlas size) to diverge
    // from g.FontSize (layout size), which triggered an assertion in InputTextEx.
    io.FontGlobalScale = 1.0f;

    // Recreate the GPU font texture for whichever renderer is active
    if(ofIsGLProgrammableRenderer()) {
        ImGui_ImplOpenGL3_DestroyDeviceObjects();
        ImGui_ImplOpenGL3_CreateDeviceObjects();
    } else {
        ImGui_ImplOpenGL2_DestroyDeviceObjects();
        ImGui_ImplOpenGL2_CreateDeviceObjects();
    }
}

void ofxOceanodeCanvas::setup(string _uid, string _pid){
    transformationMatrix = &container->getTransformationMatrix();
    
    selectedRect = ofRectangle(0, 0, 0, 0);
    isSelecting = false;
    
    parentID = _pid;
    if(parentID != "Canvas" && parentID != ""){ //It is not canvas, and it is not the first branch of macros
        uniqueID = parentID + " / " + _uid; //Apend Parent to name;
    }else{
        uniqueID = _uid;
    }
    container->setCanvasID(uniqueID);
    
    scrolling = glm::vec2(0,0);
    moveSelectedModulesWithDrag = glm::vec2(0,0);
	
	NODE_WINDOW_PADDING = glm::vec2(8.0f,7.0f);
	gridDivisions=4;
	updateGridSize();

}

void ofxOceanodeCanvas::draw(bool *open, ofColor color, string title){
    // Save the previous shared zoom level so nested canvas draws (e.g. macros)
    // restore the parent's value when they finish.
    float previousSharedZoom = ofxOceanodeShared::getZoomLevel();

    //Draw Guis
    if(onTop){
        ImGui::SetNextWindowFocus();
        onTop = false;
    }
    // focusPending is set by requestFocus(). We defer the actual focus call
    // until isFirstDraw is false, because the isFirstDraw path below calls
    // ImGui::FocusWindow(parentID) which would steal focus away again.
    if(focusPending && !isFirstDraw){
        ImGui::SetNextWindowFocus();
        focusPending = false;
    }
    // Draw a list of nodes on the left side
    bool open_context_menu = false;
    string node_hovered_in_list = "";
    string node_hovered_in_scene = "";
    
    bool isAnyNodeHovered = false;
    bool connectionIsDoable = false;
    
    ImGui::SetNextWindowDockID(ofxOceanodeShared::getDockspaceID(), ImGuiCond_FirstUseEver);
    string windowName;
    if(title != "") {
        windowName = "(" + title + ") " + uniqueID + "###" + uniqueID;
    } else {
        windowName = uniqueID + "###" + uniqueID;
    }
    if(ImGui::Begin(windowName.c_str(), open)){
        // Detect canvas tab activation — consistent check for both active-canvas-ID
        // and layout switching. Uses dock tab selection when available to avoid
        // the RootAndChildWindows false-positive with shared dock hosts.
        bool isActiveTab = false;
        ImGuiWindow* win = ImGui::GetCurrentWindow();
        if(win->DockNode && win->DockNode->TabBar){
            // Check if this window's tab is the selected one in the dock node
            isActiveTab = (win->DockNode->TabBar->SelectedTabId == win->TabId);
        } else {
            // Not docked as a tab — fall back to plain window focus
            isActiveTab = ImGui::IsWindowFocused(ImGuiFocusedFlags_None);
        }
        
        // Track which canvas is active for MiniMap/Hierarchy coordination
        if(isActiveTab){
            ofxOceanodeShared::setActiveCanvasUniqueID(uniqueID);
        }
        
        if(isActiveTab && !wasFocusedLastFrame && !layoutIniPath.empty()){
            if(ofxOceanodeShared::getGuiLayoutChangesWithMacros()
               && ofxOceanodeShared::getLayoutSwitchSuppressFrames() <= 0){
                string newIniPath = ofToDataPath(layoutIniPath);
                string& activeLayoutPath = ofxOceanodeShared::getActiveCanvasLayoutPath();
                
                // Only switch if we're actually changing to a different layout
                if(newIniPath != activeLayoutPath){
                    // Queue: save current layout to previous canvas, load this canvas's layout
                    ofxOceanodeShared::getPendingLayoutSavePath() = activeLayoutPath;
                    ofxOceanodeShared::getPendingLayoutLoadPath() = newIniPath;
                    // Update active path immediately so subsequent focus changes in the same frame don't re-trigger
                    activeLayoutPath = newIniPath;
                }
            }
        }
        wasFocusedLastFrame = isActiveTab;
        ImGui::SameLine();
        ImGui::BeginGroup();
                
        // Create our child canvas
        offsetToCenter = glm::vec2(
        	int(scrolling.x - (ImGui::GetContentRegionAvail().x / (2.0f * zoomLevel))),
        	int(scrolling.y - (ImGui::GetContentRegionAvail().y / (2.0f * zoomLevel))) + 8);
        
        // CANVAS POSITION
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1.0,1.0,1.0,0.5));
        ImGui::Text("[%d,%d]",int(scrolling.x - (ImGui::GetContentRegionAvail().x / (2.0f * zoomLevel))), int( scrolling.y - (ImGui::GetContentRegionAvail().y / (2.0f * zoomLevel)))+8);
        ImGui::SameLine();
		ImGui::PopStyleColor();
		
		// FPS
        float fps = ofGetFrameRate();
        if(fps>=60.0) ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.0,1.0,0.0,0.5));
        else if(fps>30.0) ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1.0,0.5,0.0,0.5));
        else ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1.0,0.0,0.0,0.5));
        ImGui::Text("%d fps",int(fps));
        ImGui::PopStyleColor();
        
		// [S]NAP
		// Push appropriate text color for [S] button based on snap_to_grid state
		
		ImGui::SameLine();
		if(snap_to_grid) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.86f, 0.38f, 0.0f, 1.0f)); // Orange when enabled
		}
		else ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.4,0.4,0.4,1.0));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));

		if(ImGui::Button("[S]"))
		{
			snap_to_grid = !snap_to_grid;
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();

		// [C]ENTER
		
		ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.4,0.4,0.4,1.0));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));

		ImGui::SameLine();
		bool recenterCanvas = false;
		if(ImGui::Button("[C]") || isFirstDraw)
		{
			recenterCanvas = true;
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();

		// [Z]OOM SLIDER
		ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.4,0.4,0.4,1.0));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));

		ImGui::SameLine();
		if(ImGui::SmallButton("[Z]")) {
			rawZoomLevel = 1.0f;
			zoomLevel = 1.0f;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);

		float zoomPercent = zoomLevel * 100.0f;
		if(ImGui::SliderFloat("##zoom", &zoomPercent, ZOOM_MIN * 100.0f, ZOOM_MAX * 100.0f, "%.0f%%")) {
			glm::vec2 canvasCenter = glm::vec2(ImGui::GetContentRegionAvail()) * 0.5f;
			glm::vec2 worldCenterBefore = screenToWorld(canvasOrigin + canvasCenter);
			rawZoomLevel = ofClamp(zoomPercent / 100.0f, ZOOM_MIN, ZOOM_MAX);
			zoomLevel = std::round(rawZoomLevel / 0.05f) * 0.05f;
			glm::vec2 worldCenterAfter = screenToWorld(canvasOrigin + canvasCenter);
			scrolling += glm::vec2(worldCenterAfter - worldCenterBefore);
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();


		// COMMENTS
		
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
		ImGui::SetNextItemWidth(90);
		
        int numComments = container->getComments().size();
        
        // Initialize checkbox states if needed
        if(commentCheckboxStates.size() != numComments) {
            // Handle size changes - this should mainly happen during initialization
            // or if comments were added/removed outside the normal deletion flow
            if(commentCheckboxStates.size() > numComments) {
                // More states than comments - clean up excess entries
                for(int i = numComments; i < commentCheckboxStates.size(); i++) {
                    if(i < commentToSlot.size() && commentToSlot[i] != -1) {
                        keyboardSlots[commentToSlot[i]] = -1;
                    }
                }
            }
            
            commentCheckboxStates.resize(numComments, false);
            commentToSlot.resize(numComments, -1);
            
            // Validate existing assignments after resize
            for(int slot = 0; slot < MAX_KEYBOARD_SLOTS; slot++)
			{
				if(slot<keyboardSlots.size())
				{
					if(keyboardSlots[slot] >= numComments)
					{
						keyboardSlots[slot] = -1;
					}
				}
            }
        }
        
        // Initialize keyboard slots if needed
        if(keyboardSlots.size() != MAX_KEYBOARD_SLOTS) {
            keyboardSlots.resize(MAX_KEYBOARD_SLOTS, -1);
        }
        
        if(numComments > 0)
        {
            ImGui::SameLine();
            
            // Create dropdown for comment navigation
            // Set size constraints to show more items before scrolling (approximately double the default)
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 24));
            if(ImGui::BeginCombo("##CommentDropdown", "Go To..."))
            {
                for(int i = 0; i < numComments; i++)
                {
                    auto &c = container->getComments()[i];
                    
                    // Checkbox for keyboard shortcut assignment
                    bool isChecked = commentCheckboxStates[i];
                    string checkboxId = "##checkbox" + ofToString(i);
                    
                    if(ImGui::Checkbox(checkboxId.c_str(), &isChecked))
                    {
                        if(isChecked && !commentCheckboxStates[i])
                        {
                            // Trying to activate checkbox - check if we have available slots
                            int availableSlot = -1;
                            for(int slot = 0; slot < MAX_KEYBOARD_SLOTS; slot++)
                            {
                                if(keyboardSlots[slot] == -1)
                                {
                                    availableSlot = slot;
                                    break;
                                }
                            }
                            
                            if(availableSlot != -1)
                            {
                                // Assign to available slot
                                commentCheckboxStates[i] = true;
                                keyboardSlots[availableSlot] = i;
                                commentToSlot[i] = availableSlot;
                            }
                            else
                            {
                                // No available slots - revert checkbox
                                isChecked = false;
                            }
                        }
                        else if(!isChecked && commentCheckboxStates[i])
                        {
                            // Deactivating checkbox - free up the slot
                            int slot = commentToSlot[i];
                            if(slot != -1)
                            {
                                keyboardSlots[slot] = -1;
                                commentToSlot[i] = -1;
                            }
                            commentCheckboxStates[i] = false;
                        }
                    }
                    
                    ImGui::SameLine();
                    
                    // Create display text with keyboard shortcut if assigned
                    string displayText;
                    if(commentCheckboxStates[i] && commentToSlot[i] != -1)
                    {
                        int keyNum = (commentToSlot[i] + 1) % 10; // 0 becomes 10, others stay 1-9
                        if(keyNum == 0) keyNum = 10;
                        displayText = "[" + ofToString(keyNum == 10 ? 0 : keyNum) + "] " + c.text;
                    }
                    else
                    {
                        displayText = c.text;
                    }
                    
                    // Apply comment color to the item
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(c.color.r, c.color.g, c.color.b, 1.0));
					string sName = displayText + "##" + ofToString(i);
					if(ImGui::Selectable(sName.c_str() ))
                    {
                        // Navigate to comment position
                        scrolling.x = -c.position.x;
                        scrolling.y = -c.position.y;
                    }
                    
                    ImGui::PopStyleColor();
                }
                ImGui::EndCombo();
            }
        }
        
        // Keyboard shortcut functionality for numeric keys 1-0 using slot-based system
        for(int slot = 0; slot < MAX_KEYBOARD_SLOTS; slot++)
        {
            int commentIndex = keyboardSlots[slot];
            if(commentIndex != -1 && commentIndex < numComments)
            {
                // Map slot to key: slot 0-8 -> keys 1-9, slot 9 -> key 0
                int keyCode = (slot < 9) ? (ImGuiKey_1 + slot) : ImGuiKey_0;
                
                if(!ImGui::IsAnyItemActive() && ImGui::IsKeyDown(ImGuiKey(keyCode)))
                {
                    auto &c = container->getComments()[commentIndex];
                    scrolling.x = -c.position.x;
                    scrolling.y = -c.position.y;
                }
            }
        }
        
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
		        
		// MAIN CANVAS
		
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(color.r/4, color.g/4, color.b/4, 200));
        
		ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
		      contentRegionSize = glm::vec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
		      ImGui::PushItemWidth(120.0f);
		      
		      canvasOrigin = glm::vec2(ImGui::GetCursorScreenPos());
		      ImVec2 offset = ImVec2(canvasOrigin.x + scrolling.x * zoomLevel,
		                             canvasOrigin.y + scrolling.y * zoomLevel);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
		// COMMENT CREATION WITH ALT MOUSE
		
        bool selectionHasBeenMade = false;
        bool newComment = false;
        if(ImGui::IsMouseReleased(0)){
            if(isSelecting){
                isSelecting = false;
                if(ImGui::GetIO().KeyAlt){
                    newComment = true;
                }
                    selectionHasBeenMade = true;
            }
        }
        
        // Define selection helper function at this scope so it's accessible to both nodes and comments
        auto getIsSelectedByRect = [this](ofRectangle rect) -> bool{
            if(entireSelect){
                return selectedRect.inside(rect);
            }else{
                return selectedRect.intersects(rect);
            }
        };
        
        // Display grid
        if (show_grid)
        {
            ImU32 GRID_COLOR = IM_COL32(90, 90, 90, 40);
            ImU32 GRID_COLOR_CENTER = IM_COL32(30, 30, 30, 80);
            ImVec2 win_pos = ImGui::GetCursorScreenPos();
            ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
            
            if (recenterCanvas )
            {
                if(!ImGui::GetIO().KeyShift)
                {
                    scrolling.x = ImGui::GetContentRegionAvail().x / (2.0f * zoomLevel);
                    scrolling.y = ImGui::GetContentRegionAvail().y / (2.0f * zoomLevel);
                }
                else
                {
                    vector<ofxOceanodeNode*> allNodes = container->getAllModules();
                    glm::vec2 centerOfMass = glm::vec2(0.0f,0.0f);
                    glm::vec2 currentNodeCenter;
                    glm::vec2 currentNodeRectangle;
                    for(int i=0;i<allNodes.size();i++)
                    {
                        currentNodeRectangle =  glm::vec2(allNodes[i]->getNodeGui().getRectangle().width,allNodes[i]->getNodeGui().getRectangle().height);
                        currentNodeCenter = allNodes[i]->getNodeGui().getPosition() + currentNodeRectangle/2.0f  ;
                        centerOfMass = centerOfMass + currentNodeCenter;
                    }
                    scrolling.x = (ImGui::GetContentRegionAvail().x / (2.0f * zoomLevel)) - (centerOfMass.x/allNodes.size());
                    scrolling.y = (ImGui::GetContentRegionAvail().y / (2.0f * zoomLevel)) - (centerOfMass.y/allNodes.size());
                }
            }

            // Scaled grid spacing
            float scaledGridSize = GRID_SIZE * zoomLevel;

            // Compute fade opacity
            float gridOpacity = 1.0f;
            if(scaledGridSize < 40.0f) {
                gridOpacity = (scaledGridSize <= 10.0f) ? 0.0f : (scaledGridSize - 10.0f) / 30.0f;
            }

            if(gridOpacity > 0.01f) {
                int alpha = (int)(40 * gridOpacity);
                ImU32 gridCol = IM_COL32(90, 90, 90, alpha);
                float startX = fmodf(scrolling.x * zoomLevel, scaledGridSize);
                float startY = fmodf(scrolling.y * zoomLevel, scaledGridSize);
                for(float x = startX; x < canvas_sz.x; x += scaledGridSize)
                    draw_list->AddLine(ImVec2(x, 0.0f) + win_pos, ImVec2(x, canvas_sz.y) + win_pos, gridCol);
                for(float y = startY; y < canvas_sz.y; y += scaledGridSize)
                    draw_list->AddLine(ImVec2(0.0f, y) + win_pos, ImVec2(canvas_sz.x, y) + win_pos, gridCol);
            }

            // Always draw origin cross
            ImVec2 origin = ImVec2(win_pos.x + scrolling.x * zoomLevel, win_pos.y + scrolling.y * zoomLevel);
            draw_list->AddLine(ImVec2(origin.x, win_pos.y), ImVec2(origin.x, win_pos.y + canvas_sz.y), GRID_COLOR_CENTER, 2.0f);
            draw_list->AddLine(ImVec2(win_pos.x, origin.y), ImVec2(win_pos.x + canvas_sz.x, origin.y), GRID_COLOR_CENTER, 2.0f);
        }
		
		vector<pair<string, ofxOceanodeNode*>> nodesInThisFrame = vector<pair<string, ofxOceanodeNode*>>(container->getParameterGroupNodesMap().begin(), container->getParameterGroupNodesMap().end());
		
		//Look for deleted Nodes in drawing nodes order map
        // Create a set of valid node IDs for quick lookup
        std::unordered_set<std::string> validNodeIds;
        for (const auto& node : nodesInThisFrame) {
            validNodeIds.insert(node.first);
        }
        
        // Temporary vector to store valid nodes with their updated indices
        std::vector<std::pair<std::string, int>> updatedNodes;
        updatedNodes.reserve(nodesDrawingOrder.size());
        
        // Filter out invalid nodes and prepare for reordering
        for (const auto& [nodeId, index] : nodesDrawingOrder) {
            if (validNodeIds.count(nodeId)) {
                updatedNodes.emplace_back(nodeId, index);
            }
        }
        
        // Rebuild the map and update indices
        nodesDrawingOrder.clear();
        for (int newIndex = 0; newIndex < updatedNodes.size(); ++newIndex) {
            nodesDrawingOrder[updatedNodes[newIndex].first] = newIndex;
        }
        
		
		
		//Draw List layers
		draw_list->ChannelsSplit(max(nodesInThisFrame.size(), (size_t)2)*2 + 1 + 1); //We have foreground + background of each node + connections + comments on the background
		
		for(auto &n : nodesInThisFrame){
			if(nodesDrawingOrder.count(n.first) == 0){
				nodesDrawingOrder[n.first] = nodesDrawingOrder.size();
			}
		}
        
        vector<pair<string, ofxOceanodeNode*>> nodesVisibleInThisFrame;
        
        for(auto nodePair : nodesInThisFrame)
        {
            auto node = nodePair.second;
            auto &nodeGui = node->getNodeGui();
            string nodeId = nodePair.first;
            
            bool visibleNode = true;
            if(ofxOceanodeShared::getConfigurationFlags() & ofxOceanodeConfigurationFlags_DisableRenderAll){
                glm::vec2 screenNodePos = worldToScreen(nodeGui.getPosition());
                glm::vec2 screenNodeSize = glm::vec2(nodeGui.getRectangle().getWidth() * zoomLevel,
                                                     nodeGui.getRectangle().getHeight() * zoomLevel);
                ofRectangle screenRect(screenNodePos.x, screenNodePos.y, screenNodeSize.x, screenNodeSize.y);
                ofRectangle windowRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y,
                                       ImGui::GetWindowWidth(), ImGui::GetWindowHeight());
                if(!windowRect.intersects(screenRect) && screenRect.getWidth() > 0){
                    if(!nodeGui.getSelected())
                        visibleNode = false;
                }
            }
            if(visibleNode){
                nodesVisibleInThisFrame.push_back(nodePair);
                nodeGui.setVisibility(true);
            }else{
                nodeGui.setVisibility(false);
            }
            // TODO: At zoom < 0.3, consider rendering nodes as simplified colored rectangles
            // for performance with large patches
        }
        
        //reorder nodesInThisFrame, so they are in correct drawing order, for the interaction to work properly
        std::sort(nodesVisibleInThisFrame.begin(), nodesVisibleInThisFrame.end(), [this](std::pair<std::string, ofxOceanodeNode*> a, std::pair<std::string, ofxOceanodeNode*> b){
//            if (nodesDrawingOrder.count(a.first) == 0) nodesDrawingOrder[a.first] = nodesDrawingOrder.size();
//            if (nodesDrawingOrder.count(b.first) == 0) nodesDrawingOrder[b.first] = nodesDrawingOrder.size();
            return nodesDrawingOrder[a.first] > nodesDrawingOrder[b.first];
        });
		
        std::unordered_set<std::string> deletedIds;
        // Display nodes
        //Iterating over the map gives errors as we are removing elements from the map during the iteration.
        for(auto nodePair : nodesVisibleInThisFrame)
        {
            auto node = nodePair.second;
            auto &nodeGui = node->getNodeGui();
            string nodeId = nodePair.first;
            
            ImGui::PushID(nodeId.c_str());
			
			int nodeDrawChannel = 1;
			if(nodesDrawingOrder.count(nodeId) != 0){
				nodeDrawChannel = (nodesDrawingOrder[nodeId] * 2) + 2; //We have two channels per node, and we have to shift 2 position to draw on top of connections and comments
			}else{
				nodesDrawingOrder[nodeId] = nodesDrawingOrder.size();
			}
            
            glm::vec2 node_rect_min = worldToScreen(nodeGui.getPosition());
            // Display node contents first
            draw_list->ChannelsSetCurrent(nodeDrawChannel+1); // Foreground
            bool old_any_active = ImGui::IsAnyItemActive();

            // Publish the continuous zoom level so node GUI code (e.g. macro snapshot
            // matrix) can derive a smooth zoom factor without relying on the discrete font.
            ofxOceanodeShared::setZoomLevel(zoomLevel);

            // Apply zoom scaling for node rendering via style vars (avoids FontGlobalScale feedback loop)
            // FramePadding inside scrolling_region is (1,1); ItemSpacing defaults are (8,4)
            // Compensate FramePadding.y so that GetFrameHeight() (= FontSize + FramePadding.y*2) scales
            // smoothly with zoomLevel even when the discrete font index flips.
            // BASE_FRAME_HEIGHT = ZOOM_FONT_SIZES[2] + 2 * 1.0f  (default font + base padding)
            // Use the actual effective font size (after fontWindowScale clamping) so that when the ceiling
            // font is larger than targetSize and fwScale is clamped to 1.0, targetPaddingY still produces
            // GetFrameHeight() == BASE_FRAME_HEIGHT * zoomLevel exactly.
            static constexpr float BASE_FRAME_PADDING_Y = 1.0f;
            static constexpr float BASE_FRAME_HEIGHT = ZOOM_FONT_SIZES[2] + 2.0f * BASE_FRAME_PADDING_Y;
            int   ceilIdx          = getCeilingFontIndex();
            float ceilFontSize     = ZOOM_FONT_SIZES[ceilIdx];
            float targetSize       = ZOOM_FONT_SIZES[2] * zoomLevel;
            float fwScale          = (ceilFontSize > 0.0f) ? (targetSize / ceilFontSize) : 1.0f;
            if(fwScale > 1.0f) fwScale = 1.0f;          // never scale up (clamp to avoid blurriness)
            float effectiveFontSize = ceilFontSize * fwScale;  // actual rendered font size after clamping
            float targetPaddingY    = (BASE_FRAME_HEIGHT * zoomLevel - effectiveFontSize) / 2.0f;
            if(targetPaddingY < 0.0f) targetPaddingY = 0.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(BASE_FRAME_PADDING_Y * zoomLevel, targetPaddingY));

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * zoomLevel, 4.0f * zoomLevel));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * zoomLevel, 8.0f * zoomLevel));
            ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize,   12.0f * zoomLevel);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 14.0f * zoomLevel);

            int ceilFontIdx = ceilIdx;
            ImFont* zoomFont = zoomFonts[ceilFontIdx];
            ofxOceanodeShared::setCurrentBoldFont(zoomFontsBold[ceilFontIdx]);
            if(zoomFont) ImGui::PushFont(zoomFont);

            // fwScale already computed above (clamped fontWindowScale); reuse it here.
            ImGui::SetWindowFontScale(fwScale);

            ImGui::SetCursorScreenPos(ImVec2(node_rect_min.x + NODE_WINDOW_PADDING.x * zoomLevel,
                                             node_rect_min.y + NODE_WINDOW_PADDING.y * zoomLevel));

            //Draw Parameters
            if(nodeGui.constructGui(
                NODE_WIDTH_TEXT   * zoomLevel,
                NODE_WIDTH_WIDGET * zoomLevel,
                zoomLevel)){

                ImGui::SetWindowFontScale(1.0f);
                if(zoomFont) ImGui::PopFont();
                ImGui::PopStyleVar(5);
                
                // Save the size of what we have emitted and whether any of the widgets are being used
                bool node_widgets_active = (!old_any_active && ImGui::IsAnyItemActive());
                glm::vec2 screenSize = glm::vec2(ImGui::GetItemRectSize())
                                       + glm::vec2(NODE_WINDOW_PADDING) * zoomLevel
                                       + glm::vec2(NODE_WINDOW_PADDING) * zoomLevel;
                glm::vec2 worldSize = screenSize / zoomLevel;
                nodeGui.setSize(worldSize);

                ImVec2 node_rect_max = ImVec2(node_rect_min.x + screenSize.x, node_rect_min.y + screenSize.y);
                ImVec2 node_rect_header = ImVec2(node_rect_min.x + screenSize.x, node_rect_min.y + 29.0f * zoomLevel);
                // Keep a local alias for backward compatibility in this scope
                glm::vec2 size = screenSize;
                
                // Display node box
                draw_list->ChannelsSetCurrent(nodeDrawChannel); // Background
                ImGui::SetCursorScreenPos(node_rect_min);
                bool interacting_node = ImGui::IsItemActive();
                bool connectionCanBeInteracted = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                ImGui::InvisibleButton("node", ImVec2(screenSize.x, screenSize.y));
                
                if (ImGui::IsItemHovered())
                {
                    isAnyNodeHovered = true;
                    node_hovered_in_scene = nodeId;
                    open_context_menu |= ImGui::IsMouseClicked(1);
                }
                bool node_moving_active = ImGui::IsItemActive();
                
                bool toCheckPress = nodeGui.getSelected() && lastSelectedNode != nodeId ? ImGui::IsMouseReleased(0) : ImGui::IsMouseClicked(0);
                
                if(toCheckPress && ImGui::IsItemHovered(ImGuiHoveredFlags_None)){
                    if(nodeGui.getSelected() && lastSelectedNode == nodeId) lastSelectedNode = "";
                    if(!someDragAppliedToSelection || !nodeGui.getSelected()){
                        if(!ImGui::GetIO().KeyShift){
                            deselectAllNodes();
                            nodeGui.setSelected(true);
                            ofxOceanodeShared::nodeSelectedInCanvas(node);

       //We reorder the list do that this selected node goes to the top layer;
							int rearangeFrom = nodesDrawingOrder[nodeId];
							nodesDrawingOrder[nodeId] = nodesDrawingOrder.size();
							for_each(nodesDrawingOrder.begin(), nodesDrawingOrder.end(), [rearangeFrom](std::pair<const string, int> &orderPair){
								if(orderPair.second > rearangeFrom) orderPair.second--;
							});
                        }else{
                            nodeGui.setSelected(!nodeGui.getSelected());
                        }
                        if(nodeGui.getSelected()) lastSelectedNode = nodeId;
                    }
                }
                if(ImGui::IsMouseReleased(0) && ImGui::IsItemHovered(ImGuiHoveredFlags_None) && nodeGui.getSelected() && lastSelectedNode == nodeId){
                    lastSelectedNode = "";
                }
                
                //MultiSelect
                if(selectionHasBeenMade){
                    bool fitSelection = getIsSelectedByRect(nodeGui.getRectangle());
                    if(!ImGui::GetIO().KeyShift)
                        nodeGui.setSelected(fitSelection);
                    else if(fitSelection)
                        nodeGui.setSelected(true);
                }
                
                bool isSelectedOrSelecting = nodeGui.getSelected() || (isSelecting && getIsSelectedByRect(nodeGui.getRectangle()));
                
                //            ImU32 node_bg_color = /*(node_hovered_in_list == node->ID || node_hovered_in_scene == node->ID || (node_hovered_in_list == -1 && node_selected == node->ID)) ? IM_COL32(75, 75, 75, 255) :*/ IM_COL32(40, 40, 40, 255);
                ImU32 node_bg_color = IM_COL32(40, 40, 40, 255);
                
                ImU32 node_hd_color = (isSelectedOrSelecting) ? IM_COL32(node->getColor().r,node->getColor().g,node->getColor().b,160) : IM_COL32(node->getColor().r,node->getColor().g,node->getColor().b,64);
                
				// Check if this node should be rendered transparently
				bool isTransparent = (node->getNodeModel().getFlags() & ofxOceanodeNodeModelFlags_TransparentNode);

				if (!isTransparent) {
					//if(nodeGui.getExpanded()){
						draw_list->AddRectFilled(node_rect_min, node_rect_max, node_bg_color, 4.0f);
					//}
					draw_list->AddRectFilled(node_rect_min, node_rect_header, node_hd_color, 4.0f);
				}
                
                //draw_list->AddRect(node_rect_min, node_rect_max, IM_COL32(0, 0, 0, 255), 4.0f);
                
                if(nodeGui.getExpanded()){
                    float NODE_BULLET_MIN_SIZE = 3.0f * zoomLevel;
                    float NODE_BULLET_MAX_SIZE = 10.0f * zoomLevel;
                    float NODE_BULLET_GROW_DIST = 10.0f * zoomLevel;
                    
                    for (auto &absParam : node->getParameters()){
                        auto param = dynamic_pointer_cast<ofxOceanodeAbstractParameter>(absParam);
                        if(!(param->getFlags() & ofxOceanodeParameterFlags_DisableInConnection)){
                            auto bulletPosition = nodeGui.getSinkConnectionPositionFromParameter(*param) - glm::vec2(NODE_WINDOW_PADDING.x * zoomLevel, 0);
                            auto mouseToBulletDistance = glm::distance(glm::vec2(ImGui::GetMousePos()), bulletPosition);
                            auto bulletSize = ofMap(mouseToBulletDistance, 0, NODE_BULLET_GROW_DIST, NODE_BULLET_MAX_SIZE, NODE_BULLET_MIN_SIZE, true);
                            draw_list->AddCircleFilled(bulletPosition, bulletSize, IM_COL32(0, 0, 0, 255));
                            if(mouseToBulletDistance < NODE_BULLET_MAX_SIZE && !ImGui::IsPopupOpen("New Node") && connectionCanBeInteracted){
                                connectionIsDoable = true;
                                if(ImGui::IsMouseClicked(0)){
                                    nodeGui.setSelected(false); //Deselect node if we are making connections
                                    isCreatingConnection = true;
                                    if(param->hasInConnection()){ //Parmaeter has sink connected
                                        auto inConnection = param->getInConnection();
                                        tempSourceParameter = &inConnection->getSourceParameter();
                                        if(!ImGui::GetIO().KeyAlt){
                                            inConnection->deleteSelf();
                                        }
                                    }else{
                                        tempSinkParameter = param.get();
                                    }
                                }else if(ImGui::IsMouseReleased(0) && isCreatingConnection){
                                    isCreatingConnection = false;
                                    if(tempSinkParameter != nullptr){
                                        tempSinkParameter = nullptr;
                                        ofLog() << "Cannot create a conection from Sink to Sink";
                                    }
                                    else if(tempSourceParameter != nullptr){
                                        if(tempSourceParameter == param.get()){ //Is the same parameter, no conection between them
                                             ofLog() << "Cannot create connection with same parameter";
                                        }
                                        else{
                                            bool hasInverseConnection = false;
                                            if(param->hasOutConnections()){
                                                for(auto c : param->getOutConnections()){
                                                    if(&c->getSinkParameter() == tempSourceParameter){
                                                        hasInverseConnection = true;
                                                        break;
                                                    }
                                                }
                                            }
                                            if(hasInverseConnection){
                                                ofLog() << "Cannot create connection Already connected the other way arround";
                                            }
                                            else{
                                                //Remove previous connection connected to that parameter.
                                                if(param->hasInConnection()){
                                                    param->getInConnection()->deleteSelf();
                                                }
                                                container->createConnection(*tempSourceParameter, *param);
                                            }
                                        }
                                        tempSourceParameter = nullptr;
                                    }
                                }
                            }
                        }
                    }
                    for (auto &absParam : node->getParameters()){
                        auto param = dynamic_pointer_cast<ofxOceanodeAbstractParameter>(absParam);
                        if(!(param->getFlags() & ofxOceanodeParameterFlags_DisableOutConnection)){
                            auto bulletPosition = nodeGui.getSourceConnectionPositionFromParameter(*param) + glm::vec2(NODE_WINDOW_PADDING.x * zoomLevel, 0);
                            auto mouseToBulletDistance = glm::distance(glm::vec2(ImGui::GetMousePos()), bulletPosition);
                            auto bulletSize = ofMap(mouseToBulletDistance, 0, NODE_BULLET_GROW_DIST, NODE_BULLET_MAX_SIZE, NODE_BULLET_MIN_SIZE, true);
                            draw_list->AddCircleFilled(bulletPosition, bulletSize, IM_COL32(0, 0, 0, 255));
                            if(mouseToBulletDistance < NODE_BULLET_MAX_SIZE && !ImGui::IsPopupOpen("New Node") && connectionCanBeInteracted){
                                if(ImGui::IsMouseClicked(0)){
                                    nodeGui.setSelected(false); //Deselect node if we are making connections
                                    isCreatingConnection = true;
                                    tempSourceParameter = param.get();
                                }else if(ImGui::IsMouseReleased(0) && isCreatingConnection){
                                    isCreatingConnection = false;
                                    if(tempSourceParameter != nullptr){
                                        tempSourceParameter = nullptr;
                                        ofLog() << "Cannot create a conection from Source to Source";
                                    }
                                    if(tempSinkParameter != nullptr){
                                        if(tempSinkParameter == param.get()){ //Is the same parameter, no conection between them
                                             ofLog() << "Cannot create connection with same parameter";
                                        }
                                        else{
                                            bool hasInverseConnection = false;
                                            if(param->hasInConnection()){
                                                hasInverseConnection = &param->getInConnection()->getSourceParameter() == tempSinkParameter;
                                            }
                                            if(hasInverseConnection){
                                                ofLog() << "Cannot create connection Already connected the other way arround";
                                            }
                                            else{
                                                container->createConnection(*param, *tempSinkParameter);
                                            }
                                        }
                                        tempSinkParameter = nullptr;
                                    }
                                }
                            }
                        }
                    }
                }
                if(someSelectedModuleMove == nodeId) someSelectedModuleMove = "";
                if(!isCreatingConnection){
                    if (node_widgets_active || node_moving_active)
                        node_selected = nodeId;
                    if (node_moving_active && ImGui::IsMouseDragging(0, 0.0f) && !node_widgets_active)
                        someSelectedModuleMove = nodeId;
                    if(someSelectedModuleMove != "" && nodeGui.getSelected()) {
                        glm::vec2 newPosition = nodeGui.getPosition() + moveSelectedModulesWithDrag;
                        // During dragging, allow free movement without snapping
                        nodeGui.setPosition(newPosition);
                    }
                }
            }
            else{
                ImGui::SetWindowFontScale(1.0f);
                if(zoomFont) ImGui::PopFont();
                ImGui::PopStyleVar(5);
                deletedIds.insert(nodeId);
            }
            ImGui::PopID();
  }
        
        // Move selected comments along with nodes (once per frame, outside the node loop)
        if(someSelectedModuleMove != "" && moveSelectedModulesWithDrag != glm::vec2(0,0)){
            for(auto &c : container->getComments()){
                if(c.selected){
                    c.position = c.position + moveSelectedModulesWithDrag;
                }
            }
        }
        
        if(newComment){
            // Snap comment corners to grid if snap_to_grid is enabled
            // selectedRect is in world coordinates (Phase 6 changes), no extra conversion needed
            glm::vec2 commentPosition = selectedRect.position;
            glm::vec2 commentSize = glm::vec2(selectedRect.width, selectedRect.height);
            
            if(snap_to_grid){
                // Snap top-left corner
                glm::vec2 topLeft = snapToGrid(commentPosition);
                // Snap bottom-right corner
                glm::vec2 bottomRight = snapToGrid(commentPosition + commentSize);
                // Recalculate size based on snapped corners
                commentPosition = topLeft;
                commentSize = bottomRight - topLeft;
            }
            
            auto &comment = container->getComments().emplace_back(commentPosition, commentSize);
            deselectAllNodes();
        }

        draw_list->ChannelsSetCurrent(0);
        int removeIndex = -1;
        for(int i = 0; i < container->getComments().size(); i++){
            auto &c = container->getComments()[i];
            ImGui::PushID(("Comment " + ofToString(i)).c_str());
            
            // Handle comment selection during cross-selection
            if(selectionHasBeenMade){
                // Only select if the comment's rectangle intersects the selection rectangle,
                // but is NOT completely containing the selection rectangle.
                // This prevents selecting a large background comment when doing a small cross-selection inside it.
                bool intersects = selectedRect.intersects(c.getRectangle());
                bool containsSelection = c.getRectangle().inside(selectedRect);
                
                bool fitSelection = intersects && !containsSelection;
                
                if(!ImGui::GetIO().KeyShift)
                    c.selected = fitSelection;
                else if(fitSelection)
                    c.selected = true;
            }
            
            bool isSelectedOrSelecting = c.selected;
            
            glm::vec2 currentPosition = worldToScreen(c.position);
            glm::vec2 screenSize = c.size * zoomLevel;
            float headerH = 15.0f * zoomLevel;
            draw_list->AddRectFilled(currentPosition, currentPosition + glm::vec2(screenSize.x, headerH), IM_COL32(c.color.r*255, c.color.g*255, c.color.b*255, 255));
            // Don't draw comment text below 50% zoom
            if(zoomLevel > 0.5f){
                ImFont* commentFont = zoomFonts[getCeilingFontIndex()];
                float commentFontSize = ZOOM_FONT_SIZES[2] * zoomLevel;  // base font size scaled by zoom
                if(commentFont) {
                    draw_list->AddText(commentFont, commentFontSize, currentPosition,
                                       IM_COL32(c.textColor.r*255, c.textColor.g*255, c.textColor.b*255, 255),
                                       c.text.c_str());
                } else {
                    draw_list->AddText(currentPosition,
                                       IM_COL32(c.textColor.r*255, c.textColor.g*255, c.textColor.b*255, 255),
                                       c.text.c_str());
                }
            }
            draw_list->AddRectFilled(currentPosition + glm::vec2(0, headerH), currentPosition + screenSize, IM_COL32(c.color.r*255, c.color.g*255, c.color.b*255, 100));
            
            // Draw selection border if selected
            if(isSelectedOrSelecting){
                draw_list->AddRect(currentPosition, currentPosition + screenSize, IM_COL32(255, 127, 0, 255), 0.0f, 0, 2.0f);
            }
            ImGui::SetCursorScreenPos(currentPosition);
			// trying to avoid a crash on ImGui::InvisibleButton
			// IM_ASSERT(size_arg.x != 0.0f && size_arg.y != 0.0f);
			// if size of x or y is 0 -> crash !
			// This could happen if accidentally creating a "comment" with click+option without dragging = size = 0
			         if(c.size.x==0 || c.size.y==0)
			{
				c.size.x = 256;
				c.size.y = 15;
			}
            ImGui::InvisibleButton("Inv Button", ImVec2(c.size.x * zoomLevel, 15.0f * zoomLevel));
            
            if(ImGui::IsItemActive()){
                ofRectangle rect(c.position, c.size.x, c.size.y);
                glm::vec2 dragDelta = glm::vec2(ImGui::GetIO().MouseDelta) / zoomLevel;
                
                // If this comment is selected, move all selected items (nodes + comments)
                if(c.selected && dragDelta != glm::vec2(0,0)){
                    // Move all selected nodes
                    for(auto &n : container->getParameterGroupNodesMap()){
                        if(n.second->getNodeGui().getSelected()){
                            n.second->getNodeGui().setPosition(n.second->getNodeGui().getPosition() + dragDelta);
                        }
                    }
                    // Move all selected comments
                    for(auto &otherComment : container->getComments()){
                        if(otherComment.selected){
                            otherComment.position = otherComment.position + dragDelta;
                        }
                    }
                    someDragAppliedToSelection = true;
                }
                // If comment is not selected, use old behavior (move comment and grouped nodes + nested comments)
                else if(!c.selected){
                    c.position = c.position + dragDelta;
                    if(!ImGui::GetIO().KeyAlt){
                        if(c.nodes.size() == 0){
                            for(auto nodePair : nodesInThisFrame)
                            {
                                if(rect.inside(nodePair.second->getNodeGui().getRectangle())){
                                    c.nodes.push_back(nodePair.second);
                                }
                            }
                        }
                        for(auto n : c.nodes){
                            n->getNodeGui().setPosition(n->getNodeGui().getPosition() + dragDelta);
                        }
                        
                        // Also move any nested comments (comments inside this comment's rectangle)
                        for(auto &otherComment : container->getComments()){
                            if(&otherComment != &c){  // Don't move self
                                // Check if the other comment is inside this comment's rectangle
                                if(rect.inside(otherComment.getRectangle())){
                                    otherComment.position = otherComment.position + dragDelta;
                                }
                            }
                        }
                    }else{
                        c.nodes.clear();
                    }
                }
            }
            
            // Snap comment to grid when drag ends
            if(ImGui::IsItemDeactivated()){
                if(snap_to_grid){
                    // If this comment is selected, snap all selected items
                    if(c.selected){
                        // Snap all selected nodes to grid
                        for(auto &n : container->getParameterGroupNodesMap()){
                            if(n.second->getNodeGui().getSelected()){
                                n.second->getNodeGui().setPosition(snapToGrid(n.second->getNodeGui().getPosition()));
                            }
                        }
                        // Snap all selected comments to grid
                        for(auto &otherComment : container->getComments()){
                            if(otherComment.selected){
                                otherComment.position = snapToGrid(otherComment.position);
                            }
                        }
                    }
                    // If comment is not selected, use old behavior (snap comment and grouped nodes + nested comments)
                    else{
                        // Store pre-snap position to calculate the snap delta
                        glm::vec2 preSnapPosition = c.position;
                        c.position = snapToGrid(c.position);
                        
                        // Calculate the snap delta and apply it to all nodes inside the comment
                        glm::vec2 snapDelta = c.position - preSnapPosition;
                        if(snapDelta != glm::vec2(0, 0)){
                            for(auto n : c.nodes){
                                n->getNodeGui().setPosition(n->getNodeGui().getPosition() + snapDelta);
                            }
                            
                            // Also snap nested comments (comments inside this comment's rectangle)
                            ofRectangle rect(preSnapPosition, c.size.x, c.size.y);
                            for(auto &otherComment : container->getComments()){
                                if(&otherComment != &c){  // Don't move self
                                    if(rect.inside(otherComment.getRectangle())){
                                        otherComment.position = otherComment.position + snapDelta;
                                    }
                                }
                            }
                        }
                    }
                }
                c.nodes.clear();
            }
            
            if(ImGui::IsItemClicked(1)){
                c.openPopupInNext = true;
            }
            
            if(c.openPopupInNext){
                ImGui::OpenPopup("Comment");
                c.openPopupInNext = false;
            }
			ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));  // Dark background
			ImGui::PushStyleColor(ImGuiCol_Text,     ImVec4(0.7f, 0.7f, 0.7f, 0.7f));  // White text
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f)); // Hovered button
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

            if(ImGui::BeginPopup("Comment")){
                char * cString = new char[1024];
                strcpy(cString, c.text.c_str());
                if (ImGui::InputText("Text", cString, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    c.text = cString;
                }
                delete[] cString;
                ImGui::DragFloat2("Position", &c.position.x);
                ImGui::DragFloat2("Size", &c.size.x);
                ImGui::ColorEdit3("Color", &c.color.r);
                ImGui::ColorEdit3("TextColor", &c.textColor.r);
                if(ImGui::Button("[Remove]")){
                    removeIndex = i;
                }
                ImGui::EndPopup();
            }
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();

            ImGui::PopID();
        }
        if(removeIndex != -1){
            // Before deleting the comment, handle keyboard slot cleanup properly
            // to maintain stable assignments for remaining comments
            
            // 1. If the deleted comment had a keyboard slot, free it
            if(removeIndex < commentCheckboxStates.size() &&
               commentCheckboxStates[removeIndex] &&
               removeIndex < commentToSlot.size() &&
               commentToSlot[removeIndex] != -1) {
                int slotToFree = commentToSlot[removeIndex];
                keyboardSlots[slotToFree] = -1;
            }
            
            // 2. Update all keyboard slot assignments for comments that will shift indices
            // Comments with index > removeIndex will shift down by 1
            for(int slot = 0; slot < MAX_KEYBOARD_SLOTS; slot++) {
                if(keyboardSlots[slot] > removeIndex) {
                    keyboardSlots[slot]--; // Decrement the comment index in the slot
                }
            }
            
            // 3. Now delete the comment
            container->getComments().erase(container->getComments().begin() + removeIndex);
            
            // 4. Update the tracking vectors to match the new comment count
            // Remove the deleted comment's entries and shift remaining ones
            if(removeIndex < commentCheckboxStates.size()) {
                commentCheckboxStates.erase(commentCheckboxStates.begin() + removeIndex);
            }
            if(removeIndex < commentToSlot.size()) {
                commentToSlot.erase(commentToSlot.begin() + removeIndex);
            }
            
            // 5. Update commentToSlot mappings for comments that shifted indices
            for(int i = removeIndex; i < commentToSlot.size(); i++) {
                if(commentToSlot[i] != -1) {
                    // This comment's index shifted down by 1, but its slot assignment stays the same
                    // The keyboardSlots array was already updated in step 2
                    // commentToSlot[i] already contains the correct slot number
                }
            }
        }
        
        
        
        // Open context menu
        if (!ImGui::IsAnyItemHovered() && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1))
        {
			newNodeClickPos = ImGui::GetMousePos();
			bool commentClicked = false;
			for(auto &c : container->getComments()){
				if(ofRectangle(c.position.x, c.position.y, c.size.x, 15.0f / zoomLevel).inside(screenToWorld(glm::vec2(newNodeClickPos)))){
					c.openPopupInNext = true;
					commentClicked = true;
				}
				if(commentClicked) break;
			}
			if(!commentClicked){
				ImGui::OpenPopup("New Node");
				searchField = "";
				lastSearchField = "";
				numTimesPopup = 0;
				selectedSearchResultIndex = -1;
				filteredSearchResults.clear();
				
				//Get node registry to update newly registered nodes
				auto const &models = container->getRegistry()->getRegisteredModels();
				auto const &categories = container->getRegistry()->getCategories();
				auto const &categoriesModelsAssociation = container->getRegistry()->getRegisteredModelsCategoryAssociation();
				
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
			}
        }
        
        // Draw New Node menu
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
        bool popop_close_button = true;
        if (ImGui::BeginPopupModal("New Node", &popop_close_button, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
        {
            char * cString = new char[256];
            strcpy(cString, searchField.c_str());
            
            if(ImGui::InputText("Search", cString, 256)){
                searchField = cString;
                // Reset selection when search text changes
                if(searchField != lastSearchField) {
                    selectedSearchResultIndex = -1;
                    lastSearchField = searchField;
                }
            }
            
            if(numTimesPopup == 1){//!(!ImGui::IsItemClicked() && ImGui::IsMouseDown(0)) && searchField == ""){
                ImGui::SetKeyboardFocusHere(-1);
                numTimesPopup++;
            }else{
                numTimesPopup++;
            }
            
            bool isEnterPressed = ImGui::IsKeyDown((ImGuiKey_Enter)); //Select first option if enter is pressed
            bool isEnterReleased = ImGui::IsKeyReleased((ImGuiKey_Enter)); //Select first option if enter is pressed
            
            // Handle arrow key navigation
            bool isUpPressed = ImGui::IsKeyPressed((ImGuiKey_UpArrow));
            bool isDownPressed = ImGui::IsKeyPressed((ImGuiKey_DownArrow));
   
   // TODO: Get all things, nodes, collections, macros, scripts;
    
            if(searchField != ""){
                // Build filtered search results list with scoring
                filteredSearchResults.clear();
                
                // Create a scoring function for search relevance
                auto calculateSearchScore = [](const string& itemName, const string& searchTerm) -> int {
                    if(itemName.empty() || searchTerm.empty()) return 0;
                    
                    string lowerItemName = itemName;
                    string lowerSearchTerm = searchTerm;
                    std::transform(lowerItemName.begin(), lowerItemName.end(), lowerItemName.begin(), ::tolower);
                    std::transform(lowerSearchTerm.begin(), lowerSearchTerm.end(), lowerSearchTerm.begin(), ::tolower);
                    
                    // Exact match (case-insensitive) gets highest score
                    if(lowerItemName == lowerSearchTerm) {
                        return 1000;
                    }
                    
                    // Prefix match gets high score
                    if(lowerItemName.find(lowerSearchTerm) == 0) {
                        // Shorter names with prefix matches get higher scores
                        return 800 - (int)lowerItemName.length();
                    }
                    
                    // Substring match gets lower score
                    size_t pos = lowerItemName.find(lowerSearchTerm);
                    if(pos != string::npos) {
                        // Earlier position in string gets higher score
                        // Shorter names get higher scores
                        return 400 - (int)pos - (int)lowerItemName.length();
                    }
                    
                    return 0; // No match
                };
                
                // Collect scored results
                vector<pair<SearchResultItem, int>> scoredResults;
                
                // Add matching nodes with scores
                for(int i = 0; i < categoriesVector.size(); i++){
                    for(auto &op : options[i])
                    {
                        int score = calculateSearchScore(op, searchField);
                        if(score > 0){
                            scoredResults.push_back(make_pair(SearchResultItem(op, "node"), score));
                        }
                    }
                }
                
                // Add matching macros with scores
                std::function<void(shared_ptr<macroCategory>)> collectMacros =
    [this, &collectMacros, &scoredResults, &calculateSearchScore](shared_ptr<macroCategory> category){
                    for(auto d : category->categories){
                        collectMacros(d);
                    }
                    for(auto m : category->macros){
                        int score = calculateSearchScore(m.first, searchField);
                        if(score > 0){
                            scoredResults.push_back(make_pair(SearchResultItem(m.first, "macro", m.second), score));
                        }
                    }
                };
                
                auto macroDirectoryStructure = ofxOceanodeShared::getMacroDirectoryStructure();
                collectMacros(macroDirectoryStructure);
                
                // Sort by score (highest first)
                std::sort(scoredResults.begin(), scoredResults.end(),
                    [](const pair<SearchResultItem, int>& a, const pair<SearchResultItem, int>& b) {
                        return a.second > b.second;
                    });
                
                // Extract sorted results
                for(const auto& scoredResult : scoredResults) {
                    filteredSearchResults.push_back(scoredResult.first);
                }
                
                // Handle arrow key navigation
                if(isUpPressed && selectedSearchResultIndex > 0) {
                    selectedSearchResultIndex--;
                } else if(isDownPressed && selectedSearchResultIndex < (int)filteredSearchResults.size() - 1) {
                    selectedSearchResultIndex++;
                } else if((isUpPressed || isDownPressed) && selectedSearchResultIndex == -1 && !filteredSearchResults.empty()) {
                    selectedSearchResultIndex = isDownPressed ? 0 : filteredSearchResults.size() - 1;
                }
                
                // Clamp selection index to valid range
                if(selectedSearchResultIndex >= (int)filteredSearchResults.size()) {
                    selectedSearchResultIndex = filteredSearchResults.size() - 1;
                }
                if(selectedSearchResultIndex < -1) {
                    selectedSearchResultIndex = -1;
                }
                
                // Display search results
                for(int i = 0; i < filteredSearchResults.size(); i++) {
                    const auto& result = filteredSearchResults[i];
                    bool isSelected = (i == selectedSearchResultIndex);
                    
                    // Highlight selected item
                    if(isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.43f, 0.19f, 0.0f, 0.8f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.86f,0.38f, 0.0f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.8f, 0.3f, 0.0f, 0.6f));
                    }
                    
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(0.6f, 0.6f, 0.6f,1.0f)));
                    
                    bool itemClicked = ImGui::Selectable(result.name.c_str(), isSelected);
                    bool itemActivated = itemClicked || (isSelected && isEnterPressed);
                    
                    if(itemClicked) {
                        selectedSearchResultIndex = i;
                    }
                    
                    if(itemActivated) {
                        if(result.type == "node") {
                            unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create(result.name);
                            if (type) {
                                auto &node = container->createNode(std::move(type));
                                glm::vec2 nodePosition = screenToWorld(glm::vec2(newNodeClickPos));
                                if(snap_to_grid) nodePosition = snapToGrid(nodePosition);
                                node.getNodeGui().setPosition(nodePosition);
                            }
                        } else if(result.type == "macro") {
                            unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create("Macro");
                            if (type) {
                                auto &node = container->createNode(std::move(type), result.macroPath);
                                glm::vec2 nodePosition = screenToWorld(glm::vec2(newNodeClickPos));
                                if(snap_to_grid) nodePosition = snapToGrid(nodePosition);
                                node.getNodeGui().setPosition(nodePosition);
                            }
                        }
                        ImGui::PopStyleColor();
                        if(isSelected) {
                            ImGui::PopStyleColor(3);
                        }
                        ImGui::CloseCurrentPopup();
                        isEnterPressed = false;
                        break;
                    }
                    
                    ImGui::PopStyleColor();
                    if(isSelected) {
                        ImGui::PopStyleColor(3);
                    }
                }
                
                // Handle Enter key when no specific item is selected (fallback to first result)
                if(isEnterPressed && selectedSearchResultIndex == -1 && !filteredSearchResults.empty()) {
                    const auto& firstResult = filteredSearchResults[0];
                    if(firstResult.type == "node") {
                        unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create(firstResult.name);
                        if (type) {
                            auto &node = container->createNode(std::move(type));
                            glm::vec2 nodePosition = screenToWorld(glm::vec2(newNodeClickPos));
                            if(snap_to_grid) nodePosition = snapToGrid(nodePosition);
                            node.getNodeGui().setPosition(nodePosition);
                        }
                    } else if(firstResult.type == "macro") {
                        unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create("Macro");
                        if (type) {
                            auto &node = container->createNode(std::move(type), firstResult.macroPath);
                            glm::vec2 nodePosition = screenToWorld(glm::vec2(newNodeClickPos));
                            if(snap_to_grid) nodePosition = snapToGrid(nodePosition);
                            node.getNodeGui().setPosition(nodePosition);
                        }
                    }
                    ImGui::CloseCurrentPopup();
                }
            }
                
            
            
            ImGui::Separator();
            
            if(ImGui::BeginMenu("Modules")){
                bool selectedModule = false;
                for(int i = 0; i < categoriesVector.size() && !selectedModule; i++){
                    vector<string> subcategoriesSplit = ofSplitString(categoriesVector[i], "/");
                    int openedMenus = 0;
                    for(auto subCategory : subcategoriesSplit){
                        if(ImGui::BeginMenu(subCategory.c_str())){
                            openedMenus++;
                        }else{
                            break;
                        }
                    }
                    if(openedMenus == subcategoriesSplit.size())
                    {
                        for(auto &op : options[i])
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(0.65f, 0.65f, 0.65f,1.0f)));
                            if( ImGui::MenuItem(op.c_str()) || (ImGui::IsItemFocused() && isEnterPressed))
                            {
                                unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create(op);
                                if (type)
                                {
                                    auto &node = container->createNode(std::move(type));
                                    glm::vec2 nodePosition = screenToWorld(glm::vec2(newNodeClickPos));
                                    if(snap_to_grid) nodePosition = snapToGrid(nodePosition);
                                    node.getNodeGui().setPosition(nodePosition);
                                }
                                ImGui::PopStyleColor();
                                selectedModule = true;
                                break;
                            }
                            ImGui::PopStyleColor();
                        }
                    }
                    for(int i = 0; i < openedMenus; i++){
                        ImGui::EndMenu();
                    }
                }
                ImGui::EndMenu();
                if(selectedModule) ImGui::CloseCurrentPopup();
            }
            
            //TODO: Implement
            if(ImGui::BeginMenu("Module Groups")){
                ImGui::MenuItem("Example 1");
                ImGui::MenuItem("Example 2");
                ImGui::MenuItem("Example 3");
                ImGui::EndMenu();
            }
            
            if(ImGui::BeginMenu("Macros")){
				auto macroDirectoryStructure = ofxOceanodeShared::getMacroDirectoryStructure();
				
				std::function<void(shared_ptr<macroCategory>)> drawCategory =
				[this, &drawCategory](shared_ptr<macroCategory> category){
					for(auto d : category->categories){
						if(ImGui::BeginMenu(d->name.c_str())){
							drawCategory(d);
							ImGui::EndMenu();
						}
					}
					for(auto m : category->macros){
						if(ImGui::MenuItem(m.first.c_str())){
							unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry()->create("Macro");
							if (type)
							{
								auto &node = container->createNode(std::move(type), m.second);
									glm::vec2 nodePosition = screenToWorld(glm::vec2(newNodeClickPos));
									if(snap_to_grid) nodePosition = snapToGrid(nodePosition);
									node.getNodeGui().setPosition(nodePosition);
							}
						}
					}
				};
				
				drawCategory(macroDirectoryStructure);
				
                ImGui::EndMenu();
            }
            
            //TODO: How to add scripts?
            if(ImGui::BeginMenu("Scripts")){
                ImGui::MenuItem("Example 1");
                ImGui::MenuItem("Example 2");
                ImGui::MenuItem("Example 3");
                ImGui::EndMenu();
            }
			
			if(ImGui::Selectable("Comment")){
				glm::vec2 commentPosition = screenToWorld(glm::vec2(newNodeClickPos));
				glm::vec2 commentSize;
				
				// Snap comment position and size to grid if snap_to_grid is enabled
				if(snap_to_grid){
					commentPosition = snapToGrid(commentPosition);
					// Width = gridDivisions * GRID_SIZE (which equals total node width)
					// Height = 1 * GRID_SIZE (minimum height)
					commentSize = glm::vec2(gridDivisions * GRID_SIZE, GRID_SIZE);
				} else {
					// Use default size from ofxOceanodeComment constructor
					commentSize = glm::vec2(265, 20);
				}
				
				container->getComments().emplace_back(commentPosition, commentSize);
			}
            
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        //ImGui::PopStyleColor();
        
        auto getSourceConnectionPositionFromParameter = [this](ofxOceanodeAbstractParameter& param) -> glm::vec2{
            auto gui = container->getGuiFromModel(param.getNodeModel());
            if(gui != nullptr){
                if(gui->getVisibility()){
                    return gui->getSourceConnectionPositionFromParameter(param);
                }else{
                    return worldToScreen(gui->getPosition()) + glm::vec2(gui->getRectangle().getWidth() * zoomLevel, gui->getRectangle().getHeight() * zoomLevel / 2.0f);
                }
            }
            return glm::vec2();
            //TODO: Throw exception
        };
        auto getSinkConnectionPositionFromParameter = [this](ofxOceanodeAbstractParameter& param) -> glm::vec2{
            auto gui = container->getGuiFromModel(param.getNodeModel());
            if(gui != nullptr){
                if(gui->getVisibility()){
                    return gui->getSinkConnectionPositionFromParameter(param);
                }else{
                    return worldToScreen(gui->getPosition()) + glm::vec2(0, gui->getRectangle().getHeight() * zoomLevel / 2.0f);
                }
            }
            return glm::vec2();
            //TODO: Throw exception
        };
        
        // Display links
        draw_list->ChannelsSetCurrent(1); // Background
        
        std::vector<ofxOceanodeAbstractConnection*> drawnConnections;
        
        for(auto nodePair : nodesVisibleInThisFrame)
        {
            if(deletedIds.count(nodePair.first) != 0) continue;
            for (auto &absParam : nodePair.second->getParameters()){
                auto param = dynamic_pointer_cast<ofxOceanodeAbstractParameter>(absParam);
                auto connections = param->getOutConnections();
                if(param->getInConnection() != nullptr) connections.push_back(param->getInConnection());
                for(auto connection : connections){
                    if(std::find(drawnConnections.begin(), drawnConnections.end(), connection) == drawnConnections.end()){
                        glm::vec2 p1 = getSourceConnectionPositionFromParameter(connection->getSourceParameter()) + glm::vec2(NODE_WINDOW_PADDING.x * zoomLevel, 0);
                        glm::vec2 p2 = getSinkConnectionPositionFromParameter(connection->getSinkParameter()) - glm::vec2(NODE_WINDOW_PADDING.x * zoomLevel, 0);
                        glm::vec2  controlPoint(0,0);
                        controlPoint.x = ofMap(glm::distance(p1,p2), 0, 1500 * zoomLevel, 25 * zoomLevel, 400 * zoomLevel);
                        draw_list->AddBezierCubic(p1, p1 + controlPoint, p2 - controlPoint, p2, IM_COL32(200, 200, 200, 128), 2.0f);
                        drawnConnections.push_back(connection);
                    }
                }

            }
        }
        if(tempSourceParameter != nullptr || tempSinkParameter != nullptr){
            glm::vec2 p1, p2;
            if(tempSourceParameter != nullptr){
                p1 = getSourceConnectionPositionFromParameter(*tempSourceParameter) + glm::vec2(NODE_WINDOW_PADDING.x * zoomLevel, 0);
                p2 = ImGui::GetMousePos();
            }else{
                p1 = ImGui::GetMousePos();
                p2 = getSinkConnectionPositionFromParameter(*tempSinkParameter) - glm::vec2(NODE_WINDOW_PADDING.x * zoomLevel, 0);
            }
            glm::vec2  controlPoint(0,0);
            controlPoint.x = ofMap(glm::distance(p1,p2), 0, 1500 * zoomLevel, 25 * zoomLevel, 400 * zoomLevel);
            float linkWidth = 2.0f;
            ImColor c;
            if(connectionIsDoable){
                linkWidth = 3.0f;
                c = IM_COL32(255, 255, 255, 128);
            }else{
                c = IM_COL32(255, 255, 255, 64);
            }
            draw_list->AddBezierCubic(p1, p1 + controlPoint, p2 - controlPoint, p2, c, linkWidth);
        }
        
        draw_list->ChannelsMerge();
        
		moveSelectedModulesWithDrag = glm::vec2(0,0);
        // Scrolling
        if(ImGui::IsWindowFocused()){
            if(ImGui::IsMouseDragging(0, 0.0f)){
                if (ImGui::IsWindowHovered()){
                    if(ImGui::GetIO().KeyCtrl && !isCreatingConnection){
                        if(!isSelecting){
                            selectInitialPoint = screenToWorld(glm::vec2(ImGui::GetMousePos()) - glm::vec2(ImGui::GetIO().MouseDelta));
                            isSelecting  = true;
                        }
                        selectEndPoint = screenToWorld(glm::vec2(ImGui::GetMousePos()));
                        selectedRect = ofRectangle(selectInitialPoint, selectEndPoint);
                        entireSelect =  selectInitialPoint.y < selectEndPoint.y;
                        canvasHasScolled = true; //HACK to not remove selection on mouse release
                    }
                    if((!isSelecting && !isCreatingConnection && someSelectedModuleMove == "") || (ImGui::IsKeyDown(ImGuiKey_Space))){
                        scrolling = scrolling + glm::vec2(ImGui::GetIO().MouseDelta) / zoomLevel;
                        if(glm::vec2(ImGui::GetIO().MouseDelta) != glm::vec2(0,0)) canvasHasScolled = true;
                        if(isSelecting && !ImGui::GetIO().KeyCtrl){
                            selectInitialPoint += glm::vec2(ImGui::GetIO().MouseDelta) / zoomLevel;
                            selectEndPoint += glm::vec2(ImGui::GetIO().MouseDelta) / zoomLevel;
                            selectedRect = ofRectangle(selectInitialPoint, selectEndPoint);
                            entireSelect = glm::vec2(selectedRect.getTopLeft()) == selectInitialPoint;
                        }
                    }
				}else if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)){
					if(ImGui::IsKeyDown(ImGuiKey_Space)){
						scrolling = scrolling + glm::vec2(ImGui::GetIO().MouseDelta) / zoomLevel;
                        if(glm::vec2(ImGui::GetIO().MouseDelta) != glm::vec2(0,0)) canvasHasScolled = true;
					}
					else if(someSelectedModuleMove != ""){
						moveSelectedModulesWithDrag = glm::vec2(ImGui::GetIO().MouseDelta) / zoomLevel;
						if(moveSelectedModulesWithDrag != glm::vec2(0,0))
							someDragAppliedToSelection = true;
					}
				}
            }
            
            //TODO: Scroll amount in config
            if(ImGui::IsWindowHovered()){
                float wheel = ImGui::GetIO().MouseWheel;
                float wheelH = ImGui::GetIO().MouseWheelH;

                bool zoomModifier = ImGui::GetIO().KeyCtrl;

                if(zoomModifier && wheel != 0.0f) {
                    // Zoom centered on mouse position
                    glm::vec2 mouseScreen = glm::vec2(ImGui::GetMousePos());
                    glm::vec2 worldBeforeZoom = screenToWorld(mouseScreen);
                    rawZoomLevel = ofClamp(rawZoomLevel * (1.0f + wheel * 0.1f), ZOOM_MIN, ZOOM_MAX);
                    zoomLevel = std::round(rawZoomLevel / 0.05f) * 0.05f;
                    glm::vec2 worldAfterZoom = screenToWorld(mouseScreen);
                    scrolling += (worldAfterZoom - worldBeforeZoom);
                } else {
                    // Normal panning — convert screen delta to world delta
                    scrolling += glm::vec2(wheelH * 10.0f, wheel * 10.0f) / zoomLevel;
                }
            }
            
            if(isSelecting){
                //TODO: Change colors
                if(selectInitialPoint.y < selectEndPoint.y){ //From top to bottom;
                    draw_list->AddRectFilled(worldToScreen(selectInitialPoint), worldToScreen(selectEndPoint), IM_COL32(255,127,0,30));
                }else{
                    draw_list->AddRectFilled(worldToScreen(selectInitialPoint), worldToScreen(selectEndPoint), IM_COL32(0,125,255,30));
                }
            }
            
            
            if(ImGui::IsMouseReleased(0)){
                if(canvasHasScolled){
                    canvasHasScolled = false;
                }else if(!isAnyNodeHovered && !someDragAppliedToSelection){
                    deselectAllNodes();
                }
                
                // Apply snap-to-grid when drag operation ends
                if(snap_to_grid && someDragAppliedToSelection){
                    // Find and snap all selected nodes to grid
                    for(auto &n : container->getParameterGroupNodesMap()){
                        if(n.second->getNodeGui().getSelected()){
                            n.second->getNodeGui().setPosition(snapToGrid(n.second->getNodeGui().getPosition()));
                        }
                    }
                    // Also snap all selected comments to grid
                    for(auto &c : container->getComments()){
                        if(c.selected){
                            c.position = snapToGrid(c.position);
                        }
                    }
                }
                
                someDragAppliedToSelection = false;
                moveSelectedModulesWithDrag = glm::vec2(0,0);
            }
            
            if(ImGui::GetIO().KeyCtrl){
                if(ImGui::IsKeyPressed(ImGuiKey_C)){
                    container->copySelectedModulesWithConnections();
                    deselectAllNodes();
                }else if(ImGui::IsKeyPressed(ImGuiKey_V) && !ImGui::IsAnyItemActive()){
                    deselectAllNodes();
                    glm::vec2 pastePosition = screenToWorld(glm::vec2(ImGui::GetMousePos()));
                    if(snap_to_grid) pastePosition = snapToGrid(pastePosition);
                    container->pasteModulesAndConnectionsInPosition(pastePosition, ImGui::GetIO().KeyShift);
                }else if(ImGui::IsKeyPressed(ImGuiKey_X)){
                    container->cutSelectedModulesWithConnections();
                }else if(ImGui::IsKeyPressed(ImGuiKey_D)){
                    container->copySelectedModulesWithConnections();
                    deselectAllNodes();
                    glm::vec2 pastePosition = screenToWorld(glm::vec2(ImGui::GetMousePos()));
                    if(snap_to_grid) pastePosition = snapToGrid(pastePosition);
                    container->pasteModulesAndConnectionsInPosition(pastePosition, ImGui::GetIO().KeyShift);
                }else if(ImGui::IsKeyPressed(ImGuiKey_A)){
                    selectAllNodes();
				}else if(ImGui::IsKeyPressed(ImGuiKey_E)){
					container->encapsulateSelectedNodes();
				}
            }
            else if(ImGui::IsKeyPressed((ImGuiKey_Backspace)) && !ImGui::IsAnyItemActive()){
                container->deleteSelectedModules();
            }
            
            // Alt+Z: reset zoom to 100%, preserving the visible world center
            if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::GetIO().KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Z)){
                glm::vec2 canvasCenter = contentRegionSize * 0.5f;
                glm::vec2 worldCenterBefore = screenToWorld(canvasOrigin + canvasCenter);
                setZoomLevel(ZOOM_DEFAULT);
                glm::vec2 worldCenterAfter = screenToWorld(canvasOrigin + canvasCenter);
                scrolling += (worldCenterAfter - worldCenterBefore);
            }
            
            if (isCreatingConnection && !ImGui::IsMouseDown(0)){
                //Destroy temporal connection
                tempSourceParameter = nullptr;
                tempSinkParameter = nullptr;
                isCreatingConnection = false;
            }
        }
        
        ImGui::PopItemWidth();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        ImGui::EndGroup();
        
                }else{
                    deselectAllNodes();
                }
    ImGui::End();
    
    //TODO: Find better way to to this, when macro created, recoverr focus on canvas, should be its parent. something like. container->getParentCanvas? Or set a id in canvas as "Parent Canvas".
    if(isFirstDraw && parentID != ""){
        ImGuiID parentWinID = ImHashStr(parentID.c_str(), 0, 0);
        ImGuiWindow* parentWin = ImGui::FindWindowByID(parentWinID);
        if(parentWin) ImGui::FocusWindow(parentWin);
    }
    isFirstDraw = false;
    ofPushMatrix();
    ofMultMatrix(glm::inverse(transformationMatrix->get()));
    ofPopMatrix();

    // Restore the previous shared zoom level (stack-like save/restore for nested canvases)
    ofxOceanodeShared::setZoomLevel(previousSharedZoom);
}

//void ofxOceanodeCanvas::mouseScrolled(ofMouseEventArgs &e){
//#ifdef TARGET_OSX
//    if(ofGetKeyPressed(OF_KEY_COMMAND)){
//#else
//    if(ofGetKeyPressed(OF_KEY_CONTROL)){
//#endif
//        float scrollValue = -e.scrollY/100.0;
//        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(e, 0) * getMatrixScale(transformationMatrix->get()) * scrollValue));
//        transformationMatrix->set(glm::scale(transformationMatrix->get(), glm::vec3(1-(scrollValue), 1-(scrollValue), 1)));
//    }else if(ofGetKeyPressed(OF_KEY_ALT)){
//        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(e.scrollY*2, 0, 0)));
//    }else{
//        transformationMatrix->set(translateMatrixWithoutScale(transformationMatrix->get(), glm::vec3(-e.scrollX*2, -e.scrollY*2, 0)));
//    }
//}

glm::vec2 ofxOceanodeCanvas::screenToCanvas(glm::vec2 p){
    glm::vec4 result = transformationMatrix->get() * glm::vec4(p, 0, 1);
    return result;
}

glm::vec2 ofxOceanodeCanvas::canvasToScreen(glm::vec2 p){
    glm::vec4 result = glm::inverse(transformationMatrix->get()) * glm::vec4(p, 0, 1);
    return result;
}

glm::vec2 ofxOceanodeCanvas::snapToGrid(glm::vec2 position){
    if(!snap_to_grid) return position;
	
	// Snap to nearest grid point
	float snappedX = round(position.x / GRID_SIZE) * GRID_SIZE;
	float snappedY = round(position.y / GRID_SIZE) * GRID_SIZE;
    
    return glm::vec2(snappedX, snappedY);
}

glm::vec3 ofxOceanodeCanvas::getMatrixScale(const glm::mat4 &m){
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(m, scale, rotation, translation, skew, perspective);
    return scale;
}

glm::mat4 ofxOceanodeCanvas::translateMatrixWithoutScale(const glm::mat4 &m, glm::vec3 translationVector){
    return glm::translate(glm::mat4(1.0), translationVector) * m;
}

void ofxOceanodeCanvas::deselectAllNodes(){
    for(auto &n: container->getParameterGroupNodesMap()){
        n.second->getNodeGui().setSelected(false);
    }
    container->deselectAllComments();
}

void ofxOceanodeCanvas::selectAllNodes(){
    for(auto &n: container->getParameterGroupNodesMap()){
        n.second->getNodeGui().setSelected(true);
    }
    for(auto &c: container->getComments()){
        c.selected = true;
    }
}

#endif
