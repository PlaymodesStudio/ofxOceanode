#ifndef OFXOCEANODE_HEADLESS

#define IMGUI_DEFINE_MATH_OPERATORS
#include "CustomGui/ofxOceanodeCustomGuiPanel.h"
#include "CustomGui/ofxOceanodeCustomGuiWidgets.h"

#include "Managers/ofxOceanodeContainer.h"
#include "Nodes/ofxOceanodeNode.h"
#include "Nodes/ofxOceanodeNodeModel.h"
#include "ofxOceanodeParameter.h"
#include <cmath>
#include <cstdio>
#include "imgui.h"
#include "imgui_internal.h"

ofxOceanodeCustomGuiPanel::ofxOceanodeCustomGuiPanel(ofxOceanodeContainer& container, const std::string& panelId)
: container(container)
, panelId(panelId)
{
}

CustomGuiPanelData* ofxOceanodeCustomGuiPanel::getPanelData()
{
    return container.getCustomGuiPanelData(panelId);
}

const CustomGuiPanelData* ofxOceanodeCustomGuiPanel::getPanelData() const
{
    return container.getCustomGuiPanelData(panelId);
}

void ofxOceanodeCustomGuiPanel::draw()
{
    CustomGuiPanelData* panel = getPanelData();
    if(panel == nullptr || !panel->windowState.isOpen) return;

    auto ensureLayoutFitsWidgets = [&](CustomGuiLayout& layout){
        int requiredColumns = std::max(1, layout.columns);
        int requiredRows = std::max(1, layout.rows);
        for(const auto& widget : layout.widgets){
            requiredColumns = std::max(requiredColumns, widget.gridX + widget.spanW);
            requiredRows = std::max(requiredRows, widget.gridY + widget.spanH);
        }
        layout.columns = requiredColumns;
        layout.rows = requiredRows;
    };

    std::string title = panel->name.empty() ? "Custom GUI" : panel->name;
    if(panel->designMode) title += " [DESIGN]";
    title += "###CustomGui_" + panel->id;

    auto createStaticWidget = [&](CustomGuiWidgetType type, const std::string& label, int spanW, int spanH, const ofColor& color){
        CustomGuiWidget widget;
        widget.type = type;
        widget.label = label;
        widget.color = color;
        widget.spanW = spanW;
        widget.spanH = spanH;
        widget.config["showValue"] = true;
        if(type == CustomGuiWidgetType::Text){
            widget.config["fontScale"] = 1.0f;
        }else if(type == CustomGuiWidgetType::Image){
            widget.config["imagePath"] = "";
        }
        auto cell = findNextAvailableCell(panel->layout, widget.spanW, widget.spanH);
        widget.gridX = cell.first;
        widget.gridY = cell.second;
        panel->layout.widgets.push_back(widget);
        panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
        panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
        container.saveCustomGuis();
    };

    if(panel->windowState.hasConfig && !appliedWindowState){
        ImGui::SetNextWindowPos(ImVec2(panel->windowState.posX, panel->windowState.posY), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(panel->windowState.width, panel->windowState.height), ImGuiCond_Once);
        appliedWindowState = true;
    }

    bool openState = panel->windowState.isOpen;
    if(ImGui::Begin(title.c_str(), &openState, ImGuiWindowFlags_NoCollapse)){
        if(openState != panel->windowState.isOpen){
            panel->windowState.isOpen = openState;
            container.saveCustomGuis();
        }

        if(ImGui::SmallButton(panel->designMode ? "Run" : "Edit")){
            panel->designMode = !panel->designMode;
            container.saveCustomGuis();
        }
        ImGui::SameLine();
        if(ImGui::SmallButton("-")){
            panel->layout.zoom = ofClamp(panel->layout.zoom - 0.1f, 0.25f, 4.0f);
            container.saveCustomGuis();
        }
        ImGui::SameLine();
        if(ImGui::SmallButton("+")){
            panel->layout.zoom = ofClamp(panel->layout.zoom + 0.1f, 0.25f, 4.0f);
            container.saveCustomGuis();
        }
        ImGui::SameLine();
        ImGui::Text("Zoom %.2fx", panel->layout.zoom);
        if(panel->designMode){
            ImGui::SameLine();
            char nameBuffer[256];
            std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", panel->name.c_str());
            ImGui::SetNextItemWidth(220);
            if(ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))){
                container.renameCustomGuiPanel(panel->id, nameBuffer);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            if(ImGui::InputInt("Cols", &panel->layout.columns)){
                panel->layout.columns = std::max(1, panel->layout.columns);
                ensureLayoutFitsWidgets(panel->layout);
                container.saveCustomGuis();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            if(ImGui::InputInt("Rows", &panel->layout.rows)){
                panel->layout.rows = std::max(1, panel->layout.rows);
                ensureLayoutFitsWidgets(panel->layout);
                container.saveCustomGuis();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            if(ImGui::InputFloat("Cell W", &panel->layout.cellWidth, 1.0f, 10.0f, "%.0f")){
                panel->layout.cellWidth = std::max(20.0f, panel->layout.cellWidth);
                container.saveCustomGuis();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            if(ImGui::InputFloat("Cell H", &panel->layout.cellHeight, 1.0f, 10.0f, "%.0f")){
                panel->layout.cellHeight = std::max(20.0f, panel->layout.cellHeight);
                container.saveCustomGuis();
            }
            ImGui::SameLine();
            if(ImGui::SmallButton("Add Panel")){
                createStaticWidget(CustomGuiWidgetType::BackgroundPanel, "", 3, 2, ofColor(40, 40, 40, 180));
            }
            ImGui::SameLine();
            if(ImGui::SmallButton("Add Text")){
                createStaticWidget(CustomGuiWidgetType::Text, "Text", 2, 1, ofColor::white);
            }
            ImGui::SameLine();
            if(ImGui::SmallButton("Add Image")){
                createStaticWidget(CustomGuiWidgetType::Image, "", 3, 2, ofColor::white);
            }
        }

        const ImVec2 origin = ImGui::GetCursorPos();
        const float panelWidth = panel->layout.columns * panel->layout.cellWidth * panel->layout.zoom;
        const float panelHeight = panel->layout.rows * panel->layout.cellHeight * panel->layout.zoom;

        if(panel->designMode){
            ImGui::InvisibleButton("##CustomGuiGridSpace", ImVec2(panelWidth, panelHeight));
            ImGui::SetItemAllowOverlap();
        }else{
            ImGui::Dummy(ImVec2(panelWidth, panelHeight));
        }

        if(panel->designMode) drawGridOverlay(panel->layout, origin);

        int widgetToRemove = -1;
        static int draggedWidgetIndex = -1;
        static ImVec2 dragAnchorMouse = ImVec2(0, 0);
        static int dragAnchorX = 0;
        static int dragAnchorY = 0;
        static int resizedWidgetIndex = -1;
        static ImVec2 resizeAnchorMouse = ImVec2(0, 0);
        static int resizeAnchorW = 1;
        static int resizeAnchorH = 1;

        auto drawWidgetAtIndex = [&](size_t i){
            auto& widget = panel->layout.widgets[i];
            const float x = origin.x + widget.gridX * panel->layout.cellWidth * panel->layout.zoom;
            const float y = origin.y + widget.gridY * panel->layout.cellHeight * panel->layout.zoom;
            const float w = widget.spanW * panel->layout.cellWidth * panel->layout.zoom;
            const float h = widget.spanH * panel->layout.cellHeight * panel->layout.zoom;

            ImGui::SetCursorPos(ImVec2(x, y));
            ImGui::PushID((int)i);
            bool hasParameter = !(widget.type == CustomGuiWidgetType::Label ||
                                  widget.type == CustomGuiWidgetType::BackgroundPanel ||
                                  widget.type == CustomGuiWidgetType::Text ||
                                  widget.type == CustomGuiWidgetType::Image);
            ofxOceanodeAbstractParameter* parameter = hasParameter ? findParameter(widget) : nullptr;
            renderWidget(widget, parameter, ImVec2(w, h));

            if(panel->designMode){
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 min = ImGui::GetItemRectMin();
                const ImVec2 max = ImGui::GetItemRectMax();
                drawList->AddRect(min, max, IM_COL32(255, 180, 40, 180), 2.0f, 0, 1.5f);

                bool hovered = ImGui::IsMouseHoveringRect(min, max);
                ImVec2 handleMin(max.x - 12.0f, max.y - 12.0f);
                bool resizeHovered = ImGui::IsMouseHoveringRect(handleMin, max);
                drawList->AddRectFilled(handleMin, max, IM_COL32(255, 180, 40, 220), 1.0f);
                if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
                    if(resizeHovered){
                        resizedWidgetIndex = (int)i;
                        resizeAnchorMouse = ImGui::GetIO().MousePos;
                        resizeAnchorW = widget.spanW;
                        resizeAnchorH = widget.spanH;
                    }else{
                        draggedWidgetIndex = (int)i;
                        dragAnchorMouse = ImGui::GetIO().MousePos;
                        dragAnchorX = widget.gridX;
                        dragAnchorY = widget.gridY;
                    }
                }
                if(draggedWidgetIndex == (int)i && ImGui::IsMouseDown(ImGuiMouseButton_Left)){
                    ImVec2 delta = ImGui::GetIO().MousePos - dragAnchorMouse;
                    int offsetX = (int)std::round(delta.x / (panel->layout.cellWidth * panel->layout.zoom));
                    int offsetY = (int)std::round(delta.y / (panel->layout.cellHeight * panel->layout.zoom));
                    widget.gridX = std::max(0, dragAnchorX + offsetX);
                    widget.gridY = std::max(0, dragAnchorY + offsetY);
                    panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
                    panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
                }
                if(draggedWidgetIndex == (int)i && ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
                    ensureLayoutFitsWidgets(panel->layout);
                    draggedWidgetIndex = -1;
                    container.saveCustomGuis();
                }
                if(resizedWidgetIndex == (int)i && ImGui::IsMouseDown(ImGuiMouseButton_Left)){
                    ImVec2 delta = ImGui::GetIO().MousePos - resizeAnchorMouse;
                    int offsetW = (int)std::round(delta.x / (panel->layout.cellWidth * panel->layout.zoom));
                    int offsetH = (int)std::round(delta.y / (panel->layout.cellHeight * panel->layout.zoom));
                    widget.spanW = std::max(1, resizeAnchorW + offsetW);
                    widget.spanH = std::max(1, resizeAnchorH + offsetH);
                    panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
                    panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
                }
                if(resizedWidgetIndex == (int)i && ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
                    ensureLayoutFitsWidgets(panel->layout);
                    resizedWidgetIndex = -1;
                    container.saveCustomGuis();
                }

                if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)){
                    ImGui::OpenPopup("Widget Properties");
                }
                if(drawWidgetProperties(widget, i, parameter)){
                    widgetToRemove = (int)i;
                }
            }else if(parameter != nullptr){
                if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)){
                    ImGui::OpenPopup("Widget Context");
                }
                if(ImGui::BeginPopup("Widget Context")){
#ifdef OFXOCEANODE_USE_MIDI
                    if(ImGui::Selectable("Bind MIDI")){
                        container.createMidiBinding(*parameter);
                    }
                    if(ImGui::Selectable("Unbind last MIDI")){
                        container.removeLastMidiBinding(*parameter);
                    }
#else
                    ImGui::TextDisabled("No actions");
#endif
                    ImGui::EndPopup();
                }
            }
            ImGui::PopID();
        };

        for(size_t i = 0; i < panel->layout.widgets.size(); i++){
            if(panel->layout.widgets[i].type == CustomGuiWidgetType::BackgroundPanel) drawWidgetAtIndex(i);
        }
        for(size_t i = 0; i < panel->layout.widgets.size(); i++){
            if(panel->layout.widgets[i].type != CustomGuiWidgetType::BackgroundPanel) drawWidgetAtIndex(i);
        }

        if(widgetToRemove >= 0 && widgetToRemove < (int)panel->layout.widgets.size()){
            panel->layout.widgets.erase(panel->layout.widgets.begin() + widgetToRemove);
            ensureLayoutFitsWidgets(panel->layout);
            container.saveCustomGuis();
        }

        ImVec2 currentPos = ImGui::GetWindowPos();
        ImVec2 currentSize = ImGui::GetWindowSize();
        if(currentPos.x != panel->windowState.posX || currentPos.y != panel->windowState.posY ||
           currentSize.x != panel->windowState.width || currentSize.y != panel->windowState.height){
            panel->windowState.hasConfig = true;
            panel->windowState.posX = currentPos.x;
            panel->windowState.posY = currentPos.y;
            panel->windowState.width = currentSize.x;
            panel->windowState.height = currentSize.y;
            container.saveCustomGuis();
        }
    }
    ImGui::End();
}

ofxOceanodeAbstractParameter* ofxOceanodeCustomGuiPanel::findParameter(const CustomGuiWidget& widget) const
{
    return container.findCustomGuiParameter(widget.parameterRef.parameterPath);
}

std::string ofxOceanodeCustomGuiPanel::getFallbackLabel(const CustomGuiWidget& widget) const
{
    if(!widget.label.empty()) return widget.label;
    if(!widget.parameterRef.parameterDisplayName.empty()) return widget.parameterRef.parameterDisplayName;
    return widget.parameterRef.parameterPath;
}

bool ofxOceanodeCustomGuiPanel::renderWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter, const ImVec2& size)
{
    const std::string label = getFallbackLabel(widget);
    const bool showValue = shouldShowNumericValue(widget);
    const bool designMode = getPanelData() != nullptr ? getPanelData()->designMode : false;
    const bool isStaticWidget = widget.type == CustomGuiWidgetType::BackgroundPanel ||
                                widget.type == CustomGuiWidgetType::Text ||
                                widget.type == CustomGuiWidgetType::Image ||
                                widget.type == CustomGuiWidgetType::Label;
    ImGui::BeginGroup();
    if(widget.type != CustomGuiWidgetType::BackgroundPanel && widget.type != CustomGuiWidgetType::Text && widget.type != CustomGuiWidgetType::Image){
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(widget.color.r / 255.0f, widget.color.g / 255.0f, widget.color.b / 255.0f, widget.color.a / 255.0f));
        if(widget.type != CustomGuiWidgetType::Button) ImGui::TextWrapped("%s", label.c_str());
        ImGui::PopStyleColor();
    }

    const ImVec2 itemSize(size.x, std::max(1.0f, size.y - ImGui::GetFrameHeightWithSpacing()));
    if(widget.type == CustomGuiWidgetType::Label){
        ImGui::TextWrapped("%s", label.c_str());
        ImGui::EndGroup();
        return true;
    }

    if(widget.type == CustomGuiWidgetType::BackgroundPanel){
        if(designMode){
            ImGui::InvisibleButton("##background", size);
            ImGui::SetItemAllowOverlap();
        }else{
            ImGui::Dummy(size);
        }
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        drawList->AddRectFilled(min, max, IM_COL32(widget.color.r, widget.color.g, widget.color.b, widget.color.a), 4.0f);
        ImGui::EndGroup();
        return true;
    }

    if(widget.type == CustomGuiWidgetType::Text){
        const float fontScale = std::max(0.2f, widget.config.value("fontScale", 1.0f));
        ImGui::InvisibleButton("##text", size);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetItemRectMin();
        float fontSize = ImGui::GetFontSize() * fontScale;
        drawList->AddText(ImGui::GetFont(), fontSize, min, IM_COL32(widget.color.r, widget.color.g, widget.color.b, widget.color.a), label.c_str(), nullptr, size.x);
        ImGui::EndGroup();
        return true;
    }

    if(widget.type == CustomGuiWidgetType::Image){
        ImGui::InvisibleButton("##image", size);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        std::string imagePath = widget.config.value("imagePath", std::string());
        auto image = loadWidgetImage(imagePath);
        if(image != nullptr && image->isAllocated()){
            ImTextureID textureID = (ImTextureID)(uintptr_t)image->getTexture().texData.textureID;
            drawList->AddImage(textureID, min, max, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, widget.color.a));
        }else{
            drawList->AddRect(min, max, IM_COL32(160, 160, 160, 180), 2.0f);
            drawList->AddText(ImVec2(min.x + 6.0f, min.y + 6.0f), IM_COL32(200, 200, 200, 220), imagePath.empty() ? "Image path..." : "Image not found");
        }
        ImGui::EndGroup();
        return true;
    }

    if(parameter == nullptr && !isStaticWidget){
        ImGui::TextDisabled("Missing: %s", label.c_str());
        ImGui::EndGroup();
        return false;
    }

    const std::string type = parameter->valueType();
    const float itemWidth = std::max(40.0f, itemSize.x);
    ImGui::SetNextItemWidth(itemWidth);
    bool interactive = ofxOceanodeCustomGuiWidgets::isInteractive(widget, parameter);
    bool verticalSlider = itemSize.y > itemSize.x;
    bool useCustomRange = widget.config.value("useCustomRange", false);

    auto floatRangeMin = [&](float fallbackMin){
        return useCustomRange ? widget.config.value("rangeMin", fallbackMin) : fallbackMin;
    };
    auto floatRangeMax = [&](float fallbackMax){
        return useCustomRange ? widget.config.value("rangeMax", fallbackMax) : fallbackMax;
    };
    auto intRangeMin = [&](int fallbackMin){
        return useCustomRange ? (int)std::round(widget.config.value("rangeMin", (float)fallbackMin)) : fallbackMin;
    };
    auto intRangeMax = [&](int fallbackMax){
        return useCustomRange ? (int)std::round(widget.config.value("rangeMax", (float)fallbackMax)) : fallbackMax;
    };

    if(type == typeid(float).name()){
        auto& param = parameter->cast<float>().getParameter();
        float value = param.get();
        bool changed = false;
        float sliderMin = floatRangeMin(param.getMin());
        float sliderMax = floatRangeMax(param.getMax());
        if(!interactive){
            float fraction = sliderMax != sliderMin ? (value - sliderMin) / (sliderMax - sliderMin) : 0.0f;
            ImGui::ProgressBar(ofClamp(fraction, 0.0f, 1.0f), itemSize, showValue ? ofToString(value, 3).c_str() : "");
        } else if(widget.type == CustomGuiWidgetType::DragNumber){
            changed = ImGui::DragFloat("##value", &value, 0.01f, sliderMin, sliderMax);
        } else if(widget.type == CustomGuiWidgetType::Waveform){
            float fraction = sliderMax != sliderMin ? (value - sliderMin) / (sliderMax - sliderMin) : 0.0f;
            ImGui::ProgressBar(ofClamp(fraction, 0.0f, 1.0f), itemSize, showValue ? ofToString(value, 3).c_str() : "");
        } else if(verticalSlider) {
            changed = ImGui::VSliderFloat("##value", itemSize, &value, sliderMin, sliderMax, showValue ? "%.3f" : "");
        } else {
            changed = ImGui::SliderFloat("##value", &value, sliderMin, sliderMax, showValue ? "%.3f" : "");
        }
        if(changed) param.set(value);
    } else if(type == typeid(int).name()){
        auto& param = parameter->cast<int>().getParameter();
        int value = param.get();
        bool changed = false;
        int sliderMin = intRangeMin(param.getMin());
        int sliderMax = intRangeMax(param.getMax());
        auto options = parameter->cast<int>().getDropdownOptions();
        if(!interactive){
            ImGui::TextWrapped("%d", value);
        } else if(widget.type == CustomGuiWidgetType::Dropdown && !options.empty()){
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
        } else if(widget.type == CustomGuiWidgetType::DragNumber){
            changed = ImGui::DragInt("##value", &value, 1.0f, sliderMin, sliderMax);
        } else if(verticalSlider) {
            changed = ImGui::VSliderInt("##value", itemSize, &value, sliderMin, sliderMax, showValue ? "%d" : "");
        } else {
            changed = ImGui::SliderInt("##value", &value, sliderMin, sliderMax, showValue ? "%d" : "");
        }
        if(changed) param.set(value);
    } else if(type == typeid(bool).name()){
        auto& param = parameter->cast<bool>().getParameter();
        bool value = param.get();
        if(!interactive){
            ImGui::TextWrapped("%s", value ? "On" : "Off");
        } else if(ImGui::Checkbox("##value", &value)) param.set(value);
    } else if(type == typeid(void).name()){
        if(interactive && ImGui::Button(label.c_str(), itemSize)) parameter->cast<void>().getParameter().trigger();
    } else if(type == typeid(std::string).name()){
        auto& param = parameter->cast<std::string>().getParameter();
        ImGui::TextWrapped("%s", param.get().c_str());
    } else if(type == typeid(std::vector<float>).name()){
        auto& param = parameter->cast<std::vector<float>>().getParameter();
        auto value = param.get();
        if(widget.type == CustomGuiWidgetType::Waveform){
            if(!value.empty()) ImGui::PlotLines("##value", value.data(), (int)value.size(), 0, nullptr, FLT_MAX, FLT_MAX, itemSize);
        } else if(widget.type == CustomGuiWidgetType::XYPad && value.size() >= 2){
            ImGui::Button("##xypad", itemSize);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            drawList->AddRect(min, max, IM_COL32(180, 180, 180, 180));
            float x = ofMap(value[0], param.getMin()[0], param.getMax()[0], min.x, max.x, true);
            float y = ofMap(value[1], param.getMin()[1], param.getMax()[1], max.y, min.y, true);
            drawList->AddCircleFilled(ImVec2(x, y), 5.0f, IM_COL32(widget.color.r, widget.color.g, widget.color.b, 255));
            if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0)){
                ImVec2 mouse = ImGui::GetIO().MousePos;
                value[0] = ofMap(mouse.x, min.x, max.x, param.getMin()[0], param.getMax()[0], true);
                value[1] = ofMap(mouse.y, max.y, min.y, param.getMin()[1], param.getMax()[1], true);
                param.set(value);
            }
        } else if(!value.empty()){
            if(widget.type == CustomGuiWidgetType::MultiSlider){
                bool changed = drawMultiSliderWidget(widget, parameter, value, itemSize, interactive);
                if(changed) param.set(value);
                ImGui::EndGroup();
                return true;
            }
            bool changed = false;
            int visibleCount = widget.config.value("visibleCount", (int)value.size());
            visibleCount = ofClamp(visibleCount, 1, (int)value.size());
            bool vertical = widget.type == CustomGuiWidgetType::MultiSlider ? true : verticalSlider;
            bool useCustomRange = widget.config.value("useCustomRange", false);
            float rowHeight = std::max(16.0f, itemSize.y / std::max(1, visibleCount));
            if(vertical){
                const float baseCursorX = ImGui::GetCursorPosX();
                const float baseCursorY = ImGui::GetCursorPosY();
                const float spacing = visibleCount > 1 ? std::min(2.0f, itemSize.x / std::max(8.0f, (float)visibleCount * 2.0f)) : 0.0f;
                const float totalSpacing = spacing * std::max(0, visibleCount - 1);
                const float barWidth = std::max(1.0f, (itemSize.x - totalSpacing) / (float)visibleCount);
                const ImVec2 barSize(barWidth, itemSize.y);

                for(int i = 0; i < visibleCount; i++){
                    ImGui::PushID((int)i);
                    float min = i < param.getMin().size() ? param.getMin()[i] : 0.0f;
                    float max = i < param.getMax().size() ? param.getMax()[i] : 1.0f;
                    if(useCustomRange){
                        min = widget.config.value("rangeMin", min);
                        max = widget.config.value("rangeMax", max);
                    }
                    ImGui::SetCursorPos(ImVec2(baseCursorX + i * (barWidth + spacing), baseCursorY));
                    if(interactive){
                        changed |= ImGui::VSliderFloat("##bar", barSize, &value[i], min, max);
                    } else {
                        float fraction = max != min ? (value[i] - min) / (max - min) : 0.0f;
                        drawVerticalMeter(barSize, ofClamp(fraction, 0.0f, 1.0f), IM_COL32(widget.color.r, widget.color.g, widget.color.b, 220));
                    }
                    ImGui::PopID();
                }
                ImGui::SetCursorPos(ImVec2(baseCursorX, baseCursorY + itemSize.y));
            } else {
                for(int i = 0; i < visibleCount; i++){
                    ImGui::PushID((int)i);
                    float min = i < param.getMin().size() ? param.getMin()[i] : 0.0f;
                    float max = i < param.getMax().size() ? param.getMax()[i] : 1.0f;
                    if(useCustomRange){
                        min = widget.config.value("rangeMin", min);
                        max = widget.config.value("rangeMax", max);
                    }
                    if(interactive){
                        ImGui::SetNextItemWidth(itemWidth);
                        changed |= ImGui::SliderFloat("##bar", &value[i], min, max);
                    } else {
                        float fraction = max != min ? (value[i] - min) / (max - min) : 0.0f;
                        ImGui::ProgressBar(ofClamp(fraction, 0.0f, 1.0f), ImVec2(itemWidth, 0), "");
                    }
                    ImGui::PopID();
                    if((i + 1) * rowHeight >= itemSize.y) break;
                }
            }
            if(changed) param.set(value);
        }
    } else if(type == typeid(std::vector<int>).name()){
        auto& param = parameter->cast<std::vector<int>>().getParameter();
        auto value = param.get();
        int rows = widget.config.value("rows", 1);
        int cols = widget.config.value("cols", std::max(1, (int)value.size()));
        bool changed = false;
        ImVec2 buttonSize(std::max(12.0f, itemSize.x / cols - 3.0f), std::max(12.0f, itemSize.y / rows - 3.0f));
        for(int r = 0; r < rows; r++){
            for(int c = 0; c < cols; c++){
                int index = r * cols + c;
                if(index >= (int)value.size()) continue;
                ImGui::PushID(index);
                if(interactive){
                    if(value[index] > 0) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(widget.color.r / 255.0f, widget.color.g / 255.0f, widget.color.b / 255.0f, 0.8f));
                    if(ImGui::Button("##cell", buttonSize)){
                        value[index] = value[index] > 0 ? 0 : 1;
                        changed = true;
                    }
                    if(value[index] > 0) ImGui::PopStyleColor();
                } else {
                    ImGui::ProgressBar(value[index] > 0 ? 1.0f : 0.0f, buttonSize, "");
                }
                ImGui::PopID();
                if(c < cols - 1) ImGui::SameLine();
            }
        }
        if(changed) param.set(value);
    } else {
        ImGui::TextDisabled("Unsupported type");
    }

    ImGui::EndGroup();
    return true;
}

bool ofxOceanodeCustomGuiPanel::drawWidgetProperties(CustomGuiWidget& widget, size_t widgetIndex, ofxOceanodeAbstractParameter* parameter)
{
    bool removeWidget = false;
    if(ImGui::BeginPopup("Widget Properties")){
        ImGui::Text("Widget %zu", widgetIndex + 1);
        char labelBuffer[256];
        std::snprintf(labelBuffer, sizeof(labelBuffer), "%s", widget.label.c_str());
        if(ImGui::InputText("Label", labelBuffer, sizeof(labelBuffer))){
            widget.label = labelBuffer;
            container.saveCustomGuis();
        }

        int spanW = widget.spanW;
        int spanH = widget.spanH;
        if(ImGui::InputInt("Width", &spanW)){
            widget.spanW = std::max(1, spanW);
            if(CustomGuiPanelData* panel = getPanelData()){
                panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
                panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
            }
            container.saveCustomGuis();
        }
        if(ImGui::InputInt("Height", &spanH)){
            widget.spanH = std::max(1, spanH);
            if(CustomGuiPanelData* panel = getPanelData()){
                panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
                panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
            }
            container.saveCustomGuis();
        }

        float color[4] = {
            widget.color.r / 255.0f,
            widget.color.g / 255.0f,
            widget.color.b / 255.0f,
            widget.color.a / 255.0f
        };
        if(ImGui::ColorEdit4("Color", color)){
            widget.color = ofColor(color[0] * 255.0f, color[1] * 255.0f, color[2] * 255.0f, color[3] * 255.0f);
            container.saveCustomGuis();
        }

        bool interactive = ofxOceanodeCustomGuiWidgets::isInteractive(widget, parameter);
        if(parameter != nullptr && parameter->valueType() != typeid(std::string).name()){
            if(ImGui::Checkbox("Interactive", &interactive)){
                widget.config["interactive"] = interactive;
                container.saveCustomGuis();
            }
        }

        bool showValue = shouldShowNumericValue(widget);
        if(parameter != nullptr &&
           (parameter->valueType() == typeid(float).name() ||
            parameter->valueType() == typeid(int).name() ||
            parameter->valueType() == typeid(std::vector<float>).name())){
            if(ImGui::Checkbox("Show Value", &showValue)){
                widget.config["showValue"] = showValue;
                container.saveCustomGuis();
            }
        }

        if(parameter != nullptr &&
           (parameter->valueType() == typeid(float).name() ||
            parameter->valueType() == typeid(int).name() ||
            parameter->valueType() == typeid(std::vector<float>).name())){
            bool useCustomRange = widget.config.value("useCustomRange", false);
            if(ImGui::Checkbox("Custom Range", &useCustomRange)){
                widget.config["useCustomRange"] = useCustomRange;
                container.saveCustomGuis();
            }
            if(useCustomRange){
                if(parameter->valueType() == typeid(int).name()){
                    int rangeMin = (int)std::round(widget.config.value("rangeMin", (float)parameter->cast<int>().getParameter().getMin()));
                    int rangeMax = (int)std::round(widget.config.value("rangeMax", (float)parameter->cast<int>().getParameter().getMax()));
                    if(ImGui::InputInt("Range Min", &rangeMin)){
                        widget.config["rangeMin"] = rangeMin;
                        if(rangeMax < rangeMin) widget.config["rangeMax"] = rangeMin;
                        container.saveCustomGuis();
                    }
                    if(ImGui::InputInt("Range Max", &rangeMax)){
                        widget.config["rangeMax"] = std::max(rangeMin, rangeMax);
                        container.saveCustomGuis();
                    }
                }else{
                    float defaultMin = parameter->valueType() == typeid(float).name() ? parameter->cast<float>().getParameter().getMin() : 0.0f;
                    float defaultMax = parameter->valueType() == typeid(float).name() ? parameter->cast<float>().getParameter().getMax() : 1.0f;
                    if(parameter->valueType() == typeid(std::vector<float>).name()){
                        auto mins = parameter->cast<std::vector<float>>().getParameter().getMin();
                        auto maxs = parameter->cast<std::vector<float>>().getParameter().getMax();
                        if(!mins.empty()) defaultMin = mins[0];
                        if(!maxs.empty()) defaultMax = maxs[0];
                    }
                    float rangeMin = widget.config.value("rangeMin", defaultMin);
                    float rangeMax = widget.config.value("rangeMax", defaultMax);
                    if(ImGui::InputFloat("Range Min", &rangeMin)){
                        widget.config["rangeMin"] = rangeMin;
                        if(rangeMax < rangeMin) widget.config["rangeMax"] = rangeMin;
                        container.saveCustomGuis();
                    }
                    if(ImGui::InputFloat("Range Max", &rangeMax)){
                        widget.config["rangeMax"] = std::max(rangeMin, rangeMax);
                        container.saveCustomGuis();
                    }
                }
            }
        }

        if(widget.type == CustomGuiWidgetType::Text){
            float fontScale = std::max(0.2f, widget.config.value("fontScale", 1.0f));
            if(ImGui::InputFloat("Font Scale", &fontScale, 0.05f, 0.2f, "%.2f")){
                widget.config["fontScale"] = std::max(0.2f, fontScale);
                container.saveCustomGuis();
            }
        }

        if(widget.type == CustomGuiWidgetType::Image){
            char pathBuffer[512];
            std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", widget.config.value("imagePath", std::string()).c_str());
            if(ImGui::InputText("Image Path", pathBuffer, sizeof(pathBuffer))){
                widget.config["imagePath"] = std::string(pathBuffer);
                imageCache.erase(std::string(pathBuffer));
                container.saveCustomGuis();
            }
        }

        if(parameter != nullptr && parameter->valueType() == typeid(std::vector<float>).name() && widget.type == CustomGuiWidgetType::MultiSlider){
            int maxCount = (int)parameter->cast<std::vector<float>>().getParameter().get().size();
            int visibleCount = widget.config.value("visibleCount", maxCount);
            if(ImGui::SliderInt("Num Sliders", &visibleCount, 1, std::max(1, maxCount))){
                widget.config["visibleCount"] = visibleCount;
                container.saveCustomGuis();
            }

            bool vertical = widget.config.value("vertical", true);
            if(ImGui::Checkbox("Vertical", &vertical)){
                widget.config["vertical"] = vertical;
                container.saveCustomGuis();
            }

            bool canResizeVector = interactive && !parameter->hasInConnection() && !(parameter->getFlags() & ofxOceanodeParameterFlags_DisableInConnection);
            if(canResizeVector){
                int vectorSize = (int)parameter->cast<std::vector<float>>().getParameter().get().size();
                if(ImGui::SliderInt("Vector Size", &vectorSize, 1, 64)){
                    auto& param = parameter->cast<std::vector<float>>().getParameter();
                    auto values = param.get();
                    auto mins = param.getMin();
                    auto maxs = param.getMax();
                    float fillValue = values.empty() ? 0.0f : values.back();
                    float fillMin = mins.empty() ? 0.0f : mins.back();
                    float fillMax = maxs.empty() ? 1.0f : maxs.back();
                    values.resize(vectorSize, fillValue);
                    mins.resize(vectorSize, fillMin);
                    maxs.resize(vectorSize, fillMax);
                    param.setMin(mins);
                    param.setMax(maxs);
                    param.set(values);
                    widget.config["visibleCount"] = vectorSize;
                    container.saveCustomGuis();
                }
            }
        }

        if(ImGui::Button("Remove from GUI")){
            removeWidget = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return removeWidget;
}

bool ofxOceanodeCustomGuiPanel::drawMultiSliderWidget(CustomGuiWidget& widget,
                                                      ofxOceanodeAbstractParameter* parameter,
                                                      std::vector<float>& value,
                                                      const ImVec2& size,
                                                      bool interactive) const
{
    if(parameter == nullptr) return false;

    auto& param = parameter->cast<std::vector<float>>().getParameter();
    const auto& mins = param.getMin();
    const auto& maxs = param.getMax();
    const int visibleCount = ofClamp(widget.config.value("visibleCount", (int)value.size()), 1, (int)value.size());
    const bool vertical = widget.config.value("vertical", true);
    const bool showValue = shouldShowNumericValue(widget);
    const bool useCustomRange = widget.config.value("useCustomRange", false);

    ImGui::InvisibleButton("##multislider", size);
    const bool isHovered = ImGui::IsItemHovered();
    const bool isActive = interactive && ImGui::IsItemActive();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();

    drawList->AddRectFilled(min, max, IM_COL32(50, 50, 50, 255), 2.0f);
    drawList->AddRect(min, max, IM_COL32(100, 100, 100, 255), 2.0f);

    const float majorSize = vertical ? size.x : size.y;
    const float spacing = std::max(0.0f, std::min(1.0f, 1.8f - 0.12f * (float)visibleCount));
    const float totalSpacing = spacing * std::max(0, visibleCount - 1);
    const float slotSize = std::max(1.0f, (majorSize - totalSpacing) / (float)visibleCount);
    bool changed = false;

    auto setValueFromMouse = [&](const ImVec2& mousePos){
        const float majorPos = vertical ? (mousePos.x - min.x) : (mousePos.y - min.y);
        int index = (int)std::floor(majorPos / std::max(1.0f, slotSize + spacing));
        index = ofClamp(index, 0, visibleCount - 1);

        float minValue = index < mins.size() ? mins[index] : 0.0f;
        float maxValue = index < maxs.size() ? maxs[index] : 1.0f;
        if(useCustomRange){
            minValue = widget.config.value("rangeMin", minValue);
            maxValue = widget.config.value("rangeMax", maxValue);
        }
        if(minValue >= maxValue) maxValue = minValue + 1.0f;

        float normalized = 0.0f;
        if(vertical){
            normalized = 1.0f - ofClamp((mousePos.y - min.y) / std::max(1.0f, size.y), 0.0f, 1.0f);
        }else{
            normalized = ofClamp((mousePos.x - min.x) / std::max(1.0f, size.x), 0.0f, 1.0f);
        }
        const float newValue = minValue + normalized * (maxValue - minValue);
        if(index < (int)value.size() && value[index] != newValue){
            value[index] = newValue;
            changed = true;
        }
    };

    if(interactive && (isActive || (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))){
        setValueFromMouse(ImGui::GetIO().MousePos);
    }

    for(int i = 0; i < visibleCount; i++){
        float minValue = i < mins.size() ? mins[i] : 0.0f;
        float maxValue = i < maxs.size() ? maxs[i] : 1.0f;
        if(useCustomRange){
            minValue = widget.config.value("rangeMin", minValue);
            maxValue = widget.config.value("rangeMax", maxValue);
        }
        if(minValue >= maxValue) maxValue = minValue + 1.0f;
        const float normalized = ofClamp((value[i] - minValue) / (maxValue - minValue), 0.0f, 1.0f);

        if(vertical){
            const float x0 = min.x + i * (slotSize + spacing);
            const float x1 = x0 + slotSize;
            if(i > 0 && spacing > 0.0f){
                drawList->AddLine(ImVec2(x0 - spacing * 0.5f, min.y), ImVec2(x0 - spacing * 0.5f, max.y), IM_COL32(80, 80, 80, 255));
            }
            const float fillTop = max.y - size.y * normalized;
            drawList->AddRectFilled(ImVec2(x0, fillTop), ImVec2(x1, max.y), IM_COL32(widget.color.r, widget.color.g, widget.color.b, 235));
            if(showValue && slotSize > 16.0f){
                std::string text = ofToString(value[i], 2);
                ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
                if(textSize.x < slotSize - 2.0f){
                    drawList->AddText(ImVec2(x0 + (slotSize - textSize.x) * 0.5f, min.y + 2.0f), IM_COL32(235, 235, 235, 220), text.c_str());
                }
            }
        }else{
            const float y0 = min.y + i * (slotSize + spacing);
            const float y1 = y0 + slotSize;
            if(i > 0 && spacing > 0.0f){
                drawList->AddLine(ImVec2(min.x, y0 - spacing * 0.5f), ImVec2(max.x, y0 - spacing * 0.5f), IM_COL32(80, 80, 80, 255));
            }
            const float fillRight = min.x + size.x * normalized;
            drawList->AddRectFilled(ImVec2(min.x, y0), ImVec2(fillRight, y1), IM_COL32(widget.color.r, widget.color.g, widget.color.b, 235));
            if(showValue && slotSize > 14.0f){
                std::string text = ofToString(value[i], 2);
                ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
                if(textSize.y < slotSize - 2.0f){
                    drawList->AddText(ImVec2(min.x + 4.0f, y0 + (slotSize - textSize.y) * 0.5f), IM_COL32(235, 235, 235, 220), text.c_str());
                }
            }
        }
    }

    if(isHovered){
        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        const float majorPos = vertical ? (mousePos.x - min.x) : (mousePos.y - min.y);
        int hoveredIndex = (int)std::floor(majorPos / std::max(1.0f, slotSize + spacing));
        hoveredIndex = ofClamp(hoveredIndex, 0, visibleCount - 1);
        ImGui::SetTooltip("Slider %d: %s", hoveredIndex, ofToString(value[hoveredIndex], 3).c_str());
    }

    return changed;
}

void ofxOceanodeCustomGuiPanel::drawVerticalMeter(const ImVec2& size, float normalized, const ImU32& color) const
{
    ImGui::InvisibleButton("##meter", size);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    drawList->AddRect(min, max, IM_COL32(180, 180, 180, 180));
    float fillTop = max.y - (max.y - min.y) * normalized;
    drawList->AddRectFilled(ImVec2(min.x + 1.0f, fillTop), ImVec2(max.x - 1.0f, max.y - 1.0f), color);
}

void ofxOceanodeCustomGuiPanel::drawGridOverlay(const CustomGuiLayout& layout, const ImVec2& origin) const
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 screenOrigin = ImGui::GetWindowPos() + origin;
    const float cellW = layout.cellWidth * layout.zoom;
    const float cellH = layout.cellHeight * layout.zoom;
    for(int x = 0; x <= layout.columns; x++){
        float px = screenOrigin.x + x * cellW;
        drawList->AddLine(ImVec2(px, screenOrigin.y), ImVec2(px, screenOrigin.y + layout.rows * cellH), IM_COL32(255, 255, 255, 55));
    }
    for(int y = 0; y <= layout.rows; y++){
        float py = screenOrigin.y + y * cellH;
        drawList->AddLine(ImVec2(screenOrigin.x, py), ImVec2(screenOrigin.x + layout.columns * cellW, py), IM_COL32(255, 255, 255, 55));
    }
}

std::vector<CustomGuiWidgetType> ofxOceanodeCustomGuiPanel::getCompatibleWidgetTypes(ofxOceanodeAbstractParameter& parameter) const
{
    const std::string type = parameter.valueType();
    if(type == typeid(float).name()) return {CustomGuiWidgetType::Slider, CustomGuiWidgetType::Knob, CustomGuiWidgetType::DragNumber, CustomGuiWidgetType::Waveform};
    if(type == typeid(int).name()){
        auto options = parameter.cast<int>().getDropdownOptions();
        if(options.empty()) return {CustomGuiWidgetType::Slider, CustomGuiWidgetType::DragNumber};
        return {CustomGuiWidgetType::Slider, CustomGuiWidgetType::DragNumber, CustomGuiWidgetType::Dropdown};
    }
    if(type == typeid(bool).name()) return {CustomGuiWidgetType::Toggle};
    if(type == typeid(void).name()) return {CustomGuiWidgetType::Button};
    if(type == typeid(std::string).name()) return {CustomGuiWidgetType::TextDisplay};
    if(type == typeid(std::vector<float>).name()){
        auto value = parameter.cast<std::vector<float>>().getParameter().get();
        if(value.size() == 2) return {CustomGuiWidgetType::MultiSlider, CustomGuiWidgetType::Waveform, CustomGuiWidgetType::XYPad};
        return {CustomGuiWidgetType::MultiSlider, CustomGuiWidgetType::Waveform};
    }
    if(type == typeid(std::vector<int>).name()) return {CustomGuiWidgetType::ToggleGrid, CustomGuiWidgetType::MultiSlider};
    return {};
}

CustomGuiWidgetType ofxOceanodeCustomGuiPanel::getDefaultWidgetType(ofxOceanodeAbstractParameter& parameter) const
{
    auto types = getCompatibleWidgetTypes(parameter);
    if(types.empty()) return CustomGuiWidgetType::Label;
    return types.front();
}

bool ofxOceanodeCustomGuiPanel::containsParameter(ofxOceanodeAbstractParameter& parameter) const
{
    const CustomGuiPanelData* panel = getPanelData();
    if(panel == nullptr) return false;
    const std::string parameterPath = container.getCustomGuiParameterPath(parameter);
    for(const auto& widget : panel->layout.widgets){
        if(widget.parameterRef.parameterPath == parameterPath) return true;
    }
    return false;
}

bool ofxOceanodeCustomGuiPanel::removeParameter(const std::string& parameterPath)
{
    CustomGuiPanelData* panel = getPanelData();
    if(panel == nullptr) return false;

    auto& widgets = panel->layout.widgets;
    auto it = std::remove_if(widgets.begin(), widgets.end(), [&](const CustomGuiWidget& widget){
        return widget.parameterRef.parameterPath == parameterPath;
    });
    if(it == widgets.end()) return false;

    widgets.erase(it, widgets.end());
    container.saveCustomGuis();
    return true;
}

bool ofxOceanodeCustomGuiPanel::addParameter(ofxOceanodeAbstractParameter& parameter, CustomGuiWidgetType type)
{
    CustomGuiPanelData* panel = getPanelData();
    if(panel == nullptr) return false;

    const std::string parameterPath = container.getCustomGuiParameterPath(parameter);
    for(const auto& widget : panel->layout.widgets){
        if(widget.parameterRef.parameterPath == parameterPath) return false;
    }

    CustomGuiWidget widget;
    widget.parameterRef.parameterPath = parameterPath;
    widget.parameterRef.parameterDisplayName = parameter.getName();
    ofxOceanodeNode* node = container.getNodeFromParameter(parameter);
    if(node != nullptr){
        widget.parameterRef.nodeDisplayName = node->getParameters().getName();
        widget.color = node->getColor();
    } else {
        widget.color = ofColor::white;
    }
    widget.type = type;
    widget.label = parameter.getName();
    widget.spanW = type == CustomGuiWidgetType::Slider || type == CustomGuiWidgetType::MultiSlider || type == CustomGuiWidgetType::ToggleGrid ? 2 : 1;
    widget.spanH = type == CustomGuiWidgetType::ToggleGrid || type == CustomGuiWidgetType::XYPad ? 2 : 1;
    widget.config["interactive"] = ofxOceanodeCustomGuiWidgets::defaultInteractiveState(parameter);
    widget.config["showValue"] = true;

    if(type == CustomGuiWidgetType::ToggleGrid){
        int size = 1;
        if(parameter.valueType() == typeid(std::vector<int>).name()){
            size = std::max(1, (int)parameter.cast<std::vector<int>>().getParameter().get().size());
        }
        widget.config["rows"] = 1;
        widget.config["cols"] = size;
    }
    if(type == CustomGuiWidgetType::MultiSlider){
        if(parameter.valueType() == typeid(std::vector<float>).name()){
            int size = std::max(1, (int)parameter.cast<std::vector<float>>().getParameter().get().size());
            widget.config["visibleCount"] = size;
            widget.config["vertical"] = true;
            widget.spanW = 3;
            widget.spanH = 3;
        }
    }

    auto cell = findNextAvailableCell(panel->layout, widget.spanW, widget.spanH);
    widget.gridX = cell.first;
    widget.gridY = cell.second;
    panel->layout.widgets.push_back(widget);
    panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
    panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
    panel->windowState.isOpen = true;
    container.saveCustomGuis();
    return true;
}

std::pair<int, int> ofxOceanodeCustomGuiPanel::findNextAvailableCell(const CustomGuiLayout& layout, int spanW, int spanH) const
{
    auto occupied = [&](int x, int y){
        for(const auto& widget : layout.widgets){
            if(x >= widget.gridX && x < widget.gridX + widget.spanW &&
               y >= widget.gridY && y < widget.gridY + widget.spanH){
                return true;
            }
        }
        return false;
    };

    for(int y = 0; y < layout.rows; y++){
        for(int x = 0; x <= std::max(0, layout.columns - spanW); x++){
            bool free = true;
            for(int yy = y; yy < y + spanH && free; yy++){
                for(int xx = x; xx < x + spanW; xx++){
                    if(occupied(xx, yy)){
                        free = false;
                        break;
                    }
                }
            }
            if(free) return {x, y};
        }
    }
    return {0, layout.rows};
}

bool ofxOceanodeCustomGuiPanel::shouldShowNumericValue(const CustomGuiWidget& widget) const
{
    return widget.config.value("showValue", true);
}

std::shared_ptr<ofImage> ofxOceanodeCustomGuiPanel::loadWidgetImage(const std::string& imagePath) const
{
    if(imagePath.empty()) return nullptr;

    auto found = imageCache.find(imagePath);
    if(found != imageCache.end()) return found->second;

    auto image = std::make_shared<ofImage>();
    if(!image->load(imagePath)){
        imageCache[imagePath] = nullptr;
        return nullptr;
    }
    imageCache[imagePath] = image;
    return image;
}

#endif
