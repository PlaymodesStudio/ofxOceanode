//
//  testNode.cpp
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#include "testNode.h"
#include "ofxOceanodeContainer.h"

testNode::testNode() : ofxOceanodeNodeModel("Test"){
    parameters->add(intParam.set("int", 0, 0, 100));
    parameters->add(floatParam.set("float", 3, 0, 10));
    parameters->add(boolParam.set("bool", true));
    parameters->add(voidParam.set("Button"));
    parameters->add(textParam.set("testInput", "adfas"));
    parameters->add(labelParam.set("testLabel", ' '));
    parameters->add(colorParam.set("Color", ofColor::red, ofColor::white, ofColor::black));
    parameters->add(intModParam.set("Custom", nullptr));
    
    listener = voidParam.newListener([&](){
        customClass* c = new customClass();
        intModParam = c;
    });
    
    listener2 = intModParam.newListener([&](customClass* &c){
        ofLog()<<"received From custom class";
    });
}

ofxOceanodeAbstractConnection* testNode::createConnectionFromCustomType(ofxOceanodeContainer& c, ofAbstractParameter& source, ofAbstractParameter& sink, glm::vec2 pos){
    if(source.type() == typeid(ofParameter<customClass*>).name()){
        return c.connectConnection(source.cast<customClass*>(), sink.cast<customClass*>(), pos);
    }
}
