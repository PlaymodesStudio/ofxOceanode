//
//  ofxOceanodeConnectionGraphics.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#ifndef ofxOceanodeConnectionGraphics_h
#define ofxOceanodeConnectionGraphics_h

#include "ofMain.h"
#include "glm/gtx/vector_angle.hpp"


class ofxOceanodeConnectionGraphics{
public:
    ofxOceanodeConnectionGraphics(){
        points[0] = glm::vec2(-1, -1);
        points[1] = glm::vec2(-1, -1);
    };
    ~ofxOceanodeConnectionGraphics(){};
    
    void draw(ofEventArgs &args){
        if(points[1] != glm::vec2(-1, -1)){
            ofPushMatrix();
            ofMultMatrix(glm::inverse(transformationMatrix->get()));
            {
                
//                ofDrawLine(points[0], points[1]);
                ofColor back = ofGetBackgroundColor();
                if (back.getBrightness()>128)
                    wireColor = ofColor(0);
                else
                    wireColor = ofColor(255);

                ofPath wirePath;
                wirePath.setCurveResolution(64);
                wirePath.setFilled(false);
                wirePath.setStrokeWidth(2);
                wirePath.setStrokeColor(wireColor);
                
                glm::vec2  controlPoint;
                glm::vec2  endControl;
                
                float dist = glm::distance(points[0],points[1]);
                
                
                controlPoint.x = ofMap(dist,0,1500,25,400);
                controlPoint.y = 0;

                wirePath.clear();
                wirePath.moveTo(points[0].x,points[0].y);

                wirePath.bezierTo(points[0].x+controlPoint.x,points[0].y+controlPoint.y,
                                    points[1].x-controlPoint.x,points[1].y-controlPoint.y,
                                    points[1].x, points[1].y);
                wirePath.moveTo(points[1].x,points[1].y);
                
                ofSetColor(255);
                wirePath.draw();
//                wirePath.translate(glm::vec2(0,-3));
//                wirePath.draw();
                
            }
            ofPopMatrix();
        }
    }
    
    void movePoint(int index, glm::vec2 moveVec){
        points[index] += moveVec;
    }
    
    void setPoint(int index, glm::vec2 point){
        points[index] = point;
    }
    
    void deactivate(){points[1] = glm::vec2(-1, -1);};
    
    void subscribeToDrawEvent(shared_ptr<ofAppBaseWindow> w){
        drawEventListener.unsubscribe();
        if(w == nullptr){
            //drawEventListener = ofEvents().draw.newListener(this , &ofxOceanodeConnectionGraphics::draw, OF_EVENT_ORDER_BEFORE_APP);
        }else{
            drawEventListener = w->events().draw.newListener(this , &ofxOceanodeConnectionGraphics::draw, OF_EVENT_ORDER_BEFORE_APP);
        }
    }
    
    glm::vec2 getPoint(int index){return points[index];};
    void setTransformationMatrix(ofParameter<glm::mat4> *m){transformationMatrix = m;};
    glm::mat4 getTransformationMatrix(){return transformationMatrix->get();};
    void setWireColor(ofColor c){wireColor = c;};
private:
    glm::vec2 points[2];
    ofParameter<glm::mat4> *transformationMatrix;
    
    ofEventListener drawEventListener;
    ofColor wireColor;
};

#endif /* ofxOceanodeConnectionGraphics_h */
