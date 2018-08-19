//
//  ofxOceanodeBaseController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 12/03/2018.
//

#ifndef ofxOceanodeBaseController_h
#define ofxOceanodeBaseController_h

#include "ofMain.h"
#include "ofxDatGui.h"

class ofxOceanodeContainer;

class ofxOceanodeControllerButton{
public:
    ofxOceanodeControllerButton(){
        font.load(OF_TTF_SANS, 10);
        highlight = false;
        int height = 0;
        color = ofColor::blueSteel;
    };
    ~ofxOceanodeControllerButton(){};
    
    void draw(ofRectangle rect){
        height = rect.getHeight();
        ofPushStyle();
        if(highlight){
            // fondo highlighted
            ofSetColor(color);
            ofDrawRectangle(rect);
//            ofSetColor(0,255,0);
//            ofRectangle rectCopy = rect;
//            rectCopy.scaleFromCenter(.5);
//            ofDrawRectangle(rectCopy);
        }
        else{
            // fondo not highlighted
            ofSetColor(ofColor(32,32,32));
            ofDrawRectangle(rect);
        }
        ofSetColor(ofColor::white);
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
    ofxOceanodeBaseController(shared_ptr<ofxOceanodeContainer> _container, string name);
    virtual ~ofxOceanodeBaseController(){};
    
    virtual void draw();
    virtual void update();
    virtual void windowResized(ofResizeEventArgs &a);
    
    string getControllerName(){return controllerName;};
    ofxOceanodeControllerButton& getButton(){return button;};
    
    virtual void activate();
    virtual void deactivate();
protected:
    string controllerName;
    bool isActive;
    
    ofxDatGui* gui;
    ofxDatGuiTheme* mainGuiTheme;
    
    ofEventListeners listeners;
    
    shared_ptr<ofxOceanodeContainer> container;
private:
    ofxOceanodeControllerButton button;
};

#endif /* ofxOceanodeBaseController_h */
