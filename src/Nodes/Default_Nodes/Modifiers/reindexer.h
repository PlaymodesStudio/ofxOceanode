//
//  reindexer.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/03/2018.
//

#ifndef reindexer_h
#define reindexer_h

#include "ofxOceanodeNodeModelExternalWindow.h"

class reindexer : public ofxOceanodeNodeModelExternalWindow{
public:
    reindexer() : ofxOceanodeNodeModelExternalWindow("Reindexer"){};
    ~reindexer(){};
    void setup() override;
    
    void presetSave(ofJson &json) override;
    void presetRecallAfterSettingParameters(ofJson &json) override;
    
private:
    void drawInExternalWindow(ofEventArgs &e) override;
    void keyPressed(ofKeyEventArgs &a) override;
    void mousePressed(ofMouseEventArgs &a) override;
    
    void inputListener(vector<float> &vf);
    ofEventListener inputListenerEvent;
    void outputSizeListener(int &f);
    ofEventListener outputSizeListenerEvent;
    
    vector<vector<bool>> reindexGrid;
    vector<vector<bool>>    identityReindexMatrix;
    deque<vector<vector<bool>>>   identityStore;
    bool isReindexIdentity;
    void reindexChanged();
    
    ofParameter<vector<float>> input;
    ofParameter<int>    outputSize;
    ofParameter<vector<float>> output;
};

#endif /* reindexer_h */
