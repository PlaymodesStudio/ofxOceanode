#ifndef ofxOceanodeCustomGuiWidgetRegistry_h
#define ofxOceanodeCustomGuiWidgetRegistry_h


#include "CustomGui/ofxOceanodeCustomGuiLayout.h"
#include "ofGraphicsBaseTypes.h"
#include "imgui.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ofxOceanodeAbstractParameter;
class ofxOceanodeContainer;

struct CustomGuiWidgetRenderContext {
    ofxOceanodeContainer& container;
    std::string panelId;
    const CustomGuiWidget* widget = nullptr;
    bool designMode = false;
    float zoom = 1.0f;
    ImVec2 size = ImVec2(0, 0);
    std::string label;
    bool showValue = true;
    bool interactive = false;
    std::function<bool(CustomGuiWidget&, ofxOceanodeAbstractParameter*, std::vector<float>&, const ImVec2&, bool)> drawMultiSliderWidget;
    std::function<void(const ImVec2&, float, const ImU32&)> drawVerticalMeter;
    std::function<std::shared_ptr<ofImage>(const std::string&)> loadWidgetImage;
};

struct CustomGuiWidgetPropertiesContext {
    ofxOceanodeContainer& container;
    std::function<CustomGuiPanelData*()> getPanelData;
};

struct CustomGuiWidgetDefinition {
    CustomGuiWidgetType type = CustomGuiWidgetType::Label;
    std::function<bool(ofxOceanodeAbstractParameter&)> supportsParameter;
    std::function<void(CustomGuiWidget&, ofxOceanodeAbstractParameter&)> initializeWidget;
    std::function<bool(CustomGuiWidgetRenderContext&, CustomGuiWidget&, ofxOceanodeAbstractParameter*)> render;
    std::function<void(CustomGuiWidgetPropertiesContext&, CustomGuiWidget&, ofxOceanodeAbstractParameter*)> drawProperties;
    std::function<void(const std::string&, const std::string&)> cleanup;
};

class ofxOceanodeCustomGuiWidgetRegistry {
public:
    static ofxOceanodeCustomGuiWidgetRegistry& instance();

    void registerWidget(const CustomGuiWidgetDefinition& definition);
    const CustomGuiWidgetDefinition* getWidget(CustomGuiWidgetType type) const;
    std::vector<CustomGuiWidgetType> getCompatibleWidgets(ofxOceanodeAbstractParameter& parameter) const;
    CustomGuiWidgetType getDefaultWidgetType(ofxOceanodeAbstractParameter& parameter) const;

private:
    ofxOceanodeCustomGuiWidgetRegistry();

    std::unordered_map<int, CustomGuiWidgetDefinition> definitions;
};


#endif
