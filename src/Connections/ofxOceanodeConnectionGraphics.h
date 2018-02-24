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
        
        ofAddListener(ofEvents().draw, this , &ofxOceanodeConnectionGraphics::draw);
    };
    ~ofxOceanodeConnectionGraphics(){};
    
    void draw(ofEventArgs &args){
        if(points[1] != glm::vec2(-1, -1)){
            ofDrawLine(points[0], points[1]);
        }
    }
    
    void movePoint(int index, glm::vec2 point){
        points[index] = point;
    }
    
    glm::vec2 getPoint(int index){return points[index];};
private:
    glm::vec2 points[2];
};

#endif /* ofxOceanodeConnectionGraphics_h */
