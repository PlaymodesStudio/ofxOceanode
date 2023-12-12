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
    settingViaMatch = false;
}

abstractPortal::~abstractPortal(){
	ofxOceanodeShared::removePortal(this);
}

void abstractPortal::portalUpdated(){
    if(!settingViaMatch){
        ofxOceanodeShared::portalUpdated(this);
    }
}
