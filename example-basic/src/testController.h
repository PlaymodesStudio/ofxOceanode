//
//  testController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 18/04/2020.
//

#ifndef testController_h
#define testController_h

#include "ofxOceanodeBaseController.h"
#include "imgui.h"

class testController : public ofxOceanodeBaseController{
public:
    testController() : ofxOceanodeBaseController("Test"){};
    ~testController(){};
    
    void draw(){
        static float f = 0;
        ImGui::TextWrapped("%s", "Control float Param of all \"Test\" instances");
        if(ImGui::SliderFloat("float", &f, 0.0f, 1.0f)){
            newValue.notify(f);
        }
    }
    
    ofEvent<float> newValue;
private:
    
};


#endif /* testController_h */
