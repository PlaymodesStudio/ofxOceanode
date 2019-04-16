//
//  ofxOceanodeNodeModelExternalWindow.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 23/03/2018.
//

#ifndef ofxOceanodeNodeModelExternalWindow_h
#define ofxOceanodeNodeModelExternalWindow_h

#include "ofxOceanodeNodeModel.h"

class ofxOceanodeNodeModelExternalWindow : public ofxOceanodeNodeModel{
public:
    ofxOceanodeNodeModelExternalWindow(string name);
    ~ofxOceanodeNodeModelExternalWindow();
    
    
    void presetSave(ofJson &json) override;
    void presetRecallBeforeSettingParameters(ofJson &json) override;
    void loadCustomPersistent(ofJson &json) override;
    void setExternalWindowPosition(int px, int py);
    void setExternalWindowShape(int w, int h);
    void setExternalWindowFullScreen(bool b);
    void toggleFullscreen();
    void setShowWindow(bool b){
        showWindow = b;
    }
    
protected:
    virtual void drawInExternalWindow(ofEventArgs &e){};
    virtual void updateForExternalWindow(ofEventArgs &e){};
    virtual void closeExternalWindow(ofEventArgs &e);
    virtual void keyPressed(ofKeyEventArgs &a){};
    virtual void keyReleased(ofKeyEventArgs &a){};
    virtual void mouseMoved(ofMouseEventArgs &a){};
    virtual void mousePressed(ofMouseEventArgs &a){};
    virtual void mouseReleased(ofMouseEventArgs &a){};
    virtual void mouseDragged(ofMouseEventArgs &a){};
    virtual void mouseScrolled(ofMouseEventArgs &a){};
    virtual void mouseEntered(ofMouseEventArgs &a){};
    virtual void mouseExited(ofMouseEventArgs &a){};
    virtual void windowResized(ofResizeEventArgs &a){};
    virtual void dragEvent(ofDragInfo &dragInfo){};
    
    void windowResizedOwnEvent(ofResizeEventArgs &a);
    
    void showExternalWindow(bool &b);
    shared_ptr<ofAppBaseWindow> externalWindow;
    ofRectangle                 externalWindowRect;
    ofWindowMode                externalWindowMode;
    ofEventListeners            windowListenerEvents;
    ofParameter<bool>           showWindow;
    ofParameter<bool>           fullscreenWindow;
};

#endif /* ofxOceanodeNodeModelExternalWindow_h */
