//
//  testNode.cpp
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#include "testNode.h"

testNode::testNode() : ofxOceanodeNodeModel("Test"){
    parameters->add(intParam.set("int", 0, 0, 100));
    parameters->add(floatParam.set("float", 3, 0, 10));
    parameters->add(boolParam.set("bool", true));
    parameters->add(voidParam.set("Button"));
    parameters->add(textParam.set("testInput", "adfas"));
    parameters->add(labelParam.set("testLabel", ' '));
    parameters->add(colorParam.set("Color", ofColor::red, ofColor::white, ofColor::black));
    
    listener = voidParam.newListener([&](){
        cout<<"Triggler"<<endl;
    });
}
