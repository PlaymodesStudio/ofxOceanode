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
    bool hasAnySelectedNode();
	
	static void registerInspectorDropdown(const std::string& nodeTypeName, const std::string& paramName, const std::vector<std::string>& options);
	
	static std::vector<std::string> getInspectorDropdownOptions(const std::string& nodeTypeName, const std::string& paramName);
	
    
private:
	static std::map<std::string, std::vector<std::string>> inspectorDropdownOptions;

    shared_ptr<ofxOceanodeContainer> container;
    ofxOceanodeCanvas* canvas;
};

#endif /* ofxOceanodeInspectorController_h */
