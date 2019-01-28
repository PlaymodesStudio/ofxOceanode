//
//  smoother.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/03/2018.
//

#ifndef smoother_h
#define smoother_h

#include "ofxOceanodeNodeModel.h"

class smoother : public ofxOceanodeNodeModel{
public:
    smoother();
    ~smoother(){};
    
    void presetRecallBeforeSettingParameters(ofJson &json){
        isFirstInput = true;
    };
    
private:
    void inputListener(vector<float> &vf);
    ofEventListener inputEventListener;
    
    ofParameter<vector<float>>  input;
    ofParameter<vector<float>> smoothing;
    ofParameter<vector<float>> tension;
    ofParameter<vector<float>>  output;
    vector<float> previousInput;
    
    bool isFirstInput;
};

#endif /* smoother_h */
