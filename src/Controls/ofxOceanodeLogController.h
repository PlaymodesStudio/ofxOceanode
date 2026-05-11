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

#define MAX_MESSAGES 10000

class ofxOceanodeLogController: public ofxOceanodeBaseController, public ofBaseLoggerChannel{
public:
    ofxOceanodeLogController() : ofxOceanodeBaseController("Log"){
        userIsAtBottom = true;
    }
    ~ofxOceanodeLogController(){};
    
    void draw(){
        // 1. Drain incoming messages from the thread channel
        std::string message;
        while(messageChannel.tryReceive(message)){
            messagesBuffer.push_back(message);
        }
        
        // 2. Trim buffer BEFORE rendering. Count how many lines were dropped from the
        //    front so we can compensate the scroll position to keep the user's view
        //    anchored on the same logical content (not on absolute pixel position).
        int trimmedFromFront = 0;
        while(messagesBuffer.size() > MAX_MESSAGES){
            messagesBuffer.pop_front();
            ++trimmedFromFront;
        }
        
        // 3. Clear button — also resets scroll intent so auto-scroll resumes
        if(ImGui::Button("[Clear]")){
            messagesBuffer.clear();
            userIsAtBottom = true;
            trimmedFromFront = 0;  // nothing to compensate after a clear
        }
        
        // 4. Scrollable child region
        ImGui::BeginChild("##LogRegion", ImGui::GetContentRegionAvail(), false,
                          ImGuiWindowFlags_HorizontalScrollbar);
        
        // 5. If we trimmed lines from the front while the user is NOT pinned to bottom,
        //    shift the scroll up by trimmedFromFront * lineHeight so the line currently
        //    visible at the user's read position stays at the same screen Y.
        //    When pinned to bottom, SetScrollHereY(1.0f) below handles it correctly anyway.
        if(trimmedFromFront > 0 && !userIsAtBottom){
            float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            float newScrollY = ImGui::GetScrollY() - (float)trimmedFromFront * lineHeight;
            if(newScrollY < 0.0f) newScrollY = 0.0f;
            ImGui::SetScrollY(newScrollY);
        }
        
        // 6. Read scroll state AFTER any compensation to detect user intent for next frame.
        {
            float scrollY    = ImGui::GetScrollY();
            float scrollMaxY = ImGui::GetScrollMaxY();
            if(scrollMaxY > 0.0f){
                // Consider "at bottom" if within one text line of the maximum.
                userIsAtBottom = (scrollY >= scrollMaxY - ImGui::GetTextLineHeightWithSpacing());
            }
            // When content doesn't overflow (scrollMaxY <= 0), keep the current state
            // (defaults to true from constructor / clear button).
        }
        
        // 7. Render only visible lines via clipper — O(visible) regardless of buffer size.
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGuiListClipper clipper;
        clipper.Begin((int)messagesBuffer.size());
        while(clipper.Step()){
            for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i){
                ImGui::TextUnformatted(messagesBuffer[i].c_str());
            }
        }
        clipper.End();
        ImGui::PopStyleColor();
        
        // 8. Auto-scroll: pin to the very bottom if user hasn't scrolled away.
        //    SetScrollHereY(1.0f) aligns the current cursor pos (after all items)
        //    with the bottom edge of the visible region.
        if(userIsAtBottom){
            ImGui::SetScrollHereY(1.0f);
        }
        
        ImGui::EndChild();
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
    bool userIsAtBottom;   // true = auto-scroll is active (user is at the bottom)
    
    ofThreadChannel<string> messageChannel;
};


#endif /* ofxOceanodeLogController_h */
