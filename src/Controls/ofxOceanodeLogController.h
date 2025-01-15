//
//  ofxOceanodeLogController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 13/03/2018.
//

#ifndef ofxOceanodeLogController_h
#define ofxOceanodeLogController_h

#include "ofxOceanodeBaseController.h"
#include "imgui.h"
#include "imgui_internal.h"

#define MAX_MESSAGES 1000

class ofxOceanodeLogController: public ofxOceanodeBaseController, public ofBaseLoggerChannel{
public:
    ofxOceanodeLogController() : ofxOceanodeBaseController("Log"){
        oldSize = 0;
        hasToScroll = false;
    }
    ~ofxOceanodeLogController(){};
    
    void draw(){
        std::string message;
        while(messageChannel.tryReceive(message)){
            messagesBuffer.push_back(message);
        }
        
        if(ImGui::Button("[Clear]"))
        {
            messagesBuffer.clear();
        }
        
        std::string combinedMessages;
        for(auto &s : messagesBuffer){
            combinedMessages += s + "\n";
        }
        
        ImGui::InputTextMultiline("##LogText", (char *)combinedMessages.c_str(), combinedMessages.size(), ImGui::GetContentRegionAvail(), ImGuiInputTextFlags_ReadOnly);
        
        if(hasToScroll){
            //https://github.com/ocornut/imgui/issues/5484
            ImGuiContext& g = *GImGui;
            const char* child_window_name = NULL;
            ImFormatStringToTempBuffer(&child_window_name, NULL, "%s/%s_%08X", g.CurrentWindow->Name, "##LogText", ImGui::GetID("##LogText"));
            ImGuiWindow* child_window = ImGui::FindWindowByName(child_window_name);
            if(child_window != NULL){
                ImGui::SetScrollY(child_window, child_window->ScrollMax.y);
                hasToScroll = false;
            }
        }
        
        if(messagesBuffer.size() != oldSize){
            hasToScroll = true;
        }
        
        oldSize = messagesBuffer.size();
        
        while(messagesBuffer.size() > MAX_MESSAGES){
            messagesBuffer.pop_front();
        }
    }
    
    /// \brief Log a message.
    /// \param level The log level.
    /// \param module The target module.
    /// \param message The log message.
    virtual void log(ofLogLevel level, const string & module, const string & message){
        messageChannel.send(message);
    }
    
    /// \brief Log a message.
    /// \param level The log level.
    /// \param module The target module.
    /// \param format The printf-style format string.
    virtual void log(ofLogLevel level, const string & module, const char* format, ...){
        
    }
    
    /// \brief Log a message.
    /// \param level The log level.
    /// \param module The target module.
    /// \param format The printf-style format string.
    /// \param args the list of printf-style arguments.
    virtual void log(ofLogLevel level, const string & module, const char* format, va_list args){
        
    }
    
    string getLine(int i){
        if(i < messagesBuffer.size())
               return messagesBuffer[i];
           else
               return "";
    }
    
    int getSize(){return messagesBuffer.size();};
    
    void eraseLastLine(){
        messagesBuffer.pop_front();
    }
    
private:
    deque<string>  messagesBuffer;
    int oldSize;
    bool hasToScroll;
    
    ofThreadChannel<string> messageChannel;
};


#endif /* ofxOceanodeLogController_h */
