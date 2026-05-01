#ifndef OFXOCEANODE_HEADLESS

#include "CustomGui/ofxOceanodeCustomGuiWidgets.h"
#include "ofxOceanodeParameter.h"

namespace ofxOceanodeCustomGuiWidgets {

bool defaultInteractiveState(ofxOceanodeAbstractParameter& parameter)
{
    if(parameter.getFlags() & ofxOceanodeParameterFlags_ReadOnly) return false;
    if(parameter.hasInConnection()) return false;
    return true;
}

bool isInteractive(const CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter)
{
    if(parameter == nullptr) return false;
    if(widget.config.contains("interactive")) return widget.config["interactive"].get<bool>();
    return defaultInteractiveState(*parameter);
}

}

#endif
