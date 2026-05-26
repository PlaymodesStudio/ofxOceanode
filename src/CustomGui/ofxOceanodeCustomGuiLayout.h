#ifndef ofxOceanodeCustomGuiLayout_h
#define ofxOceanodeCustomGuiLayout_h

#include "ofColor.h"
#include "ofJson.h"
#include "ofMain.h"
#include <map>

enum class CustomGuiWidgetType {
    Slider,
    Knob,
    DragNumber,
	Toggle,
	Button,
    ColorSwatch,
	MultiSlider,
	MultiToggle,
    PianoKeyboard,
	XYPad,
    Waveform,
    VUMeter,
    FFT,
    TextDisplay,
    FileBrowser,
    Label,
    Dropdown,
    CustomDropdown,
    ButtonMatrix,
	BackgroundPanel,
	Text,
    Line,
	Texture,
	Image,
    SnapshotMatrix,
    CustomRegion
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
    float cellWidth = 20.0f;
    float cellHeight = 20.0f;
    float zoom = 1.0f;
    ofColor backgroundColor = ofColor(24, 24, 24, 255);
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

struct CustomGuiSnapshotValue {
    std::string type;
    ofJson value;
};

struct CustomGuiSnapshotData {
    std::string id;
    std::string name;
    int slot = -1;
    std::map<std::string, CustomGuiSnapshotValue> parameterValues;
};

struct CustomGuiSnapshotBank {
    std::string customGuiId;
    std::string customGuiName;
    std::string currentSnapshotId;
    std::vector<CustomGuiSnapshotData> snapshots;
};

inline std::string customGuiWidgetTypeToString(CustomGuiWidgetType type)
{
    switch(type){
        case CustomGuiWidgetType::Slider: return "Slider";
        case CustomGuiWidgetType::Knob: return "Knob";
        case CustomGuiWidgetType::DragNumber: return "DragNumber";
        case CustomGuiWidgetType::Toggle: return "Toggle";
        case CustomGuiWidgetType::Button: return "Button";
        case CustomGuiWidgetType::ColorSwatch: return "ColorSwatch";
        case CustomGuiWidgetType::MultiSlider: return "MultiSlider";
		case CustomGuiWidgetType::MultiToggle: return "MultiToggle";
        case CustomGuiWidgetType::PianoKeyboard: return "PianoKeyboard";
        case CustomGuiWidgetType::XYPad: return "XYPad";
        case CustomGuiWidgetType::Waveform: return "Waveform";
        case CustomGuiWidgetType::VUMeter: return "VUMeter";
        case CustomGuiWidgetType::FFT: return "FFT";
        case CustomGuiWidgetType::TextDisplay: return "TextDisplay";
        case CustomGuiWidgetType::FileBrowser: return "FileBrowser";
        case CustomGuiWidgetType::Label: return "Label";
        case CustomGuiWidgetType::Dropdown: return "Dropdown";
        case CustomGuiWidgetType::CustomDropdown: return "CustomDropdown";
        case CustomGuiWidgetType::ButtonMatrix: return "ButtonMatrix";
	        case CustomGuiWidgetType::BackgroundPanel: return "BackgroundPanel";
	        case CustomGuiWidgetType::Text: return "Text";
            case CustomGuiWidgetType::Line: return "Line";
	        case CustomGuiWidgetType::Texture: return "Texture";
	        case CustomGuiWidgetType::Image: return "Image";
            case CustomGuiWidgetType::SnapshotMatrix: return "SnapshotMatrix";
            case CustomGuiWidgetType::CustomRegion: return "CustomRegion";
    }
    return "Slider";
}

inline CustomGuiWidgetType customGuiWidgetTypeFromString(const std::string& type)
{
    if(type == "Knob") return CustomGuiWidgetType::Knob;
    if(type == "DragNumber") return CustomGuiWidgetType::DragNumber;
    if(type == "Toggle") return CustomGuiWidgetType::Toggle;
	if(type == "Button") return CustomGuiWidgetType::Button;
    if(type == "ColorSwatch") return CustomGuiWidgetType::ColorSwatch;
	if(type == "MultiSlider") return CustomGuiWidgetType::MultiSlider;
	if(type == "MultiToggle" || type == "ToggleGrid") return CustomGuiWidgetType::MultiToggle;
    if(type == "PianoKeyboard") return CustomGuiWidgetType::PianoKeyboard;
    if(type == "XYPad") return CustomGuiWidgetType::XYPad;
    if(type == "Waveform") return CustomGuiWidgetType::Waveform;
    if(type == "VUMeter") return CustomGuiWidgetType::VUMeter;
    if(type == "FFT") return CustomGuiWidgetType::FFT;
    if(type == "TextDisplay") return CustomGuiWidgetType::TextDisplay;
    if(type == "FileBrowser") return CustomGuiWidgetType::FileBrowser;
    if(type == "Label") return CustomGuiWidgetType::Label;
    if(type == "Dropdown") return CustomGuiWidgetType::Dropdown;
    if(type == "CustomDropdown") return CustomGuiWidgetType::CustomDropdown;
    if(type == "ButtonMatrix") return CustomGuiWidgetType::ButtonMatrix;
	    if(type == "BackgroundPanel") return CustomGuiWidgetType::BackgroundPanel;
	    if(type == "Text") return CustomGuiWidgetType::Text;
        if(type == "Line") return CustomGuiWidgetType::Line;
	    if(type == "Texture") return CustomGuiWidgetType::Texture;
	    if(type == "Image") return CustomGuiWidgetType::Image;
    if(type == "SnapshotMatrix") return CustomGuiWidgetType::SnapshotMatrix;
    if(type == "CustomRegion") return CustomGuiWidgetType::CustomRegion;
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
    json["backgroundColor"] = customGuiColorToJson(layout.backgroundColor);
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
    if(json.contains("backgroundColor")) layout.backgroundColor = customGuiColorFromJson(json["backgroundColor"], ofColor(24, 24, 24, 255));
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

inline ofJson customGuiSnapshotValueToJson(const CustomGuiSnapshotValue& snapshotValue)
{
    ofJson json;
    json["type"] = snapshotValue.type;
    json["value"] = snapshotValue.value;
    return json;
}

inline CustomGuiSnapshotValue customGuiSnapshotValueFromJson(const ofJson& json)
{
    CustomGuiSnapshotValue snapshotValue;
    if(json.contains("type")) snapshotValue.type = json["type"].get<std::string>();
    if(json.contains("value")) snapshotValue.value = json["value"];
    return snapshotValue;
}

inline ofJson customGuiSnapshotDataToJson(const CustomGuiSnapshotData& snapshot)
{
    ofJson json;
    json["id"] = snapshot.id;
    json["name"] = snapshot.name;
    json["slot"] = snapshot.slot;
    json["parameterValues"] = ofJson::object();
    for(const auto& pair : snapshot.parameterValues){
        json["parameterValues"][pair.first] = customGuiSnapshotValueToJson(pair.second);
    }
    return json;
}

inline CustomGuiSnapshotData customGuiSnapshotDataFromJson(const ofJson& json)
{
    CustomGuiSnapshotData snapshot;
    if(json.contains("id")) snapshot.id = json["id"].get<std::string>();
    if(json.contains("name")) snapshot.name = json["name"].get<std::string>();
    if(json.contains("slot")) snapshot.slot = json["slot"].get<int>();
    if(json.contains("parameterValues") && json["parameterValues"].is_object()){
        for(auto it = json["parameterValues"].begin(); it != json["parameterValues"].end(); ++it){
            snapshot.parameterValues[it.key()] = customGuiSnapshotValueFromJson(it.value());
        }
    }
    return snapshot;
}

inline ofJson customGuiSnapshotBankToJson(const CustomGuiSnapshotBank& bank)
{
    ofJson json;
    json["customGuiId"] = bank.customGuiId;
    json["customGuiName"] = bank.customGuiName;
    json["currentSnapshotId"] = bank.currentSnapshotId;
    json["snapshots"] = ofJson::array();
    for(const auto& snapshot : bank.snapshots){
        json["snapshots"].push_back(customGuiSnapshotDataToJson(snapshot));
    }
    return json;
}

inline CustomGuiSnapshotBank customGuiSnapshotBankFromJson(const ofJson& json)
{
    CustomGuiSnapshotBank bank;
    if(json.contains("customGuiId")) bank.customGuiId = json["customGuiId"].get<std::string>();
    if(json.contains("customGuiName")) bank.customGuiName = json["customGuiName"].get<std::string>();
    if(json.contains("currentSnapshotId")) bank.currentSnapshotId = json["currentSnapshotId"].get<std::string>();
    if(json.contains("snapshots") && json["snapshots"].is_array()){
        for(const auto& snapshotJson : json["snapshots"]){
            bank.snapshots.push_back(customGuiSnapshotDataFromJson(snapshotJson));
        }
    }
    return bank;
}

inline ofJson customGuiSnapshotBanksToJson(const std::vector<CustomGuiSnapshotBank>& banks)
{
    ofJson json;
    json["banks"] = ofJson::array();
    for(const auto& bank : banks){
        json["banks"].push_back(customGuiSnapshotBankToJson(bank));
    }
    return json;
}

inline std::vector<CustomGuiSnapshotBank> customGuiSnapshotBanksFromJson(const ofJson& json)
{
    std::vector<CustomGuiSnapshotBank> banks;
    if(json.contains("banks") && json["banks"].is_array()){
        for(const auto& bankJson : json["banks"]){
            banks.push_back(customGuiSnapshotBankFromJson(bankJson));
        }
    }
    return banks;
}

#endif
