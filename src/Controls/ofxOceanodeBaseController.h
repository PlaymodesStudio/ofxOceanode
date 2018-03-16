//
//  ofxOceanodeBaseController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#ifndef ofxOceanodeBaseController_h
#define ofxOceanodeBaseController_h

#include "ofMain.h"

class ofxOceanodeControllerButton{
public:
    ofxOceanodeControllerButton(){
        font.load(OF_TTF_SANS, 15);
        highlight = false;
        int height = 0;
    };
    ~ofxOceanodeControllerButton(){};
    
    void draw(ofRectangle rect){
        height = rect.getHeight();
        ofPushStyle();
        if(highlight){
            ofSetColor(ofColor::white);
            ofDrawRectRounded(rect, 3);
            ofSetColor(color);
            ofRectangle rectCopy = rect;
            rectCopy.scaleFromCenter(0.8);
            ofDrawRectRounded(rectCopy, 3);
        }
        else{
            ofSetColor(color);
            ofDrawRectRounded(rect, 3);
        }
        ofSetColor(color.getInverted());
        float xLabelPos = rect.getCenter().x - (font.stringWidth(name)/2);
        float yLabelPos = rect.getCenter().y + (font.stringHeight(name)/2);
        font.drawString(name, xLabelPos, yLabelPos);
        ofPopStyle();
    }
    
    void setName(string _name){name = _name;};
    void setColor(ofColor _color){color = _color;};
    void setHighlight(bool h){highlight = h;};
    int getHeight(){return height;};
private:
    ofTrueTypeFont font;
    string  name;
    ofColor color;
    bool highlight;
    int height;
};

class ofxOceanodeBaseController{
public:
    ofxOceanodeBaseController(string name);
    ~ofxOceanodeBaseController(){};
    
    virtual void draw(){};
    virtual void update(){};
    
    string getControllerName(){return controllerName;};
    ofxOceanodeControllerButton& getButton(){return button;};
    
    virtual void windowResized(ofResizeEventArgs &a){};
    virtual void activate();
    virtual void deactivate();
protected:
    string controllerName;
    bool isActive;
private:
    ofxOceanodeControllerButton button;
};

#endif /* ofxOceanodeBaseController_h */
