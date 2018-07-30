//
//  localPresetController.cpp
//  VJYourself_OCEAN
//
//  Created by Eduard Frigola on 25/04/2018.
//

#include "localPresetController.h"

localPresetController::localPresetController() : ofxOceanodeNodeModel("Local Preset Controller"){
    addParameterToGroupAndInfo(matrix.set("Matrix", make_pair(42, true))).acceptInConnection = false;
    
    addOutputParameterToGroupAndInfo(on.set("On", false));
    addOutputParameterToGroupAndInfo(preset.set("Preset", 0));
    
    listener = matrix.newListener([&](pair<int, bool> &val){        
        if(!ofGetKeyPressed(OF_KEY_SHIFT)){
            if(val.second){
                preset = val.first;
                on = true;
            }else{
                on = false;
            }
        }
        else{
            preset = -val.first;
        }
    });
}
