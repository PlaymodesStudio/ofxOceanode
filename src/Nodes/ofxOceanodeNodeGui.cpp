//
//  ofxOceanodeNodeGui.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 22/02/2018.
//

#include "ofxOceanodeNodeGui.h"
#include "ofxOceanodeNode.h"
#include "ofxOceanodeContainer.h"

ofxOceanodeNodeGui::ofxOceanodeNodeGui(ofxOceanodeContainer& _container, ofxOceanodeNode& _node) : container(_container), node(_node){
    color = ofColor::red;
    position = glm::vec2(10, 10);
    
    createGuiFromParameters();
    ofRegisterMouseEvents(this);
}

void ofxOceanodeNodeGui::createGuiFromParameters(){
    ofParameterGroup *parameters = getParameters();
    
    ofxDatGuiLog::quiet();
    ofxDatGui::setAssetPath("");
    
    gui = make_unique<ofxDatGui>();
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
    if(position == glm::vec2(-1, -1)){
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
        }else if(absParam.type() == typeid(ofParameter<vector<float>>).name()){
            gui->addMultiSlider(parameters->get(i).cast<vector<float>>());
        }else {
            gui->addLabel(absParam.getName());
        }
    }
    
    //GUIS EVENT LISTERNERS
    gui->onButtonEvent(this, &ofxOceanodeNodeGui::onGuiButtonEvent);
    gui->onToggleEvent(this, &ofxOceanodeNodeGui::onGuiToggleEvent);
    gui->onDropdownEvent(this, &ofxOceanodeNodeGui::onGuiDropdownEvent);
    gui->onTextInputEvent(this, &ofxOceanodeNodeGui::onGuiTextInputEvent);
    gui->onColorPickerEvent(this, &ofxOceanodeNodeGui::onGuiColorPickerEvent);
    gui->onRightClickEvent(this, &ofxOceanodeNodeGui::onGuiRightClickEvent);
    
    
    //OF PARAMETERS LISTERENRS
    ofAddListener(parameters->parameterChangedE(), this, &ofxOceanodeNodeGui::parameterListener);
}

void ofxOceanodeNodeGui::updateGuiForParameter(string &parameterName){
    ofAbstractParameter &absParam = getParameters()->get(parameterName);
    if(absParam.type() == typeid(ofParameter<float>).name()){
        auto slider = gui->getSlider(absParam.getName());
        slider->setMin(absParam.cast<float>().getMin());
        slider->setMax(absParam.cast<float>().getMax());
    }else if(absParam.type() == typeid(ofParameter<int>).name()){
        auto slider = gui->getSlider(absParam.getName());
        slider->setMin(absParam.cast<int>().getMin());
        slider->setMax(absParam.cast<int>().getMax());
//    }else if(absParam.type() == typeid(ofParameterGroup).name()){
//        gui->addLabel(parameters->getGroup(i).getName());
//        gui->addDropdown(parameters->getGroup(i).getName(), ofSplitString(parameters->getGroup(i).getString(0), "-|-"))->select(parameters->getGroup(i).getInt(1));
    }else if(absParam.type() == typeid(ofParameter<vector<float>>).name()){
        auto slider = gui->getMultiSlider(absParam.getName());
        slider->setMin(absParam.cast<vector<float>>().getMin()[0]);
        slider->setMax(absParam.cast<vector<float>>().getMax()[0]);
    }
}

ofParameterGroup* ofxOceanodeNodeGui::getParameters(){
    return node.getParameters();
    
}

void ofxOceanodeNodeGui::setPosition(glm::vec2 _position){
    gui->setPosition(_position.x, _position.y);
    position = _position;
}

void ofxOceanodeNodeGui::parameterListener(ofAbstractParameter &parameter){
    
}

void ofxOceanodeNodeGui::mouseDragged(ofMouseEventArgs &args){
    glm::vec2 guiCurrentPos = glm::vec2(gui->getPosition().x, gui->getPosition().y);
    if(guiCurrentPos != position){
        node.moveConnections(guiCurrentPos - position);
        position = guiCurrentPos;
    }
}

void ofxOceanodeNodeGui::onGuiButtonEvent(ofxDatGuiButtonEvent e){
    getParameters()->getVoid(e.target->getName()).trigger();
}
void ofxOceanodeNodeGui::onGuiToggleEvent(ofxDatGuiToggleEvent e){
    getParameters()->getBool(e.target->getName()) = e.checked;
}

void ofxOceanodeNodeGui::onGuiDropdownEvent(ofxDatGuiDropdownEvent e){
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

void ofxOceanodeNodeGui::onGuiTextInputEvent(ofxDatGuiTextInputEvent e){
    getParameters()->getString(e.target->getName()) = e.text;
}

void ofxOceanodeNodeGui::onGuiColorPickerEvent(ofxDatGuiColorPickerEvent e){
    getParameters()->getColor(e.target->getName()) = e.color;
}

void ofxOceanodeNodeGui::onGuiRightClickEvent(ofxDatGuiRightClickEvent e){
        if(e.down == 1){
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
//                            connections.push_back(make_shared<nodeConnection>(e.target, datGuis[i], &parameter));
            glm::vec2 point;
            point.x = e.target->getX() + e.target->getWidth();
            point.y = e.target->getY() + e.target->getHeight()/2;
            node.createConnection(container, getParameters()->get(e.target->getName()), point);
    //                        ofAddListener(connections.back()->destroyEvent, this, &ofxOceanodeNode::destroyedConnection);
    //                    }
    //                }
    //            }
    //        }
        }else{
            glm::vec2 point;
            point.x = e.target->getX();
            point.y = e.target->getY() + e.target->getHeight()/2;
            node.makeConnection(container, getParameters()->getPosition(e.target->getName()), point);
//            container.connectConnection(node, getParameters()->getPosition(e.target->getName()));
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
        }
}

