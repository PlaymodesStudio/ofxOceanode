//
//  ofxOceanodeParameter.cpp
//  example-basic
//
//  Created by Eduard Frigola on 29/04/2020.
//

#include "ofxOceanodeParameter.h"
#include "ofMath.h"
#include "ofxOceanodeConnection.h"
#include "ofxOceanodeScope.h"

void ofxOceanodeAbstractParameter::removeAllConnections(){
    if(inConnection != nullptr) inConnection->deleteSelf();
    while(outConnections.size() != 0) outConnections.front()->deleteSelf();
    
    if(hasScope) ofxOceanodeScope::getInstance()->removeParameter(this);
};