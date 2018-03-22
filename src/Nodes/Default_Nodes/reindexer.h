//
//  reindexer.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/03/2018.
//

#ifndef reindexer_h
#define reindexer_h

#include "ofxOceanodeNodeModel.h"

class reindexer : public ofxOceanodeNodeModel{
public:
    reindexer();
    ~reindexer(){
        if(reindexWindow != nullptr){
            reindexWindow->setWindowShouldClose();
        }
    };
    
private:
    void drawCustomWindow(ofEventArgs &e);
    void keyPressed(ofKeyEventArgs &a);
    void mouseMoved(ofMouseEventArgs &a);
    void mousePressed(ofMouseEventArgs &a);
    void mouseReleased(ofMouseEventArgs &a);
    void mouseDragged(ofMouseEventArgs &a);
    
    void inputListener(vector<float> &vf);
    ofEventListener inputListenerEvent;
    void outputSizeListener(int &f);
    ofEventListener outputSizeListenerEvent;
    
    vector<vector<bool>> reindexGrid;
    vector<vector<bool>>    identityReindexMatrix;
    deque<vector<vector<bool>>>   identityStore;
    bool    isReindexIdentity;
    void reindexChanged();
    
    void showReindexWindow(bool &b);
    shared_ptr<ofAppBaseWindow> reindexWindow;
    ofRectangle                 reindexWindowRect;
    vector<ofEventListener>     windowListenerEvents;
    
    void reindexChanged(vector<vector<bool>> &vb);
    
    ofParameter<vector<float>> input;
    ofParameter<int>    outputSize;
    ofParameter<bool>   showGui;
    ofParameter<vector<float>> output;
};

#endif /* reindexer_h */
