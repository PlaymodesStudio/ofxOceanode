#ifndef ofxOceanodeCustomGuiWidgetHelpers_h
#define ofxOceanodeCustomGuiWidgetHelpers_h


#include "CustomGui/ofxOceanodeCustomGuiWidgetRegistry.h"
#include "ofxOceanodeParameter.h"
#include <cmath>

namespace ofxOceanodeCustomGuiWidgetHelpers {

inline void registerWidget(ofxOceanodeCustomGuiWidgetRegistry& registry,
                           CustomGuiWidgetType type,
                           const std::function<bool(ofxOceanodeAbstractParameter&)>& supportsParameter,
                           const std::function<void(CustomGuiWidget&, ofxOceanodeAbstractParameter&)>& initializeWidget,
                           const std::function<bool(CustomGuiWidgetRenderContext&, CustomGuiWidget&, ofxOceanodeAbstractParameter*)>& render,
                           const std::function<void(CustomGuiWidgetPropertiesContext&, CustomGuiWidget&, ofxOceanodeAbstractParameter*)>& drawProperties = nullptr,
                           const std::function<void(const std::string&, const std::string&)>& cleanup = nullptr)
{
    CustomGuiWidgetDefinition definition;
    definition.type = type;
    definition.supportsParameter = supportsParameter;
    definition.initializeWidget = initializeWidget;
    definition.render = render;
    definition.drawProperties = drawProperties;
    definition.cleanup = cleanup;
    registry.registerWidget(definition);
}

inline ImVec4 colorToImVec4(const ofColor& color, float alphaScale = 1.0f)
{
    return ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, (color.a / 255.0f) * alphaScale);
}

inline ofColor widgetBodyColor(const CustomGuiWidget& widget, const ofColor& fallback = ofColor(90, 90, 90, 220))
{
    if(widget.config.contains("bodyColor") && widget.config["bodyColor"].is_array()){
        return customGuiColorFromJson(widget.config["bodyColor"], fallback);
    }
    return fallback;
}

inline void ensureWidgetBodyColor(CustomGuiWidget& widget, const ofColor& fallback = ofColor(90, 90, 90, 220))
{
    if(!widget.config.contains("bodyColor")){
        widget.config["bodyColor"] = customGuiColorToJson(fallback);
    }
}

inline ofColor widgetLabelColor(const CustomGuiWidget& widget, const ofColor& fallback = ofColor::black)
{
    if(widget.config.contains("labelColor") && widget.config["labelColor"].is_array()){
        return customGuiColorFromJson(widget.config["labelColor"], fallback);
    }
    return fallback;
}

inline void ensureWidgetLabelColor(CustomGuiWidget& widget, const ofColor& fallback = ofColor::black)
{
    if(!widget.config.contains("labelColor")){
        widget.config["labelColor"] = customGuiColorToJson(fallback);
    }
}

inline void pushWidgetFrameColors(const ofColor& bodyColor, const ofColor& accentColor)
{
    const ImVec4 base = colorToImVec4(bodyColor);
    const ImVec4 accent = colorToImVec4(accentColor);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, base);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(std::min(1.0f, base.x + 0.08f),
                                                          std::min(1.0f, base.y + 0.08f),
                                                          std::min(1.0f, base.z + 0.08f),
                                                          std::min(1.0f, base.w + 0.08f)));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(std::min(1.0f, base.x + 0.12f),
                                                         std::min(1.0f, base.y + 0.12f),
                                                         std::min(1.0f, base.z + 0.12f),
                                                         std::min(1.0f, base.w + 0.12f)));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(accent.x, accent.y, accent.z, std::max(0.6f, accent.w)));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(std::min(1.0f, accent.x + 0.12f),
                                                            std::min(1.0f, accent.y + 0.12f),
                                                            std::min(1.0f, accent.z + 0.12f),
                                                            std::max(0.8f, accent.w)));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(accent.x, accent.y, accent.z, std::max(0.8f, accent.w)));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(accent.x, accent.y, accent.z, std::max(0.8f, accent.w)));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogramHovered, ImVec4(std::min(1.0f, accent.x + 0.12f),
                                                                std::min(1.0f, accent.y + 0.12f),
                                                                std::min(1.0f, accent.z + 0.12f),
                                                                std::max(0.9f, accent.w)));
}

inline bool widgetSupportsLabel(const CustomGuiWidget& widget)
{
    return widget.type != CustomGuiWidgetType::BackgroundPanel &&
           widget.type != CustomGuiWidgetType::Text &&
           widget.type != CustomGuiWidgetType::Line &&
           widget.type != CustomGuiWidgetType::Image &&
           widget.type != CustomGuiWidgetType::Button;
}

inline bool shouldShowWidgetLabel(const CustomGuiWidget& widget)
{
    return widgetSupportsLabel(widget) && widget.config.value("showLabel", true);
}

inline void drawWidgetLabel(const CustomGuiWidget& widget, const std::string& label)
{
    if(!shouldShowWidgetLabel(widget)){
        return;
    }

    const ofColor labelColor = widgetLabelColor(widget, ofColor::black);
    ImGui::PushStyleColor(ImGuiCol_Text, colorToImVec4(labelColor));
    ImGui::TextWrapped("%s", label.c_str());
    ImGui::PopStyleColor();
}

inline ImVec2 widgetItemSize(const CustomGuiWidgetRenderContext& context)
{
    const bool reserveLabelSpace = shouldShowWidgetLabel(*context.widget) && !context.label.empty();
    const float labelHeight = reserveLabelSpace ? ImGui::GetFrameHeightWithSpacing() : 0.0f;
    return ImVec2(context.size.x, std::max(1.0f, context.size.y - labelHeight));
}

inline float widgetItemWidth(const ImVec2& itemSize)
{
    return std::max(40.0f, itemSize.x);
}

inline bool useCustomRange(const CustomGuiWidget& widget)
{
    return widget.config.value("useCustomRange", false);
}

inline int quantization(const CustomGuiWidget& widget)
{
    return std::max(0, widget.config.value("quantization", 0));
}

inline float floatRangeMin(const CustomGuiWidget& widget, float fallbackMin)
{
    return useCustomRange(widget) ? widget.config.value("rangeMin", fallbackMin) : fallbackMin;
}

inline float floatRangeMax(const CustomGuiWidget& widget, float fallbackMax)
{
    return useCustomRange(widget) ? widget.config.value("rangeMax", fallbackMax) : fallbackMax;
}

inline int intRangeMin(const CustomGuiWidget& widget, int fallbackMin)
{
    return useCustomRange(widget) ? (int)std::round(widget.config.value("rangeMin", (float)fallbackMin)) : fallbackMin;
}

inline int intRangeMax(const CustomGuiWidget& widget, int fallbackMax)
{
    return useCustomRange(widget) ? (int)std::round(widget.config.value("rangeMax", (float)fallbackMax)) : fallbackMax;
}

inline float quantizeFloatValue(const CustomGuiWidget& widget, float rawValue, float minValue, float maxValue)
{
    const int steps = quantization(widget);
    if(steps < 2 || maxValue <= minValue) return rawValue;
    const float step = (maxValue - minValue) / (float)(steps - 1);
    if(step <= 0.0f) return rawValue;
    const float snapped = minValue + std::round((rawValue - minValue) / step) * step;
    return ofClamp(snapped, minValue, maxValue);
}

inline void pushToggleOnColors(const ofColor& color)
{
    ImGui::PushStyleColor(ImGuiCol_Button, colorToImVec4(color, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(std::min(1.0f, color.r / 255.0f + 0.12f),
                                                         std::min(1.0f, color.g / 255.0f + 0.12f),
                                                         std::min(1.0f, color.b / 255.0f + 0.12f),
                                                         0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(std::min(1.0f, color.r / 255.0f + 0.2f),
                                                        std::min(1.0f, color.g / 255.0f + 0.2f),
                                                        std::min(1.0f, color.b / 255.0f + 0.2f),
                                                        1.0f));
}

inline bool isFloatParameter(ofxOceanodeAbstractParameter& parameter)
{
    return dynamic_cast<ofxOceanodeParameter<float>*>(&parameter) != nullptr ||
           parameter.valueType() == typeid(float).name();
}

inline bool isIntParameter(ofxOceanodeAbstractParameter& parameter)
{
    return dynamic_cast<ofxOceanodeParameter<int>*>(&parameter) != nullptr ||
           parameter.valueType() == typeid(int).name();
}

inline bool isBoolParameter(ofxOceanodeAbstractParameter& parameter)
{
    return dynamic_cast<ofxOceanodeParameter<bool>*>(&parameter) != nullptr ||
           parameter.valueType() == typeid(bool).name();
}

inline bool isTriggerParameter(ofxOceanodeAbstractParameter& parameter)
{
    return dynamic_cast<ofxOceanodeParameter<void>*>(&parameter) != nullptr ||
           parameter.valueType() == typeid(void).name();
}

inline bool isStringParameter(ofxOceanodeAbstractParameter& parameter)
{
    return dynamic_cast<ofxOceanodeParameter<std::string>*>(&parameter) != nullptr ||
           parameter.valueType() == typeid(std::string).name();
}

inline bool isFunctionParameter(ofxOceanodeAbstractParameter& parameter)
{
    return dynamic_cast<ofxOceanodeParameter<std::function<void()>>*>(&parameter) != nullptr ||
           parameter.valueType() == typeid(std::function<void()>).name();
}

inline bool isRegisteredCustomRegionParameter(ofxOceanodeAbstractParameter& parameter)
{
    if(!isFunctionParameter(parameter)) return false;
    if(parameter.getName().empty()) return false;
    if(parameter.getName().find("SEPARATOR:|") == 0) return false;
    if(parameter.getName().find("Separator") != std::string::npos) return false;

    constexpr int requiredFlags =
        ofxOceanodeParameterFlags_DisableSavePreset |
        ofxOceanodeParameterFlags_DisableSaveProject |
        ofxOceanodeParameterFlags_DisableInConnection |
        ofxOceanodeParameterFlags_DisableOutConnection;

    return (parameter.getFlags() & requiredFlags) == requiredFlags;
}

inline bool isFloatVectorParameter(ofxOceanodeAbstractParameter& parameter)
{
    return dynamic_cast<ofxOceanodeParameter<std::vector<float>>*>(&parameter) != nullptr ||
           parameter.valueType() == typeid(std::vector<float>).name();
}

inline bool isIntVectorParameter(ofxOceanodeAbstractParameter& parameter)
{
    return dynamic_cast<ofxOceanodeParameter<std::vector<int>>*>(&parameter) != nullptr ||
           parameter.valueType() == typeid(std::vector<int>).name();
}

inline bool isTextureParameter(ofxOceanodeAbstractParameter& parameter)
{
    return dynamic_cast<ofxOceanodeParameter<ofTexture*>*>(&parameter) != nullptr ||
           parameter.valueType() == typeid(ofTexture*).name();
}

} // namespace ofxOceanodeCustomGuiWidgetHelpers


#endif
