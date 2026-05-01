#ifndef ofxOceanodeCustomGuiLayout_h
#define ofxOceanodeCustomGuiLayout_h

#include "ofColor.h"
#include "ofJson.h"
#include "ofMain.h"

enum class CustomGuiWidgetType {
    Slider,
    Knob,
    DragNumber,
    Toggle,
    Button,
    MultiSlider,
    ToggleGrid,
    XYPad,
    Waveform,
    TextDisplay,
    Label,
    Dropdown,
    BackgroundPanel,
    Text,
    Image
};

struct CustomGuiParameterReference {
    std::string parameterPath;
    std::string nodeDisplayName;
    std::string parameterDisplayName;
};

struct CustomGuiWidget {
    CustomGuiParameterReference parameterRef;
    CustomGuiWidgetType type = CustomGuiWidgetType::Slider;
    int gridX = 0;
    int gridY = 0;
    int spanW = 1;
    int spanH = 1;
    std::string label;
    ofColor color = ofColor::white;
    ofJson config = ofJson::object();
};

struct CustomGuiLayout {
    int columns = 4;
    int rows = 3;
    float cellWidth = 80.0f;
    float cellHeight = 60.0f;
    float zoom = 1.0f;
    std::vector<CustomGuiWidget> widgets;
};

struct CustomGuiWindowState {
    bool hasConfig = false;
    float posX = 100.0f;
    float posY = 100.0f;
    float width = 400.0f;
    float height = 300.0f;
    bool isOpen = false;
};

struct CustomGuiPanelData {
    std::string id;
    std::string name;
    bool designMode = false;
    CustomGuiLayout layout;
    CustomGuiWindowState windowState;
};

inline std::string customGuiWidgetTypeToString(CustomGuiWidgetType type)
{
    switch(type){
        case CustomGuiWidgetType::Slider: return "Slider";
        case CustomGuiWidgetType::Knob: return "Knob";
        case CustomGuiWidgetType::DragNumber: return "DragNumber";
        case CustomGuiWidgetType::Toggle: return "Toggle";
        case CustomGuiWidgetType::Button: return "Button";
        case CustomGuiWidgetType::MultiSlider: return "MultiSlider";
        case CustomGuiWidgetType::ToggleGrid: return "ToggleGrid";
        case CustomGuiWidgetType::XYPad: return "XYPad";
        case CustomGuiWidgetType::Waveform: return "Waveform";
        case CustomGuiWidgetType::TextDisplay: return "TextDisplay";
        case CustomGuiWidgetType::Label: return "Label";
        case CustomGuiWidgetType::Dropdown: return "Dropdown";
        case CustomGuiWidgetType::BackgroundPanel: return "BackgroundPanel";
        case CustomGuiWidgetType::Text: return "Text";
        case CustomGuiWidgetType::Image: return "Image";
    }
    return "Slider";
}

inline CustomGuiWidgetType customGuiWidgetTypeFromString(const std::string& type)
{
    if(type == "Knob") return CustomGuiWidgetType::Knob;
    if(type == "DragNumber") return CustomGuiWidgetType::DragNumber;
    if(type == "Toggle") return CustomGuiWidgetType::Toggle;
    if(type == "Button") return CustomGuiWidgetType::Button;
    if(type == "MultiSlider") return CustomGuiWidgetType::MultiSlider;
    if(type == "ToggleGrid") return CustomGuiWidgetType::ToggleGrid;
    if(type == "XYPad") return CustomGuiWidgetType::XYPad;
    if(type == "Waveform") return CustomGuiWidgetType::Waveform;
    if(type == "TextDisplay") return CustomGuiWidgetType::TextDisplay;
    if(type == "Label") return CustomGuiWidgetType::Label;
    if(type == "Dropdown") return CustomGuiWidgetType::Dropdown;
    if(type == "BackgroundPanel") return CustomGuiWidgetType::BackgroundPanel;
    if(type == "Text") return CustomGuiWidgetType::Text;
    if(type == "Image") return CustomGuiWidgetType::Image;
    return CustomGuiWidgetType::Slider;
}

inline ofJson customGuiColorToJson(const ofColor& color)
{
    return ofJson::array({color.r, color.g, color.b, color.a});
}

inline ofColor customGuiColorFromJson(const ofJson& json, const ofColor& fallback = ofColor::white)
{
    if(!json.is_array() || json.size() < 3) return fallback;
    int a = json.size() > 3 ? json[3].get<int>() : 255;
    return ofColor(json[0].get<int>(), json[1].get<int>(), json[2].get<int>(), a);
}

inline ofJson customGuiParameterReferenceToJson(const CustomGuiParameterReference& ref)
{
    ofJson json;
    json["parameterPath"] = ref.parameterPath;
    json["nodeDisplayName"] = ref.nodeDisplayName;
    json["parameterDisplayName"] = ref.parameterDisplayName;
    return json;
}

inline CustomGuiParameterReference customGuiParameterReferenceFromJson(const ofJson& json)
{
    CustomGuiParameterReference ref;
    if(json.contains("parameterPath")) ref.parameterPath = json["parameterPath"].get<std::string>();
    if(json.contains("nodeDisplayName")) ref.nodeDisplayName = json["nodeDisplayName"].get<std::string>();
    if(json.contains("parameterDisplayName")) ref.parameterDisplayName = json["parameterDisplayName"].get<std::string>();
    return ref;
}

inline ofJson customGuiWidgetToJson(const CustomGuiWidget& widget)
{
    ofJson json;
    json["parameterRef"] = customGuiParameterReferenceToJson(widget.parameterRef);
    json["type"] = customGuiWidgetTypeToString(widget.type);
    json["gridX"] = widget.gridX;
    json["gridY"] = widget.gridY;
    json["spanW"] = widget.spanW;
    json["spanH"] = widget.spanH;
    json["label"] = widget.label;
    json["color"] = customGuiColorToJson(widget.color);
    json["config"] = widget.config.is_null() ? ofJson::object() : widget.config;
    return json;
}

inline CustomGuiWidget customGuiWidgetFromJson(const ofJson& json)
{
    CustomGuiWidget widget;
    if(json.contains("parameterRef")) {
        widget.parameterRef = customGuiParameterReferenceFromJson(json["parameterRef"]);
    } else if(json.contains("parameterName")) {
        widget.parameterRef.parameterDisplayName = json["parameterName"].get<std::string>();
    }
    if(json.contains("type")) widget.type = customGuiWidgetTypeFromString(json["type"].get<std::string>());
    if(json.contains("gridX")) widget.gridX = json["gridX"].get<int>();
    if(json.contains("gridY")) widget.gridY = json["gridY"].get<int>();
    if(json.contains("spanW")) widget.spanW = std::max(1, json["spanW"].get<int>());
    if(json.contains("spanH")) widget.spanH = std::max(1, json["spanH"].get<int>());
    if(json.contains("label")) widget.label = json["label"].get<std::string>();
    if(json.contains("color")) widget.color = customGuiColorFromJson(json["color"]);
    if(json.contains("config")) widget.config = json["config"];
    return widget;
}

inline ofJson customGuiLayoutToJson(const CustomGuiLayout& layout)
{
    ofJson json;
    json["columns"] = layout.columns;
    json["rows"] = layout.rows;
    json["cellWidth"] = layout.cellWidth;
    json["cellHeight"] = layout.cellHeight;
    json["zoom"] = layout.zoom;
    json["widgets"] = ofJson::array();
    for(const auto& widget : layout.widgets){
        json["widgets"].push_back(customGuiWidgetToJson(widget));
    }
    return json;
}

inline CustomGuiLayout customGuiLayoutFromJson(const ofJson& json)
{
    CustomGuiLayout layout;
    if(json.contains("columns")) layout.columns = std::max(1, json["columns"].get<int>());
    if(json.contains("rows")) layout.rows = std::max(1, json["rows"].get<int>());
    if(json.contains("cellWidth")) layout.cellWidth = std::max(20.0f, json["cellWidth"].get<float>());
    if(json.contains("cellHeight")) layout.cellHeight = std::max(20.0f, json["cellHeight"].get<float>());
    if(json.contains("zoom")) layout.zoom = ofClamp(json["zoom"].get<float>(), 0.25f, 4.0f);
    if(json.contains("widgets") && json["widgets"].is_array()){
        for(const auto& widgetJson : json["widgets"]){
            layout.widgets.push_back(customGuiWidgetFromJson(widgetJson));
        }
    }
    return layout;
}

inline ofJson customGuiWindowStateToJson(const CustomGuiWindowState& state)
{
    ofJson json;
    json["posX"] = state.posX;
    json["posY"] = state.posY;
    json["width"] = state.width;
    json["height"] = state.height;
    json["isOpen"] = state.isOpen;
    return json;
}

inline CustomGuiWindowState customGuiWindowStateFromJson(const ofJson& json)
{
    CustomGuiWindowState state;
    state.hasConfig = true;
    if(json.contains("posX")) state.posX = json["posX"].get<float>();
    if(json.contains("posY")) state.posY = json["posY"].get<float>();
    if(json.contains("width")) state.width = json["width"].get<float>();
    if(json.contains("height")) state.height = json["height"].get<float>();
    if(json.contains("isOpen")) state.isOpen = json["isOpen"].get<bool>();
    return state;
}

inline ofJson customGuiPanelDataToJson(const CustomGuiPanelData& panel)
{
    ofJson json;
    json["id"] = panel.id;
    json["name"] = panel.name;
    json["designMode"] = panel.designMode;
    json["windowState"] = customGuiWindowStateToJson(panel.windowState);
    json["layout"] = customGuiLayoutToJson(panel.layout);
    return json;
}

inline CustomGuiPanelData customGuiPanelDataFromJson(const ofJson& json)
{
    CustomGuiPanelData panel;
    if(json.contains("id")) panel.id = json["id"].get<std::string>();
    if(json.contains("name")) panel.name = json["name"].get<std::string>();
    if(json.contains("designMode")) panel.designMode = json["designMode"].get<bool>();
    if(json.contains("windowState")) panel.windowState = customGuiWindowStateFromJson(json["windowState"]);
    if(json.contains("layout")) panel.layout = customGuiLayoutFromJson(json["layout"]);
    return panel;
}

inline ofJson customGuiPanelsToJson(const std::vector<CustomGuiPanelData>& panels)
{
    ofJson json;
    json["panels"] = ofJson::array();
    for(const auto& panel : panels){
        json["panels"].push_back(customGuiPanelDataToJson(panel));
    }
    return json;
}

inline std::vector<CustomGuiPanelData> customGuiPanelsFromJson(const ofJson& json)
{
    std::vector<CustomGuiPanelData> panels;
    if(json.contains("panels") && json["panels"].is_array()){
        for(const auto& panelJson : json["panels"]){
            panels.push_back(customGuiPanelDataFromJson(panelJson));
        }
    } else if(json.contains("columns")) {
        CustomGuiPanelData panel;
        panel.id = "legacy-panel";
        panel.name = "Custom GUI";
        panel.layout = customGuiLayoutFromJson(json);
        panels.push_back(panel);
    }
    return panels;
}

#endif
