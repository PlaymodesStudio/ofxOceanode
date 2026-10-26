#include "ofxOceanodeProfilingController.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeCanvas.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeGlDiagnostics.h"
#include <algorithm>
#include <functional>
#include <unordered_set>

using namespace oceanodeProfiling;

ofxOceanodeProfilingController::ofxOceanodeProfilingController(
    std::shared_ptr<ofxOceanodeContainer> container, ofxOceanodeCanvas* canvas)
    : ofxOceanodeBaseController("Profiling"), container(std::move(container)), canvas(canvas),
      session(this->container->getProfilingSession()) {}

ofxOceanodeProfilingController::~ofxOceanodeProfilingController(){ session->setRecording(false); }

void ofxOceanodeProfilingController::update(){
    // No timer reads, aggregation or refresh while disabled. Copied rows remain
    // intact even if the graph is subsequently edited or replaced.
    if(!session->isRecording()) return;
    const double now = ofGetElapsedTimef();
    if(now >= nextRefresh){ refresh(); nextRefresh = now + 0.25; }
}

void ofxOceanodeProfilingController::refresh(){
    std::unordered_set<const Record*> expanded;
    for(const auto& row : rows) if(row.expanded) if(auto record = row.record.lock()) expanded.insert(record.get());
    rows.clear();
    measuredMs = 0;
    displayedFrames = session->sampleFrames();
    const double denominator = static_cast<double>(std::max<std::size_t>(1, displayedFrames));
    for(const auto& record : session->liveRecords()){
        const auto identity = record->identity();
        Row row;
        row.record = record;
        row.name = identity.name;
        row.path = identity.path;
        row.total = record->total;
        row.expanded = expanded.count(record.get()) != 0;
        row.selfMs = row.total.selfNs() / denominator / 1e6;
        row.inclusiveMs = row.total.inclusiveNs() / denominator / 1e6;
        row.peakMs = record->peakSelfNs() / 1e6;
        for(const auto& phase : row.total.phases) row.callsPerFrame += phase.calls / denominator;
        std::uint64_t publications = 0;
        unsigned activeOutputs = 0;
        record->forEachPublication([&](Publication& publication){
            if(!publication.outputCapable) return;
            OutputRow output{publication.name(), publication.bidirectional, session->publicationStats(publication)};
            publications += output.stats.count;
            if(output.stats.count){ ++activeOutputs; row.outputIntervalMs = output.stats.intervalMs; }
            row.outputs.push_back(std::move(output));
        });
        std::sort(row.outputs.begin(), row.outputs.end(), [](const OutputRow& a, const OutputRow& b){ return a.name < b.name; });
        row.outputsPerFrame = publications / denominator;
        if(session->sampledSeconds() > 0) row.outputsPerSecond = publications / session->sampledSeconds();
        // Never present the spacing between different output pins as a node's
        // publication interval. It would make a three-output 60 Hz node 180 Hz.
        if(activeOutputs != 1) row.outputIntervalMs = -1;
        measuredMs += row.selfMs;
        rows.push_back(std::move(row));
    }
    needsSort = true;
}

void ofxOceanodeProfilingController::sortRows(const ImGuiTableSortSpecs* specs){
    std::sort(rows.begin(), rows.end(), [specs](const Row& a, const Row& b){
        const int column = specs && specs->SpecsCount ? specs->Specs[0].ColumnIndex : 2;
        const bool ascending = specs && specs->SpecsCount && specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
        if(column < 2){
            const auto& left = column == 0 ? a.name : a.path;
            const auto& right = column == 0 ? b.name : b.path;
            if(left != right) return ascending ? left < right : left > right;
        }else{
            auto value = [column](const Row& row){
                if(column == 2 || column == 3) return row.selfMs;
                if(column == 4) return row.inclusiveMs;
                if(column == 5) return row.peakMs;
                if(column >= 6 && column <= 9) return row.total.phases[column - 6].selfNs;
                if(column == 10) return row.callsPerFrame;
                if(column == 11) return row.outputsPerFrame;
                if(column == 12) return row.outputsPerSecond;
                return row.outputIntervalMs;
            };
            const double left = value(a), right = value(b);
            if(left != right) return ascending ? left < right : left > right;
        }
        if(a.path != b.path) return a.path < b.path;
        return a.name < b.name;
    });
    needsSort = false;
}

bool ofxOceanodeProfilingController::locate(const std::shared_ptr<Record>& record){
    if(!record) return false;
    ofxOceanodeNode* found = nullptr;
    ofxOceanodeContainer* host = nullptr;
    ofxOceanodeCanvas* targetCanvas = nullptr;
    ofxOceanodeNodeMacro* owner = nullptr;
    std::function<void(ofxOceanodeContainer*, ofxOceanodeCanvas*, ofxOceanodeNodeMacro*)> find;
    find = [&](ofxOceanodeContainer* current, ofxOceanodeCanvas* currentCanvas, ofxOceanodeNodeMacro* macro){
        for(const auto& item : current->getParameterGroupNodesMap()){
            auto* node = item.second;
            if(node->getNodeModel().getProfilingRecord() == record){
                found = node; host = current; targetCanvas = currentCanvas; owner = macro;
                return;
            }
            if(auto* child = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())){
                if(child->getContainer()) find(child->getContainer().get(), child->getCanvas(), child);
                if(found) return;
            }
        }
    };
    find(container.get(), canvas, nullptr);
    if(!found) return false; // Deleted nodes and private MacroMixer graphs.
    if(owner){
        // Instances of a global Macro share one tab: go to the matching node there.
        const auto shown = owner->windowTargetFor(found);
        owner = shown.macro;
        host = owner->getContainer().get();
        targetCanvas = owner->getCanvas();
        if(!shown.node){
            owner->activateWindow();
            ofxOceanodeShared::setActiveCanvasUniqueID(targetCanvas->getUniqueID());
            return true;
        }
        found = shown.node;
    }
    if(owner) owner->activateWindow();
    else { targetCanvas->requestFocus(); targetCanvas->bringOnTop(); }
    ofxOceanodeShared::setActiveCanvasUniqueID(targetCanvas->getUniqueID());
    for(const auto& item : host->getParameterGroupNodesMap()) item.second->getNodeGui().setSelected(false);
    found->getNodeGui().setSelected(true);
    ofxOceanodeShared::nodeSelectedInCanvas(found);
    targetCanvas->requestCenterOnNode(found, 1);
    return true;
}

void ofxOceanodeProfilingController::draw(){
    ImGui::Checkbox("Trace OpenGL errors", &oceanodeGlDiagnostics::enabled());
    if(oceanodeGlDiagnostics::enabled())
        ImGui::TextWrapped("Logs node and macro paths after update/draw callbacks. Driver polling can affect frame times. Errors consumed inside an addon may still use that addon's own log.");
    const bool recording = session->isRecording();
    if(ImGui::Button(recording ? "Stop profiling" : "Profile!")){
        if(!recording) session->reset(pendingWindow);
        session->setRecording(!recording);
        // On stop, copy the latest COMPLETED samples once, then freeze them.
        refresh(); nextRefresh = 0; locateFailed = false;
    }
    const char* status = session->isRecording() ? "Recording CPU times" : (displayedFrames ? "Stopped - last capture" : "Off");
    if(ImGui::GetContentRegionAvail().x >= ImGui::GetItemRectSize().x + ImGui::CalcTextSize(status).x + ImGui::GetStyle().ItemSpacing.x)
        ImGui::SameLine();
    ImGui::TextUnformatted(status);
    ImGui::SetNextItemWidth(140);
    const int windows[] = {60, 120, 300};
    const char* labels[] = {"60 frames", "120 frames", "300 frames"};
    int selected = pendingWindow == 60 ? 0 : pendingWindow == 300 ? 2 : 1;
    if(ImGui::Combo("Averaging window", &selected, labels, 3)){
        pendingWindow = windows[selected];
        // Preserve a stopped capture until the next Profile! action.
        if(session->isRecording()) { session->reset(pendingWindow); refresh(); }
    }
    ImGui::SetNextItemWidth(-1);
    if(ImGui::InputTextWithHint("##profile_filter", "Filter nodes / paths", filter.InputBuf, sizeof(filter.InputBuf)))
        filter.Build();
    ImGui::Text("Measured nodes: %.3f ms/frame | %zu sampled frames", measuredMs, displayedFrames);
    ImGui::TextWrapped("Self excludes nested nodes. Inclusive includes downstream work. Share is a percentage of measured node time. All times include CPU GUI work.");
    ImGui::TextWrapped("Output counts sum output-capable pins. Expand a node for each pin's rate and interval. I/O pins can also forward incoming values; unchanged-value notifications count.");
    if(session->incompleteSpans()) ImGui::TextWrapped("%llu incomplete timing scopes; affected frames were discarded.",
        static_cast<unsigned long long>(session->incompleteSpans()));
    if(locateFailed) ImGui::TextWrapped("This node is no longer on a visible canvas, or belongs to a private MacroMixer graph.");
    if(rows.empty()){
        ImGui::TextWrapped(session->isRecording() ? "Waiting for completed frames..." : "Press Profile! to capture node CPU costs. Stopping preserves the results.");
        return;
    }
    const auto flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
    std::shared_ptr<Record> clicked;
    if(ImGui::BeginTable("##cpu_profile", 14, flags, ImVec2(0, std::max(120.0f, ImGui::GetContentRegionAvail().y)))){
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthFixed, 180);
        ImGui::TableSetupColumn("Canvas / Macro path", ImGuiTableColumnFlags_WidthFixed, 200);
        ImGui::TableSetupColumn("Self ms/frame", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("Share %", ImGuiTableColumnFlags_WidthFixed, 75);
        ImGui::TableSetupColumn("Inclusive ms/frame", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableSetupColumn("Peak self ms/frame", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableSetupColumn("Update ms", ImGuiTableColumnFlags_WidthFixed, 95);
        ImGui::TableSetupColumn("Draw ms", ImGuiTableColumnFlags_WidthFixed, 95);
        ImGui::TableSetupColumn("Listeners ms", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("GUI ms", ImGuiTableColumnFlags_WidthFixed, 95);
        ImGui::TableSetupColumn("Calls/frame", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Out events/frame", ImGuiTableColumnFlags_WidthFixed, 130);
        ImGui::TableSetupColumn("Out events/s", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("Out interval ms", ImGuiTableColumnFlags_WidthFixed, 125);
        ImGui::TableHeadersRow();
        auto* specs = ImGui::TableGetSortSpecs();
        if(needsSort || (specs && specs->SpecsDirty)){
            sortRows(specs);
            if(specs) specs->SpecsDirty = false;
        }
        ImGuiListClipper clipper;
        struct VisibleRow { Row* node; const OutputRow* output; };
        std::vector<VisibleRow> visible;
        for(auto& row : rows) if(filter.PassFilter((row.name + " " + row.path).c_str())){
            visible.push_back({&row, nullptr});
            if(row.expanded) for(const auto& output : row.outputs) visible.push_back({&row, &output});
        }
        auto metric = [](int column, double value, int decimals){
            ImGui::TableSetColumnIndex(column);
            if(value < 0) ImGui::TextUnformatted("--");
            else ImGui::Text("%.*f", decimals, value);
        };
        clipper.Begin(static_cast<int>(visible.size()));
        while(clipper.Step()) for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i){
            auto& row = *visible[i].node;
            ImGui::PushID(i);
            // Expanded pin rows have the same height as node rows for clipping.
            ImGui::TableNextRow(0, ImGui::GetFrameHeight());
            ImGui::TableSetColumnIndex(0);
            if(const auto* output = visible[i].output){
                ImGui::Indent(ImGui::GetFrameHeight());
                ImGui::TextUnformatted(output->name.c_str());
                ImGui::Unindent(ImGui::GetFrameHeight());
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s\nOne emitted parameter notification is one publication, regardless of cable count. Intervals need at least two observed publications.", output->name.c_str(), output->bidirectional ? "I/O pin: includes forwarding received values." : "Output-only pin.");
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(output->bidirectional ? "I/O pin" : "Output pin");
                metric(11, output->stats.perFrame, 2);
                metric(12, output->stats.perSecond, 2);
                metric(13, output->stats.intervalMs, 3);
                ImGui::PopID();
                continue;
            }
            if(!row.outputs.empty()){
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0));
                const bool toggle = ImGui::ArrowButton("##outputs", row.expanded ? ImGuiDir_Down : ImGuiDir_Right);
                ImGui::PopStyleVar();
                if(toggle) row.expanded = !row.expanded;
                ImGui::SameLine();
            }
            if(ImGui::Selectable(row.name.c_str())){
                clicked = row.record.lock();
                if(!clicked) locateFailed = true;
            }
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s\nClick to locate. Calls count timed lifecycle, GUI and parameter/group notifications.", row.name.c_str(), row.path.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(row.path.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.3f", row.selfMs);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.1f", measuredMs > 0 ? 100 * row.selfMs / measuredMs : 0);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%.3f", row.inclusiveMs);
            ImGui::TableSetColumnIndex(5); ImGui::Text("%.3f", row.peakMs);
            for(unsigned phase = 0; phase < phaseCount; ++phase){
                ImGui::TableSetColumnIndex(6 + phase);
                ImGui::Text("%.3f", row.total.phases[phase].selfNs / std::max<std::size_t>(1, displayedFrames) / 1e6);
            }
            ImGui::TableSetColumnIndex(10); ImGui::Text("%.1f", row.callsPerFrame);
            metric(11, row.outputsPerFrame, 2);
            metric(12, row.outputsPerSecond, 2);
            metric(13, row.outputIntervalMs, 3);
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Mean interval between notifications of the single active output pin. With multiple active pins, expand the node to inspect each interval. -- means unavailable.");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if(clicked) locateFailed = !locate(clicked);
}
