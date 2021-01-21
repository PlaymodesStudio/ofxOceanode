//
//  testNode.cpp
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#include "testNode.h"
#include "testController.h"

testNode::testNode(shared_ptr<testController> tController) : controller(tController), ofxOceanodeNodeModel("Test"){
    addParameter(intParam.set("int", 0, 0, 100), ofxOceanodeParameterFlags_DisableOutConnection);
    addParameter(floatParam.set("float", 3, 0, 10));
    addParameter(boolParam.set("bool", true));
    addParameter(voidParam.set("Request BPM"));
    addParameter(textParam.set("testInput", "adfas"));
    addParameter(labelParam.set("testLabel", ' '));
    addParameter(colorParam.set("Color", ofColor::red, ofColor::white, ofColor::black));
    addParameter(intModParam.set("Custom", nullptr));
    
    addInspectorParameter(inspectorColor.set("Node Color", ofColor::purple));
    
    color = inspectorColor;
    
    listener = voidParam.newListener([&](){
        customClass* c = new customClass();
        intModParam = c;
        ofLog()<<"current BPM is " << bpm;
    });
    
    listener2 = intModParam.newListener([&](customClass* &c){
        ofLog()<<"received From custom class " << ofGetElapsedTimeMicros();
    });
    
    listener3 = controller->newValue.newListener([&](float &f){
        floatParam = f;
    });
    
    listener4 = floatParam.newListener([this](float &f){
        ofLog() << "evnet" << floatParam;
    });
    
    listener5 = inspectorColor.newListener([this](ofColor &c){
        color = c;
    });
}
