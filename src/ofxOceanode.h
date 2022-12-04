//
//  ofxOceanode.h
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#ifndef ofxOceanode_h
#define ofxOceanode_h

#ifndef OFXOCEANODE_HEADLESS
#include "ofxOceanodeCanvas.h"
#include "ofxOceanodeControls.h"
#endif
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeTypesRegistry.h"
#include "ofxOceanodeScope.h"
#include "ofxOceanodeTime.h"

//#include "ofxOceanodeAbstractTimeline.h"

#include "ofxImGuiSimple.h"
#include "imgui.h"
#include "router.h"
#include "portal.h"

class ofxOceanode {
public:
    ofxOceanode();
    ~ofxOceanode(){};
    
    void setup();
    
    void update();
    
    void draw();
    
    template<typename T>
    void registerModel(string category = "Default"){
        nodeRegistry->registerModel<T>(category);
    };
    
    template<typename T, typename... Args>
    void registerModel(string category, Args... args){
        nodeRegistry->registerModel<T, Args...>(category, args...);
    };
    
    template<typename T>
    void registerType(string name = typeid(T).name(), T defaultValue = T()){
        typesRegistry->registerType<T>();
        nodeRegistry->registerModel<router<T>>("Router", name, defaultValue);
		nodeRegistry->registerModel<portal<T>>("Portal", name, defaultValue);
    };
    
    template<typename T>
    shared_ptr<T> addController(){
        return controls->addController<T>();
    }
    
    template<typename T>
    shared_ptr<T> getController(){
        return controls->get<T>();
    }

	void loadPreset(std::string preset) {
		container->loadPreset(preset);
	}
    
   template<typename T>
    void registerScope(std::function<void(ofxOceanodeAbstractParameter* p, ImVec2 size)> func){
        ofxOceanodeScope::getInstance()->addScopeFunc([func](ofxOceanodeAbstractParameter *p, ImVec2 size) -> bool{
            if(p->valueType() == typeid(T).name())
            {
                func(p, size);
                return true;
            }
            return false;
        });
    }
    
    void togglePlay(){
        oceanodeTime->togglePlay();
    }
    
    //TODO: Clean
    // From imgui_demo.cpp
    //-----------------------------------------------------------------------------
    // [SECTION] Example App: Docking, DockSpace / ShowExampleAppDockSpace()
    //-----------------------------------------------------------------------------
    
    // Demonstrate using DockSpace() to create an explicit docking node within an existing window.
    // Note that you already dock windows into each others _without_ a DockSpace() by just moving windows
    // from their title bar (or by holding SHIFT if io.ConfigDockingWithShift is set).
    // DockSpace() is only useful to construct to a central location for your application.
    void ShowExampleAppDockSpace(bool* p_open);

private:
    ofxOceanodeCanvas canvas;
    shared_ptr<ofxOceanodeContainer> container;
    unique_ptr<ofxOceanodeControls> controls;
    
    shared_ptr<ofxOceanodeNodeRegistry> nodeRegistry;
    shared_ptr<ofxOceanodeTypesRegistry> typesRegistry;
    ofxOceanodeScope* scope;
    
    ofxOceanodeTime* oceanodeTime;
    
//    vector<ofxOceanodeAbstractTimeline> timelines;
    
    ofxImGuiSimple gui;
    void showManualWindow(bool *b);
    void showHelpPopUp();
    bool firstDraw;
    bool settingsLoaded;
};

#endif /* ofxOceanode_h */
