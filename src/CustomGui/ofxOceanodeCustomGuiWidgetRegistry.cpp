#ifndef OFXOCEANODE_HEADLESS

#include "CustomGui/ofxOceanodeCustomGuiWidgetRegistry.h"
#include "CustomGui/ofxOceanodeCustomGuiBuiltinWidgets.h"
#include "ofxOceanodeParameter.h"

ofxOceanodeCustomGuiWidgetRegistry& ofxOceanodeCustomGuiWidgetRegistry::instance()
{
    static ofxOceanodeCustomGuiWidgetRegistry registry;
    return registry;
}

ofxOceanodeCustomGuiWidgetRegistry::ofxOceanodeCustomGuiWidgetRegistry()
{
    ofxOceanodeCustomGuiBuiltinWidgets::registerWidgets(*this);
}

void ofxOceanodeCustomGuiWidgetRegistry::registerWidget(const CustomGuiWidgetDefinition& definition)
{
    definitions[(int)definition.type] = definition;
}

const CustomGuiWidgetDefinition* ofxOceanodeCustomGuiWidgetRegistry::getWidget(CustomGuiWidgetType type) const
{
    auto it = definitions.find((int)type);
    if(it == definitions.end()) return nullptr;
    return &it->second;
}

std::vector<CustomGuiWidgetType> ofxOceanodeCustomGuiWidgetRegistry::getCompatibleWidgets(ofxOceanodeAbstractParameter& parameter) const
{
    std::vector<CustomGuiWidgetType> result;
    for(int typeIndex = (int)CustomGuiWidgetType::Slider; typeIndex <= (int)CustomGuiWidgetType::CustomRegion; typeIndex++){
        auto it = definitions.find(typeIndex);
        if(it == definitions.end()) continue;
        if(it->second.supportsParameter && it->second.supportsParameter(parameter)){
            result.push_back((CustomGuiWidgetType)typeIndex);
        }
    }
    return result;
}

CustomGuiWidgetType ofxOceanodeCustomGuiWidgetRegistry::getDefaultWidgetType(ofxOceanodeAbstractParameter& parameter) const
{
    auto compatible = getCompatibleWidgets(parameter);
    if(compatible.empty()) return CustomGuiWidgetType::Label;
    return compatible.front();
}

#endif
