//
//  ofxOceanodeConnectionGraphics.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#ifndef ofxOceanodeConnectionGraphics_h
#define ofxOceanodeConnectionGraphics_h

#include "ofMain.h"

class ofxOceanodeConnectionGraphics{
public:
    ofxOceanodeConnectionGraphics(){
        points[0] = glm::vec2(-1, -1);
        points[1] = glm::vec2(-1, -1);
        
        drawEventListener = ofEvents().draw.newListener(this , &ofxOceanodeConnectionGraphics::draw);
    };
    ~ofxOceanodeConnectionGraphics(){};
    
    void draw(ofEventArgs &args){
        if(points[1] != glm::vec2(-1, -1)){
            ofPushMatrix();
            ofMultMatrix(transformationMatrix->get());
            ofDrawLine(points[0], points[1]);
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
    
    glm::vec2 getPoint(int index){return points[index];};
    void setTransformationMatrix(ofParameter<glm::mat4> *m){transformationMatrix = m;};
    glm::mat4 getTransformationMatrix(){return transformationMatrix->get();};
    
private:
    glm::vec2 points[2];
    ofParameter<glm::mat4> *transformationMatrix;
    
    ofEventListener drawEventListener;
};

#endif /* ofxOceanodeConnectionGraphics_h */
