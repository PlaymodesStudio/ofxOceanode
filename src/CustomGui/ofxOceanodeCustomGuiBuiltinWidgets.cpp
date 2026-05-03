#ifndef OFXOCEANODE_HEADLESS

#include "CustomGui/ofxOceanodeCustomGuiBuiltinWidgets.h"
#include "CustomGui/Widgets/ofxOceanodeCustomGuiArrayWidgets.h"
#include "CustomGui/Widgets/ofxOceanodeCustomGuiSignalWidgets.h"
#include "CustomGui/Widgets/ofxOceanodeCustomGuiStaticWidgets.h"
#include "CustomGui/Widgets/ofxOceanodeCustomGuiValueWidgets.h"
#include "CustomGui/ofxOceanodeCustomGuiWidgetRegistry.h"

namespace ofxOceanodeCustomGuiBuiltinWidgets {

void registerWidgets(ofxOceanodeCustomGuiWidgetRegistry& registry)
{
    ofxOceanodeCustomGuiStaticWidgets::registerWidgets(registry);
    ofxOceanodeCustomGuiValueWidgets::registerWidgets(registry);
    ofxOceanodeCustomGuiArrayWidgets::registerWidgets(registry);
    ofxOceanodeCustomGuiSignalWidgets::registerWidgets(registry);
}

}

#endif
