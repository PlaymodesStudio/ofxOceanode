#ifndef OFXOCEANODE_HEADLESS

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
    return isFloatParameter(parameter);
}

bool supportsDragNumberWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isFloatParameter(parameter) || isIntParameter(parameter);
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
}

void initializeButtonWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.spanW = 2;
    widget.spanH = 2;
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

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    ImGui::SetNextItemWidth(widgetItemWidth(itemSize));

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

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    ImGui::SetNextItemWidth(widgetItemWidth(itemSize));

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

    ImGui::EndGroup();
    return true;
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
    ImGui::TextWrapped("%s", parameter->cast<std::string>().getParameter().get().c_str());
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
                   renderFloatWidget);

    registerWidget(registry, CustomGuiWidgetType::DragNumber,
                   supportsDragNumberWidget,
                   initializeWideWidget,
                   [](CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter){
                       if(parameter == nullptr) return false;
                       return isFloatParameter(*parameter) ? renderFloatWidget(context, widget, parameter) : renderIntWidget(context, widget, parameter);
                   });

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
                   [](CustomGuiWidget&, ofxOceanodeAbstractParameter&){},
                   renderTextDisplayWidget);

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

#endif
