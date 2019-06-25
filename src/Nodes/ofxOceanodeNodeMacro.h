//
//  ofxOceanodeNodeMacro.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 20/06/2019.
//

#ifndef ofxOceanodeNodeMacro_h
#define ofxOceanodeNodeMacro_h

#include "ofxOceanode.h"
#include "ofxOceanodeNodeModelExternalWindow.h"

template<typename T, typename Enable = void>
class router : public ofxOceanodeNodeModel{
public:
    router(string name) : ofxOceanodeNodeModel(name){};
    void setup(){
        setupValueParameter();
        addParameterToGroupAndInfo(value).acceptInConnection = false;
        addParameterToGroupAndInfo(min.set("Min", "0")).acceptInConnection = false;
        addParameterToGroupAndInfo(max.set("Max", "1")).acceptInConnection = false;
    }
    
    void setupValueParameter(){
        value.set(T());
        value.setMin({0});
        value.setMax({1});
    }
    
protected:
    ofParameter<T> value;
    ofParameter<string> min;
    ofParameter<string> max;
};

template<typename T>
class inlet : public router<T>{
public:
    inlet() : router<T>("Inlet"){};
    void setup(){
        this->value.setName("Input");
        router<T>::setup();
    }
};

template<typename T>
class outlet : public router<T>{
public:
    outlet() : router<T>("Outlet"){};
    void setup(){
        this->value.setName("Output");
        router<T>::setup();
    }
};

class ofxOceanodeNodeMacro : public ofxOceanodeNodeModelExternalWindow{
public:
    ofxOceanodeNodeMacro();
    ~ofxOceanodeNodeMacro(){
        if(canvas != nullptr) delete canvas;
    };
    
    void setup();
    void setContainer(ofxOceanodeContainer* container);
    
    void setupForExternalWindow();
    void closeExternalWindow(ofEventArgs &e);
    
    void presetSave(ofJson &json);
    void loadBeforeConnections(ofJson &json);
    
private:
    ofxOceanodeCanvas* canvas;
    shared_ptr<ofxOceanodeContainer> container;
    shared_ptr<ofxOceanodeNodeRegistry> registry;
    
    ofEventListener newNodeListener;
    vector<ofParameter<float>> inputs;
    vector<ofParameter<float>> outputs;
    
    std::unordered_map<string, ofEventListeners> inoutListeners;
    ofEventListeners deleteListeners;
};

#endif /* ofxOceanodeNodeMacro_h */
