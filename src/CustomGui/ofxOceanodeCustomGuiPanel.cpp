
#define IMGUI_DEFINE_MATH_OPERATORS
#include "CustomGui/ofxOceanodeCustomGuiPanel.h"
#include "CustomGui/ofxOceanodeCustomGuiWidgetRegistry.h"
#include "CustomGui/ofxOceanodeCustomGuiWidgets.h"
#include "CustomGui/Widgets/ofxOceanodeCustomGuiWidgetHelpers.h"

#include "Managers/ofxOceanodeContainer.h"
#include "Nodes/ofxOceanodeNode.h"
#include "Nodes/ofxOceanodeNodeModel.h"
#include "ofxOceanodeParameter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
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

    static int draggedWidgetIndex = -1;
    static ImVec2 dragAnchorMouse = ImVec2(0, 0);
    static std::vector<int> draggedWidgetIndices;
    static std::vector<std::pair<int, int>> dragAnchorPositions;
    static std::string draggedWidgetPanelId;
    static int resizedWidgetIndex = -1;
    static ImVec2 resizeAnchorMouse = ImVec2(0, 0);
    static int resizeAnchorW = 1;
    static int resizeAnchorH = 1;
    static std::string resizedWidgetPanelId;
    static bool marqueeSelectionActive = false;
    static ImVec2 marqueeAnchorMouse = ImVec2(0, 0);
    static ImVec2 marqueeCurrentMouse = ImVec2(0, 0);
    static std::string marqueePanelId;

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

    auto isWidgetSelected = [&](int index){
        return std::find(selectedWidgetIndices.begin(), selectedWidgetIndices.end(), index) != selectedWidgetIndices.end();
    };

    auto sanitizeSelectedIndices = [&](){
        selectedWidgetIndices.erase(std::remove_if(selectedWidgetIndices.begin(), selectedWidgetIndices.end(),
                                                   [&](int index){
                                                       return index < 0 || index >= (int)panel->layout.widgets.size();
                                                   }),
                                    selectedWidgetIndices.end());
    };
    sanitizeSelectedIndices();

    std::string title = panel->name.empty() ? "Custom GUI" : panel->name;
    std::string canvasLabel = container.getCanvasID();
    if(!canvasLabel.empty() && canvasLabel != "Canvas"){
        ofStringReplace(canvasLabel, "_", " ");
        title += " (" + canvasLabel + ")";
    }
    if(panel->designMode) title += " [DESIGN]";
    title += "###CustomGui_" + container.getCanvasID() + "_" + panel->id;

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
        }else if(type == CustomGuiWidgetType::BackgroundPanel){
            widget.config["showLabel"] = true;
            widget.config["labelFontScale"] = 1.0f;
            widget.config["labelColor"] = customGuiColorToJson(ofColor::black);
        }else if(type == CustomGuiWidgetType::Line){
            widget.config["horizontal"] = true;
            widget.config["lineWeight"] = 2.0f;
        }else if(type == CustomGuiWidgetType::Image){
            widget.config["imagePath"] = "";
        }else if(type == CustomGuiWidgetType::SnapshotMatrix){
            widget.config["rows"] = 2;
            widget.config["cols"] = 4;
            widget.config["showSnapshotNames"] = true;
            widget.config["activeColor"] = customGuiColorToJson(ofColor(102, 0, 0, 255));
            widget.config["filledColor"] = customGuiColorToJson(ofColor(51, 102, 0, 255));
            widget.config["emptyColor"] = customGuiColorToJson(ofColor(70, 70, 70, 255));
        }
        auto cell = findNextAvailableCell(panel->layout, widget.spanW, widget.spanH);
        widget.gridX = cell.first;
        widget.gridY = cell.second;
        panel->layout.widgets.push_back(widget);
        panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
        panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
        container.markCustomGuisDirty();
    };

    if(panel->windowState.hasConfig && !appliedWindowState){
        ImGui::SetNextWindowPos(ImVec2(panel->windowState.posX, panel->windowState.posY), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(panel->windowState.width, panel->windowState.height), ImGuiCond_FirstUseEver);
        appliedWindowState = true;
    }

    const float panelWidth = panel->layout.columns * panel->layout.cellWidth * panel->layout.zoom;
    const float panelHeight = panel->layout.rows * panel->layout.cellHeight * panel->layout.zoom;
    ImGui::SetNextWindowContentSize(ImVec2(panelWidth, panelHeight));

    bool openState = panel->windowState.isOpen;
    const bool beginVisible = ImGui::Begin(title.c_str(), &openState, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar);
    if(openState != panel->windowState.isOpen){
        panel->windowState.isOpen = openState;
        container.markCustomGuisDirty();
    }

    if(!openState){
        if(draggedWidgetPanelId == panel->id){
            draggedWidgetIndex = -1;
            draggedWidgetIndices.clear();
            dragAnchorPositions.clear();
            draggedWidgetPanelId.clear();
        }
        if(resizedWidgetPanelId == panel->id){
            resizedWidgetIndex = -1;
            resizedWidgetPanelId.clear();
        }
        requestOpenSetValuePopup = false;
        requestOpenRenameSnapshotPopup = false;
        requestOpenDeleteSnapshotPopup = false;
        requestOpenDeletePanelPopup = false;
        createSnapshotFromPopup = false;
        setValueParameterPath.clear();
        setValueLabel.clear();
        setValueVectorValues.clear();
        snapshotRenameId.clear();
        snapshotRenameValue.clear();
        snapshotDeleteId.clear();
        ImGui::ClosePopupsOverWindow(ImGui::GetCurrentWindow(), false);
        ImGui::End();
        return;
    }

    if(beginVisible){
        auto* snapshotBank = container.getCustomGuiSnapshotBank(panel->id);
        const bool canSnapshot = container.customGuiPanelHasSnapshotEligibleParameters(panel->id);
        auto getSelectedSnapshot = [&]() -> CustomGuiSnapshotData* {
            if(snapshotBank == nullptr) return nullptr;
            for(auto& snapshot : snapshotBank->snapshots){
                if(snapshot.id == snapshotBank->currentSnapshotId) return &snapshot;
            }
            if(!snapshotBank->snapshots.empty()){
                snapshotBank->currentSnapshotId = snapshotBank->snapshots.front().id;
                return &snapshotBank->snapshots.front();
            }
            return nullptr;
        };
        auto* selectedSnapshot = getSelectedSnapshot();

        if(ImGui::SmallButton(panel->designMode ? "Run" : "Edit")){
            panel->designMode = !panel->designMode;
            container.markCustomGuisDirty();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        const char* snapshotPreview = selectedSnapshot != nullptr ? selectedSnapshot->name.c_str() : "Snapshots";
        if(ImGui::BeginCombo("##CustomGuiSnapshots", snapshotPreview)){
            if(snapshotBank == nullptr || snapshotBank->snapshots.empty()){
                ImGui::TextDisabled("No snapshots");
            }else{
                for(const auto& snapshot : snapshotBank->snapshots){
                    const bool isSelected = snapshot.id == snapshotBank->currentSnapshotId;
                    if(ImGui::Selectable(snapshot.name.c_str(), isSelected)){
                        container.recallCustomGuiSnapshot(panel->id, snapshot.id);
                        selectedSnapshot = getSelectedSnapshot();
                    }
                    if(isSelected) ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0, 6.0f);
        if(!canSnapshot) ImGui::BeginDisabled();
        if(ImGui::SmallButton("+##CreateSnapshot") && canSnapshot){
            createSnapshotFromPopup = true;
            snapshotRenameId.clear();
            snapshotRenameValue = "Snapshot";
            requestOpenRenameSnapshotPopup = true;
        }
        if(!canSnapshot) ImGui::EndDisabled();
        ImGui::SameLine(0, 6.0f);
        if(selectedSnapshot == nullptr) ImGui::BeginDisabled();
        if(ImGui::SmallButton("Replace") && selectedSnapshot != nullptr){
            container.updateCustomGuiSnapshot(panel->id, selectedSnapshot->id);
            snapshotBank = container.getCustomGuiSnapshotBank(panel->id);
            selectedSnapshot = getSelectedSnapshot();
        }
        ImGui::SameLine(0, 6.0f);
        if(ImGui::SmallButton("Rename") && selectedSnapshot != nullptr){
            snapshotRenameId = selectedSnapshot->id;
            snapshotRenameValue = selectedSnapshot->name;
            requestOpenRenameSnapshotPopup = true;
        }
        ImGui::SameLine(0, 6.0f);
        if(ImGui::SmallButton("-##DeleteSnapshot") && selectedSnapshot != nullptr){
            snapshotDeleteId = selectedSnapshot->id;
            requestOpenDeleteSnapshotPopup = true;
        }
        if(selectedSnapshot == nullptr) ImGui::EndDisabled();
        ImGui::SameLine(0, 12.0f);
        if(ImGui::SmallButton("-##ZoomOut")){
            panel->layout.zoom = ofClamp(panel->layout.zoom - 0.1f, 0.25f, 4.0f);
            container.markCustomGuisDirty();
        }
        ImGui::SameLine(0, 6.0f);
        if(ImGui::SmallButton("+##ZoomIn")){
            panel->layout.zoom = ofClamp(panel->layout.zoom + 0.1f, 0.25f, 4.0f);
            container.markCustomGuisDirty();
        }
        ImGui::SameLine(0, 6.0f);
        ImGui::Text("Zoom %.2fx", panel->layout.zoom);
        if(panel->designMode){
            ImGui::NewLine();
            char nameBuffer[256];
            std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", panel->name.c_str());
            ImGui::SetNextItemWidth(220);
            if(ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))){
                container.renameCustomGuiPanel(panel->id, nameBuffer);
            }

            ImGui::SameLine(0, 12.0f);
            if(ImGui::SmallButton("Delete GUI")){
                requestOpenDeletePanelPopup = true;
            }

            ImGui::SameLine(0, 18.0f);
            ImGui::SetNextItemWidth(70);
            if(ImGui::InputInt("Cols", &panel->layout.columns)){
                panel->layout.columns = std::max(1, panel->layout.columns);
                ensureLayoutFitsWidgets(panel->layout);
                container.markCustomGuisDirty();
            }

            ImGui::SameLine(0, 10.0f);
            ImGui::SetNextItemWidth(70);
            if(ImGui::InputInt("Rows", &panel->layout.rows)){
                panel->layout.rows = std::max(1, panel->layout.rows);
                ensureLayoutFitsWidgets(panel->layout);
                container.markCustomGuisDirty();
            }

            ImGui::SameLine(0, 18.0f);
            ImGui::SetNextItemWidth(70);
            if(ImGui::InputFloat("Cell W", &panel->layout.cellWidth, 1.0f, 10.0f, "%.0f")){
                panel->layout.cellWidth = std::max(20.0f, panel->layout.cellWidth);
                container.markCustomGuisDirty();
            }

            ImGui::SameLine(0, 10.0f);
            ImGui::SetNextItemWidth(70);
            if(ImGui::InputFloat("Cell H", &panel->layout.cellHeight, 1.0f, 10.0f, "%.0f")){
                panel->layout.cellHeight = std::max(20.0f, panel->layout.cellHeight);
                container.markCustomGuisDirty();
            }

            ImGui::SameLine(0, 18.0f);
            float backgroundColor[4] = {
                panel->layout.backgroundColor.r / 255.0f,
                panel->layout.backgroundColor.g / 255.0f,
                panel->layout.backgroundColor.b / 255.0f,
                panel->layout.backgroundColor.a / 255.0f
            };
            if(ImGui::ColorEdit4("BG", backgroundColor, ImGuiColorEditFlags_NoInputs)){
                panel->layout.backgroundColor = ofColor(backgroundColor[0] * 255.0f,
                                                        backgroundColor[1] * 255.0f,
                                                        backgroundColor[2] * 255.0f,
                                                        backgroundColor[3] * 255.0f);
                container.markCustomGuisDirty();
            }

            ImGui::NewLine();
            if(ImGui::SmallButton("Add Panel")){
                createStaticWidget(CustomGuiWidgetType::BackgroundPanel, "", 3, 2, ofColor(40, 40, 40, 180));
            }
            ImGui::SameLine(0, 8.0f);
            if(ImGui::SmallButton("Add Text")){
                createStaticWidget(CustomGuiWidgetType::Text, "Text", 2, 1, ofColor::white);
            }
            ImGui::SameLine(0, 8.0f);
            if(ImGui::SmallButton("Add Line")){
                createStaticWidget(CustomGuiWidgetType::Line, "", 2, 1, ofColor(180, 180, 180, 255));
            }
            ImGui::SameLine(0, 8.0f);
            if(ImGui::SmallButton("Add Image")){
                createStaticWidget(CustomGuiWidgetType::Image, "", 3, 2, ofColor::white);
            }
            ImGui::SameLine(0, 8.0f);
            if(ImGui::SmallButton("Add Snapshot Matrix")){
                createStaticWidget(CustomGuiWidgetType::SnapshotMatrix, "Snapshots", 4, 2, ofColor::white);
            }
            ImGui::SameLine(0, 14.0f);
            if(ImGui::SmallButton("Custom Regions")){
                ImGui::OpenPopup("Custom Regions Popup");
            }
            if(ImGui::BeginPopup("Custom Regions Popup")){
                struct AvailableCustomRegion {
                    ofxOceanodeAbstractParameter* parameter = nullptr;
                    std::string label;
                };

                std::vector<AvailableCustomRegion> availableRegions;
                for(auto* node : container.getAllModules()){
                    if(node == nullptr) continue;
                    for(int paramIndex = 0; paramIndex < node->getParameters().size(); paramIndex++){
                        auto& absParam = static_cast<ofxOceanodeAbstractParameter&>(node->getParameters().get(paramIndex));
                        if(absParam.valueType() != typeid(std::function<void()>).name()) continue;
                        if(absParam.getName().find("SEPARATOR:|") == 0) continue;
                        if(!ofxOceanodeCustomGuiWidgetHelpers::isRegisteredCustomRegionParameter(absParam)) continue;

                        auto customTypes = container.getCompatibleCustomGuiWidgetTypes(absParam);
                        if(std::find(customTypes.begin(), customTypes.end(), CustomGuiWidgetType::CustomRegion) == customTypes.end()) continue;

                        availableRegions.push_back({&absParam, node->getParameters().getName() + " / " + absParam.getName()});
                    }
                }

                std::sort(availableRegions.begin(), availableRegions.end(), [](const AvailableCustomRegion& a, const AvailableCustomRegion& b){
                    return a.label < b.label;
                });

                if(availableRegions.empty()){
                    ImGui::TextDisabled("No custom regions");
                }else{
                    for(const auto& region : availableRegions){
                        if(region.parameter == nullptr) continue;
                        const bool alreadyAdded = containsParameter(*region.parameter);
                        const std::string parameterPath = container.getCustomGuiParameterPath(*region.parameter);
                        ImGui::PushID(parameterPath.c_str());
                        if(ImGui::MenuItem(region.label.c_str(), nullptr, false, !alreadyAdded)){
                            if(addParameter(*region.parameter, CustomGuiWidgetType::CustomRegion)){
                                ImGui::CloseCurrentPopup();
                            }
                        }
                        ImGui::PopID();
                    }
                }

                ImGui::EndPopup();
            }
        }

        const ImVec2 origin = ImGui::GetCursorPos();
        const ImVec2 originScreen = ImGui::GetCursorScreenPos();

        auto getWidgetRect = [&](const CustomGuiWidget& widget){
            const float x = widget.gridX * panel->layout.cellWidth * panel->layout.zoom;
            const float y = widget.gridY * panel->layout.cellHeight * panel->layout.zoom;
            const float w = widget.spanW * panel->layout.cellWidth * panel->layout.zoom;
            const float h = widget.spanH * panel->layout.cellHeight * panel->layout.zoom;
            return std::pair<ImVec2, ImVec2>(ImVec2(originScreen.x + x, originScreen.y + y),
                                             ImVec2(originScreen.x + x + w, originScreen.y + y + h));
        };

        auto widgetDrawsBehind = [&](const CustomGuiWidget& widget){
            return widget.type == CustomGuiWidgetType::BackgroundPanel ||
                   (widget.type == CustomGuiWidgetType::Image && widget.config.value("sendToBack", false));
        };

        auto rectsIntersect = [&](const ImVec2& minA, const ImVec2& maxA, const ImVec2& minB, const ImVec2& maxB){
            return minA.x < maxB.x && maxA.x > minB.x &&
                   minA.y < maxB.y && maxA.y > minB.y;
        };

        auto rectContainsRect = [&](const ImVec2& outerMin, const ImVec2& outerMax, const ImVec2& innerMin, const ImVec2& innerMax){
            return innerMin.x >= outerMin.x && innerMax.x <= outerMax.x &&
                   innerMin.y >= outerMin.y && innerMax.y <= outerMax.y;
        };

        auto findTopmostWidgetAt = [&](const ImVec2& mousePos){
            (void)mousePos;
            int topIndex = -1;
            auto considerPass = [&](bool backPass){
                for(size_t index = 0; index < panel->layout.widgets.size(); index++){
                    const auto& widget = panel->layout.widgets[index];
                    if(widgetDrawsBehind(widget) != backPass) continue;
                    const auto rect = getWidgetRect(widget);
                    if(ImGui::IsMouseHoveringRect(rect.first, rect.second)){
                        topIndex = (int)index;
                    }
                }
            };
            considerPass(true);
            considerPass(false);
            return topIndex;
        };

        auto findTopmostResizeHandleAt = [&](const ImVec2& mousePos){
            (void)mousePos;
            int topIndex = -1;
            auto considerPass = [&](bool backPass){
                for(size_t index = 0; index < panel->layout.widgets.size(); index++){
                    const auto& widget = panel->layout.widgets[index];
                    if(widgetDrawsBehind(widget) != backPass) continue;
                    if(widget.config.value("locked", false)) continue;
                    const auto rect = getWidgetRect(widget);
                    const ImVec2 handleMin(rect.second.x - 12.0f, rect.second.y - 12.0f);
                    if(ImGui::IsMouseHoveringRect(handleMin, rect.second)){
                        topIndex = (int)index;
                    }
                }
            };
            considerPass(true);
            considerPass(false);
            return topIndex;
        };

        auto findBackgroundPanelAt = [&](const ImVec2& mousePos){
            int panelIndex = -1;
            for(size_t index = 0; index < panel->layout.widgets.size(); index++){
                const auto& widget = panel->layout.widgets[index];
                if(widget.type != CustomGuiWidgetType::BackgroundPanel) continue;
                const auto rect = getWidgetRect(widget);
                if(ImGui::IsMouseHoveringRect(rect.first, rect.second)){
                    panelIndex = (int)index;
                }
            }
            return panelIndex;
        };

        ImDrawList* panelDrawList = ImGui::GetWindowDrawList();
        panelDrawList->AddRectFilled(originScreen,
                                     ImVec2(originScreen.x + panelWidth, originScreen.y + panelHeight),
                                     IM_COL32(panel->layout.backgroundColor.r,
                                              panel->layout.backgroundColor.g,
                                              panel->layout.backgroundColor.b,
                                              panel->layout.backgroundColor.a));

        if(panel->designMode){
            ImGui::InvisibleButton("##CustomGuiGridSpace", ImVec2(panelWidth, panelHeight));
            ImGui::SetNextItemAllowOverlap();
        }else{
            ImGui::Dummy(ImVec2(panelWidth, panelHeight));
        }

        const int topmostHoveredWidgetIndex = panel->designMode ? findTopmostWidgetAt(ImGui::GetIO().MousePos) : -1;
        const int topmostResizeHandleIndex = panel->designMode ? findTopmostResizeHandleAt(ImGui::GetIO().MousePos) : -1;
        const int hoveredBackgroundPanelIndex = panel->designMode ? findBackgroundPanelAt(ImGui::GetIO().MousePos) : -1;
        const bool gridHovered = panel->designMode && ImGui::IsItemHovered();
        const bool foregroundWidgetHovered =
            topmostHoveredWidgetIndex >= 0 &&
            panel->layout.widgets[topmostHoveredWidgetIndex].type != CustomGuiWidgetType::BackgroundPanel;
        const bool propertiesPopupOpen = panel->designMode && ImGui::IsPopupOpen("Widget Properties");
        const bool ctrlPressed = ImGui::GetIO().KeyCtrl;

        if(propertiesPopupOpen){
            if(draggedWidgetPanelId == panel->id){
                draggedWidgetIndex = -1;
                draggedWidgetIndices.clear();
                dragAnchorPositions.clear();
                draggedWidgetPanelId.clear();
            }
            if(resizedWidgetPanelId == panel->id){
                resizedWidgetIndex = -1;
                resizedWidgetPanelId.clear();
            }
            if(marqueeSelectionActive && marqueePanelId == panel->id){
                marqueeSelectionActive = false;
                marqueePanelId.clear();
            }
        }

        if(panel->designMode && !propertiesPopupOpen && ctrlPressed &&
           !marqueeSelectionActive && gridHovered && !foregroundWidgetHovered &&
           ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
            marqueeSelectionActive = true;
            marqueePanelId = panel->id;
            marqueeAnchorMouse = ImGui::GetIO().MousePos;
            marqueeCurrentMouse = marqueeAnchorMouse;
            draggedWidgetIndex = -1;
            draggedWidgetIndices.clear();
            dragAnchorPositions.clear();
            draggedWidgetPanelId.clear();
            resizedWidgetIndex = -1;
            resizedWidgetPanelId.clear();
        }else if(panel->designMode && !propertiesPopupOpen && !ctrlPressed &&
                 !marqueeSelectionActive && gridHovered &&
                 topmostHoveredWidgetIndex < 0 &&
                 ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
            selectedWidgetIndices.clear();
        }
        if(panel->designMode) drawGridOverlay(panel->layout, origin);

        int widgetToRemove = -1;
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
                                  widget.type == CustomGuiWidgetType::Line ||
                                  widget.type == CustomGuiWidgetType::Image ||
                                  widget.type == CustomGuiWidgetType::SnapshotMatrix);
            ofxOceanodeAbstractParameter* parameter = hasParameter ? findParameter(widget) : nullptr;
            renderWidget(widget, parameter, ImVec2(w, h));

            if(!panel->designMode){
                const std::string hint = widget.config.value("hint", std::string());
                if(!hint.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)){
                    ImGui::SetTooltip("%s", hint.c_str());
                }
            }

            if(panel->designMode){
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const auto widgetRect = getWidgetRect(widget);
                const ImVec2 min = widgetRect.first;
                const ImVec2 max = widgetRect.second;
                const bool selected = isWidgetSelected((int)i);
                const bool locked = widget.config.value("locked", false);
                drawList->AddRect(min, max,
                                  locked ? IM_COL32(120, 180, 255, 220) :
                                  (selected ? IM_COL32(255, 255, 255, 220) : IM_COL32(255, 180, 40, 180)),
                                  2.0f, 0, selected ? 2.0f : 1.5f);

                bool hovered = topmostHoveredWidgetIndex == (int)i && ImGui::IsMouseHoveringRect(min, max);
                ImVec2 handleMin(max.x - 12.0f, max.y - 12.0f);
                bool resizeHovered = !locked &&
                                     topmostResizeHandleIndex == (int)i &&
                                     ImGui::IsMouseHoveringRect(handleMin, max);
                if(!locked) drawList->AddRectFilled(handleMin, max, IM_COL32(255, 180, 40, 220), 1.0f);
                if(!propertiesPopupOpen && (hovered || resizeHovered) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
                    const bool shiftPressed = ImGui::GetIO().KeyShift;
                    if(ctrlPressed){
                        marqueeSelectionActive = true;
                        marqueePanelId = panel->id;
                        marqueeAnchorMouse = ImGui::GetIO().MousePos;
                        marqueeCurrentMouse = marqueeAnchorMouse;
                    }else if(!shiftPressed && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                             hoveredBackgroundPanelIndex >= 0){
                        const auto panelRect = getWidgetRect(panel->layout.widgets[hoveredBackgroundPanelIndex]);
                        selectedWidgetIndices.clear();
                        selectedWidgetIndices.push_back(hoveredBackgroundPanelIndex);
                        for(size_t otherIndex = 0; otherIndex < panel->layout.widgets.size(); otherIndex++){
                            if((int)otherIndex == hoveredBackgroundPanelIndex) continue;
                            const auto& otherWidget = panel->layout.widgets[otherIndex];
                            if(otherWidget.type == CustomGuiWidgetType::BackgroundPanel) continue;
                            const auto otherRect = getWidgetRect(otherWidget);
                            if(rectsIntersect(panelRect.first, panelRect.second, otherRect.first, otherRect.second)){
                                selectedWidgetIndices.push_back((int)otherIndex);
                            }
                        }
                    }else if(shiftPressed){
                        auto selectedIt = std::find(selectedWidgetIndices.begin(), selectedWidgetIndices.end(), (int)i);
                        if(selectedIt == selectedWidgetIndices.end()) selectedWidgetIndices.push_back((int)i);
                        else selectedWidgetIndices.erase(selectedIt);
                    }else if(!selected){
                        selectedWidgetIndices = {(int)i};
                    }

                    if(ctrlPressed){
                    }else if(!shiftPressed && resizeHovered){
                        resizedWidgetIndex = (int)i;
                        resizedWidgetPanelId = panel->id;
                        resizeAnchorMouse = ImGui::GetIO().MousePos;
                        resizeAnchorW = widget.spanW;
                        resizeAnchorH = widget.spanH;
                    }else if(!shiftPressed && !locked){
                        draggedWidgetIndex = (int)i;
                        draggedWidgetPanelId = panel->id;
                        dragAnchorMouse = ImGui::GetIO().MousePos;
                        draggedWidgetIndices = selectedWidgetIndices;
                        if(draggedWidgetIndices.empty()) draggedWidgetIndices.push_back((int)i);
                        draggedWidgetIndices.erase(std::remove_if(draggedWidgetIndices.begin(), draggedWidgetIndices.end(),
                                                                  [&](int selectedIndex){
                                                                      return selectedIndex < 0 ||
                                                                             selectedIndex >= (int)panel->layout.widgets.size() ||
                                                                             panel->layout.widgets[selectedIndex].config.value("locked", false);
                                                                  }),
                                                 draggedWidgetIndices.end());
                        if(draggedWidgetIndices.empty()) draggedWidgetIndices.push_back((int)i);
                        dragAnchorPositions.clear();
                        dragAnchorPositions.reserve(draggedWidgetIndices.size());
                        for(int selectedIndex : draggedWidgetIndices){
                            const auto& selectedWidget = panel->layout.widgets[selectedIndex];
                            dragAnchorPositions.emplace_back(selectedWidget.gridX, selectedWidget.gridY);
                        }
                    }
                }
                if(!propertiesPopupOpen && draggedWidgetPanelId == panel->id && ImGui::IsMouseDown(ImGuiMouseButton_Left)){
                    ImVec2 delta = ImGui::GetIO().MousePos - dragAnchorMouse;
                    int offsetX = (int)std::round(delta.x / (panel->layout.cellWidth * panel->layout.zoom));
                    int offsetY = (int)std::round(delta.y / (panel->layout.cellHeight * panel->layout.zoom));
                    auto draggedIt = std::find(draggedWidgetIndices.begin(), draggedWidgetIndices.end(), (int)i);
                    if(draggedIt != draggedWidgetIndices.end()){
                        const size_t draggedOffset = std::distance(draggedWidgetIndices.begin(), draggedIt);
                        if(draggedOffset < dragAnchorPositions.size()){
                            widget.gridX = std::max(0, dragAnchorPositions[draggedOffset].first + offsetX);
                            widget.gridY = std::max(0, dragAnchorPositions[draggedOffset].second + offsetY);
                            panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
                            panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
                        }
                    }
                }
                if(draggedWidgetPanelId == panel->id && ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
                    ensureLayoutFitsWidgets(panel->layout);
                    draggedWidgetIndex = -1;
                    draggedWidgetIndices.clear();
                    dragAnchorPositions.clear();
                    draggedWidgetPanelId.clear();
                    container.markCustomGuisDirty();
                }
                if(!propertiesPopupOpen && resizedWidgetPanelId == panel->id && resizedWidgetIndex == (int)i && ImGui::IsMouseDown(ImGuiMouseButton_Left)){
                    ImVec2 delta = ImGui::GetIO().MousePos - resizeAnchorMouse;
                    int offsetW = (int)std::round(delta.x / (panel->layout.cellWidth * panel->layout.zoom));
                    int offsetH = (int)std::round(delta.y / (panel->layout.cellHeight * panel->layout.zoom));
                    widget.spanW = std::max(1, resizeAnchorW + offsetW);
                    widget.spanH = std::max(1, resizeAnchorH + offsetH);
                    panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
                    panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
                }
                if(resizedWidgetPanelId == panel->id && resizedWidgetIndex == (int)i && ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
                    ensureLayoutFitsWidgets(panel->layout);
                    resizedWidgetIndex = -1;
                    resizedWidgetPanelId.clear();
                    container.markCustomGuisDirty();
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
                    const std::string parameterType = parameter->valueType();
                    const bool canSetValue =
                        parameterType == typeid(float).name() ||
                        parameterType == typeid(int).name() ||
                        parameterType == typeid(std::vector<float>).name() ||
                        parameterType == typeid(std::vector<int>).name();
#ifdef OFXOCEANODE_USE_MIDI
                    if(canSetValue){
                        if(ImGui::Selectable("Set Value")){
                            openSetValuePopup(*parameter, getFallbackLabel(widget));
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::Separator();
                    }
#else
                    if(canSetValue && ImGui::Selectable("Set Value")){
                        openSetValuePopup(*parameter, getFallbackLabel(widget));
                        ImGui::CloseCurrentPopup();
                    }
#endif
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
            const auto& widget = panel->layout.widgets[i];
            if(widget.type == CustomGuiWidgetType::BackgroundPanel ||
               (widget.type == CustomGuiWidgetType::Image && widget.config.value("sendToBack", false))){
                drawWidgetAtIndex(i);
            }
        }
        for(size_t i = 0; i < panel->layout.widgets.size(); i++){
            const auto& widget = panel->layout.widgets[i];
            if(widget.type != CustomGuiWidgetType::BackgroundPanel &&
               !(widget.type == CustomGuiWidgetType::Image && widget.config.value("sendToBack", false))){
                drawWidgetAtIndex(i);
            }
        }

        if(panel->designMode && marqueeSelectionActive && marqueePanelId == panel->id){
            marqueeCurrentMouse = ImGui::GetIO().MousePos;
            const ImVec2 selectionMin(std::min(marqueeAnchorMouse.x, marqueeCurrentMouse.x),
                                      std::min(marqueeAnchorMouse.y, marqueeCurrentMouse.y));
            const ImVec2 selectionMax(std::max(marqueeAnchorMouse.x, marqueeCurrentMouse.x),
                                      std::max(marqueeAnchorMouse.y, marqueeCurrentMouse.y));
            panelDrawList->AddRectFilled(selectionMin, selectionMax, IM_COL32(90, 150, 255, 35));
            panelDrawList->AddRect(selectionMin, selectionMax, IM_COL32(90, 150, 255, 220), 0.0f, 0, 1.5f);

            if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
                selectedWidgetIndices.clear();
                for(size_t i = 0; i < panel->layout.widgets.size(); i++){
                    const auto rect = getWidgetRect(panel->layout.widgets[i]);
                    if(rectContainsRect(selectionMin, selectionMax, rect.first, rect.second)){
                        selectedWidgetIndices.push_back((int)i);
                    }
                }
                marqueeSelectionActive = false;
                marqueePanelId.clear();
            }
        }

        if(widgetToRemove >= 0 && widgetToRemove < (int)panel->layout.widgets.size()){
            if(const CustomGuiWidgetDefinition* definition = ofxOceanodeCustomGuiWidgetRegistry::instance().getWidget(panel->layout.widgets[widgetToRemove].type)){
                if(definition->cleanup) definition->cleanup(panel->id, panel->layout.widgets[widgetToRemove].parameterRef.parameterPath);
            }
            panel->layout.widgets.erase(panel->layout.widgets.begin() + widgetToRemove);
            selectedWidgetIndices.erase(std::remove(selectedWidgetIndices.begin(), selectedWidgetIndices.end(), widgetToRemove), selectedWidgetIndices.end());
            for(int& selectedIndex : selectedWidgetIndices){
                if(selectedIndex > widgetToRemove) selectedIndex--;
            }
            ensureLayoutFitsWidgets(panel->layout);
            container.markCustomGuisDirty();
        }

        drawSetValuePopup();

        if(requestOpenRenameSnapshotPopup){
            ImGui::OpenPopup(createSnapshotFromPopup ? "New Snapshot" : "Rename Snapshot");
            requestOpenRenameSnapshotPopup = false;
        }
        if(ImGui::BeginPopupModal("New Snapshot", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
            char snapshotNameBuffer[256];
            std::snprintf(snapshotNameBuffer, sizeof(snapshotNameBuffer), "%s", snapshotRenameValue.c_str());
            ImGui::SetNextItemWidth(240.0f);
            if(ImGui::InputText("Name", snapshotNameBuffer, sizeof(snapshotNameBuffer))){
                snapshotRenameValue = snapshotNameBuffer;
            }
            if(ImGui::Button("Create")){
                const std::string createdSnapshotId = container.createCustomGuiSnapshot(panel->id, snapshotRenameValue);
                if(!createdSnapshotId.empty()){
                    container.recallCustomGuiSnapshot(panel->id, createdSnapshotId);
                    snapshotBank = container.getCustomGuiSnapshotBank(panel->id);
                    selectedSnapshot = getSelectedSnapshot();
                }
                createSnapshotFromPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel")){
                createSnapshotFromPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if(ImGui::BeginPopupModal("Rename Snapshot", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
            char snapshotNameBuffer[256];
            std::snprintf(snapshotNameBuffer, sizeof(snapshotNameBuffer), "%s", snapshotRenameValue.c_str());
            ImGui::SetNextItemWidth(240.0f);
            if(ImGui::InputText("Name", snapshotNameBuffer, sizeof(snapshotNameBuffer))){
                snapshotRenameValue = snapshotNameBuffer;
            }
            if(ImGui::Button("Apply")){
                container.renameCustomGuiSnapshot(panel->id, snapshotRenameId, snapshotRenameValue);
                snapshotBank = container.getCustomGuiSnapshotBank(panel->id);
                selectedSnapshot = getSelectedSnapshot();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if(requestOpenDeleteSnapshotPopup){
            ImGui::OpenPopup("Delete Snapshot?");
            requestOpenDeleteSnapshotPopup = false;
        }
        if(ImGui::BeginPopupModal("Delete Snapshot?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
            const CustomGuiSnapshotData* snapshotToDelete = nullptr;
            if(snapshotBank != nullptr){
                for(const auto& snapshot : snapshotBank->snapshots){
                    if(snapshot.id == snapshotDeleteId){
                        snapshotToDelete = &snapshot;
                        break;
                    }
                }
            }
            if(snapshotToDelete != nullptr){
                ImGui::TextWrapped("Delete snapshot \"%s\"?", snapshotToDelete->name.c_str());
            }else{
                ImGui::TextDisabled("Snapshot unavailable");
            }

            if(ImGui::Button("Delete")){
                container.deleteCustomGuiSnapshot(panel->id, snapshotDeleteId);
                snapshotBank = container.getCustomGuiSnapshotBank(panel->id);
                selectedSnapshot = getSelectedSnapshot();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if(requestOpenDeletePanelPopup){
            ImGui::OpenPopup("Delete Custom GUI?");
            requestOpenDeletePanelPopup = false;
        }
        if(ImGui::BeginPopupModal("Delete Custom GUI?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextWrapped("Delete custom GUI \"%s\"?", panel->name.c_str());
            ImGui::TextDisabled("This removes the GUI layout and its snapshots from this canvas.");
            if(ImGui::Button("Delete", ImVec2(120, 0))){
                container.requestDeleteCustomGuiPanel(panel->id);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel", ImVec2(120, 0))){
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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
            container.markCustomGuisDirty();
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

void ofxOceanodeCustomGuiPanel::openSetValuePopup(ofxOceanodeAbstractParameter& parameter, const std::string& label)
{
    setValueParameterPath = container.getCustomGuiParameterPath(parameter);
    setValueLabel = label;
    setValueVectorValues.clear();

    const std::string type = parameter.valueType();
    if(type == typeid(float).name()){
        setValueScalar = parameter.cast<float>().getParameter().get();
    }else if(type == typeid(int).name()){
        setValueScalar = parameter.cast<int>().getParameter().get();
    }else if(type == typeid(std::vector<float>).name()){
        auto values = parameter.cast<std::vector<float>>().getParameter().get();
        setValueVectorValues.reserve(values.size());
        for(float value : values) setValueVectorValues.push_back(value);
    }else if(type == typeid(std::vector<int>).name()){
        auto values = parameter.cast<std::vector<int>>().getParameter().get();
        setValueVectorValues.reserve(values.size());
        for(int value : values) setValueVectorValues.push_back(value);
    }

    requestOpenSetValuePopup = true;
}

void ofxOceanodeCustomGuiPanel::drawSetValuePopup()
{
    if(requestOpenSetValuePopup){
        ImGui::OpenPopup("Set Value");
        requestOpenSetValuePopup = false;
    }

    if(!ImGui::BeginPopupModal("Set Value", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ofxOceanodeAbstractParameter* parameter = setValueParameterPath.empty() ? nullptr : container.findCustomGuiParameter(setValueParameterPath);
    if(parameter == nullptr){
        ImGui::TextDisabled("Parameter unavailable");
        if(ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    ImGui::TextWrapped("%s", setValueLabel.c_str());
    ImGui::Spacing();

    const std::string type = parameter->valueType();
    bool valueEdited = false;

    if(type == typeid(float).name()){
        float value = (float)setValueScalar;
        if(ImGui::InputFloat("Value", &value)){
            setValueScalar = value;
        }
        if(ImGui::Button("Apply")){
            parameter->cast<float>().getParameter().set((float)setValueScalar);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    }else if(type == typeid(int).name()){
        int value = (int)std::round(setValueScalar);
        if(ImGui::InputInt("Value", &value)){
            setValueScalar = value;
        }
        if(ImGui::Button("Apply")){
            parameter->cast<int>().getParameter().set((int)std::round(setValueScalar));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    }else if(type == typeid(std::vector<float>).name()){
        auto currentValues = parameter->cast<std::vector<float>>().getParameter().get();
        if(setValueVectorValues.size() != currentValues.size()){
            setValueVectorValues.assign(currentValues.begin(), currentValues.end());
        }
        for(size_t i = 0; i < setValueVectorValues.size(); i++){
            float value = (float)setValueVectorValues[i];
            ImGui::PushID((int)i);
            if(ImGui::InputFloat("##value", &value)){
                setValueVectorValues[i] = value;
                valueEdited = true;
            }
            ImGui::SameLine();
            ImGui::Text("Value %zu", i + 1);
            ImGui::PopID();
        }
        if(ImGui::Button("Apply")){
            std::vector<float> values(setValueVectorValues.size(), 0.0f);
            for(size_t i = 0; i < setValueVectorValues.size(); i++) values[i] = (float)setValueVectorValues[i];
            parameter->cast<std::vector<float>>().getParameter().set(values);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    }else if(type == typeid(std::vector<int>).name()){
        auto currentValues = parameter->cast<std::vector<int>>().getParameter().get();
        if(setValueVectorValues.size() != currentValues.size()){
            setValueVectorValues.resize(currentValues.size(), 0.0);
            for(size_t i = 0; i < currentValues.size(); i++) setValueVectorValues[i] = currentValues[i];
        }
        for(size_t i = 0; i < setValueVectorValues.size(); i++){
            int value = (int)std::round(setValueVectorValues[i]);
            ImGui::PushID((int)i);
            if(ImGui::InputInt("##value", &value)){
                setValueVectorValues[i] = value;
                valueEdited = true;
            }
            ImGui::SameLine();
            ImGui::Text("Value %zu", i + 1);
            ImGui::PopID();
        }
        if(ImGui::Button("Apply")){
            std::vector<int> values(setValueVectorValues.size(), 0);
            for(size_t i = 0; i < setValueVectorValues.size(); i++) values[i] = (int)std::round(setValueVectorValues[i]);
            parameter->cast<std::vector<int>>().getParameter().set(values);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    }else{
        ImGui::TextDisabled("This widget does not support manual value entry");
        if(ImGui::Button("Close")) ImGui::CloseCurrentPopup();
    }

    if(valueEdited) ImGui::Spacing();
    ImGui::EndPopup();
}

bool ofxOceanodeCustomGuiPanel::renderWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter, const ImVec2& size)
{
    const CustomGuiWidgetDefinition* definition = ofxOceanodeCustomGuiWidgetRegistry::instance().getWidget(widget.type);
    if(definition == nullptr || !definition->render){
        ImGui::TextDisabled("Unsupported widget");
        return false;
    }

    CustomGuiWidgetRenderContext context {
        container,
        panelId,
        &widget,
        getPanelData() != nullptr ? getPanelData()->designMode : false,
        getPanelData() != nullptr ? getPanelData()->layout.zoom : 1.0f,
        size,
        getFallbackLabel(widget),
        shouldShowNumericValue(widget),
        ofxOceanodeCustomGuiWidgets::isInteractive(widget, parameter),
        [this](CustomGuiWidget& widgetRef, ofxOceanodeAbstractParameter* parameterRef, std::vector<float>& valueRef, const ImVec2& sizeRef, bool interactiveRef){
            return drawMultiSliderWidget(widgetRef, parameterRef, valueRef, sizeRef, interactiveRef);
        },
        [this](const ImVec2& sizeRef, float normalizedRef, const ImU32& colorRef){
            drawVerticalMeter(sizeRef, normalizedRef, colorRef);
        },
        [this](const std::string& imagePath){
            return loadWidgetImage(imagePath);
        }
    };

    ImGui::SetWindowFontScale(std::max(0.5f, context.zoom));
    const bool rendered = definition->render(context, widget, parameter);
    ImGui::SetWindowFontScale(1.0f);
    return rendered;
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
            container.markCustomGuisDirty();
        }
        char hintBuffer[512];
        const std::string currentHint = widget.config.value("hint", std::string());
        std::snprintf(hintBuffer, sizeof(hintBuffer), "%s", currentHint.c_str());
        if(ImGui::InputText("Hint", hintBuffer, sizeof(hintBuffer))){
            widget.config["hint"] = std::string(hintBuffer);
            container.markCustomGuisDirty();
        }
        bool locked = widget.config.value("locked", false);
        if(ImGui::Checkbox("Locked", &locked)){
            widget.config["locked"] = locked;
            container.markCustomGuisDirty();
        }
        if(ofxOceanodeCustomGuiWidgetHelpers::widgetSupportsLabel(widget) ||
           widget.type == CustomGuiWidgetType::BackgroundPanel){
            bool showLabel = widget.config.value("showLabel", true);
            if(ImGui::Checkbox("Show Label", &showLabel)){
                widget.config["showLabel"] = showLabel;
                container.markCustomGuisDirty();
            }
        }
        if(widget.type != CustomGuiWidgetType::Label){
            if(widget.type == CustomGuiWidgetType::BackgroundPanel){
                float panelColor[4] = {
                    widget.color.r / 255.0f,
                    widget.color.g / 255.0f,
                    widget.color.b / 255.0f,
                    widget.color.a / 255.0f
                };
                if(ImGui::ColorEdit4("Panel Color", panelColor)){
                    widget.color = ofColor(panelColor[0] * 255.0f,
                                           panelColor[1] * 255.0f,
                                           panelColor[2] * 255.0f,
                                           panelColor[3] * 255.0f);
                    container.markCustomGuisDirty();
                }

                ofColor labelColor = ofxOceanodeCustomGuiWidgetHelpers::widgetLabelColor(widget, ofColor::black);
                float labelColorFloat[4] = {
                    labelColor.r / 255.0f,
                    labelColor.g / 255.0f,
                    labelColor.b / 255.0f,
                    labelColor.a / 255.0f
                };
                if(ImGui::ColorEdit4("Label Color", labelColorFloat)){
                    widget.config["labelColor"] = customGuiColorToJson(ofColor(labelColorFloat[0] * 255.0f,
                                                                               labelColorFloat[1] * 255.0f,
                                                                               labelColorFloat[2] * 255.0f,
                                                                               labelColorFloat[3] * 255.0f));
                    container.markCustomGuisDirty();
                }

                float labelFontScale = std::max(0.2f, widget.config.value("labelFontScale", 1.0f));
                if(ImGui::InputFloat("Label Font Scale", &labelFontScale, 0.05f, 0.2f, "%.2f")){
                    widget.config["labelFontScale"] = std::max(0.2f, labelFontScale);
                    container.markCustomGuisDirty();
                }
            }else if(widget.type == CustomGuiWidgetType::Text){
                float textColor[4] = {
                    widget.color.r / 255.0f,
                    widget.color.g / 255.0f,
                    widget.color.b / 255.0f,
                    widget.color.a / 255.0f
                };
                if(ImGui::ColorEdit4("Text Color", textColor)){
                    widget.color = ofColor(textColor[0] * 255.0f,
                                           textColor[1] * 255.0f,
                                           textColor[2] * 255.0f,
                                           textColor[3] * 255.0f);
                    container.markCustomGuisDirty();
                }
            }else if(widget.type == CustomGuiWidgetType::Line){
            }else{
            ofColor labelColor = ofxOceanodeCustomGuiWidgetHelpers::widgetLabelColor(widget, ofColor::black);
            float labelColorFloat[4] = {
                labelColor.r / 255.0f,
                labelColor.g / 255.0f,
                labelColor.b / 255.0f,
                labelColor.a / 255.0f
            };
            if(ImGui::ColorEdit4("Label Color", labelColorFloat)){
                widget.config["labelColor"] = customGuiColorToJson(ofColor(labelColorFloat[0] * 255.0f,
                                                                           labelColorFloat[1] * 255.0f,
                                                                           labelColorFloat[2] * 255.0f,
                                                                           labelColorFloat[3] * 255.0f));
                container.markCustomGuisDirty();
            }
            ofColor bodyColor = ofxOceanodeCustomGuiWidgetHelpers::widgetBodyColor(widget);
            float bodyColorFloat[4] = {
                bodyColor.r / 255.0f,
                bodyColor.g / 255.0f,
                bodyColor.b / 255.0f,
                bodyColor.a / 255.0f
            };
            if(ImGui::ColorEdit4("Body Color", bodyColorFloat)){
                widget.config["bodyColor"] = customGuiColorToJson(ofColor(bodyColorFloat[0] * 255.0f,
                                                                          bodyColorFloat[1] * 255.0f,
                                                                          bodyColorFloat[2] * 255.0f,
                                                                          bodyColorFloat[3] * 255.0f));
                container.markCustomGuisDirty();
            }

            float accentColor[4] = {
                widget.color.r / 255.0f,
                widget.color.g / 255.0f,
                widget.color.b / 255.0f,
                widget.color.a / 255.0f
            };
            if(ImGui::ColorEdit4("Accent Color", accentColor)){
                widget.color = ofColor(accentColor[0] * 255.0f,
                                       accentColor[1] * 255.0f,
                                       accentColor[2] * 255.0f,
                                       accentColor[3] * 255.0f);
                container.markCustomGuisDirty();
            }
            }
        }

        if(parameter != nullptr){
            auto compatibleTypes = getCompatibleWidgetTypes(*parameter);
            auto addCompatibleType = [&](CustomGuiWidgetType type){
                if(std::find(compatibleTypes.begin(), compatibleTypes.end(), type) == compatibleTypes.end()){
                    compatibleTypes.push_back(type);
                }
            };

            if(ofxOceanodeCustomGuiWidgetHelpers::isFloatVectorParameter(*parameter)){
                addCompatibleType(CustomGuiWidgetType::Knob);
                addCompatibleType(CustomGuiWidgetType::DragNumber);
                addCompatibleType(CustomGuiWidgetType::MultiSlider);
                addCompatibleType(CustomGuiWidgetType::MultiToggle);
                addCompatibleType(CustomGuiWidgetType::PianoKeyboard);
                addCompatibleType(CustomGuiWidgetType::Waveform);
                addCompatibleType(CustomGuiWidgetType::VUMeter);
                addCompatibleType(CustomGuiWidgetType::FFT);
                if(parameter->cast<std::vector<float>>().getParameter().get().size() == 2){
                    addCompatibleType(CustomGuiWidgetType::XYPad);
                }
            }else if(widget.type == CustomGuiWidgetType::Knob &&
                     parameter->valueType() == typeid(std::vector<float>).name()){
                addCompatibleType(CustomGuiWidgetType::Knob);
                addCompatibleType(CustomGuiWidgetType::DragNumber);
                addCompatibleType(CustomGuiWidgetType::MultiSlider);
                addCompatibleType(CustomGuiWidgetType::MultiToggle);
                addCompatibleType(CustomGuiWidgetType::PianoKeyboard);
                addCompatibleType(CustomGuiWidgetType::Waveform);
                addCompatibleType(CustomGuiWidgetType::VUMeter);
                addCompatibleType(CustomGuiWidgetType::FFT);
            }else if(ofxOceanodeCustomGuiWidgetHelpers::isIntVectorParameter(*parameter)){
                addCompatibleType(CustomGuiWidgetType::DragNumber);
                addCompatibleType(CustomGuiWidgetType::MultiSlider);
                addCompatibleType(CustomGuiWidgetType::MultiToggle);
                addCompatibleType(CustomGuiWidgetType::PianoKeyboard);
            }else if(ofxOceanodeCustomGuiWidgetHelpers::isFloatParameter(*parameter)){
                addCompatibleType(CustomGuiWidgetType::Slider);
                addCompatibleType(CustomGuiWidgetType::Knob);
                addCompatibleType(CustomGuiWidgetType::DragNumber);
                addCompatibleType(CustomGuiWidgetType::MultiToggle);
            }else if(ofxOceanodeCustomGuiWidgetHelpers::isIntParameter(*parameter)){
                addCompatibleType(CustomGuiWidgetType::Slider);
                addCompatibleType(CustomGuiWidgetType::DragNumber);
                addCompatibleType(CustomGuiWidgetType::MultiToggle);
                addCompatibleType(CustomGuiWidgetType::CustomDropdown);
                addCompatibleType(CustomGuiWidgetType::ButtonMatrix);
                if(!parameter->cast<int>().getDropdownOptions().empty()){
                    addCompatibleType(CustomGuiWidgetType::Dropdown);
                }
            }else if(ofxOceanodeCustomGuiWidgetHelpers::isBoolParameter(*parameter)){
                addCompatibleType(CustomGuiWidgetType::Toggle);
            }else if(ofxOceanodeCustomGuiWidgetHelpers::isTriggerParameter(*parameter)){
                addCompatibleType(CustomGuiWidgetType::Button);
            }else if(ofxOceanodeCustomGuiWidgetHelpers::isStringParameter(*parameter)){
                addCompatibleType(CustomGuiWidgetType::TextDisplay);
                addCompatibleType(CustomGuiWidgetType::FileBrowser);
            }else if(ofxOceanodeCustomGuiWidgetHelpers::isTextureParameter(*parameter)){
                addCompatibleType(CustomGuiWidgetType::Texture);
                addCompatibleType(CustomGuiWidgetType::Waveform);
            }else if(ofxOceanodeCustomGuiWidgetHelpers::isRegisteredCustomRegionParameter(*parameter)){
                addCompatibleType(CustomGuiWidgetType::CustomRegion);
            }

            if(std::find(compatibleTypes.begin(), compatibleTypes.end(), widget.type) == compatibleTypes.end()){
                compatibleTypes.insert(compatibleTypes.begin(), widget.type);
            }
            if(!compatibleTypes.empty()){
                const std::string currentTypeLabel = customGuiWidgetTypeToString(widget.type);
                if(ImGui::BeginCombo("Widget Type", currentTypeLabel.c_str())){
                    for(CustomGuiWidgetType compatibleType : compatibleTypes){
                        const std::string compatibleTypeLabel = customGuiWidgetTypeToString(compatibleType);
                        const bool selected = compatibleType == widget.type;
                        if(ImGui::Selectable(compatibleTypeLabel.c_str(), selected) && !selected){
                            if(const CustomGuiWidgetDefinition* oldDefinition =
                                   ofxOceanodeCustomGuiWidgetRegistry::instance().getWidget(widget.type)){
                                if(oldDefinition->cleanup){
                                    oldDefinition->cleanup(panelId, widget.parameterRef.parameterPath);
                                }
                            }

                            const int gridX = widget.gridX;
                            const int gridY = widget.gridY;
                            const CustomGuiParameterReference parameterRef = widget.parameterRef;
                            const std::string label = widget.label;
                            const ofColor color = widget.color;

                            widget.type = compatibleType;
                            widget.gridX = gridX;
                            widget.gridY = gridY;
                            widget.parameterRef = parameterRef;
                            widget.label = label;
                            widget.color = color;
                            widget.config = ofJson::object();

                            const CustomGuiWidgetDefinition* newDefinition =
                                ofxOceanodeCustomGuiWidgetRegistry::instance().getWidget(compatibleType);
                            if(newDefinition != nullptr && newDefinition->initializeWidget){
                                newDefinition->initializeWidget(widget, *parameter);
                            }

                            if(CustomGuiPanelData* panel = getPanelData()){
                                panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
                                panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
                            }
                            container.markCustomGuisDirty();
                            break;
                        }
                        if(selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
        }

        int spanW = widget.spanW;
        int spanH = widget.spanH;
        if(ImGui::InputInt("Width", &spanW)){
            widget.spanW = std::max(1, spanW);
            if(CustomGuiPanelData* panel = getPanelData()){
                panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
                panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
            }
            container.markCustomGuisDirty();
        }
        if(ImGui::InputInt("Height", &spanH)){
            widget.spanH = std::max(1, spanH);
            if(CustomGuiPanelData* panel = getPanelData()){
                panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
                panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
            }
            container.markCustomGuisDirty();
        }

        bool interactive = ofxOceanodeCustomGuiWidgets::isInteractive(widget, parameter);
        if(parameter != nullptr && parameter->valueType() != typeid(std::string).name()){
            if(ImGui::Checkbox("Interactive", &interactive)){
                widget.config["interactive"] = interactive;
                container.markCustomGuisDirty();
            }
        }

        bool showValue = shouldShowNumericValue(widget);
	        if(parameter != nullptr &&
		   (parameter->valueType() == typeid(float).name() ||
		    parameter->valueType() == typeid(int).name() ||
		    parameter->valueType() == typeid(std::vector<float>).name() ||
		    parameter->valueType() == typeid(std::vector<int>).name())){
	            if(ImGui::Checkbox("Show Value", &showValue)){
	                widget.config["showValue"] = showValue;
	                container.markCustomGuisDirty();
            }
        }

	        if(parameter != nullptr &&
		   (parameter->valueType() == typeid(float).name() ||
		    parameter->valueType() == typeid(int).name() ||
		    parameter->valueType() == typeid(std::vector<float>).name())){
	            bool useCustomRange = widget.config.value("useCustomRange", false);
	            if(ImGui::Checkbox("Custom Range", &useCustomRange)){
	                widget.config["useCustomRange"] = useCustomRange;
                container.markCustomGuisDirty();
            }
            if(useCustomRange){
                if(parameter->valueType() == typeid(int).name()){
                    int rangeMin = (int)std::round(widget.config.value("rangeMin", (float)parameter->cast<int>().getParameter().getMin()));
                    int rangeMax = (int)std::round(widget.config.value("rangeMax", (float)parameter->cast<int>().getParameter().getMax()));
                    if(ImGui::InputInt("Range Min", &rangeMin)){
                        widget.config["rangeMin"] = rangeMin;
                        if(rangeMax < rangeMin) widget.config["rangeMax"] = rangeMin;
                        container.markCustomGuisDirty();
                    }
                    if(ImGui::InputInt("Range Max", &rangeMax)){
                        widget.config["rangeMax"] = std::max(rangeMin, rangeMax);
                        container.markCustomGuisDirty();
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
                        container.markCustomGuisDirty();
                    }
                    if(ImGui::InputFloat("Range Max", &rangeMax)){
                        widget.config["rangeMax"] = std::max(rangeMin, rangeMax);
                        container.markCustomGuisDirty();
                    }
                }
	            }
	        }

	        if(parameter != nullptr &&
	           (parameter->valueType() == typeid(float).name() ||
	            parameter->valueType() == typeid(std::vector<float>).name()) &&
	           widget.type != CustomGuiWidgetType::Waveform &&
	           widget.type != CustomGuiWidgetType::XYPad &&
	           widget.type != CustomGuiWidgetType::MultiSlider &&
	           widget.type != CustomGuiWidgetType::MultiToggle){
	            int quantization = std::max(0, widget.config.value("quantization", 0));
	            if(ImGui::InputInt("Quantization", &quantization)){
	                widget.config["quantization"] = std::max(0, quantization);
	                container.markCustomGuisDirty();
	            }
	        }

        const CustomGuiWidgetDefinition* definition = ofxOceanodeCustomGuiWidgetRegistry::instance().getWidget(widget.type);
        if(definition != nullptr && definition->drawProperties){
            CustomGuiWidgetPropertiesContext context {
                container,
                [this](){ return getPanelData(); }
            };
            definition->drawProperties(context, widget, parameter);
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

    std::vector<float> mins;
    std::vector<float> maxs;
    const bool integerVector = parameter->valueType() == typeid(std::vector<int>).name();
    if(integerVector){
        const auto& intParam = parameter->cast<std::vector<int>>().getParameter();
        const auto& intMins = intParam.getMin();
        const auto& intMaxs = intParam.getMax();
        mins.assign(intMins.begin(), intMins.end());
        maxs.assign(intMaxs.begin(), intMaxs.end());
    }else{
        const auto& floatParam = parameter->cast<std::vector<float>>().getParameter();
        mins = floatParam.getMin();
        maxs = floatParam.getMax();
    }
    const int visibleCount = ofClamp(widget.config.value("visibleCount", (int)value.size()), 1, (int)value.size());
    const bool vertical = widget.config.value("vertical", true);
    const bool showValue = shouldShowNumericValue(widget);
    const bool useCustomRange = widget.config.value("useCustomRange", false);
    const int quantization = std::max(0, widget.config.value("quantization", 0));

    ImGui::InvisibleButton("##multislider", size);
    const bool isHovered = ImGui::IsItemHovered();
    const bool isActive = interactive && ImGui::IsItemActive();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();

    const ofColor bodyColor = ofxOceanodeCustomGuiWidgetHelpers::widgetBodyColor(widget);
    drawList->AddRectFilled(min, max, IM_COL32(bodyColor.r, bodyColor.g, bodyColor.b, bodyColor.a), 2.0f);
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
	        float newValue = minValue + normalized * (maxValue - minValue);
	        if(quantization >= 2){
	            const float step = (maxValue - minValue) / (float)(quantization - 1);
	            if(step > 0.0f){
	                newValue = minValue + std::round((newValue - minValue) / step) * step;
	                newValue = ofClamp(newValue, minValue, maxValue);
	            }
	        }
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
                std::string text = integerVector ? ofToString((int)std::round(value[i])) : ofToString(value[i], 2);
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
                std::string text = integerVector ? ofToString((int)std::round(value[i])) : ofToString(value[i], 2);
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
        const std::string tooltipValue = integerVector ? ofToString((int)std::round(value[hoveredIndex])) : ofToString(value[hoveredIndex], 3);
        ImGui::SetTooltip("Slider %d: %s", hoveredIndex, tooltipValue.c_str());
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
    return ofxOceanodeCustomGuiWidgetRegistry::instance().getCompatibleWidgets(parameter);
}

CustomGuiWidgetType ofxOceanodeCustomGuiPanel::getDefaultWidgetType(ofxOceanodeAbstractParameter& parameter) const
{
    return ofxOceanodeCustomGuiWidgetRegistry::instance().getDefaultWidgetType(parameter);
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
    for(const auto& widget : widgets){
        if(widget.parameterRef.parameterPath != parameterPath) continue;
        if(const CustomGuiWidgetDefinition* definition = ofxOceanodeCustomGuiWidgetRegistry::instance().getWidget(widget.type)){
            if(definition->cleanup) definition->cleanup(panel->id, parameterPath);
        }
    }

    auto it = std::remove_if(widgets.begin(), widgets.end(), [&](const CustomGuiWidget& widget){
        return widget.parameterRef.parameterPath == parameterPath;
    });
    if(it == widgets.end()) return false;
    widgets.erase(it, widgets.end());
    container.markCustomGuisDirty();
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
    widget.config["interactive"] = ofxOceanodeCustomGuiWidgets::defaultInteractiveState(parameter);
    widget.config["showValue"] = true;

    if(const CustomGuiWidgetDefinition* definition = ofxOceanodeCustomGuiWidgetRegistry::instance().getWidget(type)){
        if(definition->initializeWidget) definition->initializeWidget(widget, parameter);
    }

    auto cell = findNextAvailableCell(panel->layout, widget.spanW, widget.spanH);
    widget.gridX = cell.first;
    widget.gridY = cell.second;
    panel->layout.widgets.push_back(widget);
    panel->layout.columns = std::max(panel->layout.columns, widget.gridX + widget.spanW);
    panel->layout.rows = std::max(panel->layout.rows, widget.gridY + widget.spanH);
    panel->windowState.isOpen = true;
    container.markCustomGuisDirty();
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
