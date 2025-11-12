//
//  ofxOceanodeNodeModel.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeTypesRegistry.h"
#include "ofxOceanodeContainer.h"

ofColor colorFromString(const std::string& input) {
    // Hash the input string
    std::hash<std::string> hasher;
    auto hashedValue = hasher(input);

    // Use the hashed value to seed the random generator
    std::srand(hashedValue);

    // Generate random HSL values
    float hue = static_cast<float>(std::rand()) / RAND_MAX * 255;
    float saturation = 255;//static_cast<float>(std::rand()) / RAND_MAX * 255;
    float lightness = 255;//static_cast<float>(std::rand()) / RAND_MAX * 255;

    // Return the color
    return ofColor::fromHsb(hue, saturation, lightness);
}

ofxOceanodeNodeModel::ofxOceanodeNodeModel(string _name) : nameIdentifier(_name){
    parameters.setName(_name);
    color = colorFromString(_name);
    numIdentifier = -1;
    flags = 0;
}

void ofxOceanodeNodeModel::setNumIdentifier(unsigned int num){
    numIdentifier = num;
    parameters.setName(nameIdentifier + " " + ofToString(num));
}

shared_ptr<ofxOceanodeAbstractParameter> ofxOceanodeNodeModel::addParameter(ofAbstractParameter& p, ofxOceanodeParameterFlags flags){
    shared_ptr<ofxOceanodeAbstractParameter> oceanodeParam = ofxOceanodeTypesRegistry::getInstance()->createOceanodeAbstractFromAbstract(p);
    oceanodeParam->setFlags(flags);
    oceanodeParam->setNodeModel(this);
    parameters.add(*oceanodeParam);
    return dynamic_pointer_cast<ofxOceanodeAbstractParameter>(*(parameters.end()-1));
}

void ofxOceanodeNodeModel::deserializeParameter(ofJson &json, ofAbstractParameter &p){
	if(p.valueType() == typeid(vector<float>).name()){
		float value = 0;
		if(json[p.getEscapedName()].is_string()){
			p.cast<vector<float>>() = vector<float>(1, ofToFloat(json[p.getEscapedName()]));
		}else{
			p.cast<vector<float>>() = vector<float>(1, float(json[p.getEscapedName()]));
		}
	}
	else if(p.valueType() == typeid(vector<int>).name()){
		if(json[p.getEscapedName()].is_string()){
			p.cast<vector<int>>() = vector<int>(1, ofToInt(json[p.getEscapedName()]));
		}else{
			p.cast<vector<int>>() = vector<int>(1, int(json[p.getEscapedName()]));
		}
	}else{
		ofDeserialize(json, p);
	}
}

void ofxOceanodeNodeModel::setContainer(ofxOceanodeContainer* container){
	canvasID = container->getCanvasID();
};

//parameterInfo& ofxOceanodeNodeModel::addParameterToGroupAndInfo(ofAbstractParameter& p){
//    addParameter(p);
//    if(parametersInfo.count(p.getName()) == 0) parametersInfo[p.getName()] = parameterInfo();
//    return parametersInfo[p.getName()];
//}
//
//parameterInfo& ofxOceanodeNodeModel::addOutputParameterToGroupAndInfo(ofAbstractParameter& p){
//    parameters->add(p);
//
//    if(parametersInfo.count(p.getName()) == 0) parametersInfo[p.getName()] = parameterInfo(false, true, false, true);
//    else{
//        parametersInfo[p.getName()].acceptInConnection = false;
//        parametersInfo[p.getName()].isSavePreset = false;
//    }
//
//    return parametersInfo[p.getName()];
//}

string ofxOceanodeNodeModel::getParents(){
    return canvasID;
}

void ofxOceanodeNodeModel::addSeparator(string name, ofColor color){
    ofParameter<std::function<void()>> separator;
    
#ifndef OFXOCEANODE_HEADLESS
    // Store the label and color as a string that will be parsed in the GUI
    // Format: "SEPARATOR:|label|r,g,b,a"
    string separatorData = "SEPARATOR:|" + name + "|" +
                          ofToString((int)color.r) + "," +
                          ofToString((int)color.g) + "," +
                          ofToString((int)color.b) + "," +
                          ofToString((int)color.a);
    separator.setName(separatorData);
    addCustomRegion(separator, [](){});  // Empty function, rendering will be handled by GUI
#else
    // In headless mode, just add an empty function
    addCustomRegion(separator, [](){});
#endif
}
