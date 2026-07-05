#ifndef ofxOceanodeCustomGuiPanel_h
#define ofxOceanodeCustomGuiPanel_h


#include "CustomGui/ofxOceanodeCustomGuiLayout.h"
#include <map>
#include <memory>
#include <vector>

class ofxOceanodeContainer;
class ofxOceanodeAbstractParameter;
struct ImVec2;
typedef unsigned int ImU32;

class ofxOceanodeCustomGuiPanel {
public:
    ofxOceanodeCustomGuiPanel(ofxOceanodeContainer& container, const std::string& panelId);

    void draw();

    bool addParameter(ofxOceanodeAbstractParameter& parameter, CustomGuiWidgetType type);
    bool containsParameter(ofxOceanodeAbstractParameter& parameter) const;
    bool removeParameter(const std::string& parameterPath);

    std::vector<CustomGuiWidgetType> getCompatibleWidgetTypes(ofxOceanodeAbstractParameter& parameter) const;
    CustomGuiWidgetType getDefaultWidgetType(ofxOceanodeAbstractParameter& parameter) const;

private:
    CustomGuiPanelData* getPanelData();
    const CustomGuiPanelData* getPanelData() const;

    ofxOceanodeAbstractParameter* findParameter(const CustomGuiWidget& widget) const;
    bool drawWidgetProperties(CustomGuiWidget& widget, size_t widgetIndex, ofxOceanodeAbstractParameter* parameter);
    bool renderWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter, const ImVec2& size);
    bool drawMultiSliderWidget(CustomGuiWidget& widget,
                               ofxOceanodeAbstractParameter* parameter,
                               std::vector<float>& value,
                               const ImVec2& size,
                               bool interactive) const;
    void openSetValuePopup(ofxOceanodeAbstractParameter& parameter, const std::string& label);
    void drawSetValuePopup();
    void openSetVectorSizePopup(ofxOceanodeAbstractParameter& parameter, const std::string& label);
    void drawSetVectorSizePopup();
    bool resetParameterToDefaultValue(const CustomGuiWidget& widget, ofxOceanodeAbstractParameter& parameter) const;
    bool widgetSupportsDefaultValue(ofxOceanodeAbstractParameter& parameter) const;
    bool applyWidgetConfiguredDefaultValue(const CustomGuiWidget& widget, ofxOceanodeAbstractParameter& parameter) const;
    ofJson makeParameterDefaultValueJson(ofxOceanodeAbstractParameter& parameter) const;
    void drawWidgetDefaultValueProperties(CustomGuiWidget& widget, ofxOceanodeAbstractParameter& parameter);
    void drawVerticalMeter(const ImVec2& size, float normalized, const ImU32& color) const;
    void drawGridOverlay(const CustomGuiLayout& layout, const ImVec2& origin) const;
    void ensureLayoutFitsWidgets(CustomGuiLayout& layout) const;
    std::pair<int, int> findNextAvailableCell(const CustomGuiLayout& layout, int spanW, int spanH) const;
    std::string getFallbackLabel(const CustomGuiWidget& widget) const;
    bool shouldShowNumericValue(const CustomGuiWidget& widget) const;
    std::shared_ptr<ofImage> loadWidgetImage(const std::string& imagePath) const;
    bool removeWidgetsByIndices(const std::vector<int>& widgetIndices);
    bool removeSelectedWidgets();

    ofxOceanodeContainer& container;
    std::string panelId;
    mutable bool appliedWindowState = false;
    mutable std::map<std::string, std::shared_ptr<ofImage>> imageCache;
    bool requestOpenSetValuePopup = false;
    std::string setValueParameterPath;
    std::string setValueLabel;
    double setValueScalar = 0.0;
    std::vector<double> setValueVectorValues;
    bool requestOpenSetVectorSizePopup = false;
    std::string setVectorSizeParameterPath;
    std::string setVectorSizeLabel;
    int setVectorSizeValue = 1;
    bool requestOpenWidgetPropertiesPopup = false;
    int propertiesWidgetIndex = -1;
    bool requestOpenRenameSnapshotPopup = false;
    bool requestOpenDeleteSnapshotPopup = false;
    bool requestOpenDeletePanelPopup = false;
    bool createSnapshotFromPopup = false;
    std::string snapshotRenameId;
    std::string snapshotRenameValue;
    std::string snapshotDeleteId;
    std::vector<int> selectedWidgetIndices;
};


#endif /* ofxOceanodeCustomGuiPanel_h */
