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
    
    settingViaMatch = false;
    
	addParameter(name.set("Name", typelabel));
    addInspectorParameter(local.set("Local", true));
	addInspectorParameter(resendOnNameChange.set("Resend On Name Change", false));

    // currentName must be set before addPortal so the map bucket key is correct
    currentName = typelabel;
    ofxOceanodeShared::addPortal(this);
    
    nameListener = name.newListener([this](string &s){
        // Move the portal to the correct bucket in the map registry
        ofxOceanodeShared::updatePortalName(this, currentName, s);
        currentName = s;
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
