#pragma once
#include "ofxOceanodeBaseController.h"
#include "Profiling/ofxOceanodeProfiler.h"
#include "imgui.h"

class ofxOceanodeProfilingController : public ofxOceanodeBaseController {
public:
    ofxOceanodeProfilingController(std::shared_ptr<ofxOceanodeContainer> container, ofxOceanodeCanvas* canvas);
    ~ofxOceanodeProfilingController() override;
    void draw() override;
    void update() override;

private:
    struct Row {
        std::weak_ptr<oceanodeProfiling::Record> record;
        std::string name, path;
        oceanodeProfiling::Sample total;
        double peakMs = 0, selfMs = 0, inclusiveMs = 0, callsPerFrame = 0;
    };
    void refresh();
    bool locate(const std::shared_ptr<oceanodeProfiling::Record>& record);
    void sortRows(const ImGuiTableSortSpecs* specs);

    std::shared_ptr<ofxOceanodeContainer> container;
    ofxOceanodeCanvas* canvas;
    std::shared_ptr<oceanodeProfiling::Session> session;
    std::vector<Row> rows;
    ImGuiTextFilter filter;
    double nextRefresh = 0, measuredMs = 0;
    std::size_t displayedFrames = 0, pendingWindow = 120;
    bool needsSort = true, locateFailed = false;
};
