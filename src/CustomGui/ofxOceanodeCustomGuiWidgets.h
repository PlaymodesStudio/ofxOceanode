#ifndef ofxOceanodeCustomGuiWidgets_h
#define ofxOceanodeCustomGuiWidgets_h


#include "CustomGui/ofxOceanodeCustomGuiLayout.h"

class ofxOceanodeAbstractParameter;

namespace ofxOceanodeCustomGuiWidgets {
    inline bool canResizeVector(CustomGuiWidgetType type) {
        return type == CustomGuiWidgetType::MultiSlider || type == CustomGuiWidgetType::MultiToggle;
    }
    bool defaultInteractiveState(ofxOceanodeAbstractParameter& parameter);
    bool isInteractive(const CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter);
}


#endif /* ofxOceanodeCustomGuiWidgets_h */
