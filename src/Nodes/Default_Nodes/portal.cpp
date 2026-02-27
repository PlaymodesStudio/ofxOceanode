//
//  portal.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 14/06/2021.
//

#include "portal.h"
#include "ofxOceanodeShared.h"



abstractPortal::abstractPortal(string typelabel) : ofxOceanodeNodeModel("Portal " + typelabel){
    description = "To Send " + typelabel + " values anywhere in the patch. Either in the same canvas (local) or in all macros and parents (global)";
	ofxOceanodeShared::addPortal(this);
	
	addParameter(name.set("Name", typelabel));
    addInspectorParameter(local.set("Local", true));
	addInspectorParameter(resendOnNameChange.set("Resend On Name Change", false));

    settingViaMatch = false;
    
    nameListener = name.newListener([this](string &s){
        ofxOceanodeShared::requestPortalUpdate(this);
        if(resendOnNameChange){
            resendValue();
        }
    });
    
    localListener = local.newListener([this](bool &b){
        ofxOceanodeShared::requestPortalUpdate(this);
        resendValue();
    });
}

abstractPortal::~abstractPortal(){
	ofxOceanodeShared::removePortal(this);
}

void abstractPortal::portalUpdated(){
    if(!settingViaMatch){
        ofxOceanodeShared::portalUpdated(this);
    }
}

void abstractPortal::activate(){
    ofxOceanodeShared::requestPortalUpdate(this);
    resendValue();
}

void abstractPortal::presetHasLoaded(){
    ofxOceanodeShared::requestPortalUpdate(this);
    resendValue();
}
