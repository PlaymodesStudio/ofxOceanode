
#include "CustomGui/Widgets/ofxOceanodeCustomGuiArrayWidgets.h"
#include "CustomGui/Widgets/ofxOceanodeCustomGuiWidgetHelpers.h"
#include "Managers/ofxOceanodeContainer.h"
#include "CustomGui/ofxOceanodeCustomGuiWidgets.h"
#include <algorithm>
#include <cmath>

namespace {

using namespace ofxOceanodeCustomGuiWidgetHelpers;

bool supportsMultiSliderWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isFloatVectorParameter(parameter);
}

bool supportsMultiToggleWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isFloatParameter(parameter) ||
           isIntParameter(parameter) ||
           isFloatVectorParameter(parameter) ||
           isIntVectorParameter(parameter);
}

bool supportsXYPadWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isFloatVectorParameter(parameter) &&
           parameter.cast<std::vector<float>>().getParameter().get().size() == 2;
}

bool supportsPianoKeyboardWidget(ofxOceanodeAbstractParameter& parameter)
{
    return isIntVectorParameter(parameter) || isFloatVectorParameter(parameter);
}

void initializeMultiSliderWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter& parameter)
{
    widget.spanW = 3;
    widget.spanH = 3;
    int size = std::max(1, (int)parameter.cast<std::vector<float>>().getParameter().get().size());
    widget.config["visibleCount"] = size;
    widget.config["vertical"] = true;
}

void initializeMultiToggleWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter& parameter)
{
    widget.spanW = 2;
    widget.spanH = isFloatVectorParameter(parameter) || isIntVectorParameter(parameter) ? 2 : 1;

    int size = 1;
    if(isFloatVectorParameter(parameter)){
        size = std::max(1, (int)parameter.cast<std::vector<float>>().getParameter().get().size());
    }else if(isIntVectorParameter(parameter)){
        size = std::max(1, (int)parameter.cast<std::vector<int>>().getParameter().get().size());
    }

    widget.config["rows"] = 1;
    widget.config["cols"] = size;
}

void initializeXYPadWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.spanW = 2;
    widget.spanH = 2;
}

void initializePianoKeyboardWidget(CustomGuiWidget& widget, ofxOceanodeAbstractParameter&)
{
    widget.spanW = 6;
    widget.spanH = 3;
    widget.config["loNote"] = 48;
    widget.config["hiNote"] = 72;
    widget.config["dimOutsidePianoRange"] = false;
}

struct PianoKeyGeometry {
    bool isBlack = false;
    float x = 0.0f;
    float w = 0.0f;
};

bool isWhitePianoKey(int note)
{
    const int noteInOctave = note % 12;
    return noteInOctave == 0 || noteInOctave == 2 || noteInOctave == 4 ||
           noteInOctave == 5 || noteInOctave == 7 || noteInOctave == 9 ||
           noteInOctave == 11;
}

std::vector<PianoKeyGeometry> buildPianoGeometry(int startNote, int endNote, float width)
{
    std::vector<PianoKeyGeometry> geometry;
    if(endNote < startNote) return geometry;

    int whiteKeyCount = 0;
    for(int note = startNote; note <= endNote; note++){
        if(isWhitePianoKey(note)) whiteKeyCount++;
    }

    if(whiteKeyCount <= 0) return geometry;

    const float whiteKeyWidth = width / (float)whiteKeyCount;
    const float blackKeyWidth = whiteKeyWidth * 0.6f;
    float currentX = 0.0f;

    for(int note = startNote; note <= endNote; note++){
        PianoKeyGeometry key;
        const int noteInOctave = note % 12;
        switch(noteInOctave){
            case 0: case 2: case 4: case 5: case 7: case 9: case 11:
                key.isBlack = false;
                key.x = currentX;
                key.w = whiteKeyWidth;
                currentX += whiteKeyWidth;
                break;
            default:
                key.isBlack = true;
                key.x = currentX - (blackKeyWidth * 0.5f);
                key.w = blackKeyWidth;
                break;
        }
        geometry.push_back(key);
    }

    return geometry;
}

int pianoNoteFromPosition(const std::vector<PianoKeyGeometry>& geometry, int startNote, const ImVec2& min, const ImVec2& mousePos, float height)
{
    const float relativeX = mousePos.x - min.x;
    const float relativeY = mousePos.y - min.y;
    const float blackKeyHeight = height * 0.6f;

    if(relativeY <= blackKeyHeight){
        for(int i = 0; i < (int)geometry.size(); i++){
            if(!geometry[i].isBlack) continue;
            if(relativeX >= geometry[i].x && relativeX <= geometry[i].x + geometry[i].w){
                return startNote + i;
            }
        }
    }

    for(int i = 0; i < (int)geometry.size(); i++){
        if(geometry[i].isBlack) continue;
        if(relativeX >= geometry[i].x && relativeX <= geometry[i].x + geometry[i].w){
            return startNote + i;
        }
    }

    return -1;
}

bool renderPianoKeyboardWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    std::set<int> selectedNotes;
    std::function<void(const std::set<int>&)> applySelection;

    if(isIntVectorParameter(*parameter)){
        auto& param = parameter->cast<std::vector<int>>().getParameter();
        auto value = param.get();
        selectedNotes = std::set<int>(value.begin(), value.end());
        applySelection = [&](const std::set<int>& notes){
            param.set(std::vector<int>(notes.begin(), notes.end()));
        };
    }else if(isFloatVectorParameter(*parameter)){
        auto& param = parameter->cast<std::vector<float>>().getParameter();
        auto value = param.get();
        for(float note : value){
            selectedNotes.insert((int)std::round(note));
        }
        applySelection = [&](const std::set<int>& notes){
            std::vector<float> floatNotes;
            floatNotes.reserve(notes.size());
            for(int note : notes) floatNotes.push_back((float)note);
            param.set(floatNotes);
        };
    }else{
        return false;
    }

    const bool interactive = context.interactive && !context.designMode;
    const int startNote = ofClamp(widget.config.value("loNote", 48), 0, 127);
    const int endNote = ofClamp(widget.config.value("hiNote", 72), 0, 127);
    const int lo = std::min(startNote, endNote);
    const int hi = std::max(startNote, endNote);
    const bool dimOutsidePianoRange = widget.config.value("dimOutsidePianoRange", false);
    const ImVec2 itemSize = widgetItemSize(context);
    const float keyboardHeight = std::max(24.0f, itemSize.y);
    const float keyboardWidth = std::max(1.0f, itemSize.x);
    const auto geometry = buildPianoGeometry(lo, hi, keyboardWidth);

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    ImGui::InvisibleButton("##pianokeyboard", ImVec2(keyboardWidth, keyboardHeight));

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if(interactive && ImGui::IsItemClicked(ImGuiMouseButton_Left)){
        const int clickedNote = pianoNoteFromPosition(geometry, lo, min, ImGui::GetIO().MousePos, keyboardHeight);
        if(clickedNote >= 0){
            if(selectedNotes.count(clickedNote)) selectedNotes.erase(clickedNote);
            else selectedNotes.insert(clickedNote);
            applySelection(selectedNotes);
        }
    }

    const int PIANO_MIN = 21;
    const int PIANO_MAX = 108;

    for(int i = 0; i < (int)geometry.size(); i++){
        if(geometry[i].isBlack) continue;
        const int note = lo + i;
        const ImVec2 keyMin(min.x + geometry[i].x, min.y);
        const ImVec2 keyMax(keyMin.x + geometry[i].w, max.y);
        const bool inPianoRange = (note >= PIANO_MIN && note <= PIANO_MAX);
        drawList->AddRectFilled(keyMin, keyMax,
                                dimOutsidePianoRange && !inPianoRange
                                    ? IM_COL32(240, 240, 240, 120)
                                    : IM_COL32(255, 255, 255, 255));
        if(selectedNotes.count(note)){
            drawList->AddRectFilled(keyMin, keyMax, IM_COL32(widget.color.r, widget.color.g, widget.color.b, 120));
        }
        drawList->AddRect(keyMin, keyMax, IM_COL32(100, 100, 100, 255));
    }

    const float blackKeyHeight = keyboardHeight * 0.6f;
    for(int i = 0; i < (int)geometry.size(); i++){
        if(!geometry[i].isBlack) continue;
        const int note = lo + i;
        const ImVec2 keyMin(min.x + geometry[i].x, min.y);
        const ImVec2 keyMax(keyMin.x + geometry[i].w, min.y + blackKeyHeight);
        const bool inPianoRange = (note >= PIANO_MIN && note <= PIANO_MAX);
        drawList->AddRectFilled(keyMin, keyMax,
                                dimOutsidePianoRange && !inPianoRange
                                    ? IM_COL32(80, 80, 80, 120)
                                    : IM_COL32(0, 0, 0, 255));
        if(selectedNotes.count(note)){
            drawList->AddRectFilled(keyMin, keyMax, IM_COL32(widget.color.r, widget.color.g, widget.color.b, 160));
        }
        drawList->AddRect(keyMin, keyMax, IM_COL32(100, 100, 100, 255));
    }

    ImGui::EndGroup();
    return true;
}

template <typename T, typename IsActiveFn, typename SetStateFn>
bool drawMultiToggleGrid(const CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, std::vector<T>& values, IsActiveFn isActiveValue, SetStateFn setValueFromState)
{
    const ImVec2 itemSize = widgetItemSize(context);
    const int rows = std::max(1, widget.config.value("rows", 1));
    const int cols = std::max(1, widget.config.value("cols", (int)values.size()));
    const float cellSpacing = 1.0f;
    const float totalWidthSpacing = cellSpacing * std::max(0, cols - 1);
    const float totalHeightSpacing = cellSpacing * std::max(0, rows - 1);
    const float cellWidth = std::max(12.0f, (itemSize.x - totalWidthSpacing) / cols);
    const float cellHeight = std::max(12.0f, (itemSize.y - totalHeightSpacing) / rows);
    const ImVec2 gridSize(cellWidth * cols + totalWidthSpacing, cellHeight * rows + totalHeightSpacing);
    ImGui::InvisibleButton("##multitogglegrid", gridSize);

    const bool hovered = ImGui::IsItemHovered();
    const bool active = context.interactive && ImGui::IsItemActive();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    drawList->AddRectFilled(min, max, IM_COL32(45, 45, 45, 255), 2.0f);
    drawList->AddRect(min, max, IM_COL32(90, 90, 90, 255), 2.0f);

    static ImGuiID activePaintWidget = 0;
    static bool paintState = false;

    auto hoveredIndexFromMouse = [&](const ImVec2& mousePos){
        const float relX = mousePos.x - min.x;
        const float relY = mousePos.y - min.y;
        const int col = ofClamp((int)std::floor(relX / std::max(1.0f, cellWidth + cellSpacing)), 0, cols - 1);
        const int row = ofClamp((int)std::floor(relY / std::max(1.0f, cellHeight + cellSpacing)), 0, rows - 1);
        return row * cols + col;
    };

    const ImGuiID widgetId = ImGui::GetItemID();
    if(context.interactive && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
        const int hoveredIndex = hoveredIndexFromMouse(ImGui::GetIO().MousePos);
        if(hoveredIndex >= 0 && hoveredIndex < (int)values.size()){
            activePaintWidget = widgetId;
            paintState = !isActiveValue(values[hoveredIndex]);
            setValueFromState(values[hoveredIndex], paintState);
        }
    }
    if(activePaintWidget == widgetId && context.interactive && ImGui::IsMouseDown(ImGuiMouseButton_Left)){
        const int hoveredIndex = hoveredIndexFromMouse(ImGui::GetIO().MousePos);
        if(hoveredIndex >= 0 && hoveredIndex < (int)values.size()){
            setValueFromState(values[hoveredIndex], paintState);
        }
    }
    if(activePaintWidget == widgetId && ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
        activePaintWidget = 0;
    }

    const ImVec4 onColor = colorToImVec4(widget.color, 0.8f);
    for(int r = 0; r < rows; r++){
        for(int c = 0; c < cols; c++){
            const int index = r * cols + c;
            if(index >= (int)values.size()) continue;
            const float x0 = min.x + c * (cellWidth + cellSpacing);
            const float y0 = min.y + r * (cellHeight + cellSpacing);
            const float x1 = x0 + cellWidth;
            const float y1 = y0 + cellHeight;
            const bool cellOn = isActiveValue(values[index]);
            const ImU32 fillColor = cellOn
                ? IM_COL32((int)(onColor.x * 255.0f), (int)(onColor.y * 255.0f), (int)(onColor.z * 255.0f), (int)(onColor.w * 255.0f))
                : IM_COL32(65, 65, 65, 255);
            drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), fillColor, 1.5f);
            drawList->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), cellOn ? IM_COL32(255, 255, 255, 80) : IM_COL32(110, 110, 110, 180), 1.5f);
        }
    }

    return activePaintWidget == widgetId || active || hovered;
}

bool renderFloatVectorWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;
    auto& param = parameter->cast<std::vector<float>>().getParameter();
    auto value = param.get();
    if(value.empty()) return true;

    const ImVec2 itemSize = widgetItemSize(context);
    const bool interactive = context.interactive;
    const bool verticalSlider = itemSize.y > itemSize.x;
    const bool useRange = useCustomRange(widget);

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);

    if(widget.type == CustomGuiWidgetType::XYPad && value.size() >= 2){
        ImGui::Button("##xypad", itemSize);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        drawList->AddRect(min, max, IM_COL32(180, 180, 180, 180));
        float x = ofMap(value[0], param.getMin()[0], param.getMax()[0], min.x, max.x, true);
        float y = ofMap(value[1], param.getMin()[1], param.getMax()[1], max.y, min.y, true);
        drawList->AddCircleFilled(ImVec2(x, y), 5.0f, IM_COL32(widget.color.r, widget.color.g, widget.color.b, 255));
        if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0)){
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            value[0] = ofMap(mouse.x, min.x, max.x, param.getMin()[0], param.getMax()[0], true);
            value[1] = ofMap(mouse.y, max.y, min.y, param.getMin()[1], param.getMax()[1], true);
            param.set(value);
        }
        ImGui::EndGroup();
        return true;
    }

    if(widget.type == CustomGuiWidgetType::MultiToggle){
        auto previousValue = value;
        drawMultiToggleGrid(context, widget, value,
                            [&](float cellValue){ return cellValue > 0.5f; },
                            [&](float& cellValue, bool state){ cellValue = state ? 1.0f : 0.0f; });
        if(value != previousValue) param.set(value);
        ImGui::EndGroup();
        return true;
    }

    if(widget.type == CustomGuiWidgetType::MultiSlider){
        bool changed = context.drawMultiSliderWidget(widget, parameter, value, itemSize, interactive);
        if(changed) param.set(value);
        ImGui::EndGroup();
        return true;
    }

    bool changed = false;
    int visibleCount = widget.config.value("visibleCount", (int)value.size());
    visibleCount = ofClamp(visibleCount, 1, (int)value.size());
    const bool vertical = verticalSlider;
    const float itemWidth = widgetItemWidth(itemSize);
    const float rowHeight = std::max(16.0f, itemSize.y / std::max(1, visibleCount));

    if(vertical){
        const float baseCursorX = ImGui::GetCursorPosX();
        const float baseCursorY = ImGui::GetCursorPosY();
        const float spacing = visibleCount > 1 ? std::min(2.0f, itemSize.x / std::max(8.0f, (float)visibleCount * 2.0f)) : 0.0f;
        const float totalSpacing = spacing * std::max(0, visibleCount - 1);
        const float barWidth = std::max(1.0f, (itemSize.x - totalSpacing) / (float)visibleCount);
        const ImVec2 barSize(barWidth, itemSize.y);

        for(int i = 0; i < visibleCount; i++){
            ImGui::PushID(i);
            float min = i < param.getMin().size() ? param.getMin()[i] : 0.0f;
            float max = i < param.getMax().size() ? param.getMax()[i] : 1.0f;
            if(useRange){
                min = widget.config.value("rangeMin", min);
                max = widget.config.value("rangeMax", max);
            }
            ImGui::SetCursorPos(ImVec2(baseCursorX + i * (barWidth + spacing), baseCursorY));
            if(interactive){
                bool itemChanged = ImGui::VSliderFloat("##bar", barSize, &value[i], min, max);
                if(itemChanged){
                    value[i] = quantizeFloatValue(widget, value[i], min, max);
                    changed = true;
                }
            }else{
                float fraction = max != min ? (value[i] - min) / (max - min) : 0.0f;
                context.drawVerticalMeter(barSize, ofClamp(fraction, 0.0f, 1.0f), IM_COL32(widget.color.r, widget.color.g, widget.color.b, 220));
            }
            ImGui::PopID();
        }
        ImGui::SetCursorPos(ImVec2(baseCursorX, baseCursorY + itemSize.y));
    }else{
        for(int i = 0; i < visibleCount; i++){
            ImGui::PushID(i);
            float min = i < param.getMin().size() ? param.getMin()[i] : 0.0f;
            float max = i < param.getMax().size() ? param.getMax()[i] : 1.0f;
            if(useRange){
                min = widget.config.value("rangeMin", min);
                max = widget.config.value("rangeMax", max);
            }
            if(interactive){
                ImGui::SetNextItemWidth(itemWidth);
                bool itemChanged = ImGui::SliderFloat("##bar", &value[i], min, max);
                if(itemChanged){
                    value[i] = quantizeFloatValue(widget, value[i], min, max);
                    changed = true;
                }
            }else{
                float fraction = max != min ? (value[i] - min) / (max - min) : 0.0f;
                ImGui::ProgressBar(ofClamp(fraction, 0.0f, 1.0f), ImVec2(itemWidth, 0), "");
            }
            ImGui::PopID();
            if((i + 1) * rowHeight >= itemSize.y) break;
        }
    }

    if(changed) param.set(value);
    ImGui::EndGroup();
    return true;
}

bool renderIntVectorWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;
    auto& param = parameter->cast<std::vector<int>>().getParameter();
    auto value = param.get();

    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);
    if(widget.type == CustomGuiWidgetType::MultiToggle && !value.empty()){
        auto previousValue = value;
        drawMultiToggleGrid(context, widget, value,
                            [&](int cellValue){ return cellValue > 0; },
                            [&](int& cellValue, bool state){ cellValue = state ? 1 : 0; });
        if(value != previousValue) param.set(value);
    }else{
        ImGui::TextDisabled("Unsupported type");
    }
    ImGui::EndGroup();
    return true;
}

bool renderScalarMultiToggleWidget(CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;

    const ImVec2 itemSize = widgetItemSize(context);
    ImGui::BeginGroup();
    drawWidgetLabel(widget, context.label);

    if(isFloatParameter(*parameter)){
        auto& param = parameter->cast<float>().getParameter();
        float value = param.get();
        const bool active = value > 0.5f;
        if(active) pushToggleOnColors(widget.color);
        if(ImGui::Button("##value", itemSize)){
            param.set(active ? 0.0f : 1.0f);
        }
        if(active) ImGui::PopStyleColor(3);
    }else if(isIntParameter(*parameter)){
        auto& param = parameter->cast<int>().getParameter();
        int value = param.get();
        const bool active = value > 0;
        if(active) pushToggleOnColors(widget.color);
        if(ImGui::Button("##value", itemSize)){
            param.set(active ? 0 : 1);
        }
        if(active) ImGui::PopStyleColor(3);
    }else{
        ImGui::TextDisabled("Unsupported type");
    }

    ImGui::EndGroup();
    return true;
}

void drawMultiToggleProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return;

    if(isFloatVectorParameter(*parameter)){
        auto& param = parameter->cast<std::vector<float>>().getParameter();
        auto values = param.get();
        auto mins = param.getMin();
        auto maxs = param.getMax();
        int vectorSize = std::max(1, (int)values.size());
        if(ImGui::SliderInt("Vector Size", &vectorSize, 1, 64)){
            float fillValue = values.empty() ? 0.0f : values.back();
            float fillMin = mins.empty() ? 0.0f : mins.back();
            float fillMax = maxs.empty() ? 1.0f : maxs.back();
            values.resize(vectorSize, fillValue);
            mins.resize(vectorSize, fillMin);
            maxs.resize(vectorSize, fillMax);
            param.setMin(mins);
            param.setMax(maxs);
            param.set(values);
            widget.config["cols"] = vectorSize;
            widget.config["rows"] = 1;
            context.container.markCustomGuisDirty();
        }
    }else if(isIntVectorParameter(*parameter)){
        auto& param = parameter->cast<std::vector<int>>().getParameter();
        auto values = param.get();
        auto mins = param.getMin();
        auto maxs = param.getMax();
        int vectorSize = std::max(1, (int)values.size());
        if(ImGui::SliderInt("Vector Size", &vectorSize, 1, 64)){
            int fillValue = values.empty() ? 0 : values.back();
            int fillMin = mins.empty() ? 0 : mins.back();
            int fillMax = maxs.empty() ? 1 : maxs.back();
            values.resize(vectorSize, fillValue);
            mins.resize(vectorSize, fillMin);
            maxs.resize(vectorSize, fillMax);
            param.setMin(mins);
            param.setMax(maxs);
            param.set(values);
            widget.config["cols"] = vectorSize;
            widget.config["rows"] = 1;
            context.container.markCustomGuisDirty();
        }
    }
}

void drawMultiSliderProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr || !isFloatVectorParameter(*parameter)) return;

    int maxCount = (int)parameter->cast<std::vector<float>>().getParameter().get().size();
    int visibleCount = widget.config.value("visibleCount", maxCount);
    if(ImGui::SliderInt("Num Sliders", &visibleCount, 1, std::max(1, maxCount))){
        widget.config["visibleCount"] = visibleCount;
        context.container.markCustomGuisDirty();
    }

    bool vertical = widget.config.value("vertical", true);
    if(ImGui::Checkbox("Vertical", &vertical)){
        widget.config["vertical"] = vertical;
        context.container.markCustomGuisDirty();
    }

    bool interactive = ofxOceanodeCustomGuiWidgets::isInteractive(widget, parameter);
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
            context.container.markCustomGuisDirty();
        }
    }
}

void drawPianoKeyboardProperties(CustomGuiWidgetPropertiesContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter*)
{
    int loNote = ofClamp(widget.config.value("loNote", 48), 0, 127);
    int hiNote = ofClamp(widget.config.value("hiNote", 72), 0, 127);
    bool dimOutsidePianoRange = widget.config.value("dimOutsidePianoRange", false);

    if(ImGui::SliderInt("Lo Note", &loNote, 0, 127)){
        widget.config["loNote"] = loNote;
        context.container.markCustomGuisDirty();
    }
    if(ImGui::SliderInt("Hi Note", &hiNote, 0, 127)){
        widget.config["hiNote"] = hiNote;
        context.container.markCustomGuisDirty();
    }
    if(ImGui::Checkbox("Dim Outside 88-Key", &dimOutsidePianoRange)){
        widget.config["dimOutsidePianoRange"] = dimOutsidePianoRange;
        context.container.markCustomGuisDirty();
    }
}

} // namespace

namespace ofxOceanodeCustomGuiArrayWidgets {

void registerWidgets(ofxOceanodeCustomGuiWidgetRegistry& registry)
{
    registerWidget(registry, CustomGuiWidgetType::MultiSlider,
                   supportsMultiSliderWidget,
                   initializeMultiSliderWidget,
                   renderFloatVectorWidget,
                   drawMultiSliderProperties);

    registerWidget(registry, CustomGuiWidgetType::MultiToggle,
                   supportsMultiToggleWidget,
                   initializeMultiToggleWidget,
                   [](CustomGuiWidgetRenderContext& context, CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter){
                       if(parameter == nullptr) return false;
                       if(isFloatParameter(*parameter) || isIntParameter(*parameter)) return renderScalarMultiToggleWidget(context, widget, parameter);
                       return isFloatVectorParameter(*parameter) ? renderFloatVectorWidget(context, widget, parameter) : renderIntVectorWidget(context, widget, parameter);
                   },
                   drawMultiToggleProperties);

    registerWidget(registry, CustomGuiWidgetType::PianoKeyboard,
                   supportsPianoKeyboardWidget,
                   initializePianoKeyboardWidget,
                   renderPianoKeyboardWidget,
                   drawPianoKeyboardProperties);

    registerWidget(registry, CustomGuiWidgetType::XYPad,
                   supportsXYPadWidget,
                   initializeXYPadWidget,
                   renderFloatVectorWidget);
}

} // namespace ofxOceanodeCustomGuiArrayWidgets

