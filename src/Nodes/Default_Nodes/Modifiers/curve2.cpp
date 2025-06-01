//
//  curve.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola on 25/05/21.
//
//

#include "curve2.h"
#include "glm/gtx/closest_point.hpp"

//---------------------------------------------------------------
// StandardCoordinateTransformer Implementation
//---------------------------------------------------------------

StandardCoordinateTransformer::StandardCoordinateTransformer()
    : visualWinPos(0, 0)
    , visualCanvasSize(0, 0)
    , canvasWinPos(0, 0)
    , canvasSize(0, 0)
    , gridSnappingEnabled(false)
    , horizontalDivisions(10)
    , verticalDivisions(10)
{
}

glm::vec2 StandardCoordinateTransformer::normalizePoint(const glm::vec2& point) const {
    return glm::vec2(
        ofMap(point.x, visualWinPos.x, visualWinPos.x + visualCanvasSize.x, 0, 1),
        ofMap(point.y, visualWinPos.y, visualWinPos.y + visualCanvasSize.y, 1, 0)
    );
}

glm::vec2 StandardCoordinateTransformer::denormalizePoint(const glm::vec2& point) const {
    return glm::vec2(
        ofMap(point.x, 0, 1, visualWinPos.x, visualWinPos.x + visualCanvasSize.x),
        ofMap(point.y, 1, 0, visualWinPos.y, visualWinPos.y + visualCanvasSize.y)
    );
}

glm::vec2 StandardCoordinateTransformer::safeNormalizePoint(const glm::vec2& point) const {
    // Check if mouse is within the entire canvas (including safe zone)
    if (point.x >= canvasWinPos.x && point.x <= canvasWinPos.x + canvasSize.x &&
        point.y >= canvasWinPos.y && point.y <= canvasWinPos.y + canvasSize.y) {
        
        // Clamp mouse position to visual area boundaries
        float clampedX = std::max(visualWinPos.x, std::min(visualWinPos.x + visualCanvasSize.x, point.x));
        float clampedY = std::max(visualWinPos.y, std::min(visualWinPos.y + visualCanvasSize.y, point.y));
        
        // Normalize the clamped position
        return glm::vec2(
            ofMap(clampedX, visualWinPos.x, visualWinPos.x + visualCanvasSize.x, 0, 1),
            ofMap(clampedY, visualWinPos.y, visualWinPos.y + visualCanvasSize.y, 1, 0)
        );
    }
    
    // If outside entire canvas, use regular normalization
    return glm::vec2(
        ofMap(point.x, visualWinPos.x, visualWinPos.x + visualCanvasSize.x, 0, 1),
        ofMap(point.y, visualWinPos.y, visualWinPos.y + visualCanvasSize.y, 1, 0)
    );
}

void StandardCoordinateTransformer::setVisualBounds(const ImVec2& winPos, const ImVec2& canvasSize) {
    visualWinPos = winPos;
    visualCanvasSize = canvasSize;
}

void StandardCoordinateTransformer::setCanvasBounds(const ImVec2& winPos, const ImVec2& canvasSize) {
    canvasWinPos = winPos;
    this->canvasSize = canvasSize;
}

void StandardCoordinateTransformer::setGridSnapping(bool enabled, int horizontalDivs, int verticalDivs) {
    gridSnappingEnabled = enabled;
    horizontalDivisions = horizontalDivs;
    verticalDivisions = verticalDivs;
}

glm::vec2 StandardCoordinateTransformer::snapToGrid(const glm::vec2& point) const {
    if (!gridSnappingEnabled) {
        return point;
    }
    
    // Snap to grid divisions
    float snappedX = std::round(point.x * horizontalDivisions) / horizontalDivisions;
    float snappedY = std::round(point.y * verticalDivisions) / verticalDivisions;
    
    return glm::vec2(snappedX, snappedY);
}

bool StandardCoordinateTransformer::isWithinCanvas(const glm::vec2& point) const {
    return point.x >= canvasWinPos.x && point.x <= canvasWinPos.x + canvasSize.x &&
           point.y >= canvasWinPos.y && point.y <= canvasWinPos.y + canvasSize.y;
}

bool StandardCoordinateTransformer::isWithinVisualArea(const glm::vec2& point) const {
    return point.x >= visualWinPos.x && point.x <= visualWinPos.x + visualCanvasSize.x &&
           point.y >= visualWinPos.y && point.y <= visualWinPos.y + visualCanvasSize.y;
}

//---------------------------------------------------------------
// StandardCanvasLayout Implementation
//---------------------------------------------------------------

StandardCanvasLayout::StandardCanvasLayout()
    : safeZonePadding(DEFAULT_SAFE_ZONE_PADDING) {
}

void StandardCanvasLayout::calculateLayout(const ImVec2& availableSize, ImVec2& canvasPos, ImVec2& canvasSize,
                                         ImVec2& visualPos, ImVec2& visualSize) {
    // Get the available space
    canvasPos = ImGui::GetCursorScreenPos();
    canvasSize = availableSize;
    
    // Visual drawing area (smaller, centered within the canvas)
    visualPos = ImVec2(canvasPos.x + safeZonePadding, canvasPos.y + safeZonePadding);
    visualSize = ImVec2(canvasSize.x - 2 * safeZonePadding, canvasSize.y - 2 * safeZonePadding);
    
    // Ensure visual area is not negative
    if (visualSize.x < 50.0f) visualSize.x = 50.0f;
    if (visualSize.y < 50.0f) visualSize.y = 50.0f;
}

void StandardCanvasLayout::renderGrid(ImDrawList* drawList, const ImVec2& visualPos, const ImVec2& visualSize,
                                    int horizontalDivs, int verticalDivs) {
    ImU32 GRID_COLOR = IM_COL32(120, 120, 120, 60);
    ImVec2 cell_sz = ImVec2(visualSize.x / horizontalDivs, visualSize.y / verticalDivs);
    
    // Draw vertical lines (skip first and last by starting at cell_sz.x and stopping before visualSize.x)
    for (float x = cell_sz.x; x < visualSize.x - cell_sz.x * 0.5f; x += cell_sz.x)
        drawList->AddLine(ImVec2(x, 0.0f) + visualPos, ImVec2(x, visualSize.y) + visualPos, GRID_COLOR);
    
    // Draw horizontal lines (skip first and last by starting at cell_sz.y and stopping before visualSize.y)
    for (float y = cell_sz.y; y < visualSize.y - cell_sz.y * 0.5f; y += cell_sz.y)
        drawList->AddLine(ImVec2(0.0f, y) + visualPos, ImVec2(visualSize.x, y) + visualPos, GRID_COLOR);
}

void StandardCanvasLayout::renderBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) {
    // Optional: Could add background rendering here if needed
    // Currently not used in the original implementation
}

float StandardCanvasLayout::getSafeZonePadding() const {
    return safeZonePadding;
}

void StandardCanvasLayout::setSafeZonePadding(float padding) {
    safeZonePadding = padding;
}

float StandardCanvasLayout::calculateWindowHeight(const ImVec2& windowSize, bool showInfoPanel) const {
    if (showInfoPanel) {
        // Current behavior - calculate widget height and subtract from total
        float widgetAreaHeight = 0.0f;
        
        // Estimate widget area height based on content
        // Base height for "Curve Parameters" text and spacing
        widgetAreaHeight += ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y * 2;
        
        // Height for separator
        widgetAreaHeight += 1.0f + ImGui::GetStyle().ItemSpacing.y;
        
        // Height for parameter display (2-3 lines of text)
        widgetAreaHeight += ImGui::GetTextLineHeight() * 3 + ImGui::GetStyle().ItemSpacing.y * 2;
        
        // Add some padding for safety
        widgetAreaHeight += ImGui::GetStyle().WindowPadding.y * 2;
        
        return std::max(50.0f, windowSize.y - widgetAreaHeight);
    } else {
        // Use all available height for curve area
        return std::max(50.0f, windowSize.y);
    }
}

//---------------------------------------------------------------
// ShowAllCurveManager Implementation
//---------------------------------------------------------------

ShowAllCurveManager::ShowAllCurveManager()
    : curves(nullptr), activeCurve(nullptr), numCurves(nullptr), outputs(nullptr) {
}

void ShowAllCurveManager::setCurveData(std::vector<CurveData>* curvesPtr, ofParameter<int>* activeCurvePtr,
                                      ofParameter<int>* numCurvesPtr, std::vector<std::shared_ptr<ofParameter<std::vector<float>>>>* outputsPtr) {
    curves = curvesPtr;
    activeCurve = activeCurvePtr;
    numCurves = numCurvesPtr;
    outputs = outputsPtr;
}

void ShowAllCurveManager::setCallbacks(std::function<void()> recalculate, std::function<void()> addCurve, std::function<void(int)> removeCurve) {
    recalculateCallback = recalculate;
    addCurveCallback = addCurve;
    removeCurveCallback = removeCurve;
}

void ShowAllCurveManager::renderInterface() {
    if (!curves || !activeCurve || !numCurves || !outputs) return;
    
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
    
    handleAddCurve();
    ImGui::SameLine();
    handleRemoveCurve();
    ImGui::SameLine();
    handleResetCurve();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0,1.0,1.0,1.0));
    renderCurveSelector();
    ImGui::PopStyleColor();
    
    renderCurveProperties();
    
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void ShowAllCurveManager::handleAddCurve() {
    if (ImGui::Button("[+]") && validateCurveLimit()) {
        if (addCurveCallback) {
            addCurveCallback();
        } else {
            handleCurveAddition();
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add Curve");
    }
}

void ShowAllCurveManager::handleRemoveCurve() {
    if (ImGui::Button("[-]") && curves->size() > 1) {
        if (removeCurveCallback) {
            removeCurveCallback(activeCurve->get());
        } else {
            handleCurveRemoval(activeCurve->get());
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Remove Curve");
    }
}

void ShowAllCurveManager::handleResetCurve() {
    if(ImGui::Button("[Reset]")) {
        if(activeCurve->get() >= 0 && activeCurve->get() < curves->size()) {
            auto& currentCurve = (*curves)[activeCurve->get()];
            
            // Clear existing points and lines
            currentCurve.points.clear();
            currentCurve.lines.clear();
            
            // Create default curve with 2 points: (0,0) and (1,1)
            currentCurve.points.emplace_back(0, 0);
            currentCurve.points.emplace_back(1, 1);
            
            // Set points as not newly created
            currentCurve.points.front().firstCreated = false;
            currentCurve.points.back().firstCreated = false;
            
            // Create default line segment with default values
            currentCurve.lines.emplace_back();
            
            // Trigger recalculation
            if (recalculateCallback) {
                recalculateCallback();
            }
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reset Curve");
    }
}

void ShowAllCurveManager::renderCurveSelector() {
    // Active curve selection dropdown with curve names
    string activeCurveName = (activeCurve->get() == -1) ? "None" :
        ((activeCurve->get() < curves->size()) ? (*curves)[activeCurve->get()].name.get() : "Invalid");
    
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::BeginCombo("##ActiveCurveEditor", activeCurveName.c_str())) {
        // Add "None" option
        bool isNoneSelected = (activeCurve->get() == -1);
        if (ImGui::Selectable("None", isNoneSelected)) {
            *activeCurve = -1;
        }
        if (isNoneSelected) {
            ImGui::SetItemDefaultFocus();
        }
        
        // Add curve options with color indicators and names
        for (int i = 0; i < curves->size(); i++) {
            bool isSelected = (activeCurve->get() == i);
            string curveName = (*curves)[i].name.get();
            
            // Draw color indicator
            ofColor curveColor = (*curves)[i].color.get();
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(
                ImVec2(cursorPos.x + 2, cursorPos.y + 2),
                ImVec2(cursorPos.x + 12, cursorPos.y + 12),
                IM_COL32(curveColor.r, curveColor.g, curveColor.b, 255)
            );
            
            // Add spacing for color indicator
            ImGui::Dummy(ImVec2(16, 0));
            ImGui::SameLine();
            
            if (ImGui::Selectable(curveName.c_str(), isSelected)) {
                *activeCurve = i;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void ShowAllCurveManager::renderCurveProperties() {
    // Add curve parameters on the same line (only when a curve is selected)
    if(activeCurve->get() >= 0 && activeCurve->get() < curves->size()) {
        auto& currentCurve = (*curves)[activeCurve->get()];
        
        ImGui::SameLine();
        
        // Enabled checkbox (compact)
        bool enabled = currentCurve.enabled.get();
        if (ImGui::Checkbox("##Enabled", &enabled)) {
            currentCurve.enabled = enabled;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enabled");
        }
        
        ImGui::SameLine();
        
        // Color parameter (compact)
        float colorArray[4] = {
            currentCurve.color.get().r / 255.0f,
            currentCurve.color.get().g / 255.0f,
            currentCurve.color.get().b / 255.0f,
            currentCurve.color.get().a / 255.0f
        };
        ImGui::SetNextItemWidth(60.0f);
        if (ImGui::ColorEdit4("##Color", colorArray, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
            currentCurve.color = ofColor(
                colorArray[0] * 255,
                colorArray[1] * 255,
                colorArray[2] * 255,
                colorArray[3] * 255
            );
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Color");
        }
        
        ImGui::SameLine();
        
        // Name text input
        char nameBuffer[256];
        string currentName = currentCurve.name.get();
        strncpy(nameBuffer, currentName.c_str(), sizeof(nameBuffer) - 1);
        nameBuffer[sizeof(nameBuffer) - 1] = '\0';
        
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
            string newName = string(nameBuffer);
            if (!newName.empty()) {
                // Check for duplicate names and add "_" if necessary
                string finalName = generateUniqueName(newName);
                currentCurve.name = finalName;
                
                // Update output parameter name
                if (activeCurve->get() < outputs->size() && (*outputs)[activeCurve->get()]) {
                    (*outputs)[activeCurve->get()]->setName(finalName);
                }
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Curve Name");
        }
    }
}

void ShowAllCurveManager::renderCurveControls() {
    renderInterface();
}

bool ShowAllCurveManager::handleCurveSelection(int& activeCurveIndex) {
    // This is handled by renderCurveSelector() through the dropdown
    return false;
}

bool ShowAllCurveManager::handleCurveAddition() {
    if (!validateCurveLimit()) return false;
    
    // Create new curve with default data
    curves->emplace_back();
    auto& newCurve = curves->back();
    
    // Set unique name
    int curveNumber = curves->size();
    string baseName = "Curve " + ofToString(curveNumber);
    newCurve.name = generateUniqueName(baseName);
    
    // Set unique color (cycle through predefined colors)
    ofColor colors[] = {
        ofColor(255, 128, 0),   // Orange
        ofColor(0, 255, 128),   // Green
        ofColor(128, 0, 255),   // Purple
        ofColor(255, 255, 0),   // Yellow
        ofColor(0, 255, 255),   // Cyan
        ofColor(255, 0, 255),   // Magenta
        ofColor(128, 255, 0),   // Lime
        ofColor(255, 0, 128)    // Pink
    };
    newCurve.color = colors[(curves->size() - 1) % 8];
    
    *numCurves = curves->size();
    return true;
}

bool ShowAllCurveManager::handleCurveRemoval(int curveIndex) {
    if (curves->size() <= 1 || curveIndex < 0 || curveIndex >= curves->size()) {
        return false;
    }
    
    // Remove the curve
    curves->erase(curves->begin() + curveIndex);
    
    // Adjust active curve index if necessary
    if (activeCurve->get() >= curves->size()) {
        *activeCurve = curves->size() - 1;
    } else if (activeCurve->get() == curveIndex && curveIndex > 0) {
        *activeCurve = curveIndex - 1;
    }
    
    *numCurves = curves->size();
    return true;
}

void ShowAllCurveManager::renderCurveProperties(CurveData& curve) {
    // This is handled by renderCurveProperties() method
}

bool ShowAllCurveManager::validateCurveName(const std::string& name, int excludeIndex) {
    if (name.empty()) return false;
    
    for (int i = 0; i < curves->size(); i++) {
        if (i != excludeIndex && (*curves)[i].name.get() == name) {
            return false;
        }
    }
    return true;
}

bool ShowAllCurveManager::validateCurveLimit() const {
    return curves->size() < 16;
}

std::string ShowAllCurveManager::generateUniqueName(const std::string& baseName) const {
    string finalName = baseName;
    bool nameExists = true;
    while (nameExists) {
        nameExists = false;
        for (int i = 0; i < curves->size(); i++) {
            if ((*curves)[i].name.get() == finalName) {
                nameExists = true;
                finalName += "_";
                break;
            }
        }
    }
    return finalName;
}

//---------------------------------------------------------------
// curve2 Implementation
//---------------------------------------------------------------

curve2::curve2() : ofxOceanodeNodeModel("curve2") {
	// Initialize with one curve
	curves.resize(1);
	
	// Set default name for first curve
	curves[0].name.set("Name", "Curve 1");
	
	showCurveLabels = false;
	curveHitTestRadius = 8.0f;
	needsRedraw = true;
	
	// Initialize pointers to point to the first curve
	points = &curves[0].points;
	lines = &curves[0].lines;
	colorParam = &curves[0].color;
	globalQ = &curves[0].globalQ;
	
	// Initialize modular architecture components
	initializeComponents();
}

void curve2::setup() {
	color = ofColor(255,128,0,255);
	description = "Advanced curve editor with asymmetric logistic segments. \n| Double-click: Create points \n| Drag: Move points \n| Right-click: Delete \n| Shift+drag: Asymmetry (vertical) + Inflection (horizontal) \n| Ctrl+drag: B parameter (horizontal) \n| Visual feedback: Yellow=Inflection, Cyan=Asymmetry, Red=B parameter \n| Snap to Grid: Enable for precise grid-aligned positioning";
	
	// Initialize multi-curve system
	numCurves=1;

	// Set up parameters
	addParameter(input.set("Input", {0}, {0}, {1}));
	addParameter(showWindowAll.set("Show All", true));
	addParameter(showWindowSplit.set("Show Split", false));
	addOutputParameter(allCurvesOutput.set("All Curves", {0}, {0}, {1}));
		
	// Initialize outputs based on numCurves
	outputs.resize(numCurves.get());
	for(int i = 0; i < outputs.size(); i++){
		outputs[i] = make_shared<ofParameter<vector<float>>>();
		string outputName = (i < curves.size()) ? curves[i].name.get() : ("Curve " + ofToString(i + 1));
		if(outputs[i]) {
			addOutputParameter(outputs[i]->set(outputName, {0}, {0}, {1}));
		}
	}
	
	// Initialize parameters (but don't add them to inspector yet)
	numHorizontalDivisions.set("Hor Div", 8, 1, 512);
	numVerticalDivisions.set("Vert Div", 4, 1, 512);
	snapToGrid.set("Snap to Grid", false);
	showInfo.set("Show Info", false);
	
	if(colorParam != nullptr){
		colorListener = colorParam->newListener([this](ofColor &c){
			color = c;
		});
	}
	
	// Add the tabbed inspector GUI
	addInspectorParameter(inspectorGui.set("Inspector", [this](){
		renderInspectorInterface();
	}));
	
	// Set up listeners
	listeners.push(input.newListener([&](vector<float> &vf){
		recalculate();
	}));
	
	// Set input parameter bounds to use constants
	input.setMin(vector<float>(1, minX));
	input.setMax(vector<float>(1, maxX));
	
	// Multi-curve listeners
	listeners.push(numCurves.newListener([this](int &newCount){
		onNumCurvesChanged(newCount);
	}));
	listeners.push(activeCurve.newListener([this](int &newIndex){
		onActiveCurveChanged(newIndex);
	}));
}

void curve2::initializeComponents() {
	// Initialize the coordinate transformer component
	coordinateTransformer = std::make_shared<StandardCoordinateTransformer>();
	
	// Initialize the canvas layout component
	canvasLayout = std::make_shared<StandardCanvasLayout>();
	
	// Initialize the curve manager component
	curveManager = std::make_shared<ShowAllCurveManager>();
	curveManager->setCurveData(&curves, &activeCurve, &numCurves, &outputs);
	curveManager->setCallbacks(
		[this]() { recalculate(); },  // recalculate callback
		[this]() { addCurve(); numCurves = curves.size(); },  // add curve callback
		[this](int index) { removeCurve(index); numCurves = curves.size(); }  // remove curve callback
	);
	
	// Initialize the visual feedback component
	visualFeedback = std::make_shared<StandardVisualFeedback>();
	visualFeedback->setCoordinateTransformer(coordinateTransformer);
	visualFeedback->setShowInfo(showInfo.get());
	
	// Initialize the point interaction handler component
	pointHandler = std::make_shared<StandardPointInteractionHandler>();
	pointHandler->setCoordinateTransformer(coordinateTransformer);
	pointHandler->setRecalculateCallback([this]() { recalculate(); });
	
	// Initialize the parameter controller component
	parameterController = std::make_shared<StandardParameterController>();
	parameterController->setCoordinateTransformer(coordinateTransformer);
	parameterController->setRecalculateCallback([this]() { recalculate(); });
	
	// Initialize the curve renderer component
	curveRenderer = std::make_shared<MultiCurveRenderer>();
	curveRenderer->setCoordinateTransformer(coordinateTransformer);
	curveRenderer->setRenderQuality(MultiCurveRenderer::RenderQuality::MEDIUM);
}

void curve2::draw(ofEventArgs &args){
	
	if(showWindowAll){
		string modCanvasID = canvasID == "Canvas" ? "" : (canvasID + "/");
		string windowTitle = modCanvasID + "Curve2 " + ofToString(getNumIdentifier());
		if(ImGui::Begin(windowTitle.c_str(), (bool *)&showWindowAll.get()))
		{
			ImGui::SameLine();
			ImGui::BeginGroup();
			
			const ImVec2 NODE_WINDOW_PADDING(8.0f, 7.0f);
			
			// Render curve management interface using the ShowAllCurveManager component
			curveManager->renderInterface();
			
			// Calculate available height for curve editor using canvas layout component
			ImVec2 windowSize = ImGui::GetContentRegionAvail();
			float curveEditorHeight = canvasLayout->calculateWindowHeight(windowSize, showInfo);
			
			// Create our child canvas with calculated height
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(25, 25, 25, 200));
			ImGui::BeginChild("scrolling_region", ImVec2(0, curveEditorHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
			ImGui::PushItemWidth(120.0f);
			
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			
			// Calculate canvas layout using the canvas layout component
			ImVec2 available_sz = ImGui::GetContentRegionAvail();
			ImVec2 win_pos, canvas_sz, visual_win_pos, visual_canvas_sz;
			canvasLayout->calculateLayout(available_sz, win_pos, canvas_sz, visual_win_pos, visual_canvas_sz);
			
			// Render grid using the canvas layout component
			canvasLayout->renderGrid(draw_list, visual_win_pos, visual_canvas_sz, numHorizontalDivisions, numVerticalDivisions);

			// Make the ENTIRE canvas area interactive (including safe zone) - this is the key fix!
			ImGui::InvisibleButton("canvas", canvas_sz);
			bool canvasHovered = ImGui::IsItemHovered();
			bool canvasClicked = ImGui::IsItemClicked(0);
			bool canvasDoubleClicked = ImGui::IsItemClicked(0) && ImGui::IsMouseDoubleClicked(0);
			
			ImGui::PopItemWidth();
			
			// Configure coordinate transformer with current bounds
			coordinateTransformer->setVisualBounds(visual_win_pos, visual_canvas_sz);
			coordinateTransformer->setCanvasBounds(win_pos, canvas_sz);
			coordinateTransformer->setGridSnapping(snapToGrid.get(), numHorizontalDivisions.get(), numVerticalDivisions.get());
			
			// Direct use of coordinate transformer components (lambdas removed for cleaner architecture)
			
			// PHASE 5: Point interaction handling using StandardPointInteractionHandler component
			hoveredPointIndex = -1;
			hoveredSegmentIndex = -1;
			bool someItemClicked = false;
			
			// Only process point interactions when a curve is selected and pointHandler is available
			if(activeCurve.get() >= 0 && points != nullptr && lines != nullptr && pointHandler){
				// Detect hovered point using the component
				hoveredPointIndex = pointHandler->detectHoveredPoint(glm::vec2(ImGui::GetMousePos()), *points);
				
				// If no point is hovered, check for segment hover on active curve
				if(hoveredPointIndex == -1 && canvasHovered && activeCurve.get() < curves.size() && curves[activeCurve.get()].enabled.get()){
					glm::vec2 normalizedMousePos = coordinateTransformer->safeNormalizePoint(glm::vec2(ImGui::GetMousePos()));
					for(int i = 0; i < points->size()-1; i++){
						if(normalizedMousePos.x >= (*points)[i].point.x &&
						   normalizedMousePos.x <= (*points)[i+1].point.x &&
						   (*lines)[i].type == LINE2_TENSION){
							hoveredSegmentIndex = i;
							break;
						}
					}
				}
				
				// Create interaction context
				StandardPointInteractionHandler::InteractionContext context;
				context.snapToGrid = snapToGrid.get();
				context.horizontalDivisions = numHorizontalDivisions.get();
				context.verticalDivisions = numVerticalDivisions.get();
				context.minX = minX;
				context.maxX = maxX;
				
				// Process point interactions using the component
				auto result = pointHandler->processInteraction(canvasHovered, canvasClicked, canvasDoubleClicked, *points, *lines, context);
				
				// Update state based on interaction result
				if(result.hoveredPointIndex >= 0){
					hoveredPointIndex = result.hoveredPointIndex;
				}
				someItemClicked = pointHandler->hasItemClicked();
				
				// Trigger recalculation if needed
				if(result.needsRecalculation){
					recalculate();
				}
			}
			
			// Reset drag states on mouse release
			if(ImGui::IsMouseReleased(0) && pointHandler){
				pointHandler->resetDragStates();
			}
			
			// ========================================================================
			// PHASE 6: Parameter Control System
			// ========================================================================
			// Handle Shift+drag and Ctrl+drag parameter operations using the modular
			// StandardParameterController component
			// ========================================================================
			
			if(activeCurve.get() >= 0 && points != nullptr && lines != nullptr && parameterController){
				bool shiftPressed = ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_LeftShift));
				bool ctrlPressed = ImGui::GetIO().KeyCtrl;
				
				// Handle parameter drag operations through the component
				parameterController->handleParameterDrag(shiftPressed, ctrlPressed, canvasHovered, someItemClicked, *points, *lines);
			}
						
			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(2);
			
			// Conditional widget area below curve editor
			if (showInfo) {
				ImGui::Spacing();
				ImGui::Separator();
				
				// Determine what to display based on current editing context
				bool showPointInfo = false;
				bool showSegmentInfo = false;
				int activePointIndex = -1;
				int activeSegmentIndex = -1;
				
				// Check for active point dragging (only if curve is selected)
				if(activeCurve.get() >= 0 && points != nullptr){
					for(int i = 0; i < points->size(); i++){
						if((*points)[i].drag == 3){
							showPointInfo = true;
							activePointIndex = i;
							break;
						}
					}
				}
				
				// Check for active tension dragging
				if(parameterController && parameterController->isTensionDragActive() && parameterController->getTensionDragSegmentIndex() >= 0){
					showSegmentInfo = true;
					activeSegmentIndex = parameterController->getTensionDragSegmentIndex();
				}
				
				// Check for active B parameter dragging
				if(parameterController && parameterController->isBDragActive() && parameterController->getBDragSegmentIndex() >= 0){
					showSegmentInfo = true;
					activeSegmentIndex = parameterController->getBDragSegmentIndex();
				}
				
				// If no active dragging, use hovered point
				if(!showPointInfo && !showSegmentInfo && hoveredPointIndex >= 0){
					showPointInfo = true;
					activePointIndex = hoveredPointIndex;
				}
				
				// Display point information (only if curve is selected)
				if(activeCurve.get() >= 0 && points != nullptr && showPointInfo && activePointIndex >= 0 && activePointIndex < points->size()){
					ImGui::Text("Point %d:", activePointIndex);
					ImGui::SameLine();
					
					// Point coordinates (0-1 range)
					float pointX = ofMap((*points)[activePointIndex].point.x, 0, 1, minX, maxX);
					float pointY = (*points)[activePointIndex].point.y; // Direct 0-1 range
					
					ImGui::PushItemWidth(80);
					ImGui::Text("X: %.3f", pointX);
					ImGui::SameLine();
					ImGui::Text("Y: %.3f", pointY);
					ImGui::PopItemWidth();
				}
				
				// Display segment information
				if(showSegmentInfo && lines != nullptr && globalQ != nullptr && activeSegmentIndex >= 0 && activeSegmentIndex < lines->size()){
					if(showPointInfo) ImGui::Spacing();
					
					ImGui::Text("Segment %d:", activeSegmentIndex);
					ImGui::SameLine();
					
					// Line type
					const char* lineTypeName = ((*lines)[activeSegmentIndex].type == LINE2_HOLD) ? "HOLD" : "TENSION";
					ImGui::Text("Type: %s", lineTypeName);
					
					// Show tension parameters only for TENSION segments
					if((*lines)[activeSegmentIndex].type == LINE2_TENSION){
						ImGui::Spacing();
						
						// First row: Asymmetry and Inflection
						ImGui::PushItemWidth(80);
						ImGui::Text("Asymmetry: %.3f", (*lines)[activeSegmentIndex].tensionExponent);
						ImGui::SameLine();
						ImGui::Text("Inflection: %.3f", (*lines)[activeSegmentIndex].inflectionX);
						ImGui::PopItemWidth();
						
						// Second row: Segment B and Global Q
						ImGui::PushItemWidth(80);
						ImGui::Text("Segment B: %.2f", (*lines)[activeSegmentIndex].segmentB);
						ImGui::SameLine();
						ImGui::Text("Global Q: %.1f", globalQ->get());
						ImGui::PopItemWidth();
					}
				}
				
				// Show global parameters when no specific element is active
				if(!showPointInfo && !showSegmentInfo){
					ImGui::Text("Global Parameters:");
					ImGui::SameLine();
					ImGui::PushItemWidth(80);
					if(globalQ != nullptr){
						ImGui::Text("Q: %.1f", globalQ->get());
					} else {
						ImGui::Text("Q: N/A");
					}
					ImGui::PopItemWidth();
					
					ImGui::Spacing();
					ImGui::Text("Hover over points or drag to see details");
				}
			}
			
			// Re-push style vars for drawing operations
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 200));
			
			// ========================================================================
			// PHASE 4: Multi-curve rendering system
			// ========================================================================
			// This implementation provides:
			// - All enabled curves are drawn simultaneously with different colors/opacity
			// - Active curve is highlighted with full opacity and thicker lines (3px)
			// - Inactive curves are dimmed (50% opacity) with thinner lines (1.5px)
			// - Only the active curve shows interactive points for editing
			// - Proper layering: inactive curves drawn first, active curve on top
			// - Each curve uses its own color, opacity, and mathematical parameters
			// ========================================================================
			
			// Draw all enabled curves with proper visual hierarchy
			
			// ========================================================================
			// PHASE 3: Multi-Curve Rendering using MultiCurveRenderer component
			// ========================================================================
			// This implementation provides:
			// - Two-pass rendering system (inactive curves at 50% opacity, active curve at full opacity)
			// - Hold segment rendering (simple horizontal-then-vertical line segments)
			// - Tension segment rendering (complex asymmetric logistic curves with adaptive resolution)
			// - Extension lines (dotted lines extending curves to canvas edges)
			// - Curve drawing with proper color management and line widths
			// ========================================================================
			
			if(curveRenderer){
				// Setup render context
				MultiCurveRenderer::RenderContext renderContext;
				renderContext.canvasPos = visual_win_pos;
				renderContext.canvasSize = visual_canvas_sz;
				renderContext.performanceMode = false;
				renderContext.quality = MultiCurveRenderer::RenderQuality::MEDIUM;
				
				// Render all curves using the MultiCurveRenderer component
				curveRenderer->renderCurves(draw_list, curves, activeCurve.get(), renderContext, parameterController);
			}
			
			// ========================================================================
			// PHASE 4: Visual Feedback Rendering using StandardVisualFeedback component
			// ========================================================================
			// This implementation provides:
			// - Dotted yellow lines for inflection points during drag operations
			// - Dotted cyan lines for asymmetry values during parameter adjustments
			// - Dotted red lines for B parameter positions during curve modifications
			// - Curve name labels with proper color coding and opacity
			// - Point highlighting and input value indicators
			// ========================================================================
			
			if(visualFeedback && points != nullptr && lines != nullptr){
				// Update component state with current interaction states from parameterController
				if(parameterController){
					visualFeedback->updateDragStates(parameterController->isTensionDragActive(), parameterController->isBDragActive(),
													parameterController->getTensionDragSegmentIndex(), parameterController->getBDragSegmentIndex());
				} else {
					visualFeedback->updateDragStates(false, false, -1, -1);
				}
				visualFeedback->updateHoverStates(hoveredSegmentIndex, hoveredPointIndex, canvasHovered);
				
				// Render all visual feedback components
				visualFeedback->renderFeedback(draw_list, *points, *lines, curves,
											 activeCurve.get(), showCurveLabels, visual_win_pos);
			}
			
			// Always Draw vertices for refernce for editing the curves
			if(true)
			{
				// "None" selected: Draw all vertices from all enabled curves as visual reference (no interaction)
				for(int curveIdx = 0; curveIdx < curves.size(); curveIdx++){
					auto& curve = curves[curveIdx];
					
					// Skip disabled curves
					if(!curve.enabled.get()) continue;
					
					ofColor curveColor = curve.color.get();
					
					for(int i = 0; i < curve.points.size(); i++){
						glm::vec2 pointPos = coordinateTransformer->denormalizePoint(curve.points[i].point);
						
						// Draw small, non-interactive vertices with curve color
						draw_list->AddCircleFilled(pointPos, 3, IM_COL32(curveColor.r, curveColor.g, curveColor.b, 180));
						draw_list->AddCircle(pointPos, 3, IM_COL32(255, 255, 255, 100), 0, 1); // Light outline
					}
				}
			}
			// Interactive drawing points
			if(activeCurve.get() >= 0 && activeCurve.get() < curves.size() && curves[activeCurve.get()].enabled.get() && points != nullptr){
				// Specific curve selected: Draw interactive points for the active curve only
				auto& activeCurveData = curves[activeCurve.get()];
				ofColor activeCurveColor = activeCurveData.color.get();
				
				for(int i = 0; i < points->size(); i++){
					auto &p = (*points)[i];
					glm::vec2 pointPos = coordinateTransformer->denormalizePoint(p.point);
					
					if(i == hoveredPointIndex){
						// Draw highlighted point with larger radius and different color
						draw_list->AddCircleFilled(pointPos, 8, IM_COL32(255, 255, 255, 255)); // White highlight
						draw_list->AddCircle(pointPos, 8, IM_COL32(activeCurveColor.r, activeCurveColor.g, activeCurveColor.b, 255), 0, 2); // Active curve colored outline
						draw_list->AddCircleFilled(pointPos, 5, IM_COL32(0, 0, 0, 255)); // Original black center
					} else {
						// Draw normal point for active curve
						draw_list->AddCircleFilled(pointPos, 5, IM_COL32(activeCurveColor.r, activeCurveColor.g, activeCurveColor.b,255));
					}
				}
			}
			
			for(auto &v : input.get()){
				auto mv = ofMap(v, minX, maxX, 0, 1);
				draw_list->AddLine(coordinateTransformer->denormalizePoint(glm::vec2(mv, 0)), coordinateTransformer->denormalizePoint(glm::vec2(mv, 1)), IM_COL32(127, 127, 127, 127));
			}
			
			// Pop the style vars that were re-pushed for drawing operations
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(2);
			
			ImGui::EndGroup();
		}
		ImGui::End();
	}
	
	// New split window functionality
	if(showWindowSplit){
		string modCanvasID = canvasID == "Canvas" ? "" : (canvasID + "/");
		string splitWindowTitle = modCanvasID + "Curve2 Split " + ofToString(getNumIdentifier());
		if(ImGui::Begin(splitWindowTitle.c_str(), (bool *)&showWindowSplit.get()))
		{
			// Empty window for now as requested
			ImGui::Text("Split view window - coming soon!");
		}
		ImGui::End();
	}
}

void curve2::recalculate()
{
	if(input.get().empty()) return;
	
	const size_t inputSize = input.get().size();
	
	// Initialize consolidated output structure - simple vector with one value per curve
	vector<float> consolidatedOutput(curves.size(), 0.0f);
	
	// Process each curve and generate its corresponding output
	for(int curveIdx = 0; curveIdx < curves.size() && curveIdx < outputs.size(); curveIdx++){
		auto& curve = curves[curveIdx];
		
		// PHASE 5: Error handling - validate curve data
		if(curve.points.empty()){
			// Initialize with default points if empty
			curve.points.emplace_back(0, 0);
			curve.points.emplace_back(1, 1);
			curve.points.front().firstCreated = false;
			curve.points.back().firstCreated = false;
			curve.lines.emplace_back();
		}
		
		vector<float> tempOut(inputSize);
		
		// PHASE 5: Performance optimization - early exit for disabled curves
		if(!curve.enabled.get()){
			// Output zero values for disabled curves
			fill(tempOut.begin(), tempOut.end(), 0.0f);
			// DIAGNOSTIC: Check shared_ptr before assignment
			if(outputs[curveIdx]) {
				*outputs[curveIdx] = tempOut;
			} else {
				ofLogError("curve2::recalculate") << "NULL shared_ptr for disabled curve " << curveIdx;
			}
			// Update consolidated output with zero for this curve
			consolidatedOutput[curveIdx] = 0.0f;
			continue;
		}
		
		for(int i = 0; i < input.get().size(); i++){
			auto p = ofMap(input.get().at(i), minX, maxX, 0, 1);
			if(p <= curve.points[0].point.x){
				tempOut[i] = curve.points[0].point.y; // Direct 0-1 range output
			}else if(p >= curve.points.back().point.x){
				tempOut[i] = curve.points.back().point.y; // Direct 0-1 range output
			}else{
				for(int j = 1; j < curve.points.size(); j++){
					if(p < curve.points[j].point.x){
						if(curve.lines[j-1].type == LINE2_HOLD){
							float resultY = curve.points[j-1].point.y;
							tempOut[i] = resultY; // Direct 0-1 range output
						}
						else if(curve.lines[j-1].type == LINE2_TENSION){
							// Calculate normalized X position within the segment
							float normalizedX = ofMap(p, curve.points[j-1].point.x, curve.points[j].point.x, 0, 1);
							
							// Asymmetric logistic parameters
							float A = curve.points[j-1].point.y;      // Y value of segment start point
							float K = curve.points[j].point.y;        // Y value of segment end point
							float M = curve.lines[j-1].inflectionX;   // Inflection point X position (0-1)
							float nu = curve.lines[j-1].tensionExponent; // Asymmetry parameter ν
							float B = curve.lines[j-1].segmentB;      // Per-segment B parameter
							float Q = curve.lines[j-1].segmentQ;      // Per-segment Q parameter (FIXED: no longer global)
							
							// Calculate normalization values at segment endpoints
							auto logisticFunction = [&](float x) -> float {
								float logisticTerm = 1.0f + Q * exp(-B * (x - M));
								return A + (K - A) / pow(logisticTerm, 1.0f / nu);
							};
							
							float logistic_0 = logisticFunction(0.0f); // Value at segment start
							float logistic_1 = logisticFunction(1.0f); // Value at segment end
							float logisticRange = logistic_1 - logistic_0;
							
							float resultY;
							// Handle edge case where logistic range is very small
							if(abs(logisticRange) > 1e-6f){
								// Apply normalized logistic function to guarantee endpoint continuity
								float raw_logistic = logisticFunction(normalizedX);
								resultY = A + (K - A) * (raw_logistic - logistic_0) / logisticRange;
							} else {
								// Fall back to linear interpolation if logistic range is too small
								resultY = A + (K - A) * normalizedX;
							}
							
							tempOut[i] = resultY; // Direct 0-1 range output
						}
						break;
					}
				}
			}
			// Update consolidated output with this curve's result (use first input value)
			if(i == 0) {
				consolidatedOutput[curveIdx] = tempOut[i];
			}
		}
		if(outputs[curveIdx]) {
			*outputs[curveIdx] = tempOut;
		}
	}
	
	// Update the consolidated output parameter
	allCurvesOutput = consolidatedOutput;
}

void curve2::presetSave(ofJson &json){
	// Save multi-curve data
	json["NumCurves"] = curves.size();
	json["NumOutputs"] = outputs.size();
	json["ActiveCurve"] = activeCurve.get();
	
	// Save consolidated output parameter
	json["AllCurvesOutput"] = allCurvesOutput.get();
	
	for(int curveIdx = 0; curveIdx < curves.size(); curveIdx++){
		auto& curve = curves[curveIdx];
		auto& curveJson = json["Curves"][curveIdx];
		
		// Save curve metadata
		curveJson["Name"] = curve.name.get();
		curveJson["Color"]["r"] = curve.color.get().r;
		curveJson["Color"]["g"] = curve.color.get().g;
		curveJson["Color"]["b"] = curve.color.get().b;
		curveJson["Color"]["a"] = curve.color.get().a;
		curveJson["Enabled"] = curve.enabled.get();
		curveJson["GlobalQ"] = curve.globalQ.get();
		
		// Save points
		curveJson["NumPoints"] = curve.points.size();
		for(int i = 0; i < curve.points.size(); i++){
			curveJson["Points"][i]["Point"]["x"] = curve.points[i].point.x;
			curveJson["Points"][i]["Point"]["y"] = curve.points[i].point.y;
		}
		
		// Save lines
		for(int i = 0; i < curve.lines.size(); i++){
			curveJson["Lines"][i]["Type"] = curve.lines[i].type;
			curveJson["Lines"][i]["TensionExponent"] = curve.lines[i].tensionExponent;
			curveJson["Lines"][i]["InflectionX"] = curve.lines[i].inflectionX;
			curveJson["Lines"][i]["SegmentB"] = curve.lines[i].segmentB;
			curveJson["Lines"][i]["SegmentQ"] = curve.lines[i].segmentQ;
		}
	}
}

void curve2::presetRecallAfterSettingParameters(ofJson &json){
	try{
		// Check if this is a new multi-curve format
		if(json.contains("NumCurves") && json.contains("Curves")){
			// Load multi-curve format
			int numCurvesFromJson = json["NumCurves"];
			curves.resize(numCurvesFromJson);
			
			for(int curveIdx = 0; curveIdx < numCurvesFromJson; curveIdx++){
				auto& curveJson = json["Curves"][curveIdx];
				auto& curve = curves[curveIdx];
				
				// Load curve metadata
				if(curveJson.contains("Name")){
					curve.name.set(curveJson["Name"]);
				} else {
					// Backward compatibility: set default name
					curve.name.set("Curve " + ofToString(curveIdx + 1));
				}
				if(curveJson.contains("Color")){
					ofColor loadedColor;
					loadedColor.r = curveJson["Color"]["r"];
					loadedColor.g = curveJson["Color"]["g"];
					loadedColor.b = curveJson["Color"]["b"];
					loadedColor.a = curveJson["Color"]["a"];
					curve.color.set(loadedColor);
				}
				if(curveJson.contains("Enabled")){
					curve.enabled.set(curveJson["Enabled"]);
				}
				if(curveJson.contains("GlobalQ")){
					curve.globalQ.set(curveJson["GlobalQ"]);
				}
				
				// Load points
				if(curveJson.contains("NumPoints")){
					int numPoints = curveJson["NumPoints"];
					curve.points.resize(numPoints);
					curve.lines.resize(numPoints - 1);
					
					for(int i = 0; i < numPoints; i++){
						curve.points[i].point.x = curveJson["Points"][i]["Point"]["x"];
						curve.points[i].point.y = curveJson["Points"][i]["Point"]["y"];
						curve.points[i].firstCreated = false;
					}
					
					// Load lines
					for(int i = 0; i < curve.lines.size(); i++){
						curve.lines[i].type = static_cast<lineType2>(curveJson["Lines"][i]["Type"]);
						
						if(curveJson["Lines"][i].contains("TensionExponent")){
							curve.lines[i].tensionExponent = curveJson["Lines"][i]["TensionExponent"];
						} else {
							curve.lines[i].tensionExponent = 1.0f;
						}
						
						if(curveJson["Lines"][i].contains("InflectionX")){
							curve.lines[i].inflectionX = curveJson["Lines"][i]["InflectionX"];
						} else {
							curve.lines[i].inflectionX = 0.5f;
						}
						
						if(curveJson["Lines"][i].contains("SegmentB")){
							curve.lines[i].segmentB = curveJson["Lines"][i]["SegmentB"];
						} else {
							curve.lines[i].segmentB = 6.0f;
						}
						
						if(curveJson["Lines"][i].contains("SegmentQ")){
							curve.lines[i].segmentQ = curveJson["Lines"][i]["SegmentQ"];
						} else {
							curve.lines[i].segmentQ = 1.0f;
						}
					}
				}
			}
			
			
			// Load outputs count (default to numCurves for backward compatibility)
			int numOutputsFromJson = json.contains("NumOutputs") ? static_cast<int>(json["NumOutputs"]) : numCurvesFromJson;
			resizeOutputs(numOutputsFromJson);
			
			// BUGFIX: Synchronize output parameter names with loaded curve names
			// This fixes the issue where output parameter names were set before curve names were loaded
			
			// Always set active curve to "none" (-1) when loading presets
			activeCurve.set(-1);
			
			// Restore consolidated output parameter if available
			if(json.contains("AllCurvesOutput")){
				// Handle both old vector<vector<float>> and new vector<float> formats
				auto savedOutput = json["AllCurvesOutput"];
				if(savedOutput.is_array() && !savedOutput.empty()) {
					if(savedOutput[0].is_array()) {
						// Old format: vector<vector<float>> - convert to new format
						// Use the first input's values as the consolidated output
						vector<float> convertedOutput;
						for(const auto& curveOutputs : savedOutput[0]) {
							convertedOutput.push_back(curveOutputs);
						}
						allCurvesOutput.set(convertedOutput);
					} else {
						// New format: vector<float> - use directly
						allCurvesOutput.set(savedOutput);
					}
				}
			}
			
			// Update numCurves parameter
			numCurves.set(curves.size());
			activeCurve.setMax(curves.size() - 1);
		}
		else {
			// Load legacy single-curve format
			curves.resize(1);
			auto& curve = curves[0];
			
			// For legacy format, ensure we have one output
			resizeOutputs(1);
			
			curve.points.resize(json["NumPoints"]);
			curve.lines.resize(curve.points.size()-1);
			
			for(int i = 0; i < curve.points.size(); i++){
				curve.points[i].point.x = json["Points"][i]["Point"]["x"];
				curve.points[i].point.y = json["Points"][i]["Point"]["y"];
				curve.points[i].firstCreated = false;
			}
			
			for(int i = 0; i < curve.lines.size(); i++){
				curve.lines[i].type = static_cast<lineType2>(json["Lines"][i]["Type"]);
				
				if(json["Lines"][i].contains("TensionExponent")){
					curve.lines[i].tensionExponent = json["Lines"][i]["TensionExponent"];
				} else {
					curve.lines[i].tensionExponent = 1.0f;
				}
				
				if(json["Lines"][i].contains("InflectionX")){
					curve.lines[i].inflectionX = json["Lines"][i]["InflectionX"];
				} else {
					curve.lines[i].inflectionX = 0.5f;
				}
				
				if(json["Lines"][i].contains("SegmentB")){
					curve.lines[i].segmentB = json["Lines"][i]["SegmentB"];
				} else {
					curve.lines[i].segmentB = 6.0f;
				}
				
				if(json["Lines"][i].contains("SegmentQ")){
					curve.lines[i].segmentQ = json["Lines"][i]["SegmentQ"];
				} else {
					curve.lines[i].segmentQ = 1.0f;
				}
			}
			
			numCurves.set(1);
			activeCurve.set(-1);
			activeCurve.setMax(0);
		}
		
		// Update references to active curve
		int currentActiveCurve = activeCurve.get();
		onActiveCurveChanged(currentActiveCurve);
		
		// Trigger recalculation to populate consolidated output
		recalculate();
	}
	catch (ofJson::exception& e)
	{
		ofLog() << e.what();
	}
}

// Curve management methods implementation
void curve2::addCurve() {
	curves.emplace_back();
	
	// Set default name for new curve using gap-aware naming
	int newCurveIndex = curves.size() - 1;
	int availableNumber = findNextAvailableCurveNumber();
	curves[newCurveIndex].name.set("Name", "Curve " + ofToString(availableNumber));
	
	// Set color progression: new curve gets previous curve's color + 20% hue
	if (newCurveIndex > 0) {
		// Get the color of the previous curve (last existing curve)
		ofColor prevColor = curves[newCurveIndex - 1].color.get();
		
		// Convert RGB to HSV
		ofColor hsvColor = prevColor;
		float hue, saturation, brightness;
		hsvColor.getHsb(hue, saturation, brightness);
		
		hue += 25.0f;
		if (hue > 255.0f) {
			hue = fmod(hue, 256.0f);  // Wrap around using modulo
		}
		
		// Convert back to RGB and set the new curve's color
		ofColor newColor;
		newColor.setHsb(hue, saturation, brightness, prevColor.a);  // Keep same alpha
		curves[newCurveIndex].color.set(newColor);
	}
	// If this is the first curve (newCurveIndex == 0), keep the default orange color from CurveData constructor
	
	// Update activeCurve parameter max value
	activeCurve.setMax(curves.size() - 1);
	
	// Switch to the newly added curve and update pointers
	activeCurve = newCurveIndex;  // Auto-select new curve (not "None")
}

void curve2::removeCurve(int index) {
	if (curves.size() <= 1 || index < 0 || index >= curves.size()) {
		return;
	}
	
	// Remove the specific output parameter BEFORE removing the curve
	if (index < outputs.size()) {
		string outputName = curves[index].name.get();
		removeParameter(outputName);
		outputs.erase(outputs.begin() + index);
	}
	
	// Remove the curve from the vector
	curves.erase(curves.begin() + index);
	
	// Update activeCurve bounds and value
	activeCurve.setMax(curves.size() - 1);
	if (activeCurve >= curves.size()) {
		activeCurve = curves.size() - 1;
	}
	
	// Update remaining output parameter names to match new indices
	for (int i = index; i < outputs.size(); i++) {
		if (i < curves.size() && outputs[i]) {
			outputs[i]->setName(curves[i].name.get());
		}
	}
	
	// Update consolidated output
	vector<float> consolidatedDefault(curves.size(), 0.0f);
	allCurvesOutput = consolidatedDefault;
}

int curve2::findNextAvailableCurveNumber() {
	for(int i = 1; i <= curves.size() + 1; i++) {
		string candidateName = "Curve " + ofToString(i);
		bool nameExists = false;
		for(auto& curve : curves) {
			if(curve.name.get() == candidateName) {
				nameExists = true;
				break;
			}
		}
		if(!nameExists) return i;
	}
	return curves.size() + 1; // Fallback
}

void curve2::resizeCurves(int newCount) {
	if (newCount < 1) newCount = 1;
	if (newCount > 16) newCount = 16;
	
	int currentCount = curves.size();
	
	if (newCount > currentCount) {
		// Add new curves
		for (int i = currentCount; i < newCount; i++) {
			addCurve();
		}
	} else if (newCount < currentCount) {
		// Remove curves from the end
		for (int i = currentCount - 1; i >= newCount; i--) {
			removeCurve(i);
		}
	}
}

CurveData& curve2::getCurrentCurve() {
	int index = activeCurve.get();
	if (index < 0 || index >= curves.size()) {
		// If "None" is selected or invalid index, return first curve as fallback
		index = 0;
	}
	return curves[index];
}

const CurveData& curve2::getCurrentCurve() const {
	int index = activeCurve.get();
	if (index < 0 || index >= curves.size()) {
		// If "None" is selected or invalid index, return first curve as fallback
		index = 0;
	}
	return curves[index];
}

void curve2::onNumCurvesChanged(int& newCount) {
	resizeCurves(newCount);
	resizeOutputs(newCount);
	// Trigger recalculation to update consolidated output with new curve count
	recalculate();
}

void curve2::onActiveCurveChanged(int& newIndex) {
	// Handle "None" selection (-1)
	if (newIndex == -1) {
		// Set pointers to nullptr to indicate no active curve
		points = nullptr;
		lines = nullptr;
		colorParam = nullptr;
		globalQ = nullptr;
		
		// Reset hover states
		hoveredPointIndex = -1;
		hoveredSegmentIndex = -1;

		// No recalculation needed for overview mode
		return;
	}
	
	// Enhanced error handling and bounds checking
	if (curves.empty()) {
		// Initialize with default curve if empty
		curves.resize(1);
		newIndex = 0;
		activeCurve = newIndex;
	} else if (newIndex < -1 || newIndex >= curves.size()) {
		newIndex = std::max(-1, std::min((int)curves.size() - 1, newIndex));
		activeCurve = newIndex;
		
		// If still -1 after clamping, handle as "None"
		if (newIndex == -1) {
			points = nullptr;
			lines = nullptr;
			colorParam = nullptr;
			globalQ = nullptr;
			hoveredPointIndex = -1;
			hoveredSegmentIndex = -1;
			return;
		}
	}
	
	// Additional validation - ensure curve has valid data
	auto& targetCurve = curves[newIndex];
	
	if (targetCurve.points.empty()) {
		// Initialize with default points if empty
		targetCurve.points.emplace_back(0, 0);
		targetCurve.points.emplace_back(1, 1);
		targetCurve.points.front().firstCreated = false;
		targetCurve.points.back().firstCreated = false;
		targetCurve.lines.emplace_back();
	}
	
	// Update pointers to point to the new active curve
	points = &curves[newIndex].points;
	lines = &curves[newIndex].lines;
	colorParam = &curves[newIndex].color;
	globalQ = &curves[newIndex].globalQ;
	
	// Update color
	if(colorParam != nullptr){
		color = colorParam->get();
	}
	
	// Reset hover states when switching curves
	hoveredPointIndex = -1;
	hoveredSegmentIndex = -1;

	// Trigger recalculation
	recalculate();
}

// Output management methods implementation
void curve2::addOutput() {
	int newIndex = outputs.size();
	// Create new shared_ptr with proper initialization
	outputs.emplace_back(make_shared<ofParameter<vector<float>>>());
	string outputName = (newIndex < curves.size()) ? curves[newIndex].name.get() : ("Curve " + ofToString(newIndex + 1));
	
	if(outputs[newIndex]) {
		addOutputParameter(outputs[newIndex]->set(outputName, {0}, {0}, {1}));
	}
}

void curve2::removeOutput() {
	if (outputs.size() <= 1) {
		return; // Cannot remove if only one output
	}
	
	int lastIndex = outputs.size() - 1;
	string outputName = (lastIndex < curves.size()) ? curves[lastIndex].name.get() : ("Curve " + ofToString(lastIndex + 1));
	
	removeParameter(outputName);
	outputs.pop_back();
}

void curve2::resizeOutputs(int count) {
	if (count < 1) count = 1;
	if (count > 16) count = 16;
	
	int currentCount = outputs.size();
	
	if (count > currentCount) {
		// Add new outputs
		for (int i = currentCount; i < count; i++) {
			addOutput();
		}
	} else if (count < currentCount) {
		// Remove outputs from the end
		for (int i = currentCount - 1; i >= count; i--) {
			removeOutput();
		}
	}
	
	// Update consolidated output parameter size to match new curve count
	// Initialize with simple vector structure
	vector<float> consolidatedDefault(count, 0.0f);
	allCurvesOutput = consolidatedDefault;
}

void curve2::renderInspectorInterface() {
	// Single unified inspector interface without tabs
	
	// Global Parameters Section (includes curve management)
	if (ImGui::CollapsingHeader("Global Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
		// X range is now fixed at [0.0, 1.0] - no longer configurable
		ImGui::Text("X Range: [%.1f, %.1f] (fixed)", minX, maxX);
		
		int horDiv = numHorizontalDivisions.get();
		if (ImGui::SliderInt("Horizontal Divisions", &horDiv, 1, 512)) {
			numHorizontalDivisions = horDiv;
		}
		int vertDiv = numVerticalDivisions.get();
		if (ImGui::SliderInt("Vertical Divisions", &vertDiv, 1, 512)) {
			numVerticalDivisions = vertDiv;
		}
		bool snapGrid = snapToGrid.get();
		if (ImGui::Checkbox("Snap to Grid", &snapGrid)) {
			snapToGrid = snapGrid;
		}
		bool showInfoVal = showInfo.get();
		if (ImGui::Checkbox("Show Info", &showInfoVal)) {
			showInfo = showInfoVal;
		}
		
		// Add curve labels toggle
		if (ImGui::Checkbox("Show Curve Labels", &showCurveLabels)) {
			// Toggle handled by direct assignment
		}
		
	}
	
	// Active Curve Properties Section (hidden when "None" is selected)
	// Only contains Global Q parameter - other parameters moved to Reset button line
	if (activeCurve.get() >= 0 && ImGui::CollapsingHeader("Active Curve Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
		auto& currentCurve = getCurrentCurve();
		
		// Global Q parameter (only parameter kept in this section)
		float globalQVal = currentCurve.globalQ.get();
		if (ImGui::SliderFloat("Global Q", &globalQVal, 0.1f, 5.0f)) {
			currentCurve.globalQ = globalQVal;
			recalculate();
		}
	}
}

//---------------------------------------------------------------
// StandardVisualFeedback Implementation
//---------------------------------------------------------------

StandardVisualFeedback::StandardVisualFeedback()
    : showInfo(true)
    , tensionDragActive(false)
    , bDragActive(false)
    , tensionDragSegmentIndex(-1)
    , bDragSegmentIndex(-1)
    , hoveredSegmentIndex(-1)
    , hoveredPointIndex(-1)
    , canvasHovered(false) {
}

void StandardVisualFeedback::setCoordinateTransformer(std::shared_ptr<ICoordinateTransformer> transformer) {
    coordinateTransformer = transformer;
}

void StandardVisualFeedback::setShowInfo(bool show) {
    showInfo = show;
}

void StandardVisualFeedback::updateDragStates(bool tensionActive, bool bActive, int tensionSegment, int bSegment) {
    tensionDragActive = tensionActive;
    bDragActive = bActive;
    tensionDragSegmentIndex = tensionSegment;
    bDragSegmentIndex = bSegment;
}

void StandardVisualFeedback::updateHoverStates(int hoveredSegment, int hoveredPoint, bool canvasHover) {
    hoveredSegmentIndex = hoveredSegment;
    hoveredPointIndex = hoveredPoint;
    canvasHovered = canvasHover;
}

void StandardVisualFeedback::renderFeedback(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                                          const std::vector<line2>& lines, const std::vector<CurveData>& curves,
                                          int activeCurveIndex, bool showLabels, const ImVec2& visualWinPos) {
    if (!coordinateTransformer) return;
    
    // Store visual window position for label rendering
    this->visualWinPos = visualWinPos;
    
    // Render all visual feedback components
    renderInflectionLines(drawList, points, lines);
    renderAsymmetryLines(drawList, points, lines);
    renderBParameterLines(drawList, points, lines);
    renderCurveLabels(drawList, curves, activeCurveIndex, showLabels);
    renderPointHighlights(drawList, points, hoveredPointIndex, -1);
}

void StandardVisualFeedback::renderInflectionLines(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                                                  const std::vector<line2>& lines) {
    if (!coordinateTransformer) return;
    
    // Draw vertical line at inflection point during Shift+drag OR CTRL+drag operations OR hover with modifier keys
    bool showInflectionLine = false;
    int inflectionSegmentIndex = -1;
    
    // Check for active drag operations
    if((tensionDragActive && tensionDragSegmentIndex >= 0) ||
       (bDragActive && bDragSegmentIndex >= 0)){
        showInflectionLine = true;
        inflectionSegmentIndex = tensionDragActive ? tensionDragSegmentIndex : bDragSegmentIndex;
    }
    // Check for hover with modifier keys
    else if(hoveredSegmentIndex >= 0 && canvasHovered &&
            (ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl)){
        showInflectionLine = true;
        inflectionSegmentIndex = hoveredSegmentIndex;
    }
    
    if(showInflectionLine && inflectionSegmentIndex >= 0 && !points.empty() && !lines.empty()){
        // Ensure segment index is valid
        if(inflectionSegmentIndex < lines.size()){
            // Get the current segment being modified
            float segmentStartX = points[inflectionSegmentIndex].point.x;
            float segmentEndX = points[inflectionSegmentIndex + 1].point.x;
            
            // Calculate the actual X position of the inflection point
            float inflectionX = lines[inflectionSegmentIndex].inflectionX;
            float lineX = segmentStartX + inflectionX * (segmentEndX - segmentStartX);
            
            // Convert to screen coordinates
            glm::vec2 lineTopPoint = coordinateTransformer->denormalizePoint(glm::vec2(lineX, 0.0f));
            glm::vec2 lineBottomPoint = coordinateTransformer->denormalizePoint(glm::vec2(lineX, 1.0f));
            
            // Draw dotted vertical line with semi-transparent yellow color
            drawDottedLine(drawList, lineTopPoint, lineBottomPoint, IM_COL32(255, 255, 0, 64));
        }
    }
}

void StandardVisualFeedback::renderAsymmetryLines(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                                                 const std::vector<line2>& lines) {
    if (!coordinateTransformer) return;
    
    // Draw cyan horizontal line for asymmetry value during Shift+drag OR CTRL+drag operations OR hover with modifier keys
    bool showAsymmetryLine = false;
    int asymmetrySegmentIndex = -1;
    
    // Check for active drag operations
    if((tensionDragActive && tensionDragSegmentIndex >= 0) ||
       (bDragActive && bDragSegmentIndex >= 0)){
        showAsymmetryLine = true;
        asymmetrySegmentIndex = tensionDragActive ? tensionDragSegmentIndex : bDragSegmentIndex;
    }
    // Check for hover with modifier keys
    else if(hoveredSegmentIndex >= 0 && canvasHovered &&
            (ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl)){
        showAsymmetryLine = true;
        asymmetrySegmentIndex = hoveredSegmentIndex;
    }
    
    if(showAsymmetryLine && asymmetrySegmentIndex >= 0 && !lines.empty()){
        // Ensure segment index is valid
        if(asymmetrySegmentIndex < lines.size()){
            // Get current asymmetry value
            float currentAsymmetry = lines[asymmetrySegmentIndex].tensionExponent;
            
            // Define asymmetry range (same as in the drag calculation)
            const float minAsymmetry = MIN_ASYMMETRY;
            const float maxAsymmetry = MAX_ASYMMETRY;
            
            // Map asymmetry value to Y position using enhanced split-zone mapping
            // This mirrors the enhanced drag mapping for consistent visual feedback
            float lineY;
            
            if (currentAsymmetry <= 1.0f) {
                // Lower zone: asymmetry 0.02-1.0 maps to Y 0.0-0.5 (bottom to middle)
                float normalizedAsymmetry = (currentAsymmetry - minAsymmetry) / (1.0f - minAsymmetry);
                lineY = normalizedAsymmetry * 0.5f; // Scale to lower half (0.0 to 0.5)
            } else {
                // Upper zone: asymmetry 1.0-100.0 maps to Y 0.5-1.0 (middle to top)
                float normalizedAsymmetry = (currentAsymmetry - 1.0f) / (maxAsymmetry - 1.0f);
                lineY = 0.5f + normalizedAsymmetry * 0.5f; // Scale to upper half (0.5 to 1.0)
            }
            
            // Clamp to valid range
            lineY = std::max(0.0f, std::min(1.0f, lineY));
            
            // Convert to screen coordinates
            glm::vec2 lineLeftPoint = coordinateTransformer->denormalizePoint(glm::vec2(0.0f, lineY));
            glm::vec2 lineRightPoint = coordinateTransformer->denormalizePoint(glm::vec2(1.0f, lineY));
            
            // Draw dotted horizontal line with semi-transparent cyan color
            drawDottedLine(drawList, lineLeftPoint, lineRightPoint, IM_COL32(0, 255, 255, 64));
        }
    }
}

void StandardVisualFeedback::renderBParameterLines(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                                                  const std::vector<line2>& lines) {
    if (!coordinateTransformer) return;
    
    // Draw red vertical line for B parameter position during Shift+drag OR CTRL+drag operations OR hover with modifier keys
    bool showBParameterLine = false;
    int bParameterSegmentIndex = -1;
    
    // Check for active drag operations
    if((tensionDragActive && tensionDragSegmentIndex >= 0) ||
       (bDragActive && bDragSegmentIndex >= 0)){
        showBParameterLine = true;
        bParameterSegmentIndex = tensionDragActive ? tensionDragSegmentIndex : bDragSegmentIndex;
    }
    // Check for hover with modifier keys
    else if(hoveredSegmentIndex >= 0 && canvasHovered &&
            (ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl)){
        showBParameterLine = true;
        bParameterSegmentIndex = hoveredSegmentIndex;
    }
    
    if(showBParameterLine && bParameterSegmentIndex >= 0 && !points.empty() && !lines.empty()){
        // Ensure segment index is valid
        if(bParameterSegmentIndex < lines.size()){
            // Get the current segment being modified
            float segmentStartX = points[bParameterSegmentIndex].point.x;
            float segmentEndX = points[bParameterSegmentIndex + 1].point.x;
            float segmentWidth = segmentEndX - segmentStartX;
            
            // Get current B parameter value
            float currentB = lines[bParameterSegmentIndex].segmentB;
            
            // Map B parameter (0.01 to 100.0) to normalized position within segment (0.0 to 1.0)
            // This represents where the B parameter affects the curve steepness within the segment
            const float minB = MIN_B_PARAMETER;
            const float maxB = MAX_B_PARAMETER;
            
            // Normalize B value to 0.0-1.0 range
            float normalizedB = (currentB - minB) / (maxB - minB);
            normalizedB = std::max(0.0f, std::min(1.0f, normalizedB));
            
            // Calculate the actual X position of the B parameter line within the segment
            // The line position directly represents the normalized B parameter value (inverted for intuitive behavior)
            float lineX = segmentStartX + (1.0f - normalizedB) * segmentWidth;
            
            // Convert to screen coordinates
            glm::vec2 lineTopPoint = coordinateTransformer->denormalizePoint(glm::vec2(lineX, 0.0f));
            glm::vec2 lineBottomPoint = coordinateTransformer->denormalizePoint(glm::vec2(lineX, 1.0f));
            
            // Draw dotted vertical line with semi-transparent red color
            drawDottedLine(drawList, lineTopPoint, lineBottomPoint, IM_COL32(255, 0, 0, 64));
        }
    }
}

void StandardVisualFeedback::renderPointHighlights(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                                                   int hoveredPointIndex, int selectedPointIndex) {
    if (!coordinateTransformer) return;
    
    // Point highlighting is handled in the main curve rendering for now
    // This method is kept for interface compliance and future enhancements
}

void StandardVisualFeedback::renderCurveLabels(ImDrawList* drawList, const std::vector<CurveData>& curves,
                                              int activeCurveIndex, bool showLabels) {
    if (!coordinateTransformer || !showLabels) return;
    
    // Draw labels for all enabled curves
    for(int curveIdx = 0; curveIdx < curves.size(); curveIdx++){
        const auto& curve = curves[curveIdx];
        
        // Skip disabled curves
        if(!curve.enabled.get()) continue;
        
        // Use actual curve name
        string curveName = curve.name.get();
        
        // Determine label position and color
        ofColor curveColor = curve.color.get();
        ImU32 labelColor;
        float labelOpacity = 0.8f;
        
        if(curveIdx == activeCurveIndex){
            // Active curve: full opacity, add "[ACTIVE]" suffix
            curveName += " [ACTIVE]";
            labelColor = IM_COL32(curveColor.r, curveColor.g, curveColor.b, (int)(255 * labelOpacity));
        }
        else {
            // Inactive curve: lower opacity
            labelOpacity = 0.6f;
            labelColor = IM_COL32(curveColor.r, curveColor.g, curveColor.b, (int)(255 * labelOpacity));
        }
        
        // Position label in top-left area, stacked vertically
        // Use the visual window position stored in renderFeedback
        ImVec2 labelPos = ImVec2(visualWinPos.x + 10, visualWinPos.y + 10 + curveIdx * 20);
        
        // Add background for better readability
        ImVec2 textSize = ImGui::CalcTextSize(curveName.c_str());
        drawList->AddRectFilled(
                                ImVec2(labelPos.x - 2, labelPos.y - 2),
                                ImVec2(labelPos.x + textSize.x + 2, labelPos.y + textSize.y + 2),
                                IM_COL32(0, 0, 0, 120) // Semi-transparent black background
                                );
        
        // Draw the label text
        drawList->AddText(labelPos, labelColor, curveName.c_str());
    }
}

void StandardVisualFeedback::renderInputValueIndicator(ImDrawList* drawList, float inputValue, const ImVec2& canvasPos,
                                                      const ImVec2& canvasSize) {
    if (!coordinateTransformer) return;
    
    // Input value indicator implementation would go here
    // This is kept for interface compliance and future enhancements
}

void StandardVisualFeedback::drawDottedLine(ImDrawList* drawList, ImVec2 start, ImVec2 end, ImU32 color,
                                           float dashLength, float gapLength) {
    ImVec2 direction = ImVec2(end.x - start.x, end.y - start.y);
    float totalLength = sqrt(direction.x * direction.x + direction.y * direction.y);
    
    if (totalLength == 0) return;
    
    direction.x /= totalLength;
    direction.y /= totalLength;
    
    float currentPos = 0;
    while (currentPos < totalLength) {
        float dashEnd = std::min(currentPos + dashLength, totalLength);
        
        ImVec2 dashStart = ImVec2(start.x + direction.x * currentPos, start.y + direction.y * currentPos);
        ImVec2 dashEndPos = ImVec2(start.x + direction.x * dashEnd, start.y + direction.y * dashEnd);
        
        drawList->AddLine(dashStart, dashEndPos, color, 2.0f);
        
        currentPos += dashLength + gapLength;
    }
}

//---------------------------------------------------------------

// Helper function to draw dotted lines
void curve2::drawDottedLine(ImDrawList* drawList, ImVec2 start, ImVec2 end, ImU32 color, float dashLength, float gapLength) {
	ImVec2 direction = ImVec2(end.x - start.x, end.y - start.y);
	float totalLength = sqrt(direction.x * direction.x + direction.y * direction.y);
	
	if (totalLength == 0) return;
	
	direction.x /= totalLength;
	direction.y /= totalLength;
	
	float currentPos = 0;
	while (currentPos < totalLength) {
		float dashEnd = std::min(currentPos + dashLength, totalLength);
		
		ImVec2 dashStart = ImVec2(start.x + direction.x * currentPos, start.y + direction.y * currentPos);
		ImVec2 dashEndPos = ImVec2(start.x + direction.x * dashEnd, start.y + direction.y * dashEnd);
		
		drawList->AddLine(dashStart, dashEndPos, color, 2.0f);
		
		currentPos += dashLength + gapLength;
	}
}

//---------------------------------------------------------------
// StandardPointInteractionHandler Implementation
//---------------------------------------------------------------

StandardPointInteractionHandler::StandardPointInteractionHandler() 
    : someItemClicked(false)
    , hoveredPointIndex(-1)
    , indexToRemove(-1)
{
}

void StandardPointInteractionHandler::setCoordinateTransformer(std::shared_ptr<ICoordinateTransformer> transformer) {
    coordinateTransformer = transformer;
}

void StandardPointInteractionHandler::setRecalculateCallback(std::function<void()> callback) {
    recalculateCallback = callback;
}

void StandardPointInteractionHandler::resetDragStates() {
    someItemClicked = false;
    hoveredPointIndex = -1;
    indexToRemove = -1;
}

int StandardPointInteractionHandler::detectHoveredPoint(const glm::vec2& mousePos, const std::vector<curvePoint2>& points, float detectionRadius) {
    if (!coordinateTransformer) return -1;
    
    for (int i = 0; i < points.size(); i++) {
        glm::vec2 pointPos = coordinateTransformer->denormalizePoint(points[i].point);
        float mouseToPointDistance = glm::distance(mousePos, pointPos);
        if (mouseToPointDistance < detectionRadius) {
            return i;
        }
    }
    return -1;
}

void StandardPointInteractionHandler::sortPointsByX(std::vector<curvePoint2>& points) {
    std::sort(points.begin(), points.end(), [](curvePoint2 &p1, curvePoint2 &p2){
        return p1.point.x < p2.point.x;
    });
}

bool StandardPointInteractionHandler::validatePointRemoval(const std::vector<curvePoint2>& points, int pointIndex) {
    // Ensure minimum 2 points remain
    return points.size() > 2 && pointIndex >= 0 && pointIndex < points.size();
}

glm::vec2 StandardPointInteractionHandler::applyGridSnapping(const glm::vec2& point, const InteractionContext& context) {
    if (!context.snapToGrid) return point;
    
    // Calculate grid spacing
    float gridSpacingX = 1.0f / context.horizontalDivisions;
    float gridSpacingY = 1.0f / context.verticalDivisions;
    
    // Snap to nearest grid point
    glm::vec2 snappedPoint;
    snappedPoint.x = round(point.x / gridSpacingX) * gridSpacingX;
    snappedPoint.y = round(point.y / gridSpacingY) * gridSpacingY;
    
    return snappedPoint;
}

bool StandardPointInteractionHandler::handlePointCreation(std::vector<curvePoint2>& points, std::vector<line2>& lines, const glm::vec2& mousePos) {
    if (!coordinateTransformer) return false;
    
    glm::vec2 newPointPos = coordinateTransformer->safeNormalizePoint(mousePos);
    
    // Find which segment is being split to inherit its parameters
    int splitSegmentIndex = -1;
    for (int i = 0; i < points.size() - 1; i++) {
        if (newPointPos.x >= points[i].point.x && newPointPos.x <= points[i + 1].point.x) {
            splitSegmentIndex = i;
            break;
        }
    }
    
    points.emplace_back(newPointPos);
    points.back().drag = 3;
    
    // Create new line segment with inherited parameters from the split segment
    if (splitSegmentIndex >= 0 && splitSegmentIndex < lines.size()) {
        // Inherit all parameters from the segment being split
        line2 newLine = lines[splitSegmentIndex];
        lines.emplace_back(newLine);
    } else {
        // Fallback to default parameters if no segment found
        lines.emplace_back();
    }
    
    return true;
}

bool StandardPointInteractionHandler::handlePointDrag(std::vector<curvePoint2>& points, const MouseState& mouseState) {
    if (!coordinateTransformer) return false;
    
    bool stateChanged = false;
    for (int i = 0; i < points.size(); i++) {
        auto &p = points[i];
        
        if (mouseState.isDragging && p.drag != 0) {
            if (p.drag == 3) {
                // Use safe zone aware normalization - allows dragging to continue even when mouse is outside visual area
                glm::vec2 normalizedPos = glm::clamp(coordinateTransformer->safeNormalizePoint(mouseState.position), 0.0f, 1.0f);
                
                // Legacy key-based snapping (X and Y keys for individual axis snapping)
                if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_X))) {
                    normalizedPos.x = round(normalizedPos.x * 8) / 8; // Default to 8 divisions
                }
                if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Y))) {
                    normalizedPos.y = round(normalizedPos.y * 4) / 4; // Default to 4 divisions
                }
                
                p.point = normalizedPos;
                stateChanged = true;
            }
        }
    }
    
    return stateChanged;
}

void StandardPointInteractionHandler::renderCoordinateSliders(curvePoint2& point, float minX, float maxX) {
    float tempFloat = ofMap(point.point.x, 0, 1, minX, maxX);
    ImGui::SliderFloat("X", &tempFloat, minX, maxX);
    if (ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())) {
        point.point.x = ofMap(tempFloat, minX, maxX, 0, 1);
    }
    
    // Y value editing (0-1 range)
    tempFloat = point.point.y;
    ImGui::SliderFloat("Y", &tempFloat, 0.0f, 1.0f);
    if (ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())) {
        point.point.y = tempFloat;
    }
}

void StandardPointInteractionHandler::renderLineControls(std::vector<line2>& lines, int pointIndex) {
    if (pointIndex > 0 && lines.size() > 0) {
        int elem = lines[pointIndex-1].type;
        const char* elems_names[2] = { "Hold", "Tension"};
        if (ImGui::SliderInt("Line L", &elem, 0, 1, elems_names[elem])) {
            lines[pointIndex-1].type = static_cast<lineType2>(elem);
        }
        
        // Show asymmetric logistic controls for TENSION segments
        if (lines[pointIndex-1].type == LINE2_TENSION) {
            if (ImGui::SliderFloat("Asymmetry L", &lines[pointIndex-1].tensionExponent, MIN_ASYMMETRY, MAX_ASYMMETRY, "%.3f")) {
                if (recalculateCallback) recalculateCallback();
            }
            if (ImGui::SliderFloat("Inflection L", &lines[pointIndex-1].inflectionX, 0.0f, 1.0f, "%.3f")) {
                if (recalculateCallback) recalculateCallback();
            }
            if (ImGui::SliderFloat("Segment B L", &lines[pointIndex-1].segmentB, MIN_B_PARAMETER, MAX_B_PARAMETER, "%.2f")) {
                if (recalculateCallback) recalculateCallback();
            }
            if (ImGui::SliderFloat("Segment Q L", &lines[pointIndex-1].segmentQ, 0.1f, 5.0f, "%.2f")) {
                if (recalculateCallback) recalculateCallback();
            }
        }
    }
    
    if (lines.size() > 0 && pointIndex < lines.size()) {
        int elem = lines[pointIndex].type;
        const char* elems_names[2] = { "Hold", "Tension"};
        if (ImGui::SliderInt("Line R", &elem, 0, 1, elems_names[elem])) {
            lines[pointIndex].type = static_cast<lineType2>(elem);
        }
        
        // Show asymmetric logistic controls for TENSION segments
        if (lines[pointIndex].type == LINE2_TENSION) {
            if (ImGui::SliderFloat("Asymmetry R", &lines[pointIndex].tensionExponent, MIN_ASYMMETRY, MAX_ASYMMETRY, "%.3f")) {
                if (recalculateCallback) recalculateCallback();
            }
            if (ImGui::SliderFloat("Inflection R", &lines[pointIndex].inflectionX, 0.0f, 1.0f, "%.3f")) {
                if (recalculateCallback) recalculateCallback();
            }
            if (ImGui::SliderFloat("Segment B R", &lines[pointIndex].segmentB, MIN_B_PARAMETER, MAX_B_PARAMETER, "%.2f")) {
                if (recalculateCallback) recalculateCallback();
            }
            if (ImGui::SliderFloat("Segment Q R", &lines[pointIndex].segmentQ, 0.1f, 5.0f, "%.2f")) {
                if (recalculateCallback) recalculateCallback();
            }
        }
    }
}

void StandardPointInteractionHandler::renderCurveProperties(ofParameter<float>* globalQ) {
    ImGui::Separator();
    ImGui::Text("Curve Properties:");
    if (globalQ != nullptr) {
        float tempQ = globalQ->get();
        
        if (ImGui::SliderFloat("Q (Scaling)", &tempQ, 0.1f, 5.0f, "%.1f")) {
            globalQ->set(tempQ);
            if (recalculateCallback) recalculateCallback();
        }
    } else {
        ImGui::Text("Q (Scaling): N/A (no active curve)");
    }
}

void StandardPointInteractionHandler::renderPointContextMenu(curvePoint2& point, std::vector<line2>& lines, int pointIndex) {
    // Coordinate sliders
    float tempFloat = point.point.x;
    ImGui::SliderFloat("X", &tempFloat, 0.0f, 1.0f);
    if (ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())) {
        point.point.x = tempFloat;
    }
    
    tempFloat = point.point.y;
    ImGui::SliderFloat("Y", &tempFloat, 0.0f, 1.0f);
    if (ImGui::IsItemDeactivated() || (ImGui::IsMouseDown(0) && ImGui::IsItemEdited())) {
        point.point.y = tempFloat;
    }
    
    ImGui::Spacing();
    
    // Line controls for left segment
    if (pointIndex > 0 && lines.size() > 0) {
        int elem = lines[pointIndex-1].type;
        const char* elems_names[2] = { "Hold", "Tension"};
        if (ImGui::SliderInt("Line L", &elem, 0, 1, elems_names[elem])) {
            lines[pointIndex-1].type = static_cast<lineType2>(elem);
        }
        
        if (lines[pointIndex-1].type == LINE2_TENSION) {
            if (ImGui::SliderFloat("Asymmetry L", &lines[pointIndex-1].tensionExponent, MIN_ASYMMETRY, MAX_ASYMMETRY, "%.3f")) {
                if (recalculateCallback) recalculateCallback();
            }
            if (ImGui::SliderFloat("Inflection L", &lines[pointIndex-1].inflectionX, 0.0f, 1.0f, "%.3f")) {
                if (recalculateCallback) recalculateCallback();
            }
            if (ImGui::SliderFloat("Segment B L", &lines[pointIndex-1].segmentB, MIN_B_PARAMETER, MAX_B_PARAMETER, "%.2f")) {
                if (recalculateCallback) recalculateCallback();
            }
            if (ImGui::SliderFloat("Segment Q L", &lines[pointIndex-1].segmentQ, 0.1f, 5.0f, "%.2f")) {
                if (recalculateCallback) recalculateCallback();
            }
        }
    }
    
    // Line controls for right segment
    if (lines.size() > 0 && pointIndex < lines.size()) {
        int elem = lines[pointIndex].type;
        const char* elems_names[2] = { "Hold", "Tension"};
        if (ImGui::SliderInt("Line R", &elem, 0, 1, elems_names[elem])) {
            lines[pointIndex].type = static_cast<lineType2>(elem);
        }
        
        if (lines[pointIndex].type == LINE2_TENSION) {
            if (ImGui::SliderFloat("Asymmetry R", &lines[pointIndex].tensionExponent, MIN_ASYMMETRY, MAX_ASYMMETRY, "%.3f")) {
                if (recalculateCallback) recalculateCallback();
            }
            if (ImGui::SliderFloat("Inflection R", &lines[pointIndex].inflectionX, 0.0f, 1.0f, "%.3f")) {
                if (recalculateCallback) recalculateCallback();
            }
            if (ImGui::SliderFloat("Segment B R", &lines[pointIndex].segmentB, MIN_B_PARAMETER, MAX_B_PARAMETER, "%.2f")) {
                if (recalculateCallback) recalculateCallback();
            }
            if (ImGui::SliderFloat("Segment Q R", &lines[pointIndex].segmentQ, 0.1f, 5.0f, "%.2f")) {
                if (recalculateCallback) recalculateCallback();
            }
        }
    }
    
    ImGui::Separator();
    ImGui::Text("Curve Properties:");
    ImGui::Text("Q (Scaling): Handled by main curve interface");
    
    ImGui::Spacing();
    
    if (ImGui::Selectable("[Remove]")) {
        ImGui::CloseCurrentPopup();
        indexToRemove = pointIndex;
    }
}

StandardPointInteractionHandler::InteractionResult StandardPointInteractionHandler::handleMouseInteraction(
    const MouseState& mouseState, std::vector<curvePoint2>& points, std::vector<line2>& lines) {
    
    InteractionResult result;
    result.stateChanged = false;
    result.needsRecalculation = false;
    result.needsRedraw = false;
    result.hoveredPointIndex = -1;
    
    // Reset states on mouse release
    if (!mouseState.leftButtonDown) {
        for (int i = 0; i < points.size(); i++) {
            points[i].drag = 0;
            points[i].firstCreated = false;
        }
        result.stateChanged = true;
    }
    
    // Detect hovered point
    result.hoveredPointIndex = detectHoveredPoint(mouseState.position, points);
    hoveredPointIndex = result.hoveredPointIndex;
    
    // Handle point creation on double-click
    if (mouseState.leftButtonDoubleClicked) {
        if (handlePointCreation(points, lines, mouseState.position)) {
            result.stateChanged = true;
            result.needsRecalculation = true;
        }
    }
    
    // Sort points by X coordinate to maintain curve integrity
    sortPointsByX(points);
    
    // Handle point interactions
    indexToRemove = -1;
    someItemClicked = false;
    
    for (int i = 0; i < points.size(); i++) {
        auto &p = points[i];
        
        // Handle point dragging
        if (handlePointDrag(points, mouseState)) {
            result.stateChanged = true;
            result.needsRecalculation = true;
        }
        
        // Handle point selection
        glm::vec2 pointPos = coordinateTransformer->denormalizePoint(p.point);
        if (mouseState.leftButtonClicked && !someItemClicked) {
            auto mouseToPointDistance = glm::distance(mouseState.position, pointPos);
            if (mouseToPointDistance < POINT_DETECTION_RADIUS) {
                p.drag = 3;
                someItemClicked = true;
                result.stateChanged = true;
            }
        } else if (mouseState.rightButtonClicked && mouseState.isHovered) {
            // Use hovered point for right-click if available, otherwise check distance
            if (hoveredPointIndex == i || (hoveredPointIndex == -1 && glm::distance(mouseState.position, pointPos) < POINT_DETECTION_RADIUS)) {
                ImGui::OpenPopup(("##PointPopup " + ofToString(i)).c_str());
            }
        }
        
        // Render context menu
        if (ImGui::BeginPopup(("##PointPopup " + ofToString(i)).c_str())) {
            renderPointContextMenu(p, lines, i);
            ImGui::EndPopup();
        }
    }
    
    // Handle point removal
    if (indexToRemove != -1 && validatePointRemoval(points, indexToRemove)) {
        points.erase(points.begin() + indexToRemove);
        if (indexToRemove < lines.size()) {
            lines.erase(lines.begin() + indexToRemove);
        }
        result.stateChanged = true;
        result.needsRecalculation = true;
        indexToRemove = -1;
    }
    
    return result;
}

StandardPointInteractionHandler::InteractionResult StandardPointInteractionHandler::processInteraction(
    bool canvasHovered, bool canvasClicked, bool canvasDoubleClicked,
    std::vector<curvePoint2>& points, std::vector<line2>& lines,
    const InteractionContext& context) {
    
    // Construct mouse state from ImGui
    MouseState mouseState;
    mouseState.position = glm::vec2(ImGui::GetMousePos());
    mouseState.leftButtonDown = ImGui::IsMouseDown(0);
    mouseState.leftButtonClicked = canvasClicked;
    mouseState.leftButtonDoubleClicked = canvasDoubleClicked;
    mouseState.rightButtonClicked = ImGui::IsMouseClicked(1);
    mouseState.isDragging = ImGui::IsMouseDragging(0);
    mouseState.isHovered = canvasHovered;
    
    // Apply grid snapping if enabled
    if (context.snapToGrid && mouseState.leftButtonDoubleClicked) {
        glm::vec2 normalizedPos = coordinateTransformer->safeNormalizePoint(mouseState.position);
        glm::vec2 snappedPos = applyGridSnapping(normalizedPos, context);
        mouseState.position = coordinateTransformer->denormalizePoint(snappedPos);
    }
    
    return handleMouseInteraction(mouseState, points, lines);
}

//---------------------------------------------------------------
// StandardParameterController Implementation
//---------------------------------------------------------------

StandardParameterController::StandardParameterController() 
    : tensionDragActive(false)
    , bDragActive(false)
    , tensionDragSegmentIndex(-1)
    , bDragSegmentIndex(-1)
    , tensionDragStartExponent(1.0f)
    , bDragStartValue(6.0f)
{
}

void StandardParameterController::setCoordinateTransformer(std::shared_ptr<ICoordinateTransformer> transformer) {
    coordinateTransformer = transformer;
}

void StandardParameterController::setRecalculateCallback(std::function<void()> callback) {
    recalculateCallback = callback;
}

bool StandardParameterController::handleParameterDrag(bool shiftPressed, bool ctrlPressed, bool canvasHovered, 
                                                    bool someItemClicked, std::vector<curvePoint2>& points, 
                                                    std::vector<line2>& lines) {
    bool handled = false;
    
    // Handle Shift+drag for tension control
    if (handleTensionDrag(shiftPressed, canvasHovered, someItemClicked, points, lines)) {
        handled = true;
    }
    
    // Handle Ctrl+drag for B parameter control
    if (handleBParameterDragControl(ctrlPressed, canvasHovered, someItemClicked, points, lines)) {
        handled = true;
    }
    
    return handled;
}

bool StandardParameterController::handleTensionDrag(bool shiftPressed, bool canvasHovered, bool someItemClicked,
                                                  std::vector<curvePoint2>& points, std::vector<line2>& lines) {
    if (shiftPressed && !someItemClicked && canvasHovered) {
        if (ImGui::IsMouseDragging(0)) {
            if (!tensionDragActive) {
                // Start new tension drag operation
                glm::vec2 startPos = coordinateTransformer->safeNormalizePoint(ImGui::GetMousePos());
                startTensionDrag(startPos, points, lines);
            }
            
            // Continue existing tension drag operation
            if (tensionDragActive && tensionDragSegmentIndex >= 0) {
                glm::vec2 currentPos = coordinateTransformer->safeNormalizePoint(ImGui::GetMousePos());
                updateTensionDrag(currentPos, points, lines);
                if (recalculateCallback) recalculateCallback();
            }
        } else {
            // Reset tension drag state when not dragging
            tensionDragActive = false;
            tensionDragSegmentIndex = -1;
        }
        return true;
    } else {
        // Reset tension drag state when Shift is not pressed
        tensionDragActive = false;
        tensionDragSegmentIndex = -1;
        return false;
    }
}

bool StandardParameterController::handleBParameterDragControl(bool ctrlPressed, bool canvasHovered, bool someItemClicked,
                                                            std::vector<curvePoint2>& points, std::vector<line2>& lines) {
    if (ctrlPressed && !someItemClicked && canvasHovered) {
        if (ImGui::IsMouseDragging(0)) {
            if (!bDragActive) {
                // Start new B parameter drag operation
                glm::vec2 startPos = coordinateTransformer->safeNormalizePoint(ImGui::GetMousePos());
                startBParameterDragOperation(startPos, points, lines);
            }
            
            // Continue existing B parameter drag operation
            if (bDragActive && bDragSegmentIndex >= 0) {
                glm::vec2 currentPos = coordinateTransformer->safeNormalizePoint(ImGui::GetMousePos());
                updateBParameterDrag(currentPos, points, lines);
                if (recalculateCallback) recalculateCallback();
            }
        } else {
            // Reset B drag state when not dragging
            bDragActive = false;
            bDragSegmentIndex = -1;
        }
        return true;
    } else {
        // Reset B drag state when Ctrl is not pressed
        bDragActive = false;
        bDragSegmentIndex = -1;
        return false;
    }
}

void StandardParameterController::startTensionDrag(const glm::vec2& startPos, std::vector<curvePoint2>& points, 
                                                  std::vector<line2>& lines) {
    tensionDragActive = true;
    tensionDragStartPos = startPos;
    tensionDragSegmentIndex = -1;
    
    // Find which segment the mouse is over
    for (int i = 0; i < points.size() - 1; i++) {
        if (startPos.x >= points[i].point.x && startPos.x <= points[i + 1].point.x && lines[i].type == LINE2_TENSION) {
            tensionDragSegmentIndex = i;
            tensionDragStartExponent = lines[i].tensionExponent;
            break;
        }
    }
}

void StandardParameterController::startBParameterDragOperation(const glm::vec2& startPos, std::vector<curvePoint2>& points, 
                                                             std::vector<line2>& lines) {
    bDragActive = true;
    bDragStartPos = startPos;
    bDragSegmentIndex = -1;
    
    // Find which segment the mouse is over
    for (int i = 0; i < points.size() - 1; i++) {
        if (startPos.x >= points[i].point.x && startPos.x <= points[i + 1].point.x && lines[i].type == LINE2_TENSION) {
            bDragSegmentIndex = i;
            bDragStartValue = lines[i].segmentB;
            break;
        }
    }
}

void StandardParameterController::updateTensionDrag(const glm::vec2& currentPos, std::vector<curvePoint2>& points, 
                                                   std::vector<line2>& lines) {
    if (tensionDragSegmentIndex < 0 || tensionDragSegmentIndex >= lines.size()) return;
    
    // Calculate drag deltas
    float deltaX = currentPos.x - tensionDragStartPos.x;
    float deltaY = currentPos.y - tensionDragStartPos.y;
    
    // Horizontal drag: Set inflection point X position within segment (0.0 to 1.0)
    float segmentStartX = points[tensionDragSegmentIndex].point.x;
    float segmentEndX = points[tensionDragSegmentIndex + 1].point.x;
    float segmentWidth = segmentEndX - segmentStartX;
    
    if (segmentWidth > 0.0001f) {
        // Map current mouse X to segment-relative position
        float relativeX = (currentPos.x - segmentStartX) / segmentWidth;
        // Constrain to avoid edge issues
        lines[tensionDragSegmentIndex].inflectionX = std::max(MIN_INFLECTION_X, std::min(MAX_INFLECTION_X, relativeX));
    }
    
    // Vertical drag: Enhanced split-zone mapping for asymmetry parameter
    float normalizedY = (currentPos.y - 0.0f) / (1.0f - 0.0f); // Mouse Y position within curve area (0.0 to 1.0)
    
    // Define asymmetry range
    const float minAsymmetry = MIN_ASYMMETRY;
    const float maxAsymmetry = MAX_ASYMMETRY;
    float asymmetry;
    
    // Split the curve area height into two zones:
    // Lower half (bottom 50%): Maps from 1 (middle) to 0.02 (bottom)
    // Upper half (top 50%): Maps from 1 (middle) to 10 (top)
    if (normalizedY <= 0.5f) {
        // Lower half: map from 1 (at middle) to 0.02 (at bottom)
        float lowerHalfY = normalizedY / 0.5f; // 0.0 to 1.0
        asymmetry = minAsymmetry + lowerHalfY * (1.0f - minAsymmetry);
    } else {
        // Upper half: map from 1 (at middle) to 10 (at top)
        float upperHalfY = (normalizedY - 0.5f) / 0.5f; // 0.0 to 1.0
        asymmetry = 1.0f + upperHalfY * (maxAsymmetry - 1.0f);
    }
    
    lines[tensionDragSegmentIndex].tensionExponent = asymmetry;
    
    // Clamp to valid range
    lines[tensionDragSegmentIndex].tensionExponent = std::max(minAsymmetry, std::min(maxAsymmetry, lines[tensionDragSegmentIndex].tensionExponent));
}

void StandardParameterController::updateBParameterDrag(const glm::vec2& currentPos, std::vector<curvePoint2>& points, 
                                                      std::vector<line2>& lines) {
    if (bDragSegmentIndex < 0 || bDragSegmentIndex >= lines.size()) return;
    
    // X-position-based B parameter mapping within segment
    float segmentStartX = points[bDragSegmentIndex].point.x;
    float segmentEndX = points[bDragSegmentIndex + 1].point.x;
    float segmentWidth = segmentEndX - segmentStartX;
    
    if (segmentWidth > 0.0001f) {
        // Map current mouse X position within segment to B parameter range
        float relativeX = (currentPos.x - segmentStartX) / segmentWidth;
        // Clamp to segment bounds
        relativeX = std::max(0.0f, std::min(1.0f, relativeX));
        
        // Linear interpolation: B = maxB - relativeX * (maxB - minB)
        const float minB = MIN_B_PARAMETER;
        const float maxB = MAX_B_PARAMETER;
        lines[bDragSegmentIndex].segmentB = maxB - relativeX * (maxB - minB);
    }
}

void StandardParameterController::endParameterDrag() {
    tensionDragActive = false;
    bDragActive = false;
    tensionDragSegmentIndex = -1;
    bDragSegmentIndex = -1;
}

int StandardParameterController::detectHoveredSegment(const glm::vec2& mousePos, std::vector<curvePoint2>& points, 
                                                     std::vector<line2>& lines) {
    for (int i = 0; i < points.size() - 1; i++) {
        if (mousePos.x >= points[i].point.x && mousePos.x <= points[i + 1].point.x && lines[i].type == LINE2_TENSION) {
            return i;
        }
    }
    return -1;
}

// IParameterControlSystem interface implementation
bool StandardParameterController::handleAsymmetryDrag(const glm::vec2& mousePos, const glm::vec2& mouseDelta,
                                                     std::vector<line2>& lines, int segmentIndex) {
    if (segmentIndex < 0 || segmentIndex >= lines.size()) return false;
    
    // Vertical drag for asymmetry control
    float normalizedY = mousePos.y;
    const float minAsymmetry = MIN_ASYMMETRY;
    const float maxAsymmetry = MAX_ASYMMETRY;
    float asymmetry;
    
    if (normalizedY <= 0.5f) {
        float lowerHalfY = normalizedY / 0.5f;
        asymmetry = minAsymmetry + lowerHalfY * (1.0f - minAsymmetry);
    } else {
        float upperHalfY = (normalizedY - 0.5f) / 0.5f;
        asymmetry = 1.0f + upperHalfY * (maxAsymmetry - 1.0f);
    }
    
    lines[segmentIndex].tensionExponent = std::max(minAsymmetry, std::min(maxAsymmetry, asymmetry));
    return true;
}

bool StandardParameterController::handleInflectionDrag(const glm::vec2& mousePos, const glm::vec2& mouseDelta,
                                                      std::vector<line2>& lines, int segmentIndex) {
    if (segmentIndex < 0 || segmentIndex >= lines.size()) return false;
    
    // Horizontal drag for inflection control
    float relativeX = mousePos.x;
    lines[segmentIndex].inflectionX = std::max(MIN_INFLECTION_X, std::min(MAX_INFLECTION_X, relativeX));
    return true;
}

bool StandardParameterController::handleBParameterDrag(const glm::vec2& mousePos, const glm::vec2& mouseDelta,
                                                      std::vector<line2>& lines, int segmentIndex) {
    if (segmentIndex < 0 || segmentIndex >= lines.size()) return false;
    
    // Horizontal drag for B parameter control
    float relativeX = mousePos.x;
    relativeX = std::max(0.0f, std::min(1.0f, relativeX));
    
    const float minB = MIN_B_PARAMETER;
    const float maxB = MAX_B_PARAMETER;
    lines[segmentIndex].segmentB = maxB - relativeX * (maxB - minB);
    return true;
}

void StandardParameterController::startAsymmetryDrag(int segmentIndex, const glm::vec2& startPos, float startValue) {
    tensionDragActive = true;
    tensionDragSegmentIndex = segmentIndex;
    tensionDragStartPos = startPos;
    tensionDragStartExponent = startValue;
}

void StandardParameterController::startInflectionDrag(int segmentIndex, const glm::vec2& startPos, float startValue) {
    // Inflection is handled as part of tension drag
    tensionDragActive = true;
    tensionDragSegmentIndex = segmentIndex;
    tensionDragStartPos = startPos;
}

void StandardParameterController::startBParameterDrag(int segmentIndex, const glm::vec2& startPos, float startValue) {
    bDragActive = true;
    bDragSegmentIndex = segmentIndex;
    bDragStartPos = startPos;
    bDragStartValue = startValue;
}

void StandardParameterController::endAllDrags() {
    endParameterDrag();
}

bool StandardParameterController::isAnyDragActive() const {
    return tensionDragActive || bDragActive;
}

int StandardParameterController::getActiveDragSegment() const {
    if (tensionDragActive) return tensionDragSegmentIndex;
    if (bDragActive) return bDragSegmentIndex;
    return -1;
}

//---------------------------------------------------------------
// MultiCurveRenderer Implementation
//---------------------------------------------------------------

MultiCurveRenderer::MultiCurveRenderer() 
    : renderQuality(RenderQuality::MEDIUM) {
}

void MultiCurveRenderer::renderCurves(ImDrawList* drawList, const std::vector<CurveData>& curves,
                                     int activeCurveIndex, const RenderContext& context,
                                     std::shared_ptr<StandardParameterController> parameterController) {
    if (!coordinateTransformer) return;
    
    // First pass: Draw all enabled curves as inactive (background layer) with reduced opacity
    const float inactiveOpacity = MultiCurveRenderer::INACTIVE_OPACITY;
    const float inactiveLineWidth = MultiCurveRenderer::INACTIVE_LINE_WIDTH;
    
    for(int curveIdx = 0; curveIdx < curves.size(); curveIdx++){
        const auto& curve = curves[curveIdx];
        
        // Skip disabled curves
        if(!curve.enabled.get()) continue;
        
        // Skip active curve in first pass (will be drawn in second pass)
        if(curveIdx == activeCurveIndex) continue;
        
        renderSingleCurve(drawList, curve, false, context, nullptr);
    }
    
    // Second pass: Draw active curve (foreground layer) with full interactivity
    // Only draw if a specific curve is selected (not "None")
    if(activeCurveIndex >= 0 && activeCurveIndex < curves.size() && curves[activeCurveIndex].enabled.get()){
        const auto& activeCurveData = curves[activeCurveIndex];
        renderSingleCurve(drawList, activeCurveData, true, context, parameterController);
    }
}

void MultiCurveRenderer::renderSingleCurve(ImDrawList* drawList, const CurveData& curve,
                                          bool isActive, const RenderContext& context,
                                          std::shared_ptr<StandardParameterController> parameterController) {
    if (!coordinateTransformer || curve.points.empty()) return;
    
    const ofColor curveColor = curve.color.get();
    const float lineWidth = isActive ? MultiCurveRenderer::ACTIVE_LINE_WIDTH : MultiCurveRenderer::INACTIVE_LINE_WIDTH;
    
    // Render extension lines
    renderExtensionLines(drawList, curve.points, curveColor, isActive);
    
    // Render curve segments
    for(int i = 0; i < curve.points.size()-1; i++){
        // Determine segment color based on drag state for active curve
        bool whiteHighlight = false;
        bool activeHighlight = false;
        
        if(isActive && parameterController){
            // Check for tension drag highlighting (shift+drag) - use white
            if(parameterController->isTensionDragActive() && parameterController->getTensionDragSegmentIndex() == i){
                whiteHighlight = true;
            }
            
            // Check for B parameter drag highlighting (ctrl+drag) - use white
            if(parameterController->isBDragActive() && parameterController->getBDragSegmentIndex() == i){
                whiteHighlight = true;
            }
            
            // Check for normal point drag highlighting - use active curve color
            if(!whiteHighlight){
                for(int p = 0; p < curve.points.size(); p++){
                    if(curve.points[p].drag == 3){
                        // Highlight segments connected to the dragged point
                        if((p == i) || (p == i + 1)){
                            activeHighlight = true;
                            break;
                        }
                    }
                }
            }
        }
        
        // Calculate segment color
        ImU32 segmentColor = calculateSegmentColor(curveColor, isActive, whiteHighlight, activeHighlight);
        
        // Render segment based on type
        if(curve.lines[i].type == LINE2_HOLD){
            renderHoldSegment(drawList, curve.points[i], curve.points[i+1], segmentColor, lineWidth);
        }
        else if(curve.lines[i].type == LINE2_TENSION){
            renderTensionSegment(drawList, curve.points[i], curve.points[i+1], 
                               curve.lines[i], curve.globalQ.get(), isActive, segmentColor, lineWidth);
        }
    }
}

void MultiCurveRenderer::renderTensionSegment(ImDrawList* drawList, const curvePoint2& p1, const curvePoint2& p2,
                                             const line2& lineData, float globalQ, bool isActive,
                                             ImU32 segmentColor, float lineWidth) {
    if (!coordinateTransformer) return;
    
    // Calculate adaptive segments based on B parameter and active state
    int numSegments = calculateAdaptiveSegments(lineData.segmentB, isActive);
    
    renderLogisticCurve(drawList, p1, p2, lineData, globalQ, segmentColor, lineWidth, numSegments);
}

void MultiCurveRenderer::renderHoldSegment(ImDrawList* drawList, const curvePoint2& p1, const curvePoint2& p2,
                                          ImU32 segmentColor, float lineWidth) {
    if (!coordinateTransformer) return;
    
    auto denormP1 = coordinateTransformer->denormalizePoint(p1.point);
    auto denormP2 = coordinateTransformer->denormalizePoint(p2.point);
    
    // Draw horizontal then vertical line segments
    drawList->AddLine(denormP1, glm::vec2(denormP2.x, denormP1.y), segmentColor, lineWidth);
    drawList->AddLine(glm::vec2(denormP2.x, denormP1.y), denormP2, segmentColor, lineWidth);
}

void MultiCurveRenderer::renderExtensionLines(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                                             const ofColor& curveColor, bool isActive) {
    if (!coordinateTransformer || points.empty()) return;
    
    if(isActive){
        // Draw active curve extension lines in black
        // Start extension line
        drawList->AddLine(coordinateTransformer->denormalizePoint(glm::vec2(0, points[0].point.y)), 
                         coordinateTransformer->denormalizePoint(points[0].point), 
                         IM_COL32(10, 10, 10, 255));
        
        // End extension line
        drawList->AddLine(coordinateTransformer->denormalizePoint(points.back().point), 
                         coordinateTransformer->denormalizePoint(glm::vec2(1, points.back().point.y)), 
                         IM_COL32(10, 10, 10, 255));
    } else {
        // Draw inactive curve extension lines with reduced opacity
        const float inactiveOpacity = MultiCurveRenderer::INACTIVE_OPACITY;
        ImU32 extensionColor = IM_COL32(curveColor.r * 0.4f, curveColor.g * 0.4f, curveColor.b * 0.4f, 
                                       (int)(255 * inactiveOpacity));
        
        // End extension line only for inactive curves
        drawList->AddLine(coordinateTransformer->denormalizePoint(points.back().point),
                         coordinateTransformer->denormalizePoint(glm::vec2(1, points.back().point.y)),
                         extensionColor);
    }
}

int MultiCurveRenderer::calculateAdaptiveSegments(float segmentB, bool isActive) const {
    int baseSegments = isActive ? MultiCurveRenderer::BASE_SEGMENTS_ACTIVE : MultiCurveRenderer::BASE_SEGMENTS_INACTIVE;
    int maxSegments = isActive ? MultiCurveRenderer::MAX_SEGMENTS_ACTIVE : MultiCurveRenderer::MAX_SEGMENTS_INACTIVE;
    
    // Adaptive resolution based on B parameter - more segments for steeper curves
    float multiplier = isActive ? 2.0f : 1.5f;
    int adaptiveSegments = std::max(baseSegments, (int)(segmentB * multiplier));
    
    return std::min(adaptiveSegments, maxSegments);
}

ImU32 MultiCurveRenderer::calculateSegmentColor(const ofColor& curveColor, bool isActive, 
                                               bool whiteHighlight, bool activeHighlight) const {
    if(whiteHighlight){
        return IM_COL32(255, 255, 255, 255); // White for shift/ctrl drag
    } else if(activeHighlight || isActive){
        return IM_COL32(curveColor.r, curveColor.g, curveColor.b, 255); // Full opacity
    } else {
        // Inactive curve with reduced opacity
        return IM_COL32(curveColor.r, curveColor.g, curveColor.b, (int)(255 * MultiCurveRenderer::INACTIVE_OPACITY));
    }
}

void MultiCurveRenderer::setCoordinateTransformer(std::shared_ptr<ICoordinateTransformer> transformer) {
    coordinateTransformer = transformer;
}

void MultiCurveRenderer::setRenderQuality(RenderQuality quality) {
    renderQuality = quality;
}

void MultiCurveRenderer::setHighlightedSegments(const std::vector<int>& segmentIndices) {
    highlightedSegments = segmentIndices;
}

float MultiCurveRenderer::calculateLogisticFunction(float x, float A, float K, float M, float nu, float B, float Q) const {
    float logisticTerm = 1.0f + Q * exp(-B * (x - M));
    return A + (K - A) / pow(logisticTerm, 1.0f / nu);
}

bool MultiCurveRenderer::shouldUseNormalization(float logisticRange) const {
    return abs(logisticRange) > 1e-6f;
}

void MultiCurveRenderer::renderLogisticCurve(ImDrawList* drawList, const curvePoint2& p1, const curvePoint2& p2,
                                            const line2& lineData, float globalQ, ImU32 segmentColor,
                                            float lineWidth, int numSegments) {
    if (!coordinateTransformer) return;
    
    glm::vec2 prevPoint = p1.point;
    
    // Asymmetric logistic parameters
    float A = p1.point.y;      // Y value of segment start point
    float K = p2.point.y;      // Y value of segment end point
    float M = lineData.inflectionX;   // Inflection point X position (0-1)
    float nu = lineData.tensionExponent; // Asymmetry parameter ν
    float B = lineData.segmentB;      // Per-segment B parameter
    float Q = globalQ;          // Global Q parameter
    
    // Calculate normalization values at segment endpoints
    auto logisticFunction = [&](float x) -> float {
        return calculateLogisticFunction(x, A, K, M, nu, B, Q);
    };
    
    float logistic_0 = logisticFunction(0.0f); // Value at segment start
    float logistic_1 = logisticFunction(1.0f); // Value at segment end
    float logisticRange = logistic_1 - logistic_0;
    
    // Handle edge case where logistic range is very small
    bool useNormalization = shouldUseNormalization(logisticRange);
    
    for(int seg = 1; seg <= numSegments; seg++){
        float normalizedX = (float)seg / numSegments;
        
        // Calculate normalized X position within the segment
        float segmentX = glm::mix(p1.point.x, p2.point.x, normalizedX);
        
        float resultY;
        if(useNormalization){
            // Apply normalized logistic function to guarantee endpoint continuity
            float raw_logistic = logisticFunction(normalizedX);
            resultY = A + (K - A) * (raw_logistic - logistic_0) / logisticRange;
        } else {
            // Fall back to linear interpolation if logistic range is too small
            resultY = A + (K - A) * normalizedX;
        }
        
        glm::vec2 currentPoint = glm::vec2(segmentX, resultY);
        
        // Draw line segment from previous point to current point
        auto denormP1 = coordinateTransformer->denormalizePoint(prevPoint);
        auto denormP2 = coordinateTransformer->denormalizePoint(currentPoint);
        drawList->AddLine(denormP1, denormP2, segmentColor, lineWidth);
        
        prevPoint = currentPoint;
    }
}
