//
//  ofxOceanode.cpp
//  example-basic
//
//  Created by Eduard Frigola on 19/06/2017.
//
//

#include "ofxOceanodeCanvas.h"

void ofxOceanodeCanvas::setup(){
    ofAddListener(ofEvents().update, this, &ofxOceanodeCanvas::update);
    ofAddListener(ofEvents().draw, this, &ofxOceanodeCanvas::draw);
    
    ofRegisterKeyEvents(this);
    ofRegisterMouseEvents(this);
    
    
    ///POP UP MENuS
    popUpMenu = new ofxDatGui();
    popUpMenu->setVisible(false);
    popUpMenu->setPosition(-1, -1);
    auto const &models = container->getRegistry().getRegisteredModels();
    vector<string> options;
    for(auto &model : models){
        options.push_back(model.first);
    }
    
    popUpMenu->addDropdown("Choose module", options)->expand();
    
    popUpMenu->onDropdownEvent(this, &ofxOceanodeCanvas::newModuleListener);
}

void ofxOceanodeCanvas::newModuleListener(ofxDatGuiDropdownEvent e){
    unique_ptr<ofxOceanodeNodeModel> type = container->getRegistry().create(e.target->getSelected()->getName());
    
    if (type)
    {
        auto &node = container->createNode(std::move(type));
        node.getNodeGui().setPosition(glm::vec2(popUpMenu->getPosition().x, popUpMenu->getPosition().y));
    }
    popUpMenu->setVisible(false);
    popUpMenu->setPosition(-1, -1);
    ofxDatGuiDropdown* drop = popUpMenu->getDropdown("Choose module");
    drop->setLabel("Choose module");
}

void ofxOceanodeCanvas::mousePressed(ofMouseEventArgs &e){
//    ofVec4f transformedPos = e;
//    transformedPos -= transformMatrix.getTranslation();
//    transformedPos = transformMatrix.getInverse().postMult(transformedPos);
    if(ofGetKeyPressed(OF_KEY_COMMAND)){
        if(e.button == 0){
            popUpMenu->setPosition(e.x, e.y);
            popUpMenu->setVisible(true);
            popUpMenu->getDropdown("Choose module")->expand();
        }
//        else if(e.button == 1){
//            transformMatrix.translate(-transformedPos * (1-transformMatrix.getScale()));
//            transformMatrix = ofMatrix4x4::newTranslationMatrix(transformMatrix.getTranslation());
//            for(auto &gui : datGuis)
//                gui->setTransformMatrix(transformMatrix);//gui->setTransformMatrix(ofMatrix4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1));
//        }
    }
//    if(canvasDragging){
//        glfwSetCursor((GLFWwindow*)ofGetWindowPtr()->getWindowContext(), closedHandCursor);
//        dragCanvasInitialPoint = e;
//    }if(ofGetKeyPressed(OF_KEY_CONTROL)){
//        bool cablePressed = false;
//        for(auto connection : connections){
//            if(connection->hitTest(transformedPos) && !cablePressed){
//                connection->toggleGui(true, e);
//                cablePressed = true;
//            }
//            else if(connection->closedLine){
//                connection->toggleGui(false);
//            }
//        }
//    }if(ofGetKeyPressed('r')){
//        for(int i = 0; i<datGuis.size(); i++){
//            string moduleName = ofSplitString(parameterGroups[i]->getName(), " ")[0];
//            if(datGuis[i]->hitTest(e)
//               && moduleName != "senderManager" && moduleName != "waveScope" && moduleName != "colorApplier" && moduleName != "audioControls" && moduleName != "chartresTextureUnifier" && moduleName != "oscillatorGroup" && moduleName != "speakerPowerCalculator" && moduleName != "dataRecorder" && moduleName != "textureUnifier"){
//                destroyModuleAndConnections(i);
//            }
//        }
//    }
}
