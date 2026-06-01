
#include "CustomGui/Widgets/ofxOceanodeCustomGuiValueWidgets.h"
#include "CustomGui/Widgets/ofxOceanodeCustomGuiWidgetHelpers.h"
#include "CustomGui/ofxOceanodeCustomGuiWidgets.h"
#include "Managers/ofxOceanodeContainer.h"
#include "ofxOceanodeShared.h"
#include "ofMain.h"
#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace {

using namespace ofxOceanodeCustomGuiWidgetHelpers;

bool supportsSliderWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isFloatParameter(parameter) || isIntParameter(parameter);
}

bool supportsKnobWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isFloatParameter(parameter) ||
           isIntParameter(parameter) ||
           isFloatVectorParameter(parameter);
}

bool supportsDragNumberWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isFloatParameter(parameter) ||
           isIntParameter(parameter) ||
           isFloatVectorParameter(parameter) ||
           isIntVectorParameter(parameter);
}

bool supportsToggleWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isBoolParameter(parameter);
}

bool supportsButtonWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isTriggerParameter(parameter);
}

bool supportsColorSwatchWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isColorParameter(parameter) || isFloatColorParameter(parameter);
}

bool supportsTextDisplayWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isStringParameter(parameter);
}

bool supportsFileBrowserWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isStringParameter(parameter);
}

bool supportsCustomRegionWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isRegisteredCustomRegionParameter(parameter) && parameter.getName().find("SEPARATOR:|") != 0;
}

bool supportsDropdownWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isIntParameter(parameter) && !parameter.cast<int>().getDropdownOptions().empty();
}

bool supportsCustomDropdownWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isIntParameter(parameter);
}

bool supportsButtonMatrixWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isIntParameter(parameter);
}

enum class SliderVisualMode {
    Anchored,
    CenteredBar,
    Bipolar
};

std::vector<std::string> getCustomDropdownOptions(const CustomGuiWidget& widget)
{
    std::vector<std::string> options;
    if(!widget.config.contains("customOptions") || !widget.config["customOptions"].is_array()) return options;

    for(const auto& item : widget.config["customOptions"]){
        options.push_back(item.is_string() ? item.get<std::string>() : std::string());
    }
    return options;
}

ofJson customDropdownOptionsToJson(const std::vector<std::string>& options)
{
    ofJson json = ofJson::array();
    for(const auto& option : options) json.push_back(option);
    return json;
}

void drawCustomDropdownProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    std::vector<std::string> options = getCustomDropdownOptions(widget);
    int baseValue = 0;

    if(parameter != nullptr && isIntParameter(*parameter)){
        auto& intParam = parameter->cast<int>().getParameter();
        baseValue = intParam.getMin();
        ImGui::TextDisabled("Mapped from parameter min: %d", baseValue);
    }

    for(size_t i = 0; i < options.size(); i++){
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%s", options[i].c_str());
        ImGui::PushID((int)i);
        const std::string label = "##customOption";
        if(ImGui::InputText(label.c_str(), buffer, sizeof(buffer))){
            options[i] = buffer;
            widget.config["customOptions"] = customDropdownOptionsToJson(options);
            context.container.markCustomGuisDirty();
        }
        ImGui::SameLine();
        ImGui::Text("Value %d", baseValue + (int)i);
        ImGui::SameLine();
        if(ImGui::SmallButton("-")){
            options.erase(options.begin() + i);
            widget.config["customOptions"] = customDropdownOptionsToJson(options);
            context.container.markCustomGuisDirty();
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    if(ImGui::Button("Add Option")){
        options.push_back("");
        widget.config["customOptions"] = customDropdownOptionsToJson(options);
        context.container.markCustomGuisDirty();
    }
}

void initializeWideWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.spanW = 2;
    widget.spanH = 2;
    ensureWidgetBodyColor(widget);
    ensureWidgetLabelColor(widget);
    widget.config["labelFontScale"] = widget.config.value("labelFontScale", 1.0f);
    widget.config["valueFontScale"] = widget.config.value("valueFontScale", 1.0f);
}

void initializeFontScaledWideWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter& parameter)
{
    initializeWideWidget(widget, parameter);
    widget.config["valueFontScale"] = widget.config.value("valueFontScale", 1.0f);
}

void initializeCustomDropdownWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter& parameter)
{
    initializeWideWidget(widget, parameter);

    std::vector<std::string> options = parameter.cast<int>().getDropdownOptions();
    if(options.empty()){
        auto& intParam = parameter.cast<int>().getParameter();
        const int minValue = intParam.getMin();
        const int maxValue = intParam.getMax();
        const int range = maxValue - minValue;
        if(range >= 0 && range <= 32){
            options.reserve(range + 1);
            for(int value = minValue; value <= maxValue; value++){
                options.push_back(ofToString(value));
            }
        }
    }
    widget.config["customOptions"] = customDropdownOptionsToJson(options);
}

void initializeButtonMatrixWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.spanW = 4;
    widget.spanH = 2;
    widget.config["rows"] = 2;
    widget.config["cols"] = 4;
    ensureWidgetBodyColor(widget, ofColor(70, 70, 70, 220));
    ensureWidgetLabelColor(widget);
}

void initializeButtonWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.spanW = 2;
    widget.spanH = 2;
    ensureWidgetBodyColor(widget);
    ensureWidgetLabelColor(widget);
    widget.config["buttonHoverColor"] = widget.config.value("buttonHoverColor", customGuiColorToJson(widget.color));
    widget.config["buttonPressedColor"] = widget.config.value("buttonPressedColor", customGuiColorToJson(widget.color));
}

void initializeColorSwatchWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.spanW = 2;
    widget.spanH = 2;
    ensureWidgetBodyColor(widget, ofColor(60, 60, 60, 255));
    ensureWidgetLabelColor(widget);
    widget.config["labelFontScale"] = widget.config.value("labelFontScale", 1.0f);
    widget.config["valueFontScale"] = widget.config.value("valueFontScale", 1.0f);
    widget.config["showValue"] = false;
}

bool sliderUsesCenteredBar(const CustomGuiWidget& widget)
{
    return widget.config.value("centeredBar", true);
}

SliderVisualMode sliderVisualMode(const CustomGuiWidget& widget)
{
    if(widget.config.contains("sliderMode") && widget.config["sliderMode"].is_string()){
        const std::string sliderMode = widget.config["sliderMode"].get<std::string>();
        if(sliderMode == "anchored") return SliderVisualMode::Anchored;
        if(sliderMode == "centeredBar") return SliderVisualMode::CenteredBar;
        if(sliderMode == "bipolar") return SliderVisualMode::Bipolar;
    }

    return sliderUsesCenteredBar(widget) ? SliderVisualMode::Bipolar : SliderVisualMode::Anchored;
}

ofColor buttonHoverColor(const CustomGuiWidget& widget, const ofColor& fallback)
{
    if(widget.config.contains("buttonHoverColor") && widget.config["buttonHoverColor"].is_array()){
        return customGuiColorFromJson(widget.config["buttonHoverColor"], fallback);
    }
    return fallback;
}

ofColor buttonPressedColor(const CustomGuiWidget& widget, const ofColor& fallback)
{
    if(widget.config.contains("buttonPressedColor") && widget.config["buttonPressedColor"].is_array()){
        return customGuiColorFromJson(widget.config["buttonPressedColor"], fallback);
    }
    return fallback;
}

void drawScaledCenteredValueText(const ImVec2& min,
                                 const ImVec2& max,
                                 const std::string& text,
                                 float fontSize,
                                 const ImU32 color)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
    const float textX = min.x + ((max.x - min.x) - textSize.x) * 0.5f;
    const float textY = min.y + ((max.y - min.y) - textSize.y) * 0.5f;
    drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(textX, textY), color, text.c_str());
}

bool drawScalarSliderBar(const CustomGuiWidgetRenderContext& context,
                         const CustomGuiWidget& widget,
                         const ImVec2& itemSize,
                         bool interactive,
                         bool showValue,
                         bool verticalSlider,
                         float sliderMin,
                         float sliderMax,
                         float& value,
                         const std::string& valueText)
{
    if(sliderMax <= sliderMin) sliderMax = sliderMin + 1.0f;

    ImGui::InvisibleButton("##value", itemSize);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = interactive && ImGui::IsItemActive();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ofColor bodyColor = widgetBodyColor(widget);
    const ofColor accentColor = widget.color;
    const SliderVisualMode sliderMode = sliderVisualMode(widget);
    const float normalized = ofClamp((value - sliderMin) / (sliderMax - sliderMin), 0.0f, 1.0f);
    const float fontSize = std::max(1.0f, ImGui::GetFontSize() * widgetValueFontScale(widget));
    const float slimThickness = std::max(2.0f, std::round((verticalSlider ? itemSize.y : itemSize.x) * 0.04f));

    drawList->AddRectFilled(min, max, IM_COL32(bodyColor.r, bodyColor.g, bodyColor.b, bodyColor.a), 3.0f);

    if(verticalSlider){
        const float valueY = max.y - itemSize.y * normalized;
        if(sliderMode == SliderVisualMode::Anchored){
            drawList->AddRectFilled(ImVec2(min.x, valueY),
                                    ImVec2(max.x, max.y),
                                    IM_COL32(accentColor.r, accentColor.g, accentColor.b, accentColor.a),
                                    3.0f);
        }else if(sliderMode == SliderVisualMode::CenteredBar){
            const float lineTop = ofClamp(valueY - slimThickness * 0.5f, min.y, max.y);
            const float lineBottom = ofClamp(valueY + slimThickness * 0.5f, min.y, max.y);
            drawList->AddRectFilled(ImVec2(min.x, lineTop),
                                    ImVec2(max.x, lineBottom),
                                    IM_COL32(accentColor.r, accentColor.g, accentColor.b, accentColor.a),
                                    2.0f);
        }else{
            const float centerY = min.y + itemSize.y * 0.5f;
            const float fillTop = std::min(centerY, valueY);
            const float fillBottom = std::max(centerY, valueY);
            drawList->AddRectFilled(ImVec2(min.x, fillTop),
                                    ImVec2(max.x, fillBottom),
                                    IM_COL32(accentColor.r, accentColor.g, accentColor.b, accentColor.a),
                                    3.0f);
            drawList->AddLine(ImVec2(min.x, centerY), ImVec2(max.x, centerY), IM_COL32(255, 255, 255, 60), 1.0f);
        }
    }else{
        const float valueX = min.x + itemSize.x * normalized;
        if(sliderMode == SliderVisualMode::Anchored){
            drawList->AddRectFilled(ImVec2(min.x, min.y),
                                    ImVec2(valueX, max.y),
                                    IM_COL32(accentColor.r, accentColor.g, accentColor.b, accentColor.a),
                                    3.0f);
        }else if(sliderMode == SliderVisualMode::CenteredBar){
            const float lineLeft = ofClamp(valueX - slimThickness * 0.5f, min.x, max.x);
            const float lineRight = ofClamp(valueX + slimThickness * 0.5f, min.x, max.x);
            drawList->AddRectFilled(ImVec2(lineLeft, min.y),
                                    ImVec2(lineRight, max.y),
                                    IM_COL32(accentColor.r, accentColor.g, accentColor.b, accentColor.a),
                                    2.0f);
        }else{
            const float centerX = min.x + itemSize.x * 0.5f;
            const float fillLeft = std::min(centerX, valueX);
            const float fillRight = std::max(centerX, valueX);
            drawList->AddRectFilled(ImVec2(fillLeft, min.y),
                                    ImVec2(fillRight, max.y),
                                    IM_COL32(accentColor.r, accentColor.g, accentColor.b, accentColor.a),
                                    3.0f);
            drawList->AddLine(ImVec2(centerX, min.y), ImVec2(centerX, max.y), IM_COL32(255, 255, 255, 60), 1.0f);
        }
    }

    if(hovered || active){
        drawList->AddRect(min, max, IM_COL32(255, 255, 255, active ? 220 : 120), 3.0f, 0, active ? 2.0f : 1.0f);
    }

    bool changed = false;
    if(interactive && (active || (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))){
        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        const float mouseNormalized = verticalSlider
            ? 1.0f - ofClamp((mousePos.y - min.y) / std::max(1.0f, itemSize.y), 0.0f, 1.0f)
            : ofClamp((mousePos.x - min.x) / std::max(1.0f, itemSize.x), 0.0f, 1.0f);
        const float newValue = sliderMin + mouseNormalized * (sliderMax - sliderMin);
        if(newValue != value){
            value = newValue;
            changed = true;
        }
    }

    if(showValue){
        drawScaledCenteredValueText(min, max, valueText, fontSize, IM_COL32(245, 245, 245, 220));
    }

    return changed;
}

bool renderKnobWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    const bool interactive = context.interactive;
    const bool showValue = context.showValue;
    const ImVec2 itemSize = widgetItemSize(context);
    const float valueFontSize = std::max(1.0f, ImGui::GetFontSize() * widgetValueFontScale(widget));
    const float valueAreaHeight = showValue ? (valueFontSize + ImGui::GetStyle().ItemSpacing.y) : 0.0f;
    const float knobAreaHeight = std::max(1.0f, itemSize.y - valueAreaHeight);
    const float size = std::max(24.0f, std::min(itemSize.x, knobAreaHeight));
    const ofColor bodyColor = widgetBodyColor(widget);
    float value = 0.0f;
    float sliderMin = 0.0f;
    float sliderMax = 1.0f;
    bool changed = false;
    bool floatVectorParameter = false;
    std::vector<float> floatVectorValue;

    if(isFloatParameter(*parameter)){
        auto& param = parameter->cast<float>().getParameter();
        value = param.get();
        sliderMin = floatRangeMin(widget, param.getMin());
        sliderMax = floatRangeMax(widget, param.getMax());
    }else if(isFloatVectorParameter(*parameter)){
        auto& param = parameter->cast<std::vector<float>>().getParameter();
        floatVectorValue = param.get();
        if(floatVectorValue.empty()) return false;

        value = floatVectorValue[0];
        float minValue = !param.getMin().empty() ? param.getMin()[0] : 0.0f;
        float maxValue = !param.getMax().empty() ? param.getMax()[0] : 1.0f;
        sliderMin = floatRangeMin(widget, minValue);
        sliderMax = floatRangeMax(widget, maxValue);
        floatVectorParameter = true;
    }else if(isIntParameter(*parameter)){
        auto& param = parameter->cast<int>().getParameter();
        value = (float)param.get();
        sliderMin = (float)intRangeMin(widget, param.getMin());
        sliderMax = (float)intRangeMax(widget, param.getMax());
    }else{
        return false;
    }

    ImGui::BeginGroup();
    drawWidgetLabel(context, widget, context.label);
    ImGui::InvisibleButton("##knob", ImVec2(size, size));
    const ImVec2 knobMin = ImGui::GetItemRectMin();
    const ImVec2 knobMax = ImGui::GetItemRectMax();
    const ImVec2 center((knobMin.x + knobMax.x) * 0.5f, (knobMin.y + knobMax.y) * 0.5f);
    const float radius = size * 0.42f;
    const float normalized = sliderMax > sliderMin ? ofClamp((value - sliderMin) / (sliderMax - sliderMin), 0.0f, 1.0f) : 0.0f;
    constexpr float kPi = 3.14159265358979323846f;
    const float startAngle = kPi * 0.75f;
    const float endAngle = kPi * 2.25f;
    const float angle = startAngle + (endAngle - startAngle) * normalized;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 body = IM_COL32(bodyColor.r, bodyColor.g, bodyColor.b, bodyColor.a);
    drawList->AddCircleFilled(center, radius, body, 32);
    drawList->AddCircle(center, radius, IM_COL32(210, 210, 210, 180), 32, 1.5f);
    drawList->PathArcTo(center, radius - 3.0f, startAngle, endAngle, 32);
    drawList->PathStroke(IM_COL32(70, 70, 70, 220), false, 3.0f);
    drawList->PathArcTo(center, radius - 3.0f, startAngle, angle, 32);
    drawList->PathStroke(IM_COL32(widget.color.r, widget.color.g, widget.color.b, 255), false, 3.5f);
    drawList->AddLine(center,
                      ImVec2(center.x + std::cos(angle) * (radius - 8.0f), center.y + std::sin(angle) * (radius - 8.0f)),
                      IM_COL32(255, 255, 255, 230), 2.0f);

    if(interactive && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
        value -= ImGui::GetIO().MouseDelta.y * (sliderMax - sliderMin) * 0.005f;
        value = ofClamp(value, sliderMin, sliderMax);
        value = quantizeFloatValue(widget, value, sliderMin, sliderMax);
        changed = true;
    }

    if(showValue){
        const std::string text = isIntParameter(*parameter) ? ofToString((int)std::round(value)) : ofToString(value, 3);
        ImGui::Dummy(ImVec2(size, valueAreaHeight));
        const ImVec2 valueMin = ImGui::GetItemRectMin();
        const ImVec2 valueMax = ImGui::GetItemRectMax();
        drawScaledCenteredValueText(valueMin, valueMax, text, valueFontSize, IM_COL32(245, 245, 245, 220));
    }

    if(changed){
        if(isFloatParameter(*parameter)){
            value = quantizeFloatValue(widget, value, sliderMin, sliderMax);
            parameter->cast<float>().getParameter().set(value);
        }else if(floatVectorParameter){
            value = quantizeFloatValue(widget, value, sliderMin, sliderMax);
            floatVectorValue[0] = value;
            parameter->cast<std::vector<float>>().getParameter().set(floatVectorValue);
        }else{
            parameter->cast<int>().getParameter().set((int)std::round(ofClamp(value, sliderMin, sliderMax)));
        }
    }
    ImGui::EndGroup();
    return true;
}

bool renderFloatWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    auto& param = parameter->cast<float>().getParameter();
    float value = param.get();
    const ImVec2 itemSize = widgetItemSize(context);
    const bool interactive = context.interactive;
    const bool showValue = context.showValue;
    const bool verticalSlider = itemSize.y > itemSize.x;
    const float sliderMin = floatRangeMin(widget, param.getMin());
    const float sliderMax = floatRangeMax(widget, param.getMax());
    bool changed = false;
    const float valueScale = widgetValueFontScale(widget);
    const ofColor bodyColor = widgetBodyColor(widget);
    const ofColor accentColor = widget.color;

    ImGui::BeginGroup();
    drawWidgetLabel(context, widget, context.label);
    if(widget.type == CustomGuiWidgetType::Slider){
        changed = drawScalarSliderBar(context, widget, itemSize, interactive, showValue, verticalSlider,
                                      sliderMin, sliderMax, value, ofToString(value, 3));
    }else if(widget.type == CustomGuiWidgetType::MultiSlider){
        std::vector<float> sliderValue = {value};
        changed = context.drawMultiSliderWidget(widget, parameter, sliderValue, itemSize, interactive);
        if(changed && !sliderValue.empty()) value = sliderValue.front();
    }else{
        ImGui::SetWindowFontScale(std::max(0.2f, context.zoom * valueScale));
        pushAlignedFrameStyle(context, itemSize);
        ImGui::SetNextItemWidth(widgetItemWidth(itemSize));
        pushWidgetFrameColors(bodyColor, accentColor);

        if(!interactive){
            const float fraction = sliderMax != sliderMin ? (value - sliderMin) / (sliderMax - sliderMin) : 0.0f;
            ImGui::ProgressBar(ofClamp(fraction, 0.0f, 1.0f), itemSize, showValue ? ofToString(value, 3).c_str() : "");
        }else if(widget.type == CustomGuiWidgetType::MultiToggle){
            const bool active = value > 0.5f;
            if(active) pushToggleOnColors(widget.color);
            if(ImGui::Button("##value", itemSize)){
                value = active ? 0.0f : 1.0f;
                changed = true;
            }
            if(active) ImGui::PopStyleColor(3);
        }else if(widget.type == CustomGuiWidgetType::DragNumber){
            changed = ImGui::DragFloat("##value", &value, 0.01f, sliderMin, sliderMax);
        }else if(verticalSlider){
            changed = ImGui::VSliderFloat("##value", itemSize, &value, sliderMin, sliderMax, showValue ? "%.3f" : "");
        }else{
            changed = ImGui::SliderFloat("##value", &value, sliderMin, sliderMax, showValue ? "%.3f" : "");
        }

        ImGui::PopStyleColor(8);
        popAlignedFrameStyle();
        ImGui::SetWindowFontScale(std::max(0.5f, context.zoom));
    }

    if(changed){
        value = quantizeFloatValue(widget, value, sliderMin, sliderMax);
        param.set(value);
    }
    ImGui::EndGroup();
    return true;
}

bool renderIntWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    auto& param = parameter->cast<int>().getParameter();
    int value = param.get();
    const ImVec2 itemSize = widgetItemSize(context);
    const bool interactive = context.interactive;
    const bool showValue = context.showValue;
    const bool verticalSlider = itemSize.y > itemSize.x;
    const int sliderMin = intRangeMin(widget, param.getMin());
    const int sliderMax = intRangeMax(widget, param.getMax());
    const auto options = parameter->cast<int>().getDropdownOptions();
    const auto customOptions = getCustomDropdownOptions(widget);
    bool changed = false;
    const float valueScale = widgetValueFontScale(widget);
    const ofColor bodyColor = widgetBodyColor(widget);
    const ofColor accentColor = widget.color;

    ImGui::BeginGroup();
    drawWidgetLabel(context, widget, context.label);
    if(widget.type == CustomGuiWidgetType::Slider){
        float scalarValue = (float)value;
        changed = drawScalarSliderBar(context, widget, itemSize, interactive, showValue, verticalSlider,
                                      (float)sliderMin, (float)sliderMax, scalarValue, ofToString(value));
        if(changed) value = (int)std::round(ofClamp(scalarValue, (float)sliderMin, (float)sliderMax));
    }else if(widget.type == CustomGuiWidgetType::MultiSlider){
        std::vector<float> sliderValue = {(float)value};
        changed = context.drawMultiSliderWidget(widget, parameter, sliderValue, itemSize, interactive);
        if(changed && !sliderValue.empty()){
            value = (int)std::round(ofClamp(sliderValue.front(), (float)sliderMin, (float)sliderMax));
        }
    }else{
        ImGui::SetWindowFontScale(std::max(0.2f, context.zoom * valueScale));
        pushAlignedFrameStyle(context, itemSize);
        ImGui::SetNextItemWidth(widgetItemWidth(itemSize));
        pushWidgetFrameColors(bodyColor, accentColor);

        if(!interactive){
            if(widget.type == CustomGuiWidgetType::Dropdown && !options.empty()){
                const int index = ofClamp(value, 0, (int)options.size() - 1);
                ImGui::TextWrapped("%s", options[index].c_str());
            }else if(widget.type == CustomGuiWidgetType::CustomDropdown && !customOptions.empty()){
                const int optionIndex = value - sliderMin;
                if(optionIndex >= 0 && optionIndex < (int)customOptions.size()){
                    ImGui::TextWrapped("%s", customOptions[optionIndex].c_str());
                }else{
                    ImGui::TextWrapped("%d", value);
                }
            }else{
                ImGui::TextWrapped("%d", value);
            }
        }else if(widget.type == CustomGuiWidgetType::ButtonMatrix){
            const int rows = std::max(1, widget.config.value("rows", 2));
            const int cols = std::max(1, widget.config.value("cols", 4));
            const float spacingX = ImGui::GetStyle().ItemSpacing.x;
            const float spacingY = ImGui::GetStyle().ItemSpacing.y;
            const float btnW = std::max(18.0f, (itemSize.x - spacingX * (cols - 1)) / cols);
            const float btnH = std::max(18.0f, (itemSize.y - spacingY * (rows - 1)) / rows);
            const ofColor inactiveColor = widgetBodyColor(widget, ofColor(70, 70, 70, 220));
            const ofColor activeColor = widget.color;
            const int baseValue = sliderMin;

            for(int row = 0; row < rows; row++){
                for(int col = 0; col < cols; col++){
                    if(col > 0) ImGui::SameLine();
                    const int slot = row * cols + col;
                    const int buttonValue = baseValue + slot;
                    const bool active = value == buttonValue;
                    const ofColor buttonColor = active ? activeColor : inactiveColor;
                    ImGui::PushID(slot);
                    ImGui::PushStyleColor(ImGuiCol_Button, colorToImVec4(buttonColor));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colorToImVec4(buttonColor));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colorToImVec4(buttonColor));
                    if(ImGui::Button(ofToString(buttonValue).c_str(), ImVec2(btnW, btnH))){
                        value = buttonValue;
                        changed = true;
                    }
                    ImGui::PopStyleColor(3);
                    ImGui::PopID();
                }
            }
        }else if(widget.type == CustomGuiWidgetType::MultiToggle){
            const bool active = value > 0;
            if(active) pushToggleOnColors(widget.color);
            if(ImGui::Button("##value", itemSize)){
                value = active ? 0 : 1;
                changed = true;
            }
            if(active) ImGui::PopStyleColor(3);
        }else if(widget.type == CustomGuiWidgetType::Dropdown && !options.empty()){
            const char* preview = options[ofClamp(value, 0, (int)options.size() - 1)].c_str();
            if(ImGui::BeginCombo("##value", preview)){
                for(int i = 0; i < (int)options.size(); i++){
                    if(ImGui::Selectable(options[i].c_str(), value == i)){
                        value = i;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        }else if(widget.type == CustomGuiWidgetType::CustomDropdown && !customOptions.empty()){
            int optionIndex = value - sliderMin;
            optionIndex = ofClamp(optionIndex, 0, (int)customOptions.size() - 1);
            const char* preview = customOptions[optionIndex].c_str();
            if(ImGui::BeginCombo("##value", preview)){
                for(int i = 0; i < (int)customOptions.size(); i++){
                    if(ImGui::Selectable(customOptions[i].c_str(), optionIndex == i)){
                        value = sliderMin + i;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        }else if(widget.type == CustomGuiWidgetType::DragNumber){
            changed = ImGui::DragInt("##value", &value, 1.0f, sliderMin, sliderMax);
        }else if(verticalSlider){
            changed = ImGui::VSliderInt("##value", itemSize, &value, sliderMin, sliderMax, showValue ? "%d" : "");
        }else{
            changed = ImGui::SliderInt("##value", &value, sliderMin, sliderMax, showValue ? "%d" : "");
        }

        ImGui::PopStyleColor(8);
        popAlignedFrameStyle();
        ImGui::SetWindowFontScale(std::max(0.5f, context.zoom));
    }

    if(changed) param.set(value);
    ImGui::EndGroup();
    return true;
}

void drawButtonMatrixProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    int rows = std::max(1, widget.config.value("rows", 2));
    int cols = std::max(1, widget.config.value("cols", 4));
    if(ImGui::InputInt("Rows", &rows)){
        widget.config["rows"] = std::max(1, rows);
        context.container.markCustomGuisDirty();
    }
    if(ImGui::InputInt("Cols", &cols)){
        widget.config["cols"] = std::max(1, cols);
        context.container.markCustomGuisDirty();
    }
}

bool renderVectorDragNumberWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    const ImVec2 itemSize = widgetItemSize(context);
    const bool interactive = context.interactive;
    const float fontScale = widgetValueFontScale(widget);
    const ofColor bodyColor = widgetBodyColor(widget);
    const ofColor accentColor = widget.color;

    ImGui::BeginGroup();
    drawWidgetLabel(context, widget, context.label);
    ImGui::SetWindowFontScale(std::max(0.2f, context.zoom * fontScale));
    pushAlignedFrameStyle(context, itemSize);
    ImGui::SetNextItemWidth(widgetItemWidth(itemSize));
    pushWidgetFrameColors(bodyColor, accentColor);

    if(isFloatVectorParameter(*parameter)){
        auto& param = parameter->cast<std::vector<float>>().getParameter();
        auto value = param.get();
        if(value.empty()){
            ImGui::TextDisabled("Empty vector");
            popAlignedFrameStyle();
            ImGui::EndGroup();
            return true;
        }

        float sliderMin = !param.getMin().empty() ? param.getMin()[0] : 0.0f;
        float sliderMax = !param.getMax().empty() ? param.getMax()[0] : 1.0f;
        sliderMin = floatRangeMin(widget, sliderMin);
        sliderMax = floatRangeMax(widget, sliderMax);

        float scalarValue = value[0];
        bool changed = false;
        if(!interactive){
            ImGui::TextWrapped("%.3f", scalarValue);
        }else{
            changed = ImGui::DragFloat("##value", &scalarValue, 0.01f, sliderMin, sliderMax);
        }

        if(changed){
            value[0] = quantizeFloatValue(widget, scalarValue, sliderMin, sliderMax);
            param.set(value);
        }
    }else if(isIntVectorParameter(*parameter)){
        auto& param = parameter->cast<std::vector<int>>().getParameter();
        auto value = param.get();
        if(value.empty()){
            ImGui::TextDisabled("Empty vector");
            popAlignedFrameStyle();
            ImGui::EndGroup();
            return true;
        }

        int sliderMin = !param.getMin().empty() ? param.getMin()[0] : 0;
        int sliderMax = !param.getMax().empty() ? param.getMax()[0] : 1;
        sliderMin = intRangeMin(widget, sliderMin);
        sliderMax = intRangeMax(widget, sliderMax);

        int scalarValue = value[0];
        bool changed = false;
        if(!interactive){
            ImGui::TextWrapped("%d", scalarValue);
        }else{
            changed = ImGui::DragInt("##value", &scalarValue, 1.0f, sliderMin, sliderMax);
        }

        if(changed){
            value[0] = scalarValue;
            param.set(value);
        }
    }

    ImGui::PopStyleColor(8);
    popAlignedFrameStyle();
    ImGui::SetWindowFontScale(std::max(0.5f, context.zoom));
    ImGui::EndGroup();
    return true;
}

void drawFontScaleProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    (void)context;
    (void)widget;
}

bool renderToggleWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    auto& param = parameter->cast<bool>().getParameter();
    bool value = param.get();

    ImGui::BeginGroup();
    drawWidgetLabel(context, widget, context.label);
    const ImVec2 itemSize = widgetItemSize(context);
    const ofColor bodyColor = widgetBodyColor(widget, ofColor(70, 70, 70, 220));
    const ofColor activeColor = widget.color;
    const ofColor buttonColor = value ? activeColor : bodyColor;
    ImGui::SetWindowFontScale(std::max(0.2f, context.zoom * widgetValueFontScale(widget)));
    ImGui::PushStyleColor(ImGuiCol_Button, colorToImVec4(buttonColor));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colorToImVec4(buttonColor, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colorToImVec4(buttonColor, 1.0f));
    if(ImGui::Button(value ? "On" : "Off", itemSize) && context.interactive){
        param.set(!value);
    }
    ImGui::PopStyleColor(3);
    ImGui::SetWindowFontScale(std::max(0.5f, context.zoom));
    ImGui::EndGroup();
    return true;
}

bool renderButtonWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    ImGui::BeginGroup();
    const ofColor bodyColor = widgetBodyColor(widget, ofColor(90, 90, 90, 220));
    const ofColor hoverColor = buttonHoverColor(widget, widget.color);
    const ofColor pressedColor = buttonPressedColor(widget, hoverColor);
    const ofColor labelColor = widgetLabelColor(widget, ofColor::white);
    ImGui::SetWindowFontScale(std::max(0.2f, context.zoom * widgetValueFontScale(widget)));
    ImGui::PushStyleColor(ImGuiCol_Button, colorToImVec4(bodyColor));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colorToImVec4(hoverColor));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colorToImVec4(pressedColor));
    ImGui::PushStyleColor(ImGuiCol_Text, colorToImVec4(labelColor));
    if(ImGui::Button(context.label.c_str(), widgetItemSize(context)) && context.interactive){
        parameter->cast<void>().getParameter().trigger();
    }
    ImGui::PopStyleColor(4);
    ImGui::SetWindowFontScale(std::max(0.5f, context.zoom));
    ImGui::EndGroup();
    return true;
}

bool renderColorSwatchWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    const bool floatColorParameter = isFloatColorParameter(*parameter);
    const bool byteColorParameter = isColorParameter(*parameter);
    if(!floatColorParameter && !byteColorParameter) return false;

    float color[4];
    if(floatColorParameter){
        auto value = parameter->cast<ofFloatColor>().getParameter().get();
        color[0] = value.r;
        color[1] = value.g;
        color[2] = value.b;
        color[3] = value.a;
    }else{
        auto value = parameter->cast<ofColor>().getParameter().get();
        color[0] = value.r / 255.0f;
        color[1] = value.g / 255.0f;
        color[2] = value.b / 255.0f;
        color[3] = value.a / 255.0f;
    }

    ImGui::BeginGroup();
    drawWidgetLabel(context, widget, context.label);
    const ImVec2 itemSize = widgetItemSize(context);
    ImGui::InvisibleButton("##colorswatch", itemSize);
    const bool hovered = ImGui::IsItemHovered();
    if(context.interactive && ImGui::IsItemClicked(ImGuiMouseButton_Left)){
        ImGui::OpenPopup("Color Swatch Picker");
    }

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ofColor bodyColor = widgetBodyColor(widget, ofColor(60, 60, 60, 255));

    const float checker = std::max(6.0f, std::min(14.0f, std::min(itemSize.x, itemSize.y) * 0.15f));
    drawList->AddRectFilled(min, max, IM_COL32(bodyColor.r, bodyColor.g, bodyColor.b, bodyColor.a), 4.0f);
    for(float y = min.y; y < max.y; y += checker){
        for(float x = min.x; x < max.x; x += checker){
            const bool dark = (((int)((x - min.x) / checker) + (int)((y - min.y) / checker)) % 2) == 0;
            const ImU32 cellColor = dark ? IM_COL32(90, 90, 90, 255) : IM_COL32(130, 130, 130, 255);
            drawList->AddRectFilled(ImVec2(x, y),
                                    ImVec2(std::min(x + checker, max.x), std::min(y + checker, max.y)),
                                    cellColor);
        }
    }
    drawList->AddRectFilled(min,
                            max,
                            IM_COL32((int)std::round(color[0] * 255.0f),
                                     (int)std::round(color[1] * 255.0f),
                                     (int)std::round(color[2] * 255.0f),
                                     (int)std::round(color[3] * 255.0f)),
                            4.0f);
    drawList->AddRect(min,
                      max,
                      hovered ? IM_COL32(255, 255, 255, 220) : IM_COL32(100, 100, 100, 255),
                      4.0f, 0, hovered ? 2.0f : 1.0f);

    bool changed = false;
    if(ImGui::BeginPopup("Color Swatch Picker")){
        ImGuiColorEditFlags flags = ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_AlphaBar;
        if(floatColorParameter) flags |= ImGuiColorEditFlags_Float;
        changed = ImGui::ColorPicker4("##picker", color, flags);
        ImGui::EndPopup();
    }

    if(changed){
        if(floatColorParameter){
            parameter->cast<ofFloatColor>().getParameter().set(ofFloatColor(color[0], color[1], color[2], color[3]));
        }else{
            parameter->cast<ofColor>().getParameter().set(ofColor((int)std::round(ofClamp(color[0], 0.0f, 1.0f) * 255.0f),
                                                                  (int)std::round(ofClamp(color[1], 0.0f, 1.0f) * 255.0f),
                                                                  (int)std::round(ofClamp(color[2], 0.0f, 1.0f) * 255.0f),
                                                                  (int)std::round(ofClamp(color[3], 0.0f, 1.0f) * 255.0f)));
        }
    }

    ImGui::EndGroup();
    return true;
}

bool renderTextDisplayWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    ImGui::BeginGroup();
    drawWidgetLabel(context, widget, context.label);
    const float fontScale = widgetValueFontScale(widget);
    ImGui::SetWindowFontScale(std::max(0.2f, context.zoom * fontScale));
    ImGui::TextWrapped("%s", parameter->cast<std::string>().getParameter().get().c_str());
    ImGui::SetWindowFontScale(std::max(0.5f, context.zoom));
    ImGui::EndGroup();
    return true;
}

void initializeFileBrowserWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.spanW = 3;
    widget.spanH = 2;
    widget.config["selectFolders"] = false;
}

void drawFileBrowserProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    bool selectFolders = widget.config.value("selectFolders", false);
    if(ImGui::Checkbox("Select Folder", &selectFolders)){
        widget.config["selectFolders"] = selectFolders;
        context.container.markCustomGuisDirty();
    }
}

bool renderFileBrowserWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    auto& param = parameter->cast<std::string>().getParameter();
    const bool selectFolders = widget.config.value("selectFolders", false);
    const bool interactive = context.interactive && !context.designMode;
    const ImVec2 itemSize = widgetItemSize(context);
    const float fontScale = widgetValueFontScale(widget);
    const std::string currentValue = param.get();

    ImGui::BeginGroup();
    drawWidgetLabel(context, widget, context.label);
    ImGui::SetWindowFontScale(std::max(0.2f, context.zoom * fontScale));
    const float buttonHeight = ImGui::GetFrameHeight();
    const float buttonWidth = std::min(96.0f, std::max(70.0f, itemSize.x * 0.32f));

    if(interactive){
        if(ImGui::Button(selectFolders ? "Folder..." : "Browse...", ImVec2(buttonWidth, buttonHeight))){
            std::string defaultPath;
            if(!currentValue.empty()){
                ofFile currentFile(currentValue);
                if(currentFile.exists()){
                    defaultPath = currentFile.isDirectory() ? currentFile.getAbsolutePath() : currentFile.getEnclosingDirectory();
                }else{
                    defaultPath = ofFilePath::getEnclosingDirectory(currentValue, false);
                }
            }

            ofFileDialogResult result = ofSystemLoadDialog(selectFolders ? "Select folder" : "Select file", selectFolders, defaultPath);
            if(result.bSuccess){
                param.set(result.getPath());
            }
        }

        if(!currentValue.empty()){
            ImGui::SameLine();
            if(ImGui::Button("Clear")){
                param.set("");
            }
        }
    }

    const float pathHeight = std::max(1.0f, itemSize.y - (interactive ? buttonHeight + ImGui::GetStyle().ItemSpacing.y : 0.0f));
    ImGui::BeginChild("##filebrowserpath", ImVec2(itemSize.x, pathHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const std::string valueToShow = param.get();
    ImGui::TextWrapped("%s", valueToShow.empty() ? (selectFolders ? "No folder selected" : "No file selected") : valueToShow.c_str());
    ImGui::EndChild();
    ImGui::SetWindowFontScale(std::max(0.5f, context.zoom));
    ImGui::EndGroup();
    return true;
}

void initializeCustomRegionWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.spanW = 3;
    widget.spanH = 2;
    widget.config["showValue"] = false;
}

bool renderCustomRegionWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    ImGuiWindowFlags childFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if(context.designMode){
        childFlags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::BeginGroup();
    drawWidgetLabel(context, widget, context.label);
    const ImVec2 regionSize = widgetItemSize(context);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("##customregion", regionSize, false, childFlags);
    const ImVec2 contentMin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 contentMax(contentMin.x + std::max(1.0f, available.x),
                            contentMin.y + std::max(1.0f, available.y));
    const float overflowPad = std::max(4.0f, std::min(8.0f, std::min(regionSize.x, regionSize.y) * 0.05f));
    ImGui::PushClipRect(ImVec2(contentMin.x - overflowPad, contentMin.y - overflowPad),
                        ImVec2(contentMax.x + overflowPad, contentMax.y + overflowPad),
                        true);
    ofxOceanodeShared::pushCustomRegionRenderContext(std::max(1.0f, available.x),
                                                     std::max(1.0f, available.y),
                                                     contentMin,
                                                     contentMax);
    parameter->cast<std::function<void()>>().getParameter().get()();
    ofxOceanodeShared::popCustomRegionRenderContext();
    ImGui::PopClipRect();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    return true;
}

} // namespace

namespace ofxOceanodeCustomGuiValueWidgets {

void registerWidgets(ofxOceanodeCustomGuiWidgetRegistry& registry)
{
    registerWidget(registry, CustomGuiWidgetType::Slider,
                   supportsSliderWidget,
                   initializeWideWidget,
                   [](CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter){
                       if(parameter == nullptr) return false;
                       return isFloatParameter(*parameter) ? renderFloatWidget(context, widget, parameter) : renderIntWidget(context, widget, parameter);
                   });

    registerWidget(registry, CustomGuiWidgetType::Knob,
                   supportsKnobWidget,
                   initializeWideWidget,
                   renderKnobWidget);

    registerWidget(registry, CustomGuiWidgetType::DragNumber,
                   supportsDragNumberWidget,
                   initializeFontScaledWideWidget,
                   [](CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter){
                       if(parameter == nullptr) return false;
                       if(isFloatParameter(*parameter)) return renderFloatWidget(context, widget, parameter);
                       if(isIntParameter(*parameter)) return renderIntWidget(context, widget, parameter);
                       return renderVectorDragNumberWidget(context, widget, parameter);
                   },
                   drawFontScaleProperties);

    registerWidget(registry, CustomGuiWidgetType::Toggle,
                   supportsToggleWidget,
                   initializeWideWidget,
                   renderToggleWidget);

    registerWidget(registry, CustomGuiWidgetType::Button,
                   supportsButtonWidget,
                   initializeButtonWidget,
                   renderButtonWidget);

    registerWidget(registry, CustomGuiWidgetType::ColorSwatch,
                   supportsColorSwatchWidget,
                   initializeColorSwatchWidget,
                   renderColorSwatchWidget);

    registerWidget(registry, CustomGuiWidgetType::TextDisplay,
                   supportsTextDisplayWidget,
                   initializeFontScaledWideWidget,
                   renderTextDisplayWidget,
                   drawFontScaleProperties);

    registerWidget(registry, CustomGuiWidgetType::FileBrowser,
                   supportsFileBrowserWidget,
                   initializeFileBrowserWidget,
                   renderFileBrowserWidget,
                   drawFileBrowserProperties);

    registerWidget(registry, CustomGuiWidgetType::Dropdown,
                   supportsDropdownWidget,
                   initializeWideWidget,
                   renderIntWidget);

    registerWidget(registry, CustomGuiWidgetType::CustomDropdown,
                   supportsCustomDropdownWidget,
                   initializeCustomDropdownWidget,
                   renderIntWidget,
                   drawCustomDropdownProperties);

    registerWidget(registry, CustomGuiWidgetType::ButtonMatrix,
                   supportsButtonMatrixWidget,
                   initializeButtonMatrixWidget,
                   renderIntWidget,
                   drawButtonMatrixProperties);

    registerWidget(registry, CustomGuiWidgetType::CustomRegion,
                   supportsCustomRegionWidget,
                   initializeCustomRegionWidget,
                   renderCustomRegionWidget);
}

} // namespace ofxOceanodeCustomGuiValueWidgets
