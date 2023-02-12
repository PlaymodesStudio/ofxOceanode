//
//  portal.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 14/06/2021.
//

#include "portal.h"
#include "ofxOceanodeShared.h"



abstractPortal::abstractPortal(string typelabel) : ofxOceanodeNodeModel("Portal " + typelabel){
	ofxOceanodeShared::addPortal(this);
	
	addParameter(name.set("Name", typelabel));
    addInspectorParameter(local.set("Local", true));
}

abstractPortal::~abstractPortal(){
	ofxOceanodeShared::removePortal(this);
}

void abstractPortal::portalUpdated(){
	ofxOceanodeShared::portalUpdated(this);
}
