#ifndef ofxOceanodeCustomGuiWidgets_h
#define ofxOceanodeCustomGuiWidgets_h

#ifndef OFXOCEANODE_HEADLESS

#include "CustomGui/ofxOceanodeCustomGuiLayout.h"

class ofxOceanodeAbstractParameter;

namespace ofxOceanodeCustomGuiWidgets {
    bool defaultInteractiveState(ofxOceanodeAbstractParameter& parameter);
    bool isInteractive(const CustomGuiWidget& widget, ofxOceanodeAbstractParameter* parameter);
}

#endif

#endif /* ofxOceanodeCustomGuiWidgets_h */
