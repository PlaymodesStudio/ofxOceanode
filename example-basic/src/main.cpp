#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main( ){
    ofSetupOpenGL(1024,768,OF_WINDOW);            // <-------- setup the GL context

//    ofGLFWWindowSettings mainSettings;
//    mainSettings.setSize(1240, 720);
//    mainSettings.setPosition(glm::vec2(500,0));
//    mainSettings.windowMode = OF_WINDOW;
//    mainSettings.resizable = true;
//    mainSettings.setGLVersion(4,1);
//    ofCreateWindow(mainSettings);
    
	// this kicks off the running of my app
	// can be OF_WINDOW or OF_FULLSCREEN
	// pass in width and height too:
	ofRunApp(new ofApp());

}
