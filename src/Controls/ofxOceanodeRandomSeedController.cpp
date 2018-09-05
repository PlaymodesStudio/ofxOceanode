//
//  ofxOceanodeRandomSeedController.cpp
//  example-seedRandom
//
//  Created by Eduard Frigola on 04/09/2018.
//

#ifdef OFXOCEANODE_USE_RANDOMSEED

#include "ofxOceanodeRandomSeedController.h"
#include "ofxOceanodeContainer.h"

ofxOceanodeRandomSeedController::ofxOceanodeRandomSeedController(shared_ptr<ofxOceanodeContainer> _container) : ofxOceanodeBaseController(_container, "Random Seed"){
    gui->addSlider(randomSeedNum.set("Seed Val", 0, INT_MIN, INT_MAX));
    display = gui->addTextInput("SeedNum Display", ofToString(randomSeedNum));
    
    listeners.push(container->randomSeedLoad.newListener([this](int &val){
        randomSeedNum = val;
    }));
    
    listeners.push(randomSeedNum.newListener([this](int &val){
        display->setText(ofToString(val));
        container->setRandomSeed(val);
    }));
}

#endif
