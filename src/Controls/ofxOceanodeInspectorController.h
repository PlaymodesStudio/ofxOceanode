//
//  ofxOceanodeInspectorController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 08/01/2021.
//

#ifndef ofxOceanodeInspectorController_h
#define ofxOceanodeInspectorController_h

#include "ofxOceanodeBaseController.h"
#include "ofxOceanodeParameter.h"

class ofxOceanodeInspectorController: public ofxOceanodeBaseController{
public:

    ofxOceanodeInspectorController(shared_ptr<ofxOceanodeContainer> _container, ofxOceanodeCanvas* _canvas) : container(_container), canvas(_canvas), ofxOceanodeBaseController("Inspector"){};
    ~ofxOceanodeInspectorController(){};
    
    void draw();
	
	static void registerInspectorDropdown(const std::string& nodeTypeName, const std::string& paramName, const std::vector<std::string>& options);
	
	static std::vector<std::string> getInspectorDropdownOptions(const std::string& nodeTypeName, const std::string& paramName);
	
    
private:
    bool valueHasBeenReseted = false;
	
	static std::map<std::string, std::vector<std::string>> inspectorDropdownOptions;

    template <typename T>
    bool checkSameValue(ofxOceanodeAbstractParameter &a, ofxOceanodeAbstractParameter &b){
        if(a.valueType() != typeid(T).name()) return false;
        if(b.valueType() != typeid(T).name()) return false;
        return a.cast<T>().getParameter().get() == b.cast<T>().getParameter().get();
    }
    
    template <typename T>
    bool checkSameValue(ofAbstractParameter &a, ofAbstractParameter &b){
        if(a.valueType() != typeid(T).name()) return false;
        if(b.valueType() != typeid(T).name()) return false;
        return a.cast<T>().get() == b.cast<T>().get();
    }
    
    template <typename T>
    bool checkSameRange(ofxOceanodeAbstractParameter &a, ofxOceanodeAbstractParameter &b){
        if(a.valueType() != typeid(T).name()) return false;
        if(b.valueType() != typeid(T).name()) return false;
        return a.cast<T>().getParameter().getMin() == b.cast<T>().getParameter().getMin() && a.cast<T>().getParameter().getMax() == b.cast<T>().getParameter().getMax();
    }
    
    template <typename T>
    bool checkSameRange(ofAbstractParameter &a, ofAbstractParameter &b){
        if(a.valueType() != typeid(T).name()) return false;
        if(b.valueType() != typeid(T).name()) return false;
        return a.cast<T>().getMin() == b.cast<T>().getMin() && a.cast<T>().getMax() == b.cast<T>().getMax();
    }

    shared_ptr<ofxOceanodeContainer> container;
    ofxOceanodeCanvas* canvas;
	

};

#endif /* ofxOceanodeInspectorController_h */
