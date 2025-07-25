//
//  curve2.h
//  ofxOceanode
//
//  Created by Eduard Frigola on 25/05/21.
//
//

#ifndef curve2_h
#define curve2_h

#pragma once

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <memory>

//---------------------------------------------------------------
// Forward declarations for modular architecture
struct CurveData;
struct curvePoint2;
struct line2;
class ICoordinateTransformer;
class ICurveManagementInterface;
class ICanvasLayoutManager;
class IPointInteractionHandler;
class IParameterControlSystem;
class ICurveRenderingEngine;
class IVisualFeedbackRenderer;
class MultiCurveRenderer;

//---------------------------------------------------------------
// Abstract Base Classes for Modular Architecture
//---------------------------------------------------------------

// 1. Coordinate Transformation System Interface
class ICoordinateTransformer {
public:
    virtual ~ICoordinateTransformer() = default;
    
    // Core transformation functions
    virtual glm::vec2 normalizePoint(const glm::vec2& point) const = 0;
    virtual glm::vec2 denormalizePoint(const glm::vec2& point) const = 0;
    virtual glm::vec2 safeNormalizePoint(const glm::vec2& point) const = 0;
    
    // Configuration methods
    virtual void setVisualBounds(const ImVec2& winPos, const ImVec2& canvasSize) = 0;
    virtual void setCanvasBounds(const ImVec2& winPos, const ImVec2& canvasSize) = 0;
    virtual void setGridSnapping(bool enabled, int horizontalDivs, int verticalDivs) = 0;
};

// 2. Curve Management Interface
class ICurveManagementInterface {
public:
    virtual ~ICurveManagementInterface() = default;
    
    // Curve lifecycle management
    virtual void renderCurveControls() = 0;
    virtual bool handleCurveSelection(int& activeCurveIndex) = 0;
    virtual bool handleCurveAddition() = 0;
    virtual bool handleCurveRemoval(int curveIndex) = 0;
    
    // Curve property management
    virtual void renderCurveProperties(CurveData& curve) = 0;
    virtual bool validateCurveName(const std::string& name, int excludeIndex = -1) = 0;
};

// 3. Canvas Layout Manager Interface
class ICanvasLayoutManager {
public:
    virtual ~ICanvasLayoutManager() = default;
    
    // Layout calculation
    virtual void calculateLayout(const ImVec2& availableSize, ImVec2& canvasPos, ImVec2& canvasSize,
                               ImVec2& visualPos, ImVec2& visualSize) = 0;
    
    // Grid and background rendering
    virtual void renderGrid(ImDrawList* drawList, const ImVec2& visualPos, const ImVec2& visualSize,
                          int horizontalDivs, int verticalDivs) = 0;
    virtual void renderBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) = 0;
    
    // Safe zone management
    virtual float getSafeZonePadding() const = 0;
    virtual void setSafeZonePadding(float padding) = 0;
};

// 4. Point Interaction Handler Interface
class IPointInteractionHandler {
public:
    virtual ~IPointInteractionHandler() = default;
    
    struct MouseState {
        glm::vec2 position;
        glm::vec2 previousPosition;
        bool leftButtonDown;
        bool leftButtonClicked;
        bool leftButtonDoubleClicked;
        bool rightButtonClicked;
        bool isDragging;
        bool isHovered;
    };
    
    struct InteractionResult {
        bool stateChanged;
        bool needsRecalculation;
        bool needsRedraw;
        int hoveredPointIndex;
        std::vector<int> modifiedPointIndices;
        std::string statusMessage;
    };
    
    // Core interaction handling
    virtual InteractionResult handleMouseInteraction(const MouseState& mouseState,
                                                   std::vector<curvePoint2>& points,
                                                   std::vector<line2>& lines) = 0;
    
    // Specific interaction detection
    virtual int detectHoveredPoint(const glm::vec2& mousePos, const std::vector<curvePoint2>& points,
                                 float detectionRadius = 15.0f) = 0;
    virtual bool handlePointDrag(std::vector<curvePoint2>& points, const MouseState& mouseState) = 0;
    virtual bool handlePointCreation(std::vector<curvePoint2>& points, std::vector<line2>& lines,
                                   const glm::vec2& mousePos) = 0;
    
    // Context menu system
    virtual void renderPointContextMenu(curvePoint2& point, std::vector<line2>& lines, int pointIndex) = 0;
    
    // State management
    virtual void resetDragStates() = 0;
    virtual void setCoordinateTransformer(std::shared_ptr<ICoordinateTransformer> transformer) = 0;
};

// 5. Parameter Control System Interface
class IParameterControlSystem {
public:
    virtual ~IParameterControlSystem() = default;
    
    // Parameter drag operations
    virtual bool handleAsymmetryDrag(const glm::vec2& mousePos, const glm::vec2& mouseDelta,
                                   std::vector<line2>& lines, int segmentIndex) = 0;
    virtual bool handleInflectionDrag(const glm::vec2& mousePos, const glm::vec2& mouseDelta,
                                    std::vector<line2>& lines, int segmentIndex) = 0;
    virtual bool handleBParameterDrag(const glm::vec2& mousePos, const glm::vec2& mouseDelta,
                                    std::vector<line2>& lines, int segmentIndex) = 0;
    
    // Drag state management
    virtual void startAsymmetryDrag(int segmentIndex, const glm::vec2& startPos, float startValue) = 0;
    virtual void startInflectionDrag(int segmentIndex, const glm::vec2& startPos, float startValue) = 0;
    virtual void startBParameterDrag(int segmentIndex, const glm::vec2& startPos, float startValue) = 0;
    virtual void endAllDrags() = 0;
    
    // State queries
    virtual bool isAnyDragActive() const = 0;
    virtual int getActiveDragSegment() const = 0;
    
    // Configuration
    virtual void setCoordinateTransformer(std::shared_ptr<ICoordinateTransformer> transformer) = 0;
};

// 6. Curve Rendering Engine Interface
class ICurveRenderingEngine {
public:
    virtual ~ICurveRenderingEngine() = default;
    
    enum class RenderQuality {
        LOW = 0,     // 30 segments max, reduced calculations
        MEDIUM = 1,  // 50 segments max, standard calculations
        HIGH = 2,    // 100 segments max, full calculations
        ULTRA = 3    // 500 segments max, maximum quality
    };
    
    struct RenderContext {
        bool isActivePass;          // First pass (inactive) or second pass (active)
        bool performanceMode;       // Reduced quality for better performance
        RenderQuality quality;      // Overall quality setting
        std::vector<int> highlightedSegments;  // Segments to highlight
        std::map<int, int> dragHighlights;     // Drag-specific highlighting
    };
    
    // Core rendering pipeline
    virtual void renderCurves(ImDrawList* drawList, const std::vector<CurveData>& curves,
                            int activeCurveIndex, const RenderContext& context) = 0;
    
    // Individual curve rendering
    virtual void renderSingleCurve(ImDrawList* drawList, const CurveData& curve,
                                 bool isActive, const RenderContext& context) = 0;
    
    // Segment-specific rendering
    virtual void renderTensionSegment(ImDrawList* drawList, const curvePoint2& p1, const curvePoint2& p2,
                                    const line2& lineData, float globalQ, bool isActive,
                                    const RenderContext& context) = 0;
    
    // Configuration
    virtual void setCoordinateTransformer(std::shared_ptr<ICoordinateTransformer> transformer) = 0;
    virtual void setRenderQuality(RenderQuality quality) = 0;
    virtual void setHighlightedSegments(const std::vector<int>& segmentIndices) = 0;
};

// 7. Visual Feedback Renderer Interface
class IVisualFeedbackRenderer {
public:
    virtual ~IVisualFeedbackRenderer() = default;
    
    // Feedback line rendering
    virtual void renderInflectionLines(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                                     const std::vector<line2>& lines) = 0;
    virtual void renderAsymmetryLines(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                                    const std::vector<line2>& lines) = 0;
    virtual void renderBParameterLines(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                                     const std::vector<line2>& lines) = 0;
    
    // Point highlighting
    virtual void renderPointHighlights(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                                     int hoveredPointIndex, int selectedPointIndex) = 0;
    
    // Curve labels and info
    virtual void renderCurveLabels(ImDrawList* drawList, const std::vector<CurveData>& curves,
                                 int activeCurveIndex, bool showLabels) = 0;
    virtual void renderInputValueIndicator(ImDrawList* drawList, float inputValue, const ImVec2& canvasPos,
                                         const ImVec2& canvasSize) = 0;
    
    // Configuration
    virtual void setCoordinateTransformer(std::shared_ptr<ICoordinateTransformer> transformer) = 0;
    virtual void setShowInfo(bool show) = 0;
};

//---------------------------------------------------------------
// Standard Implementation Classes
//---------------------------------------------------------------

// Standard Coordinate Transformer Implementation
class StandardCoordinateTransformer : public ICoordinateTransformer {
private:
    ImVec2 visualWinPos;
    ImVec2 visualCanvasSize;
    ImVec2 canvasWinPos;
    ImVec2 canvasSize;
    bool gridSnappingEnabled;
    int horizontalDivisions;
    int verticalDivisions;
    
public:
    StandardCoordinateTransformer();
    virtual ~StandardCoordinateTransformer() = default;
    
    // ICoordinateTransformer implementation
    glm::vec2 normalizePoint(const glm::vec2& point) const override;
    glm::vec2 denormalizePoint(const glm::vec2& point) const override;
    glm::vec2 safeNormalizePoint(const glm::vec2& point) const override;
    
    void setVisualBounds(const ImVec2& winPos, const ImVec2& canvasSize) override;
    void setCanvasBounds(const ImVec2& winPos, const ImVec2& canvasSize) override;
    void setGridSnapping(bool enabled, int horizontalDivs, int verticalDivs) override;
    
    // Additional utility methods
    glm::vec2 snapToGrid(const glm::vec2& point) const;
    bool isWithinCanvas(const glm::vec2& point) const;
    bool isWithinVisualArea(const glm::vec2& point) const;
};

// Standard Canvas Layout Implementation
class StandardCanvasLayout : public ICanvasLayoutManager {
private:
    float safeZonePadding;
    
public:
    StandardCanvasLayout();
    virtual ~StandardCanvasLayout() = default;
    
    // ICanvasLayoutManager implementation
    void calculateLayout(const ImVec2& availableSize, ImVec2& canvasPos, ImVec2& canvasSize,
                        ImVec2& visualPos, ImVec2& visualSize) override;
    
    void renderGrid(ImDrawList* drawList, const ImVec2& visualPos, const ImVec2& visualSize,
                   int horizontalDivs, int verticalDivs) override;
    void renderBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) override;
    
    float getSafeZonePadding() const override;
    void setSafeZonePadding(float padding) override;
    
    // Additional utility methods
    float calculateWindowHeight(const ImVec2& windowSize, bool showInfoPanel) const;
    
    // Access to safe zone padding constant
    static constexpr float DEFAULT_SAFE_ZONE_PADDING = 10.0f;
};

// Standard Curve Management Implementation
class ShowAllCurveManager : public ICurveManagementInterface {
private:
    std::vector<CurveData>* curves;
    ofParameter<int>* activeCurve;
    ofParameter<int>* numCurves;
    std::vector<std::shared_ptr<ofParameter<std::vector<float>>>>* outputs;
    std::function<void()> recalculateCallback;
    std::function<void()> addCurveCallback;
    std::function<void(int)> removeCurveCallback;
    
public:
    ShowAllCurveManager();
    virtual ~ShowAllCurveManager() = default;
    
    // ICurveManagementInterface implementation
    void renderCurveControls() override;
    bool handleCurveSelection(int& activeCurveIndex) override;
    bool handleCurveAddition() override;
    bool handleCurveRemoval(int curveIndex) override;
    void renderCurveProperties(CurveData& curve) override;
    bool validateCurveName(const std::string& name, int excludeIndex = -1) override;
    
    // Configuration methods
    void setCurveData(std::vector<CurveData>* curvesPtr, ofParameter<int>* activeCurvePtr,
                     ofParameter<int>* numCurvesPtr, std::vector<std::shared_ptr<ofParameter<std::vector<float>>>>* outputsPtr);
    void setCallbacks(std::function<void()> recalculate, std::function<void()> addCurve, std::function<void(int)> removeCurve);
    
    // Main interface rendering method
    void renderInterface();
    
    // Individual component methods
    void handleAddCurve();
    void handleRemoveCurve();
    void handleResetCurve();
    void renderCurveSelector();
    void renderCurveProperties();
    
    // Validation methods
    bool validateCurveLimit() const;
    std::string generateUniqueName(const std::string& baseName) const;
};

// Standard Visual Feedback Implementation
class StandardVisualFeedback : public IVisualFeedbackRenderer {
private:
    std::shared_ptr<ICoordinateTransformer> coordinateTransformer;
    bool showInfo;
    ImVec2 visualWinPos; // Store visual window position for label rendering
    
    // State tracking for conditional rendering
    bool tensionDragActive;
    bool bDragActive;
    int tensionDragSegmentIndex;
    int bDragSegmentIndex;
    int hoveredSegmentIndex;
    int hoveredPointIndex;
    bool canvasHovered;
    
    // Parameter limits
    static constexpr float MIN_B_PARAMETER = 0.01f;
    static constexpr float MAX_B_PARAMETER = 100.0f;
    static constexpr float MIN_ASYMMETRY = 0.02f;
    static constexpr float MAX_ASYMMETRY = 10.0f;
    
public:
    StandardVisualFeedback();
    virtual ~StandardVisualFeedback() = default;
    
    // IVisualFeedbackRenderer implementation
    void renderInflectionLines(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                             const std::vector<line2>& lines) override;
    void renderAsymmetryLines(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                            const std::vector<line2>& lines) override;
    void renderBParameterLines(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                             const std::vector<line2>& lines) override;
    void renderPointHighlights(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                             int hoveredPointIndex, int selectedPointIndex) override;
    void renderCurveLabels(ImDrawList* drawList, const std::vector<CurveData>& curves,
                         int activeCurveIndex, bool showLabels) override;
    void renderInputValueIndicator(ImDrawList* drawList, float inputValue, const ImVec2& canvasPos,
                                 const ImVec2& canvasSize) override;
    
    void setCoordinateTransformer(std::shared_ptr<ICoordinateTransformer> transformer) override;
    void setShowInfo(bool show) override;
    
    // Main rendering method that orchestrates all visual feedback
    void renderFeedback(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                       const std::vector<line2>& lines, const std::vector<CurveData>& curves,
                       int activeCurveIndex, bool showLabels, const ImVec2& visualWinPos);
    
    // State management methods
    void updateDragStates(bool tensionActive, bool bActive, int tensionSegment, int bSegment);
    void updateHoverStates(int hoveredSegment, int hoveredPoint, bool canvasHover);
    
    // Utility method for drawing dotted lines
    void drawDottedLine(ImDrawList* drawList, ImVec2 start, ImVec2 end, ImU32 color,
                       float dashLength = 5.0f, float gapLength = 10.0f);
};

// Standard Parameter Controller Implementation
class StandardParameterController : public IParameterControlSystem {
private:
    std::shared_ptr<ICoordinateTransformer> coordinateTransformer;
    
    // Drag state management
    bool tensionDragActive;
    bool bDragActive;
    glm::vec2 tensionDragStartPos;
    glm::vec2 bDragStartPos;
    int tensionDragSegmentIndex;
    int bDragSegmentIndex;
    float tensionDragStartExponent;
    float bDragStartValue;
    
    // Parameter limits
    static constexpr float MIN_B_PARAMETER = 0.01f;
    static constexpr float MAX_B_PARAMETER = 100.0f;
    static constexpr float MIN_INFLECTION_X = 0.01f;
    static constexpr float MAX_INFLECTION_X = 0.99f;
    static constexpr float MIN_ASYMMETRY = 0.02f;
    static constexpr float MAX_ASYMMETRY = 10.0f;
    
    // Callbacks
    std::function<void()> recalculateCallback;
    
public:
    StandardParameterController();
    virtual ~StandardParameterController() = default;
    
    // IParameterControlSystem implementation
    bool handleAsymmetryDrag(const glm::vec2& mousePos, const glm::vec2& mouseDelta,
                           std::vector<line2>& lines, int segmentIndex) override;
    bool handleInflectionDrag(const glm::vec2& mousePos, const glm::vec2& mouseDelta,
                            std::vector<line2>& lines, int segmentIndex) override;
    bool handleBParameterDrag(const glm::vec2& mousePos, const glm::vec2& mouseDelta,
                            std::vector<line2>& lines, int segmentIndex) override;
    
    void startAsymmetryDrag(int segmentIndex, const glm::vec2& startPos, float startValue) override;
    void startInflectionDrag(int segmentIndex, const glm::vec2& startPos, float startValue) override;
    void startBParameterDrag(int segmentIndex, const glm::vec2& startPos, float startValue) override;
    void endAllDrags() override;
    
    bool isAnyDragActive() const override;
    int getActiveDragSegment() const override;
    
    void setCoordinateTransformer(std::shared_ptr<ICoordinateTransformer> transformer) override;
    
    // Main parameter drag processing method
    bool handleParameterDrag(bool shiftPressed, bool ctrlPressed, bool canvasHovered,
                           bool someItemClicked, std::vector<curvePoint2>& points,
                           std::vector<line2>& lines);
    
    // Specific drag handlers
    bool handleTensionDrag(bool shiftPressed, bool canvasHovered, bool someItemClicked,
                         std::vector<curvePoint2>& points, std::vector<line2>& lines);
    bool handleBParameterDragControl(bool ctrlPressed, bool canvasHovered, bool someItemClicked,
                                   std::vector<curvePoint2>& points, std::vector<line2>& lines);
    
    // Drag operation methods
    void startTensionDrag(const glm::vec2& startPos, std::vector<curvePoint2>& points,
                        std::vector<line2>& lines);
    void startBParameterDragOperation(const glm::vec2& startPos, std::vector<curvePoint2>& points,
                                    std::vector<line2>& lines);
    void updateTensionDrag(const glm::vec2& currentPos, std::vector<curvePoint2>& points,
                         std::vector<line2>& lines);
    void updateBParameterDrag(const glm::vec2& currentPos, std::vector<curvePoint2>& points,
                            std::vector<line2>& lines);
    void endParameterDrag();
    
    // Segment detection
    int detectHoveredSegment(const glm::vec2& mousePos, std::vector<curvePoint2>& points,
                           std::vector<line2>& lines);
    
    // State queries
    bool isTensionDragActive() const { return tensionDragActive; }
    bool isBDragActive() const { return bDragActive; }
    int getTensionDragSegmentIndex() const { return tensionDragSegmentIndex; }
    int getBDragSegmentIndex() const { return bDragSegmentIndex; }
    
    // Configuration
    void setRecalculateCallback(std::function<void()> callback);
};

// Standard Point Interaction Handler Implementation
class StandardPointInteractionHandler : public IPointInteractionHandler {
public:
    // Interaction context structure - moved to public for external access
    struct InteractionContext {
        ImVec2 canvasBounds;
        ImVec2 canvasSize;
        bool snapToGrid;
        int horizontalDivisions;
        int verticalDivisions;
        float minX;
        float maxX;
    };

private:
    std::shared_ptr<ICoordinateTransformer> coordinateTransformer;
    
    // Drag state management
    bool someItemClicked;
    int hoveredPointIndex;
    int indexToRemove;
    
    // Parameter limits
    static constexpr float MIN_B_PARAMETER = 0.01f;
    static constexpr float MAX_B_PARAMETER = 100.0f;
    static constexpr float MIN_ASYMMETRY = 0.02f;
    static constexpr float MAX_ASYMMETRY = 10.0f;
    static constexpr float POINT_DETECTION_RADIUS = 15.0f;
    
    // Callbacks
    std::function<void()> recalculateCallback;
    
public:
    StandardPointInteractionHandler();
    virtual ~StandardPointInteractionHandler() = default;
    
    // IPointInteractionHandler implementation
    InteractionResult handleMouseInteraction(const MouseState& mouseState,
                                           std::vector<curvePoint2>& points,
                                           std::vector<line2>& lines) override;
    
    int detectHoveredPoint(const glm::vec2& mousePos, const std::vector<curvePoint2>& points,
                          float detectionRadius = POINT_DETECTION_RADIUS) override;
    
    bool handlePointDrag(std::vector<curvePoint2>& points, const MouseState& mouseState) override;
    
    bool handlePointCreation(std::vector<curvePoint2>& points, std::vector<line2>& lines,
                           const glm::vec2& mousePos) override;
    
    void renderPointContextMenu(curvePoint2& point, std::vector<line2>& lines, int pointIndex) override;
    
    void resetDragStates() override;
    void setCoordinateTransformer(std::shared_ptr<ICoordinateTransformer> transformer) override;
    
    // Main interaction processing method
    InteractionResult processInteraction(bool canvasHovered, bool canvasClicked, bool canvasDoubleClicked,
                                       std::vector<curvePoint2>& points, std::vector<line2>& lines,
                                       const InteractionContext& context);
    
    // Point management methods
    void sortPointsByX(std::vector<curvePoint2>& points);
    bool validatePointRemoval(const std::vector<curvePoint2>& points, int pointIndex);
    
    // Grid snapping utilities
    glm::vec2 applyGridSnapping(const glm::vec2& point, const InteractionContext& context);
    
    // Context menu system
    void renderCoordinateSliders(curvePoint2& point, float minX, float maxX);
    void renderLineControls(std::vector<line2>& lines, int pointIndex);
    void renderCurveProperties(ofParameter<float>* globalQ);
    
    // State management
    int getHoveredPointIndex() const { return hoveredPointIndex; }
    bool hasItemClicked() const { return someItemClicked; }
    
    // Configuration
    void setRecalculateCallback(std::function<void()> callback);
};

//---------------------------------------------------------------
// MultiCurveRenderer Implementation
//---------------------------------------------------------------

class MultiCurveRenderer {
public:
    enum class RenderQuality {
        LOW = 0,     // 30 segments max, reduced calculations
        MEDIUM = 1,  // 50 segments max, standard calculations
        HIGH = 2,    // 100 segments max, full calculations
        ULTRA = 3    // 500 segments max, maximum quality
    };
    
    struct RenderContext {
        ImVec2 canvasPos;           // Canvas position
        ImVec2 canvasSize;          // Canvas size
        bool isActivePass;          // First pass (inactive) or second pass (active)
        bool performanceMode;       // Reduced quality for better performance
        RenderQuality quality;      // Overall quality setting
        std::vector<int> highlightedSegments;  // Segments to highlight
        std::map<int, int> dragHighlights;     // Drag-specific highlighting
    };

private:
    std::shared_ptr<ICoordinateTransformer> coordinateTransformer;
    RenderQuality renderQuality;
    std::vector<int> highlightedSegments;
    
    // Rendering constants
    static constexpr int BASE_SEGMENTS_ACTIVE = 50;
    static constexpr int BASE_SEGMENTS_INACTIVE = 30;
    static constexpr int MAX_SEGMENTS_ACTIVE = 100;
    static constexpr int MAX_SEGMENTS_INACTIVE = 50;
    static constexpr float EXTENSION_LINE_ALPHA = 0.3f;
    static constexpr float INACTIVE_CURVE_ALPHA = 0.5f;
    static constexpr float INACTIVE_OPACITY = 0.5f;
    static constexpr float ACTIVE_LINE_WIDTH = 2.0f;
    static constexpr float INACTIVE_LINE_WIDTH = 1.5f;

public:
    MultiCurveRenderer();
    virtual ~MultiCurveRenderer() = default;
    
    // Core rendering pipeline
    void renderCurves(ImDrawList* drawList, const std::vector<CurveData>& curves,
                     int activeCurveIndex, const RenderContext& context,
                     std::shared_ptr<StandardParameterController> paramController);
    
    // Individual curve rendering
    void renderSingleCurve(ImDrawList* drawList, const CurveData& curve,
                          bool isActive, const RenderContext& context,
                          std::shared_ptr<StandardParameterController> paramController);
    
    // Segment-specific rendering
    void renderTensionSegment(ImDrawList* drawList, const curvePoint2& p1, const curvePoint2& p2,
                             const line2& lineData, float globalQ, bool isActive,
                             ImU32 segmentColor, float lineWidth);
    
    void renderHoldSegment(ImDrawList* drawList, const curvePoint2& p1, const curvePoint2& p2,
                          ImU32 segmentColor, float lineWidth);
    
    void renderExtensionLines(ImDrawList* drawList, const std::vector<curvePoint2>& points,
                             const ofColor& curveColor, bool isActive);
    
    // Configuration
    void setCoordinateTransformer(std::shared_ptr<ICoordinateTransformer> transformer);
    void setRenderQuality(RenderQuality quality);
    void setHighlightedSegments(const std::vector<int>& segmentIndices);
    
    // Utility methods
    int calculateAdaptiveSegments(float segmentB, bool isActive) const;
    ImU32 calculateSegmentColor(const ofColor& curveColor, bool isActive,
                               bool whiteHighlight, bool activeHighlight) const;
    
    // Logistic function calculation
    float calculateLogisticFunction(float x, float A, float K, float M, float nu, float B, float Q) const;
    bool shouldUseNormalization(float logisticRange) const;
    void renderLogisticCurve(ImDrawList* drawList, const curvePoint2& p1, const curvePoint2& p2,
                            const line2& lineData, float globalQ, ImU32 segmentColor,
                            float lineWidth, int segments);
};

//---------------------------------------------------------------

struct curvePoint2{
	curvePoint2(){
		drag = 0;
		firstCreated = false;
	}
	curvePoint2(glm::vec2 p){
		point = p;
		drag = 0;
		firstCreated = true;
	}
	curvePoint2(float x, float y){
		point = glm::vec2(x, y);
		drag = 0;
		firstCreated = true;
	}
	glm::vec2 point;
	int drag;
	bool firstCreated;
};

//---------------------------------------------------------------

enum lineType2{
	LINE2_HOLD,
	LINE2_TENSION
};

//---------------------------------------------------------------

struct line2{
	lineType2 type = LINE2_TENSION;
	float tensionExponent = 1.0f;  // Now controls asymmetry parameter ν
	float inflectionX = 0.5f;      // Normalized position 0-1 within segment
	float segmentB = 6.0f;         // Per-segment B parameter (default 6.0)
	float segmentQ = 1.0f;         // Per-segment Q parameter (default 1.0)
};

//---------------------------------------------------------------

// CurveData structure to encapsulate all curve-specific data
struct CurveData {
	vector<curvePoint2> points;
	vector<line2> lines;
	
	// Per-curve parameters
	ofParameter<string> name;
	ofParameter<ofColor> color;
	ofParameter<bool> enabled;
	ofParameter<float> globalQ;  // Per-curve Q parameter
	
	CurveData() {
		// Initialize with default curve (0,0) to (1,1)
		points.emplace_back(0, 0);
		points.emplace_back(1, 1);
		points.front().firstCreated = false;
		points.back().firstCreated = false;
		lines.emplace_back();
		
		// Set default parameter values
		name.set("Name", "Curve");
		color.set("Color", ofColor(255, 128, 0, 255));
		enabled.set("Enabled", true);
		globalQ.set("Global Q", 1.0f, 0.1f, 5.0f);
	}
};

//---------------------------------------------------------------

class curve2 : public ofxOceanodeNodeModel{
public:
	curve2();
	~curve2(){};
	void setup() override;
	void draw(ofEventArgs &args) override;
	
	void recalculate();
	
	void presetSave(ofJson &json);
	void presetRecallAfterSettingParameters(ofJson &json);
	
private:
	// Parameter limits as class constants
	static constexpr float MIN_B_PARAMETER = 0.01f;
	static constexpr float MAX_B_PARAMETER = 100.0f;
	static constexpr float MIN_INFLECTION_X = 0.01f;
	static constexpr float MAX_INFLECTION_X = 0.99f;
	static constexpr float MIN_ASYMMETRY = 0.02f;
	static constexpr float MAX_ASYMMETRY = 10.0f;  // Changed from 100.0
	
	ofEventListeners listeners;
	
	ofParameter<vector<float>>  input;
	ofParameter<bool> showWindowAll;
	ofParameter<bool> showWindowSplit;
	//vector<ofParameter<vector<float>>> outputs;
	vector<shared_ptr<ofParameter<vector<float>>>> outputs;
	ofParameter<vector<float>> allCurvesOutput;  // Consolidated output parameter
	
	ofParameter<int> numVerticalDivisions;
	ofParameter<int> numHorizontalDivisions;
	
	// Global constants (apply to all curves)
	static constexpr float minX = 0.0f;
	static constexpr float maxX = 1.0f;
	
	
	// Multi-curve support
	ofParameter<int> numCurves;      // Number of curves (default: 1)
	ofParameter<int> activeCurve;    // Currently selected curve index (-1 = None, 0+ = curve index)
	vector<CurveData> curves;        // Vector of curve data
	
	// Legacy single-curve compatibility (pointers to active curve)
	vector<curvePoint2>* points;     // Pointer to active curve points
	vector<line2>* lines;            // Pointer to active curve lines
	
	// Parameter control is now handled by the StandardParameterController component
	
	// Point highlighting system
	int hoveredPointIndex = -1;
	
	// Segment hover detection system
	int hoveredSegmentIndex = -1;
	
	bool showCurveLabels = true;       // Show curve names in editor
	float curveHitTestRadius = 8.0f;   // Distance threshold for curve selection
	
	// Performance optimization flags
	bool needsRedraw = true;           // Flag to minimize unnecessary redraws
	
	// Legacy compatibility parameters (now point to active curve)
	ofParameter<ofColor>* colorParam;
	ofParameter<float>* globalQ;
	
	ofColor color;
	ofEventListener colorListener;
	
	// Snap to grid functionality
	ofParameter<bool> snapToGrid;  // Enable/disable snap to grid
	
	// Show info toggle functionality
	ofParameter<bool> showInfo;  // Enable/disable text area visibility
	
	// Inspector GUI parameter for tabbed interface
	ofParameter<std::function<void()>> inspectorGui;
	
	// Curve management methods
	void addCurve();
	void removeCurve(int index);
	void resizeCurves(int newCount);
	CurveData& getCurrentCurve();
	const CurveData& getCurrentCurve() const;
	int findNextAvailableCurveNumber();
	
	// Output management methods
	void addOutput();
	void removeOutput();
	void resizeOutputs(int count);
	
	// Parameter listeners
	void onNumCurvesChanged(int& newCount);
	void onActiveCurveChanged(int& newIndex);
	
	// Inspector interface rendering
	void renderInspectorInterface();
	void drawDottedLine(ImDrawList* drawList, ImVec2 start, ImVec2 end, ImU32 color, float dashLength = 5.0f, float gapLength = 10.0f);
	
	// Modular architecture components
	std::shared_ptr<StandardCoordinateTransformer> coordinateTransformer;
	std::shared_ptr<StandardCanvasLayout> canvasLayout;
	std::shared_ptr<ShowAllCurveManager> curveManager;
	std::shared_ptr<StandardVisualFeedback> visualFeedback;
	std::shared_ptr<StandardPointInteractionHandler> pointHandler;
	std::shared_ptr<StandardParameterController> parameterController;
	std::shared_ptr<MultiCurveRenderer> curveRenderer;
	
	// Component initialization
	void initializeComponents();
};


#endif /* curve2_h */
