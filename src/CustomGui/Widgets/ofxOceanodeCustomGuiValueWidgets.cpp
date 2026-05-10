
#include "CustomGui/Widgets/ofxOceanodeCustomGuiValueWidgets.h"
#include "CustomGui/Widgets/ofxOceanodeCustomGuiWidgetHelpers.h"
#include "CustomGui/ofxOceanodeCustomGuiWidgets.h"
#include "Managers/ofxOceanodeContainer.h"
#include "ofxOceanodeShared.h"
#include "ofMain.h"
#include <algorithm>

namespace {

using namespace ofxOceanodeCustomGuiWidgetHelpers;

bool supportsSliderWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isFloatParameter(parameter) || isIntParameter(parameter);
}

bool supportsKnobWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isFloatParameter(parameter) || isIntParameter(parameter);
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
    const float size = std::max(24.0f, std::min(itemSize.x, itemSize.y));
    const ofColor bodyColor = widgetBodyColor(widget);
    float value = 0.0f;
    float sliderMin = 0.0f;
    float sliderMax = 1.0f;
    bool changed = false;

    if(isFloatParameter(*parameter)){
        auto& param = parameter->cast<float>().getParameter();
        value = param.get();
        sliderMin = floatRangeMin(widget, param.getMin());
        sliderMax = floatRangeMax(widget, param.getMax());
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
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
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
        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        drawList->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
                          IM_COL32(245, 245, 245, 220), text.c_str());
    }

    if(changed){
        if(isFloatParameter(*parameter)){
            value = quantizeFloatValue(widget, value, sliderMin, sliderMax);
            parameter->cast<float>().getParameter().set(value);
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
        ImGui::TextWrapped("%d", value);
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

    registerWidget(registry, CustomGuiWidgetType::CustomRegion,
                   supportsCustomRegionWidget,
                   initializeCustomRegionWidget,
                   renderCustomRegionWidget);
}

} // namespace ofxOceanodeCustomGuiValueWidgets
