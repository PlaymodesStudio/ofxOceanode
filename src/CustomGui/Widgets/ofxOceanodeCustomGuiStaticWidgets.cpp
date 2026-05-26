
#include "CustomGui/Widgets/ofxOceanodeCustomGuiStaticWidgets.h"
#include "CustomGui/Widgets/ofxOceanodeCustomGuiWidgetHelpers.h"
#include "Managers/ofxOceanodeContainer.h"
#include <algorithm>
#include <cstdio>

namespace {

using namespace ofxOceanodeCustomGuiWidgetHelpers;

ofColor matrixColorFromConfig(const CustomGuiWidget& widget, const char* key, const ofColor& fallback)
{
    if(widget.config.contains(key) && widget.config[key].is_array()){
        return customGuiColorFromJson(widget.config[key], fallback);
    }
    return fallback;
}

void drawMatrixColorProperty(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, const char* key, const char* label, const ofColor& fallback)
{
    ofColor color = matrixColorFromConfig(widget, key, fallback);
    float colorFloat[4] = {
        color.r / 255.0f,
        color.g / 255.0f,
        color.b / 255.0f,
        color.a / 255.0f
    };
    if(ImGui::ColorEdit4(label, colorFloat)){
        widget.config[key] = customGuiColorToJson(ofColor(colorFloat[0] * 255.0f,
                                                          colorFloat[1] * 255.0f,
                                                          colorFloat[2] * 255.0f,
                                                          colorFloat[3] * 255.0f));
        context.container.markCustomGuisDirty();
    }
}

bool renderLabelWidget(CustomGuiWidgetRenderContext&, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    ImGui::BeginGroup();
    ImGui::TextWrapped("%s", widget.label.c_str());
    ImGui::EndGroup();
    return true;
}

bool renderBackgroundPanelWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    ImGui::BeginGroup();
    if(context.designMode){
        ImGui::InvisibleButton("##background", context.size);
        ImGui::SetNextItemAllowOverlap();
    }else{
        ImGui::Dummy(context.size);
    }
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    drawList->AddRectFilled(min, max, IM_COL32(widget.color.r, widget.color.g, widget.color.b, widget.color.a), 4.0f);

    const bool showLabel = widget.config.value("showLabel", true);
    if(showLabel && !widget.label.empty()){
        const ofColor labelColor = widgetLabelColor(widget, ofColor::black);
        const float labelFontScale = std::max(0.2f, widget.config.value("labelFontScale", 1.0f));
        const float labelFontSize = ImGui::GetFontSize() * labelFontScale;
        const ImVec2 textPos(min.x + 8.0f, min.y + 6.0f);
        const float wrapWidth = std::max(1.0f, context.size.x - 16.0f);
        drawList->AddText(ImGui::GetFont(), labelFontSize, textPos,
                          IM_COL32(labelColor.r, labelColor.g, labelColor.b, labelColor.a),
                          widget.label.c_str(), nullptr, wrapWidth);
    }

    ImGui::EndGroup();
    return true;
}

bool renderTextWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    ImGui::BeginGroup();
    const float fontScale = std::max(0.2f, widget.config.value("fontScale", 1.0f));
    ImGui::InvisibleButton("##text", context.size);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const float fontSize = ImGui::GetFontSize() * fontScale;
    drawList->AddText(ImGui::GetFont(), fontSize, min, IM_COL32(widget.color.r, widget.color.g, widget.color.b, widget.color.a), widget.label.c_str(), nullptr, context.size.x);
    ImGui::EndGroup();
    return true;
}

bool renderImageWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    ImGui::BeginGroup();
    ImGui::InvisibleButton("##image", context.size);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const std::string imagePath = widget.config.value("imagePath", std::string());
    auto image = context.loadWidgetImage(imagePath);
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

bool renderLineWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    ImGui::BeginGroup();
    if(context.designMode){
        ImGui::InvisibleButton("##line", context.size);
    }else{
        ImGui::Dummy(context.size);
    }

    const bool horizontal = widget.config.value("horizontal", true);
    const float lineWeight = std::max(1.0f, widget.config.value("lineWeight", 2.0f));
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImU32 color = IM_COL32(widget.color.r, widget.color.g, widget.color.b, widget.color.a);

    if(horizontal){
        const float y = (min.y + max.y) * 0.5f;
        drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), color, lineWeight);
    }else{
        const float x = (min.x + max.x) * 0.5f;
        drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), color, lineWeight);
    }

    ImGui::EndGroup();
    return true;
}

bool renderSnapshotMatrixWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    const CustomGuiSnapshotBank* bank = context.container.getCustomGuiSnapshotBank(context.panelId);
    const int rows = std::max(1, widget.config.value("rows", 2));
    const int cols = std::max(1, widget.config.value("cols", 4));
    const bool showNames = widget.config.value("showSnapshotNames", true);
    const float spacingX = ImGui::GetStyle().ItemSpacing.x;
    const float spacingY = ImGui::GetStyle().ItemSpacing.y;
    const ImVec2 itemSize = widgetItemSize(context);
    const float btnW = std::max(18.0f, (itemSize.x - spacingX * (cols - 1)) / cols);
    const float btnH = std::max(18.0f, (itemSize.y - spacingY * (rows - 1)) / rows);
    const ofColor activeColor = matrixColorFromConfig(widget, "activeColor", ofColor(102, 0, 0, 255));
    const ofColor filledColor = matrixColorFromConfig(widget, "filledColor", ofColor(51, 102, 0, 255));
    const ofColor emptyColor = matrixColorFromConfig(widget, "emptyColor", ofColor(70, 70, 70, 255));

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    for(int row = 0; row < rows; row++){
        for(int col = 0; col < cols; col++){
            if(col > 0) ImGui::SameLine();
            const int slot = row * cols + col;
            const CustomGuiSnapshotData* snapshot = context.container.getCustomGuiSnapshotBySlot(context.panelId, slot);
            const bool hasData = snapshot != nullptr;
            const bool isActive = bank != nullptr && snapshot != nullptr && bank->currentSnapshotId == snapshot->id;
            const ofColor buttonColor = isActive ? activeColor : (hasData ? filledColor : emptyColor);

            ImGui::PushID(slot);
            ImGui::PushStyleColor(ImGuiCol_Button, colorToImVec4(buttonColor));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colorToImVec4(buttonColor, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, colorToImVec4(buttonColor, 1.0f));

            std::string label = (hasData && showNames) ? snapshot->name : ofToString(slot + 1);
            if(ImGui::Button(label.c_str(), ImVec2(btnW, btnH)) && !context.designMode){
                context.container.recallCustomGuiSnapshotSlot(context.panelId, slot);
            }

            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }
    }
    ImGui::EndGroup();
    return true;
}

void drawTextProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    float fontScale = std::max(0.2f, widget.config.value("fontScale", 1.0f));
    if(ImGui::InputFloat("Font Scale", &fontScale, 0.05f, 0.2f, "%.2f")){
        widget.config["fontScale"] = std::max(0.2f, fontScale);
        context.container.markCustomGuisDirty();
    }
}

void drawImageProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    char pathBuffer[512];
    const std::string currentPath = widget.config.value("imagePath", std::string());
    std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", currentPath.c_str());
    if(ImGui::InputText("Image Path", pathBuffer, sizeof(pathBuffer))){
        widget.config["imagePath"] = std::string(pathBuffer);
        context.container.markCustomGuisDirty();
    }

    if(ImGui::Button("Browse...")){
        std::string defaultPath;
        if(!currentPath.empty()){
            ofFile currentFile(currentPath);
            if(currentFile.exists()){
                defaultPath = currentFile.isDirectory() ? currentFile.getAbsolutePath() : currentFile.getEnclosingDirectory();
            }else{
                defaultPath = ofFilePath::getEnclosingDirectory(currentPath, false);
            }
        }

        ofFileDialogResult result = ofSystemLoadDialog("Select image", false, defaultPath);
        if(result.bSuccess){
            widget.config["imagePath"] = result.getPath();
            context.container.markCustomGuisDirty();
        }
    }

    ImGui::SameLine();
    if(ImGui::Button("Clear")){
        widget.config["imagePath"] = "";
        context.container.markCustomGuisDirty();
    }

    bool sendToBack = widget.config.value("sendToBack", false);
    if(ImGui::Checkbox("Send To Back", &sendToBack)){
        widget.config["sendToBack"] = sendToBack;
        context.container.markCustomGuisDirty();
    }
}

void drawLineProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    bool horizontal = widget.config.value("horizontal", true);
    if(ImGui::Checkbox("Horizontal", &horizontal)){
        widget.config["horizontal"] = horizontal;
        widget.spanW = horizontal ? std::max(2, widget.spanW) : 1;
        widget.spanH = horizontal ? 1 : std::max(2, widget.spanH);
        context.container.markCustomGuisDirty();
    }

    float lineWeight = std::max(1.0f, widget.config.value("lineWeight", 2.0f));
    if(ImGui::InputFloat("Line Weight", &lineWeight, 0.5f, 1.0f, "%.1f")){
        widget.config["lineWeight"] = std::max(1.0f, lineWeight);
        context.container.markCustomGuisDirty();
    }

    float lineColor[4] = {
        widget.color.r / 255.0f,
        widget.color.g / 255.0f,
        widget.color.b / 255.0f,
        widget.color.a / 255.0f
    };
    if(ImGui::ColorEdit4("Line Color", lineColor)){
        widget.color = ofColor(lineColor[0] * 255.0f,
                               lineColor[1] * 255.0f,
                               lineColor[2] * 255.0f,
                               lineColor[3] * 255.0f);
        context.container.markCustomGuisDirty();
    }
}

void drawSnapshotMatrixProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
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

    bool showNames = widget.config.value("showSnapshotNames", true);
    if(ImGui::Checkbox("Show Names", &showNames)){
        widget.config["showSnapshotNames"] = showNames;
        context.container.markCustomGuisDirty();
    }

    drawMatrixColorProperty(context, widget, "activeColor", "Active Color", ofColor(102, 0, 0, 255));
    drawMatrixColorProperty(context, widget, "filledColor", "Filled Color", ofColor(51, 102, 0, 255));
    drawMatrixColorProperty(context, widget, "emptyColor", "Empty Color", ofColor(70, 70, 70, 255));
}

void initializeTextWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.config["fontScale"] = 1.0f;
}

void initializeImageWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.config["imagePath"] = "";
    widget.config["sendToBack"] = false;
}

void initializeBackgroundPanelWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.config["showLabel"] = true;
    widget.config["labelFontScale"] = 1.0f;
    ensureWidgetLabelColor(widget);
}

void initializeLineWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.spanW = 2;
    widget.spanH = 1;
    widget.config["horizontal"] = true;
    widget.config["lineWeight"] = 2.0f;
}

void initializeSnapshotMatrixWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.spanW = 4;
    widget.spanH = 2;
    widget.config["rows"] = 2;
    widget.config["cols"] = 4;
    widget.config["showSnapshotNames"] = true;
    widget.config["activeColor"] = customGuiColorToJson(ofColor(102, 0, 0, 255));
    widget.config["filledColor"] = customGuiColorToJson(ofColor(51, 102, 0, 255));
    widget.config["emptyColor"] = customGuiColorToJson(ofColor(70, 70, 70, 255));
    ensureWidgetLabelColor(widget);
}

} // namespace

namespace ofxOceanodeCustomGuiStaticWidgets {

void registerWidgets(ofxOceanodeCustomGuiWidgetRegistry& registry)
{
    registerWidget(registry, CustomGuiWidgetType::Label,
                   [](ofxOceanodeAbstractParameter&){ return false; },
                   [](CustomGuiWidget&, ofxOceanodeAbstractParameter&){},
                   renderLabelWidget);

    registerWidget(registry, CustomGuiWidgetType::BackgroundPanel,
                   [](ofxOceanodeAbstractParameter&){ return false; },
                   initializeBackgroundPanelWidget,
                   renderBackgroundPanelWidget);

    registerWidget(registry, CustomGuiWidgetType::Text,
                   [](ofxOceanodeAbstractParameter&){ return false; },
                   initializeTextWidget,
                   renderTextWidget,
                   drawTextProperties);

    registerWidget(registry, CustomGuiWidgetType::Line,
                   [](ofxOceanodeAbstractParameter&){ return false; },
                   initializeLineWidget,
                   renderLineWidget,
                   drawLineProperties);

    registerWidget(registry, CustomGuiWidgetType::Image,
                   [](ofxOceanodeAbstractParameter&){ return false; },
                   initializeImageWidget,
                   renderImageWidget,
                   drawImageProperties);

    registerWidget(registry, CustomGuiWidgetType::SnapshotMatrix,
                   [](ofxOceanodeAbstractParameter&){ return false; },
                   initializeSnapshotMatrixWidget,
                   renderSnapshotMatrixWidget,
                   drawSnapshotMatrixProperties);
}

} // namespace ofxOceanodeCustomGuiStaticWidgets
