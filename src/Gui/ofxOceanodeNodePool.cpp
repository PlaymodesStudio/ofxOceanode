#include "ofxOceanodeNodePool.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeNodeGui.h"
#include "ofxOceanodeNodeHelpCatalog.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <functional>
#include <sstream>

namespace {
bool matches(const std::string& text, const char* query) {
    const auto lower = ofToLower(text);
    std::istringstream words(ofToLower(std::string(query)));
    std::string word;
    while(words >> word) if(lower.find(word) == std::string::npos) return false;
    return true;
}
std::string descriptionFor(ofxOceanodeNodeRegistry& registry, const std::string& name) {
    const auto& descriptions = registry.getModelDescriptions();
    const auto found = descriptions.find(name);
    return found != descriptions.end() && !found->second.empty()
        ? found->second : ofxOceanodeNodeHelpDescription(name);
}
}

ofxOceanodeNodePool::ofxOceanodeNodePool(std::shared_ptr<ofxOceanodeContainer> owner,
                                       ofxOceanodeCanvas* canvas)
    : root(std::move(owner)), rootCanvas(canvas) {}

void ofxOceanodeNodePool::open() {
    visible = focusRequested = true;
}

void ofxOceanodeNodePool::drawMenu() {
#ifdef TARGET_OSX
    const char* shortcut = "CMD+N";
#else
    const char* shortcut = "CTRL+N";
#endif
    if(ImGui::MenuItem("Node Pool", shortcut, visible)) open();
}

void ofxOceanodeNodePool::draw() {
    const auto& io = ImGui::GetIO();
#ifdef TARGET_OSX
    // ImGui swaps Cmd/Ctrl when macOS behaviors are enabled. KeyCtrl then
    // represents physical Command; KeySuper represents physical Control.
    const bool command = io.ConfigMacOSXBehaviors
        ? io.KeyCtrl && !io.KeySuper
        : io.KeySuper && !io.KeyCtrl;
#else
    const bool command = io.KeyCtrl && !io.KeySuper;
#endif
    if(command && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_N, false)) open();
    if(!visible) return;
    ImGui::SetNextWindowSize(ImVec2(780, 540), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(560, 320), ImVec2(FLT_MAX, FLT_MAX));
    if(focusRequested) ImGui::SetNextWindowFocus();
    // The pool stays a floating utility window, independent of canvas docking.
    if(ImGui::Begin("Node Pool", &visible, ImGuiWindowFlags_NoDocking)) {
        if(ImGui::BeginTabBar("##poolTabs")) {
            if(ImGui::BeginTabItem("Nodes")) { drawTab(false); ImGui::EndTabItem(); }
            if(ImGui::BeginTabItem("Macros")) { drawTab(true); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    focusRequested = false;
}

void ofxOceanodeNodePool::drawTab(bool macro) {
    auto& tab = macro ? macros : nodes;
    auto& registry = *root->getRegistry();
    std::vector<Entry> entries;
    if(macro) {
        std::function<void(const std::shared_ptr<macroCategory>&, std::string)> collect;
        collect = [&](const std::shared_ptr<macroCategory>& category, std::string path) {
            if(!category) return;
            for(const auto& child : category->categories)
                collect(child, path.empty() ? child->name : path + " / " + child->name);
            for(const auto& item : category->macros)
                entries.push_back({item.first, path.empty() ? "Uncategorized" : path, item.second});
        };
        collect(ofxOceanodeShared::getMacroDirectoryStructure(), "");
    } else {
        for(const auto& item : registry.getRegisteredModelsCategoryAssociation())
            entries.push_back({item.first, item.second.empty() ? "Uncategorized" : item.second, ""});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        return a.category == b.category ? a.name < b.name : a.category < b.category;
    });
    ImGui::SetNextItemWidth(-1);
    if(focusRequested) ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("##search", macro ? "Search macros..." : "Search nodes...", tab.search, sizeof(tab.search));
    const float width = ImGui::GetContentRegionAvail().x;
    const float listWidth = std::min(300.0f, width * 0.36f);
    const Entry* selected = nullptr;
    for(const auto& entry : entries)
        if((macro ? entry.path : entry.name) == tab.selected) { selected = &entry; break; }
    if(ImGui::BeginChild("##categories", ImVec2(listWidth, 0), true)) {
        std::string category;
        int count = 0;
        for(const auto& entry : entries) {
            std::string searchable = entry.name + " " + entry.category;
            if(!macro) searchable += " " + descriptionFor(registry, entry.name);
            if(!matches(searchable, tab.search)) continue;
            ++count;
            if(category != entry.category) {
                category = entry.category;
                ImGui::Spacing();
                ImGui::TextDisabled("%s", category.c_str());
                ImGui::Separator();
            }
            const auto& key = macro ? entry.path : entry.name;
            ImGui::PushID(key.c_str());
            if(ImGui::Selectable(entry.name.c_str(), key == tab.selected)) {
                tab.selected = key;
                selected = &entry;
                error.clear();
            }
            ImGui::PopID();
        }
        if(count == 0) ImGui::TextWrapped(macro ? "No macros found." : "No nodes found.");
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if(ImGui::BeginChild("##details", ImVec2(0, 0))) {
        ofxOceanodeContainer* owner = nullptr;
        ofxOceanodeCanvas* canvas = nullptr;
        findTarget(root.get(), rootCanvas, owner, canvas);
        if(!owner) { owner = root.get(); canvas = rootCanvas; }
        ImGui::TextWrapped("Add to: %s", owner->getCanvasDisplayPath().c_str());
        ImGui::BeginDisabled(!selected);
        if(ImGui::Button("Add", ImVec2(100, 0)) && selected) add(*selected, macro);
        ImGui::EndDisabled();
        if(!error.empty()) ImGui::TextWrapped("%s", error.c_str());
        ImGui::Separator();
        if(selected) {
            ImGui::TextWrapped("%s", selected->name.c_str());
            ImGui::TextDisabled("%s", selected->category.c_str());
            ImGui::Spacing();
            std::string description;
            if(macro) {
                if(descriptionPath != selected->path) {
                    descriptionPath = selected->path;
                    macroDescription.clear();
                    const auto file = selected->path + "/description.json";
                    if(ofFile::doesFileExist(file)) {
                        try {
                            const auto json = ofLoadJson(file);
                            if(json.is_object() && json.contains("Description") && json["Description"].is_string())
                                macroDescription = json["Description"].get<std::string>();
                        } catch(const std::exception&) {}
                    }
                }
                description = macroDescription;
            } else description = descriptionFor(registry, selected->name);
            ImGui::TextWrapped("%s", description.empty() ? "No description available." : description.c_str());
            if(!macro) {
                const auto* hint = ofxOceanodeNodeHelpEntry(selected->name);
                if(hint && hint->contains("parameters") && !(*hint)["parameters"].empty()) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Parameter preview");
                    // Catalog names are hints, not live port direction/type metadata.
                    // Do not instantiate resource-owning models to draw a preview.
                    if(ImGui::BeginChild("##preview", ImVec2(0, 0), true)) {
                        ImGui::TextWrapped("%s", selected->name.c_str());
                        ImGui::Separator();
                        for(const auto& parameter : (*hint)["parameters"])
                            if(parameter.is_string()) ImGui::BulletText("%s", parameter.get<std::string>().c_str());
                    }
                    ImGui::EndChild();
                }
            }
        } else ImGui::TextWrapped("Select an item to read its description and add it to the canvas.");
    }
    ImGui::EndChild();
}

bool ofxOceanodeNodePool::findTarget(ofxOceanodeContainer* owner, ofxOceanodeCanvas* canvas,
    ofxOceanodeContainer*& target, ofxOceanodeCanvas*& targetCanvas) {
    if(canvas->getUniqueID() == ofxOceanodeShared::getActiveCanvasUniqueID()) {
        target = owner;
        targetCanvas = canvas;
        return true;
    }
    for(auto* node : owner->getAllModules())
        if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel()))
            if(&macro->windowInstance() == macro && findTarget(macro->getContainer().get(), macro->getCanvas(), target, targetCanvas)) return true;
    return false;
}

void ofxOceanodeNodePool::add(const Entry& entry, bool macro) {
    ofxOceanodeContainer* owner = nullptr;
    ofxOceanodeCanvas* canvas = nullptr;
    findTarget(root.get(), rootCanvas, owner, canvas);
    if(!owner) { owner = root.get(); canvas = rootCanvas; }
    if(macro && !ofDirectory(entry.path).exists()) {
        error = "This macro is no longer available. Use Help > Reload Macros.";
        return;
    }
    owner->beginCanvasEdit();
    try {
        auto model = owner->getRegistry()->create(macro ? "Macro" : entry.name);
        if(!model) error = "This node is no longer available.";
        else {
            auto& node = owner->createNode(std::move(model), macro ? entry.path : "");
            auto position = canvas->getContentRegionSizeInWorld() * 0.5f - canvas->getScrolling();
            // Repeated Add clicks should leave each new header reachable.
            const float step = std::max(24.0f, static_cast<float>(canvas->getGridSize()));
            for(size_t attempt = 0; attempt < owner->getAllModules().size(); ++attempt) {
                bool occupied = false;
                for(auto* item : owner->getAllModules()) {
                    if(item == &node) continue;
                    const auto other = item->getNodeGui().getPosition();
                    if(glm::distance(position, glm::vec2(other)) < step) { occupied = true; break; }
                }
                if(!occupied) break;
                position += glm::vec2(step);
            }
            if(canvas->getSnapToGrid() && canvas->getGridSize() > 0) {
                const float grid = canvas->getGridSize();
                position = glm::round(position / grid) * grid;
            }
            node.getNodeGui().setPosition(position);
            for(auto* item : owner->getAllModules()) item->getNodeGui().setSelected(item == &node);
            ofxOceanodeShared::nodeSelectedInCanvas(&node);
            canvas->requestCenterOnNode(&node, 1);
            error.clear();
        }
    } catch(const std::exception& exception) {
        error = std::string("Could not add: ") + exception.what();
    }
    owner->endCanvasEdit();
}
