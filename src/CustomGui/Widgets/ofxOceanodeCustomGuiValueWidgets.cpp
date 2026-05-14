
#include "CustomGui/Widgets/ofxOceanodeCustomGuiValueWidgets.h"
#include "CustomGui/Widgets/ofxOceanodeCustomGuiWidgetHelpers.h"
#include "CustomGui/ofxOceanodeCustomGuiWidgets.h"
#include "Managers/ofxOceanodeContainer.h"
#include "ofxOceanodeShared.h"
#include "ofMain.h"
#include <algorithm>
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
    widget.spanH = 1;
    ensureWidgetBodyColor(widget);
    ensureWidgetLabelColor(widget);
}

void initializeFontScaledWideWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter& parameter)
{
    initializeWideWidget(widget, parameter);
    widget.config["fontScale"] = 1.0f;
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
}

bool renderKnobWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    const bool interactive = context.interactive;
    const bool showValue = context.showValue;
    const ImVec2 itemSize = widgetItemSize(context);
    const float valueAreaHeight = showValue ? ImGui::GetTextLineHeightWithSpacing() : 0.0f;
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
    drawWidgetLabel(widget, context.label);
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
        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        const float textX = valueMin.x + ((valueMax.x - valueMin.x) - textSize.x) * 0.5f;
        const float textY = valueMin.y + ((valueMax.y - valueMin.y) - textSize.y) * 0.5f;
        drawList->AddText(ImVec2(textX, textY),
                          IM_COL32(245, 245, 245, 220), text.c_str());
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
    const bool customFontScale = widget.type == CustomGuiWidgetType::DragNumber;
    const float fontScale = std::max(0.2f, widget.config.value("fontScale", 1.0f));
    const ofColor bodyColor = widgetBodyColor(widget);
    const ofColor accentColor = widget.color;

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    if(customFontScale) ImGui::SetWindowFontScale(std::max(0.2f, context.zoom * fontScale));
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

    if(changed){
        value = quantizeFloatValue(widget, value, sliderMin, sliderMax);
        param.set(value);
    }

    ImGui::PopStyleColor(8);
    if(customFontScale) ImGui::SetWindowFontScale(std::max(0.5f, context.zoom));
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
    const bool customFontScale = widget.type == CustomGuiWidgetType::DragNumber;
    const float fontScale = std::max(0.2f, widget.config.value("fontScale", 1.0f));
    const ofColor bodyColor = widgetBodyColor(widget);
    const ofColor accentColor = widget.color;

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    if(customFontScale) ImGui::SetWindowFontScale(std::max(0.2f, context.zoom * fontScale));
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

    if(changed) param.set(value);

    ImGui::PopStyleColor(8);
    if(customFontScale) ImGui::SetWindowFontScale(std::max(0.5f, context.zoom));
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
    const float fontScale = std::max(0.2f, widget.config.value("fontScale", 1.0f));
    const ofColor bodyColor = widgetBodyColor(widget);
    const ofColor accentColor = widget.color;

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    ImGui::SetWindowFontScale(std::max(0.2f, context.zoom * fontScale));
    ImGui::SetNextItemWidth(widgetItemWidth(itemSize));
    pushWidgetFrameColors(bodyColor, accentColor);

    if(isFloatVectorParameter(*parameter)){
        auto& param = parameter->cast<std::vector<float>>().getParameter();
        auto value = param.get();
        if(value.empty()){
            ImGui::TextDisabled("Empty vector");
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
    ImGui::SetWindowFontScale(std::max(0.5f, context.zoom));
    ImGui::EndGroup();
    return true;
}

void drawFontScaleProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    float fontScale = std::max(0.2f, widget.config.value("fontScale", 1.0f));
    if(ImGui::InputFloat("Font Scale", &fontScale, 0.05f, 0.2f, "%.2f")){
        widget.config["fontScale"] = std::max(0.2f, fontScale);
        context.container.markCustomGuisDirty();
    }
}

bool renderToggleWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    auto& param = parameter->cast<bool>().getParameter();
    bool value = param.get();

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    if(!context.interactive){
        ImGui::TextWrapped("%s", value ? "On" : "Off");
    }else if(ImGui::Checkbox("##value", &value)){
        param.set(value);
    }
    ImGui::EndGroup();
    return true;
}

bool renderButtonWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget&, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    ImGui::BeginGroup();
    if(context.interactive && ImGui::Button(context.label.c_str(), widgetItemSize(context))){
        parameter->cast<void>().getParameter().trigger();
    }
    ImGui::EndGroup();
    return true;
}

bool renderTextDisplayWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    const float fontScale = std::max(0.2f, widget.config.value("fontScale", 1.0f));
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
    const float buttonHeight = ImGui::GetFrameHeight();
    const float buttonWidth = std::min(96.0f, std::max(70.0f, itemSize.x * 0.32f));
    const std::string currentValue = param.get();

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);

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
    drawWidgetLabel(widget, context.label);
    ImGui::BeginChild("##customregion", widgetItemSize(context), true, childFlags);
    ImVec2 available = ImGui::GetContentRegionAvail();
    ofxOceanodeShared::pushCustomRegionRenderContext(std::max(1.0f, available.x), std::max(1.0f, available.y));
    parameter->cast<std::function<void()>>().getParameter().get()();
    ofxOceanodeShared::popCustomRegionRenderContext();
    ImGui::EndChild();
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
                   [](CustomGuiWidget&, ofxOceanodeAbstractParameter&){},
                   renderToggleWidget);

    registerWidget(registry, CustomGuiWidgetType::Button,
                   supportsButtonWidget,
                   initializeButtonWidget,
                   renderButtonWidget);

    registerWidget(registry, CustomGuiWidgetType::TextDisplay,
                   supportsTextDisplayWidget,
                   [](CustomGuiWidget& widget, ofxOceanodeAbstractParameter&){ widget.config["fontScale"] = 1.0f; },
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
