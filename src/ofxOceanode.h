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
#include "bufferNode.h"
#include "bufferHeader.h"

class ofxOceanode {
public:
    ofxOceanode();
    ~ofxOceanode(){};
    
    void setup();
    
    void update();
    
    void draw();
    
    void exit();
    
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
    
    template<typename T1, typename T2 = T1>
    void registerTypeWithBufferAndHeader(string name = typeid(T1).name(), T1 defaultValue = T1(),
                      std::function<void(T1&, T2&)> bufferAssignFunction = [](T1 &data, T2 &container){container = data;},
                      std::function<T1(T2&)> bufferReturnFunction = [](T2 &container)->T1{return container;},
                      std::function<bool(T1&)> bufferCheckFunction = [](T1 &data)->bool{return true;}){
        typesRegistry->registerType<T1>();
        nodeRegistry->registerModel<router<T1>>("Router", name, defaultValue);
        nodeRegistry->registerModel<portal<T1>>("Portal", name, defaultValue);
        nodeRegistry->registerModel<bufferNode<T1, T2>>("Buffer", name, defaultValue, false, bufferAssignFunction, bufferReturnFunction, bufferCheckFunction);
        typesRegistry->registerType<buffer<T1, T2>*>();
        nodeRegistry->registerModel<router<buffer<T1, T2>*>>("Router", "buffer_" + name, nullptr);
        nodeRegistry->registerModel<portal<buffer<T1, T2>*>>("Portal", "buffer_" + name, nullptr);
        nodeRegistry->registerModel<bufferHeader<T1, T2>>("Header", name, defaultValue);
//        registerType<std::vector<T1>>("v_" + name, std::vector<T1>(1, defaultValue));
    };
    
    template<typename T>
    shared_ptr<T> addController(){
        return controls->addController<T>();
    }
    
    template<typename T>
    shared_ptr<T> getController(){
        return controls->get<T>();
    }

    void loadPreset(std::string presetPathRelativeToData); //call preset via path
    
    void loadPreset(std::string bank, std::string name); //call preset via bank and name
    
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
    
    void disableRenderAll();
    void disableRenderHistograms();
    
    void setShowMode(bool b){showMode = b;}
    void toggleShowMode(){showMode = !showMode;}
    
    void saveConfig();
    void loadConfig();

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
    bool showMode;
    
    void drawShowModeWindow();
    
    ofParameter<int> oscReceiverPort;
#ifdef OFXOCEANODE_USE_OSC
    ofxOscReceiver oscReceiver;
    ofEventListener receiverPortChanged;
    
    void receiveOsc();
#endif
};

#endif /* ofxOceanode_h */
