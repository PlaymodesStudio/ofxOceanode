//
//  ofxOceanodeSplash.h
//
//  Startup splash shared by every Oceanode app: the embedded artwork with the
//  startup log overlaid, shown until the app reports it is ready.
//  Header-only and included from ofxOceanode.cpp alone, so existing host
//  projects pick it up without being regenerated.
//

#ifndef ofxOceanodeSplash_h
#define ofxOceanodeSplash_h

#include "ofMain.h"
#include "ofxOceanode.h"
#include "ofxOceanodeSplashImage.h"
#include "imgui.h"
#include <algorithm>
#include <deque>
#include <iostream>
#include <mutex>
#include <sstream>

// Tees startup output, including background-thread output such as scsynth's,
// to a bounded UI buffer without replacing the existing console/file logger.
class ofxOceanodeSplashLog : public ofBaseLoggerChannel {
public:
    explicit ofxOceanodeSplashLog(std::shared_ptr<ofBaseLoggerChannel> destination)
    : destination(std::move(destination)) {}

    void log(ofLogLevel level, const std::string& module, const std::string& message) override {
        std::lock_guard<std::mutex> lock(mutex);
        if(destination) destination->log(level, module, message);
        if(!recording) return;
        std::istringstream input(message);
        for(std::string line; std::getline(input, line); ){
            if(line.empty()) continue;
            lines.push_back(module.empty() ? line : module + ": " + line);
            if(lines.size() > 160) lines.pop_front();
        }
    }

    std::deque<std::string> snapshot() {
        std::lock_guard<std::mutex> lock(mutex);
        return lines;
    }
    void finish() {
        std::lock_guard<std::mutex> lock(mutex);
        recording = false;
    }
    std::shared_ptr<ofBaseLoggerChannel> previous() const { return destination; }
private:
    std::shared_ptr<ofBaseLoggerChannel> destination;
    std::mutex mutex;
    std::deque<std::string> lines;
    bool recording = true;
};

class ofxOceanodeSplash {
public:
    ~ofxOceanodeSplash(){ exit(); }

    // Call at the end of ofxOceanode::setup(), after the log panel is installed,
    // so the startup buffer and Oceanode's panel both receive the output.
    void setup(ofxOceanode& oceanode){
        log = std::make_shared<ofxOceanodeSplashLog>(ofGetLoggerChannel());
        ofSetLoggerChannel(log);

        // One preference per app, named after its executable.
        const std::string appName = ofFilePath::getBaseName(ofFilePath::getCurrentExePath());
#ifdef TARGET_OSX
        preferencePath = ofFilePath::join(ofFilePath::getUserHomeDir(),
            "Library/Application Support/" + appName + "/startup.json");
#else
        preferencePath = ofFilePath::join(ofFilePath::getUserHomeDir(), "." + appName + "/startup.json");
#endif
        if(ofFile::doesFileExist(preferencePath, false)){
            try{
                const auto settings = ofLoadJson(preferencePath);
                if(settings.is_object() && settings.contains("dontShowSplash") &&
                   settings["dontShowSplash"].is_boolean()){
                    dontShow = settings["dontShowSplash"].get<bool>();
                }
            }catch(const std::exception &error){
                ofLogWarning("ofxOceanode") << "Could not read startup preference: " << error.what();
            }
        }
        show = !dontShow;
        oceanode.registerConfigMenuItem("Startup splash...", [this]{
            show = true;
            reopened = true;
        }, [this]{ update(); }, [this]{ draw(); });
    }

    void exit(){
        if(log && ofGetLoggerChannel() == log) ofSetLoggerChannel(log->previous());
    }

    // Addons that start something slow keep the splash up until it is ready.
    void hold(){ holds++; totalHolds++; everHeld = true; }
    void release(){ if(holds > 0) holds--; }

private:
    // Seconds the splash stays when nothing holds it, and the longest a hold
    // (a server that never boots, say) can keep it open.
    static constexpr float minimumSeconds = 1.5f;
    static constexpr float maximumSeconds = 30.0f;

    void update(){
        if(startupComplete) return;
        const float now = ofGetElapsedTimef();
        if(firstUpdateTime < 0) firstUpdateTime = now;
        const float elapsed = now - firstUpdateTime;
        if((holds == 0 && elapsed >= minimumSeconds) || elapsed >= maximumSeconds){
            startupComplete = true;
            log->finish();
            // Also to the launching terminal, for apps started unattended.
            if(holds == 0){
                std::cout << "[Oceanode] Startup complete" << (everHeld ? ": everything that was starting is ready" : "") << std::endl;
            }else{
                std::cerr << "[Oceanode] Startup NOT complete after " << maximumSeconds << " s: "
                          << holds << " of " << totalHolds << " still not ready" << std::endl;
            }
        }
    }

    bool savePreference(){
        const auto directory = ofFilePath::getEnclosingDirectory(preferencePath);
        if(!ofDirectory::doesDirectoryExist(directory, false) &&
           !ofDirectory::createDirectory(directory, false, true)) return false;
        return ofSaveJson(preferencePath, ofJson{{"dontShowSplash", dontShow}});
    }

    // The splash uses Oceanode's ImGui frame so it blocks canvas interaction while open.
    void draw(){
        if(!show) return;
        if(!image.isAllocated()){
            // ImGui samples with normalized coordinates.
            const bool arbTextures = ofGetUsingArbTex();
            ofDisableArbTex();
            const ofBuffer png(reinterpret_cast<const char*>(ofxOceanodeSplashPng), ofxOceanodeSplashPngSize);
            const bool loaded = image.load(png);
            if(arbTextures) ofEnableArbTex();
            if(!loaded){
                ofLogWarning("ofxOceanode") << "Splash image unavailable";
                show = false;
                return;
            }
        }

        const char *popupName = "Oceanode startup splash";
        const auto *viewport = ImGui::GetMainViewport();
        const float padding = 16.0f * 2.0f / 3.0f;
        const float aspect = image.getWidth() / image.getHeight();
        const float imageWidth = std::max(1.0f, std::min({1000.0f * 2.0f / 3.0f,
            viewport->WorkSize.x * 0.9f - padding * 2.0f,
            (viewport->WorkSize.y * 0.9f - padding * 2.0f) * aspect}));
        const ImVec2 imageSize(imageWidth, imageWidth / aspect);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                     viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(imageSize.x + padding * 2.0f,
                                      imageSize.y + padding * 2.0f));
        if(!ImGui::IsPopupOpen(popupName)) ImGui::OpenPopup(popupName);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.02f, 0.08f, 0.32f, 1.0f));
        const auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;
        if(ImGui::BeginPopupModal(popupName, nullptr, flags)){
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const ImTextureID texture = (ImTextureID)(uintptr_t)image.getTexture().getTextureData().textureID;
            ImGui::Image(texture, imageSize);

            // Artwork coordinates: the clear cyan column between beta and the URL.
            ImGui::SetCursorScreenPos(ImVec2(origin.x + imageSize.x * 0.032f,
                                            origin.y + imageSize.y * 0.415f));
            ImGui::PushFont(nullptr, 10.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.035f, 0.15f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
            if(ImGui::BeginChild("Startup logs", ImVec2(imageSize.x * 0.17f, imageSize.y * 0.46f),
                                 ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)){
                for(const auto& line : log->snapshot())
                    ImGui::TextWrapped("%s", line.c_str());
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::PopFont();

            // Overlay the preference inside the artwork, with no extra footer.
            ImGui::PushFont(nullptr, 16.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
            const char *label = "Don't show on future startups";
            const float checkWidth = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
                                     ImGui::CalcTextSize(label).x;
            ImGui::SetCursorScreenPos(ImVec2(origin.x + imageSize.x - checkWidth - 5.0f,
                                            origin.y + imageSize.y - ImGui::GetFrameHeight() - 5.0f));
            if(ImGui::Checkbox(label, &dontShow))
                preferenceSaveFailed = !savePreference();
            const bool overCheckbox = ImGui::IsItemHovered();
            if(preferenceSaveFailed && overCheckbox)
                ImGui::SetTooltip("Could not save startup preference.");
            ImGui::PopStyleVar();
            ImGui::PopFont();

            // Closes by itself once startup is done; Escape or a click on the
            // artwork closes it at any time.
            const bool dismissed = ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !overCheckbox &&
                 ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows));
            if(dismissed || (!reopened && startupComplete)){
                show = false;
                reopened = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    ofImage image;
    std::string preferencePath;
    std::shared_ptr<ofxOceanodeSplashLog> log;
    int holds = 0;
    int totalHolds = 0;
    bool everHeld = false;
    float firstUpdateTime = -1.0f;
    bool reopened = false;
    bool startupComplete = false;
    bool show = true;
    bool dontShow = false;
    bool preferenceSaveFailed = false;
};

#endif /* ofxOceanodeSplash_h */
