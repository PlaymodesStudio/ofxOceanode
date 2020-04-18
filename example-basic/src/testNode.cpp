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
    addParameterToGroupAndInfo(intParam.set("int", 0, 0, 100)).acceptOutConnection = false;
    parameters->add(floatParam.set("float", 3, 0, 10));
    parameters->add(boolParam.set("bool", true));
    parameters->add(voidParam.set("Request BPM"));
    parameters->add(textParam.set("testInput", "adfas"));
    parameters->add(labelParam.set("testLabel", ' '));
    parameters->add(colorParam.set("Color", ofColor::red, ofColor::white, ofColor::black));
    parameters->add(intModParam.set("Custom", nullptr));
    
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
}
