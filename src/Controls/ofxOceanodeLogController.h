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

#define MAX_MESSAGES 1000

class ofxOceanodeLogController: public ofxOceanodeBaseController, public ofBaseLoggerChannel{
public:
    ofxOceanodeLogController() : ofxOceanodeBaseController("Log"){
        oldSize = 0;
    }
    ~ofxOceanodeLogController(){};
    
    void draw(){
        //https://github.com/ocornut/imgui/issues/300
        if(ImGui::Button("[Clear]"))
        {
            messagesBuffer.clear();
        }
        ImGui::Separator();
        ImGui::BeginChild("logWindow");
        auto messagesCopy = messagesBuffer;
        for(auto &l : messagesCopy){
            ImGui::TextUnformatted(l.c_str());
        }
        if(messagesCopy.size() != oldSize){
            ImGui::SetScrollHereY();
        }
        oldSize = messagesCopy.size();
        
        while(messagesBuffer.size() > MAX_MESSAGES){
            messagesBuffer.pop_front();
        }
        ImGui::EndChild();
    }
    
    /// \brief Log a message.
    /// \param level The log level.
    /// \param module The target module.
    /// \param message The log message.
    virtual void log(ofLogLevel level, const string & module, const string & message){
        messagesBuffer.push_back(message);
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
};


#endif /* ofxOceanodeLogController_h */
