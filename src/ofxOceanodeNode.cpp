//
//  ofxOceanodeNode.cpp
//  example-basic
//
//  Created by Eduard Frigola on 22/06/2017.
//
//

#include "ofxOceanodeNode.h"

ofxOceanodeNode::ofxOceanodeNode(unique_ptr<ofxOceanodeNodeModel> && _nodeModel) : nodeModel(move(_nodeModel)){
    
    color = ofColor::red;
    position = ofPoint(10, 10);
    
    createGuiFromParameters();
}

void ofxOceanodeNode::createGuiFromParameters(){
    ofParameterGroup *parameters = nodeModel->getParameterGroup();
    
    ofxDatGuiLog::quiet();
    ofxDatGui::setAssetPath("");
    
    gui = make_shared<ofxDatGui>();
//        gui->setAutoDraw(false);
    
//    gui->setTransformMatrix(transformMatrix);
    
    ofxDatGuiTheme* theme = new ofxDatGuiThemeCharcoal;
    theme->color.slider.fill = color;
    theme->color.textInput.text = color;
    theme->color.icons = color;
    gui->setTheme(theme);
    
    gui->setWidth(290);
    gui->addHeader(parameters->getName());
    gui->addFooter();
    if(position == ofPoint(-1, -1)){
        gui->setPosition(0, 0);
    }else{
        gui->setPosition(position.x, position.y);
    }
    
    for(int i=0 ; i<parameters->size(); i++){
        ofAbstractParameter &absParam = parameters->get(i);
        if(absParam.type() == typeid(ofParameter<float>).name()){
            gui->addSlider(parameters->getFloat(i));
        }else if(absParam.type() == typeid(ofParameter<int>).name()){
            gui->addSlider(parameters->getInt(i));
        }else if(absParam.type() == typeid(ofParameter<bool>).name()){
            ofxDatGuiToggle* toggle =  gui->addToggle(parameters->getName(i));
            toggle->setChecked(parameters->getBool(i).get());
            //Add a listener that automatically puts the parameter to the gui.
            //Best will be to include this to datGui. But this way we can change the gui we use, independent of ofParameter
            parameterChangedListeners.push_back(parameters->getBool(i).newListener([&, toggle](bool &val){
                toggle->setChecked(val);
            }));
        }else if(absParam.type() == typeid(ofParameter<void>).name()){
            gui->addButton(parameters->getName(i));
        }else if(absParam.type() == typeid(ofParameter<string>).name()){
            auto textInput = gui->addTextInput(absParam.getName(), absParam.cast<string>());
            parameterChangedListeners.push_back(parameters->getString(i).newListener([&, textInput](string &val){
                textInput->setText(val);
            }));
        }else if(absParam.type() == typeid(ofParameter<char>).name()){
            gui->addLabel(absParam.getName());
        }else if(absParam.type() == typeid(ofParameter<ofColor>).name()){
            auto colorGui = gui->addColorPicker(parameters->getName(i), absParam.cast<ofColor>());
            parameterChangedListeners.push_back(parameters->get(i).cast<ofColor>().newListener([&, colorGui](ofColor &val){
                colorGui->setColor(val);
            }));  
        }else if(absParam.type() == typeid(ofParameterGroup).name()){
            gui->addLabel(parameters->getGroup(i).getName());
            gui->addDropdown(parameters->getGroup(i).getName(), ofSplitString(parameters->getGroup(i).getString(0), "-|-"))->select(parameters->getGroup(i).getInt(1));
        }else{
            gui->addLabel(absParam.getName());
        }
    }
    
    //GUIS EVENT LISTERNERS
    gui->onButtonEvent(this, &ofxOceanodeNode::onGuiButtonEvent);
    gui->onToggleEvent(this, &ofxOceanodeNode::onGuiToggleEvent);
    gui->onDropdownEvent(this, &ofxOceanodeNode::onGuiDropdownEvent);
    gui->onTextInputEvent(this, &ofxOceanodeNode::onGuiTextInputEvent);
    gui->onColorPickerEvent(this, &ofxOceanodeNode::onGuiColorPickerEvent);
//    gui->onRightClickEvent(this, &ofxOceanodeNode::onGuiRightClickEvent);
    
    
    //OF PARAMETERS LISTERENRS
    ofAddListener(parameters->parameterChangedE(), this, &ofxOceanodeNode::parameterListener);
}

void ofxOceanodeNode::setPosition(glm::vec2 position){
    gui->setPosition(position.x, position.y);
}

void ofxOceanodeNode::parameterListener(ofAbstractParameter &parameter){
    
}

void ofxOceanodeNode::onGuiButtonEvent(ofxDatGuiButtonEvent e){
    nodeModel->getParameterGroup()->getVoid(e.target->getName()).trigger();
}
void ofxOceanodeNode::onGuiToggleEvent(ofxDatGuiToggleEvent e){
    nodeModel->getParameterGroup()->getBool(e.target->getName()) = e.checked;
}

void ofxOceanodeNode::onGuiDropdownEvent(ofxDatGuiDropdownEvent e){
//    if(e.target == bankSelect){
//        oldPresetButton = nullptr;
//        if(e.child == bankSelect->getNumOptions()-1){
//            bankSelect->addOption("Bank " + ofGetTimestampString(), bankSelect->getNumOptions()-1);
//            bankSelect->select(bankSelect->getNumOptions()-2);
//        }
//        loadBank();
//    }
//    else{
//        for (int i=0; i < datGuis.size() ; i++){
//            if(datGuis[i]->getDropdown(e.target->getName()) == e.target){
//                parameterGroups[i]->getGroup(e.target->getName()).getInt(1) = e.child;
//                //            if(datGuis[i]->getHeight() > ofGetHeight())
//                //                ofSetWindowShape(ofGetWidth(), datGuis[i]->getHeight());
//            }
//        }
//    }
}

void ofxOceanodeNode::onGuiTextInputEvent(ofxDatGuiTextInputEvent e){
    nodeModel->getParameterGroup()->getString(e.target->getName()) = e.text;
}

void ofxOceanodeNode::onGuiColorPickerEvent(ofxDatGuiColorPickerEvent e){
    nodeModel->getParameterGroup()->getColor(e.target->getName()) = e.color;
}

void ofxOceanodeNode::onGuiRightClickEvent(ofxDatGuiRightClickEvent e){
//    if(e.down == 1){
//        for (int i=0; i < datGuis.size() ; i++){
//            if(datGuis[i]->getComponent(e.target->getType(), e.target->getName()) == e.target){
//                ofAbstractParameter &parameter = parameterGroups[i]->get(e.target->getName());
//                if(midiListenActive){
//                    if(parameter.type() == typeid(ofParameter<float>).name()){
//                        bool erasedConnection = false;
//                        if(ofGetKeyPressed(OF_KEY_SHIFT)){
//                            for(int i = 0 ; i < midiFloatConnections.size(); i++){
//                                if(midiFloatConnections[i].getParameter() == &parameter){
//                                    erasedConnection = true;
//                                    midiFloatConnections.erase(midiFloatConnections.begin()+i);
//                                    return;
//                                }
//                            }
//                        }
//                        if(!erasedConnection)
//                            midiFloatConnections.push_back(midiConnection<float>(&parameter.cast<float>()));
//                    }
//                    else if(parameter.type() == typeid(ofParameter<int>).name()){
//                        bool erasedConnection = false;
//                        if(ofGetKeyPressed(OF_KEY_SHIFT)){
//                            for(int i = 0 ; i < midiIntConnections.size(
//                                ); i++){
//                                if(midiIntConnections[i].getParameter() == &parameter){
//                                    erasedConnection = true;
//                                    midiIntConnections.erase(midiIntConnections.begin()+i);
//                                    return;
//                                }
//                            }
//                        }
//                        if(!erasedConnection)
//                            midiIntConnections.push_back(midiConnection<int>(&parameter.cast<int>()));
//                    }
//                    else if(parameter.type() == typeid(ofParameter<bool>).name()){
//                        bool erasedConnection = false;
//                        if(ofGetKeyPressed(OF_KEY_SHIFT)){
//                            for(int i = 0 ; i < midiBoolConnections.size(); i++){
//                                if(midiBoolConnections[i].getParameter() == &parameter){
//                                    erasedConnection = true;
//                                    midiBoolConnections.erase(midiBoolConnections.begin()+i);
//                                    return;
//                                }
//                            }
//                        }
//                        if(!erasedConnection)
//                            midiBoolConnections.push_back(midiConnection<bool>(&parameter.cast<bool>()));
//                    }
//                    else if(parameter.type() == typeid(ofParameterGroup).name()){
//                        parameter = parameterGroups[i]->getGroup(e.target->getName()).get(1);
//                        bool erasedConnection = false;
//                        if(ofGetKeyPressed(OF_KEY_SHIFT)){
//                            for(int i = 0 ; i < midiIntConnections.size(); i++){
//                                if(midiFloatConnections[i].getParameter() == &parameter){
//                                    erasedConnection = true;
//                                    midiIntConnections.erase(midiIntConnections.begin()+i);
//                                    return;
//                                }
//                            }
//                        }
//                        if(!erasedConnection)
//                            midiIntConnections.push_back(midiConnection<int>(&parameter.cast<int>()));
//                    }
//                    else
//                        ofLog() << "Cannot midi to parameter " << parameter.getName();
//                }
//                else{
//                    bool foundParameter = false;
//                    for(int j = 0 ; j < connections.size() ; j++){
//                        if(connections[j]->getSinkParameter() == &parameter){
//                            swap(connections[j], connections.back());
//                            connections.back()->disconnect();
//                            foundParameter = true;
//                            break;
//                        }
//                    }
//                    if(!foundParameter){
//                        connections.push_back(make_shared<nodeConnection>(e.target, datGuis[i], &parameter));
//                        ofAddListener(connections.back()->destroyEvent, this, &ofxOceanodeNode::destroyedConnection);
//                    }
//                }
//            }
//        }
//    }else if(connections.size() > 0){
//        for (int i=0; i < datGuis.size() ; i++){
//            if(datGuis[i]->getComponent(e.target->getType(), e.target->getName()) == e.target
//               && !connections.back()->closedLine
//               && connections.back()->getSourceParameter() != &parameterGroups[i]->get(e.target->getName())){
//                connections.back()->connectTo(e.target, datGuis[i], &parameterGroups[i]->get(e.target->getName()));
//                for(int i = 0; i<connections.size()-1 ; i++){
//                    if(connections.back()->getSinkParameter() == connections[i]->getSourceParameter() ||
//                       connections.back()->getSinkParameter() == connections[i]->getSinkParameter() ||
//                       connections.back()->getSourceParameter() == connections[i]->getSinkParameter())
//                        connections.erase(connections.begin()+i);
//                }
//            }
//        }
//    }
}

