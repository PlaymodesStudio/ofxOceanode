//
//  ofxOceanodeTime.cpp
//  ofxOceanode
//
//  Created by Eduard Frigola Bagué on 13/10/2021.
//

#include "ofxOceanodeTime.h"
#include "ofxOceanodeContainer.h"
#include "phasor.h"
#include "ofxOceanodeNodeMacro.h"

void ofxOceanodeTime::setup(shared_ptr<ofxOceanodeContainer> c, shared_ptr<ofxOceanodeBPMController> contr){
    container = c;
    controller = contr;
    startTime = ofGetCurrentTime();
    
    parameters.add(isPlaying.set("Is Playing", true));
    parameters.add(frameMode.set("Frame Mode", false));
    parameters.add(frameInterval.set("Frame Interval", 1));
    parameters.add(stop.set("Stop"));
    parameters.add(time.set("Time", 0));
    parameters.add(scrub.set("Scrub", 0));
    listeners.push(isPlaying.newListener([this](bool &b){
        if(b){
            startTime = ofGetCurrentTime() + std::chrono::duration<double>(-time);
        }
    }));
    
    listeners.push(stop.newListener([this](){
        isPlaying = false;
        time = 0;
        container->resetPhase();
    }));
    
    listeners.push(scrub.newListener([this](float &f){
        startTime = startTime + std::chrono::duration<double>(-f);
        if(!isPlaying){
            time += f;
            if(time < 0){
                time = 0;
            }
        }else{
            auto currentTime = ofGetCurrentTime();
            if(startTime > currentTime){
                startTime = currentTime;
            }
        }
    }));
    
    controller->setTimeGroup(&parameters);
    
    
    timer.setPeriodicEvent(1000000);
    startThread();
    
    ofSoundStreamSettings settings;

    // if you want to set the device id to be different than the default
    // auto devices = soundStream.getDeviceList();
    // settings.device = devices[4];

    // you can also get devices for an specific api
    // auto devices = soundStream.getDevicesByApi(ofSoundDevice::Api::PULSE);
    // settings.device = devices[0];

    // or get the default device for an specific api:
    // settings.api = ofSoundDevice::Api::PULSE;

    // or by name
    auto devices = soundStream.getMatchingDevices("Speakers");
    if(!devices.empty()){
        settings.setOutDevice(devices[0]);
    }

    settings.setOutListener(this);
    settings.sampleRate = 44100;
    settings.numOutputChannels = 1;
    settings.numInputChannels = 0;
    settings.bufferSize = 256;
    soundStream.setup(settings);
}

void ofxOceanodeTime::update(){
    bool forceFrameMode = false;
    vector<shared_ptr<basePhasor>> phasors;
    vector<timeGenerator*> timeGenerators;
    std::function<void(shared_ptr<ofxOceanodeContainer>)> getPhasorsFromContainer = [this, &phasors, &getPhasorsFromContainer, &timeGenerators, &forceFrameMode](shared_ptr<ofxOceanodeContainer> c){
        for(auto &n : c->getAllModules()){
            ofxOceanodeNodeModel *model = &n->getNodeModel();
//            if(model->getFlags() & ofxOceanodeNodeModelFlags_ForceFrameMode){
//                forceFrameMode = true;
//            }
            if(dynamic_cast<phasor*>(model) != nullptr){
                phasors.push_back(dynamic_cast<phasor*>(model)->getBasePhasor());
            }
            else if(dynamic_cast<timeGenerator*>(model) != nullptr){
                timeGenerators.push_back(dynamic_cast<timeGenerator*>(model));
            }
            else if(dynamic_cast<ofxOceanodeNodeMacro*>(model) != nullptr){
                getPhasorsFromContainer(dynamic_cast<ofxOceanodeNodeMacro*>(model)->getContainer());
            }
        }
    };
    
    getPhasorsFromContainer(container);
    phasorChannel.send(phasors);
    phasorChannel2.send(phasors);
    
    for(auto c : timeGenerators){
        c->setTime(time);
    }
    
    if(isPlaying){
        if(frameMode || forceFrameMode){
            if(ofGetFrameNum() % frameInterval == 0 || forceFrameMode){
                float targetFR = ofGetTargetFrameRate();
                if(targetFR == 0) targetFR = 60;
                time += (1.0f/targetFR);
                for(auto p : phasors){
                    p->advanceForFrameRate(targetFR);
                }
            }
        }else{
            time = std::chrono::duration<double>(ofGetCurrentTime() - startTime).count();
        }
    }
}

void ofxOceanodeTime::threadedFunction(){
//    while(isThreadRunning()){
//        timer.waitNext();
//        if(!frameMode && isPlaying){
//            phasorChannel.tryReceive(phasorsInThread);
//            for(auto p : phasorsInThread){
//                if(!p->isAudio())
//                    p->threadedFunction(1000);
//            }
//        }
//    }
}

void ofxOceanodeTime::audioIn(ofSoundBuffer & input){
    if(!frameMode){
        phasorChannel2.tryReceive(phasorsInThread2);
        for(auto p : phasorsInThread2){
            if(p->isAudio())
                p->advanceForFrameRate(44100.0/256.0);
            else
                p->threadedFunction(44100.0/256.0);
        }
    }
}

void ofxOceanodeTime::audioOut(ofSoundBuffer & input){
    if(!frameMode){
        phasorChannel2.tryReceive(phasorsInThread2);
        for(auto p : phasorsInThread2){
            if(p->isAudio())
                p->advanceForFrameRate(44100.0/256.0);
            else
                p->threadedFunction(44100.0/256.0);
        }
    }
}

#include "imgui_internal.h"
// https://github.com/ocornut/imgui/issues/1720
bool Splitter2(int splitNum, bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID(("##Splitter" + ofToString(splitNum)).c_str());
    ImRect bb;
    bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 4.0f, 0.04f);
}

void ofxOceanodeTime::draw(){
    ImGui::SetNextWindowSize(ImVec2(0, 30));
    if(ImGui::Begin("Timeline")){
        //Width Tables
//        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;
//        if(ImGui::BeginTable("Table", 2, flags)){
//            ImGui::TableSetupColumn("Esquerra");
//            ImGui::TableSetupColumn("Dreta");
//
//            ImGui::TableNextRow();
//            ImGui::TableSetColumnIndex(0);
//            ImGui::Button("Propietats timelin");
//
//            ImGui::TableSetColumnIndex(1);
//
//            float availWidth = ImGui::GetContentRegionAvail().x;
//            ImDrawList* draw_list = ImGui::GetWindowDrawList();
//            draw_list->AddRectFilled(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImVec2(availWidth, 10), IM_COL32(255, 255, 0, 50));
//
//            ImGui::TableNextRow();
//            ImGui::TableSetColumnIndex(0);
//            ImGui::Button("Parameter");
//            ImGui::TableSetColumnIndex(1);
//            ImGui::Button("Currva");
//
//            ImGui::EndTable();
//        }
        //Like scope
        float availWidth = ImGui::GetContentRegionAvail().x;
        
        float rulerHeight = 10;
        float left = 100;
        float right = availWidth-left;
        Splitter2(0, true, 2, &left, &right, 10, 10);
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(ImGui::GetCursorScreenPos() + ImVec2(left, 0), ImGui::GetCursorScreenPos() + ImVec2(availWidth, rulerHeight), IM_COL32(255, 255, 0, 50));
        
        //TODO: Add timelines
        /*
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rulerHeight + ImGui::GetFrameHeightWithSpacing() - ImGui::GetFrameHeight());
        ImGui::Spacing();
        ImGui::Separator();
        float topPos = ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y;
        
        if(timlinedParameters.size() > 0){
            //ImGui::Begin("Scopes", NULL, ImGuiWindowFlags_NoScrollbar);
            windowHeight = ImGui::GetContentRegionAvail().y;
            for(int i = 0; i < timlinedParameters.size(); i++){
                float topHeight = 0;
                float bottomHeight = 0;
                    for(int j = 0; j < i+1; j++) topHeight += timlinedParameters[j].open ? timlinedParameters[j].height : ImGui::GetFrameHeight();
                    for(int j = i+1; j < timlinedParameters.size(); j++) bottomHeight += timlinedParameters[j].open ? timlinedParameters[j].height : ImGui::GetFrameHeight();
                float oldTopHeight = topHeight;
                float oldBottomHeight = bottomHeight;
                
                
//                float minTop = 10;
//                float minBottom = 10;
                float minTop = topHeight - timlinedParameters[i].height;
                float minBottom = 0;
                
                //TODO: remove hack
//                if(timlinedParameters[i].open){
                    if(Splitter2(i+1, false, 1, &topHeight, &bottomHeight, minTop, minBottom) && timlinedParameters[i].open){
                        float topInc = topHeight - oldTopHeight;
                        timlinedParameters[i].height += topInc;
                    }
//                }
//                else{
                    // Draw division no interaction
//                }
                    
            }
            
            float accumPos = topPos;
            for(int i = 0; i < timlinedParameters.size(); i++)
            {
                auto &p = timlinedParameters[i];
                auto itemHeight = (p.height);
//
                auto size = ImVec2(ImGui::GetContentRegionAvail().x, itemHeight);
//                
                ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(p.color*0.75f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(p.color*0.75f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4(p.color*0.75f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55,0.55,0.55,1.0));
                ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(0.0,0.0,0.0,0.0));
//
                ImGui::SetCursorPosY(accumPos);
                if(ImGui::TreeNode(p.parameter->getName().c_str())){
                    p.open = true;
                    //Draw Slider / Control
                    ImGui::Button(p.parameter->getName().c_str());
                    
                    //Draw Curve
                    ImGui::SameLine(left + 20);
                    ImGui::BeginGroup();
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 200));
                    ImGui::BeginChild("scrolling_region", ImVec2(right - 10, p.height - ImGui::GetFrameHeightWithSpacing()), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
                    
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar(2);
                    ImGui::EndGroup();
                    
                    accumPos += p.height;
                    ImGui::TreePop();
                }else{
                    p.open = false;
                    accumPos += ImGui::GetFrameHeight();// + (ImGui::GetStyle().ItemSpacing.y*4);
                }
                ImGui::PopStyleColor(5);
                ImGui::Spacing();
            }
            //ImGui::End();
        }
         */
        
    }
    ImGui::End();
}

void ofxOceanodeTime::addParameter(ofxOceanodeAbstractParameter* p, ofColor _color){
    p->setTimelined(true);
    timlinedParameters.emplace_back(p,_color, 100);
}

void ofxOceanodeTime::removeParameter(ofxOceanodeAbstractParameter* p){
    p->setTimelined(false);
    auto timelineToRemove = std::find_if(timlinedParameters.begin(), timlinedParameters.end(), [p](const ofxOceanodeTimelinedItem& i){return i.parameter == p;});
//    float sizeBackup = timelineToRemove->sizeRelative;
    timlinedParameters.erase(timelineToRemove);
//    for (auto &sp : timlinedParameters) {
//        sp.sizeRelative += ((sizeBackup - 1) / timlinedParameters.size());
//    }
    
}

Timestamp::Timestamp() : currentTime(std::chrono::system_clock::now()) {
    
}

Timestamp::Timestamp(int64_t microsecondsSinceEpoch) {
    currentTime = std::chrono::system_clock::time_point(std::chrono::microseconds(microsecondsSinceEpoch));
}

uint64_t Timestamp::epochMicroseconds() const {
    auto epoch = currentTime.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();
}

void Timestamp::update() {
    currentTime = std::chrono::system_clock::now();
}

void Timestamp::substractMs(float ms){
    // Convert milliseconds to microseconds for higher precision
    auto microDuration = std::chrono::microseconds(static_cast<int64_t>(ms * 1000.0f));
    
    // Subtract the duration from the current time
    currentTime -= microDuration;
}

bool Timestamp::operator==(const Timestamp& other) const {
    return currentTime == other.currentTime;
}

bool Timestamp::operator!=(const Timestamp& other) const {
    return currentTime != other.currentTime;
}

bool Timestamp::operator<(const Timestamp& other) const {
    return currentTime < other.currentTime;
}

bool Timestamp::operator<=(const Timestamp& other) const {
    return currentTime <= other.currentTime;
}

bool Timestamp::operator>(const Timestamp& other) const {
    return currentTime > other.currentTime;
}

bool Timestamp::operator>=(const Timestamp& other) const {
    return currentTime >= other.currentTime;
}
