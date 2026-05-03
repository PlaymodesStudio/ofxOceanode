#ifndef OFXOCEANODE_HEADLESS

#include "CustomGui/Widgets/ofxOceanodeCustomGuiStaticWidgets.h"
#include "CustomGui/Widgets/ofxOceanodeCustomGuiWidgetHelpers.h"
#include "Managers/ofxOceanodeContainer.h"
#include <algorithm>
#include <cstdio>

namespace {

using namespace ofxOceanodeCustomGuiWidgetHelpers;

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
}

void initializeTextWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.config["fontScale"] = 1.0f;
}

void initializeImageWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.config["imagePath"] = "";
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
                   [](CustomGuiWidget&, ofxOceanodeAbstractParameter&){},
                   renderBackgroundPanelWidget);

    registerWidget(registry, CustomGuiWidgetType::Text,
                   [](ofxOceanodeAbstractParameter&){ return false; },
                   initializeTextWidget,
                   renderTextWidget,
                   drawTextProperties);

    registerWidget(registry, CustomGuiWidgetType::Image,
                   [](ofxOceanodeAbstractParameter&){ return false; },
                   initializeImageWidget,
                   renderImageWidget,
                   drawImageProperties);
}

} // namespace ofxOceanodeCustomGuiStaticWidgets

#endif
