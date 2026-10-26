#include "ofxOceanodeNodePool.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeNodeGui.h"
#include "ofxOceanodeNodeHelpCatalog.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <functional>
#include <stdexcept>
#include "ofxOceanodeTransport.h"

namespace {
bool matches(const std::string& text, const char* query) {
    return ofToLower(text).find(ofToLower(std::string(query))) != std::string::npos;
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
    if(!visible) { resetPreview(); return; }
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
    if(!visible) resetPreview();
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
        const auto tree = ofxOceanodeShared::getMacroDirectoryStructure();
        if(macroTree.lock() != tree) {
            macroDescriptions.clear();
            macroTree = tree;
        }
        collect(tree, "");
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
    const float clearWidth = ImGui::CalcTextSize("Clear").x + ImGui::GetStyle().FramePadding.x * 2;
    const float filterWidth = width - clearWidth - ImGui::GetStyle().ItemSpacing.x;
    std::vector<std::string> categories;
    for(const auto& entry : entries)
        if(categories.empty() || categories.back() != entry.category) categories.push_back(entry.category);
    if(!tab.category.empty() && std::find(categories.begin(), categories.end(), tab.category) == categories.end())
        tab.category.clear();
    ImGui::SetNextItemWidth(filterWidth);
    if(ImGui::BeginCombo("##category", tab.category.empty() ? "All categories" : tab.category.c_str())) {
        if(ImGui::Selectable("All categories", tab.category.empty())) tab.category.clear();
        for(const auto& category : categories)
            if(ImGui::Selectable(category.c_str(), category == tab.category)) tab.category = category;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if(ImGui::Button("Clear")) {
        tab.search[0] = '\0';
        tab.category.clear();
        tab.searchName = true;
        tab.searchDescription = false;
        tab.searchParameters = false;
    }
    ImGui::TextUnformatted("Search in:");
    ImGui::SameLine();
    ImGui::Checkbox("Name", &tab.searchName);
    ImGui::SameLine();
    ImGui::Checkbox("Description", &tab.searchDescription);
    if(!macro) {
        ImGui::SameLine();
        ImGui::Checkbox("Parameter names", &tab.searchParameters);
    }
    std::vector<const Entry*> filtered;
    for(const auto& entry : entries) {
        if(!tab.category.empty() && entry.category != tab.category) continue;
        bool matched = !tab.search[0];
        if(!matched && tab.searchName) matched = matches(entry.name, tab.search);
        if(!matched && tab.searchDescription)
            matched = matches(macro ? descriptionForMacro(entry.path) : descriptionFor(registry, entry.name), tab.search);
        if(!matched && !macro && tab.searchParameters) {
            const auto* hint = ofxOceanodeNodeHelpEntry(entry.name);
            if(hint && hint->contains("parameters"))
                for(const auto& parameter : (*hint)["parameters"])
                    if(parameter.is_string() && matches(parameter.get<std::string>(), tab.search)) {
                        matched = true;
                        break;
                    }
        }
        if(matched) filtered.push_back(&entry);
    }
    ImGui::TextDisabled("%d / %d %s", static_cast<int>(filtered.size()), static_cast<int>(entries.size()), macro ? "macros" : "nodes");
    const float listWidth = std::min(300.0f, width * 0.36f);
    const Entry* selected = nullptr;
    for(const auto* entry : filtered)
        if((macro ? entry->path : entry->name) == tab.selected) { selected = entry; break; }
    if(ImGui::BeginChild("##categories", ImVec2(listWidth, 0), true)) {
        const bool expandAll = ImGui::Button("Expand all");
        ImGui::SameLine();
        const bool collapseAll = ImGui::Button("Collapse all");
        ImGui::Separator();
        // Keep browsing expansion state separate from the branches opened by search.
        const std::string treeFilter = std::string(tab.search) + "\n" + tab.category +
            (tab.searchName ? "1" : "0") + (tab.searchDescription ? "1" : "0") +
            (tab.searchParameters ? "1" : "0");
        const bool revealMatches = tab.search[0] && treeFilter != tab.categoryTreeFilter;
        tab.categoryTreeFilter = treeFilter;
        struct CategoryTree {
            std::map<std::string, CategoryTree> children;
            std::vector<const Entry*> items;
        } tree;
        for(const auto* entry : filtered) {
            auto* branch = &tree;
            for(const auto& part : ofSplitString(entry->category, "/", true, true))
                branch = &branch->children[part];
            branch->items.push_back(entry);
        }
        ImGui::PushID(tab.search[0] ? "searchTree" : "browseTree");
        std::function<void(const CategoryTree&)> closeBranches;
        closeBranches = [&](const CategoryTree& branch) {
            for(const auto& child : branch.children) {
                ImGui::GetStateStorage()->SetInt(ImGui::GetID(child.first.c_str()), 0);
                ImGui::PushID(child.first.c_str());
                closeBranches(child.second);
                ImGui::PopID();
            }
        };
        if(collapseAll) closeBranches(tree);
        std::function<void(const CategoryTree&)> drawCategory;
        drawCategory = [&](const CategoryTree& branch) {
            for(const auto& child : branch.children) {
                if(expandAll || (revealMatches && !collapseAll))
                    ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                const auto flags = ImGuiTreeNodeFlags_SpanAvailWidth;
                const bool open = ImGui::TreeNodeEx(child.first.c_str(), flags);
                if(open) {
                    drawCategory(child.second);
                    ImGui::TreePop();
                }
            }
            for(const auto* entry : branch.items) {
                const auto& key = macro ? entry->path : entry->name;
                ImGui::PushID(key.c_str());
                if(ImGui::Selectable(entry->name.c_str(), key == tab.selected)) {
                    tab.selected = key;
                    selected = entry;
                    error.clear();
                }
                ImGui::PopID();
            }
        };
        drawCategory(tree);
        ImGui::PopID();
        if(filtered.empty()) ImGui::TextWrapped("No matches. Change the search or clear the filters.");
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
            description = macro ? descriptionForMacro(selected->path) : descriptionFor(registry, selected->name);
            const char* text = description.empty() ? "No description available." : description.c_str();
            const auto space = ImGui::GetContentRegionAvail();
            const float textHeight = ImGui::CalcTextSize(text, nullptr, false, std::max(1.0f, space.x - 16)).y;
            const float descriptionHeight = std::max(1.0f, std::min(textHeight + 8, space.y * 0.28f));
            if(ImGui::BeginChild("##description", ImVec2(0, descriptionHeight)))
                ImGui::TextWrapped("%s", text);
            ImGui::EndChild();
            ImGui::Spacing();
            drawPreview(*selected, macro);
        } else {
            resetPreview();
            ImGui::TextWrapped("Select an item to read its description and add it to the canvas.");
        }
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

void ofxOceanodeNodePool::resetPreview() {
    previewNode = nullptr;
    previewOwner.reset();
    previewKey.clear();
    previewError.clear();
}

void ofxOceanodeNodePool::drawPreview(const Entry& entry, bool macro) {
    const auto key = (macro ? "macro:" + entry.path : "node:" + entry.name);
    if(previewKey != key) {
        resetPreview();
        previewKey = key;
        try {
            // Keep graph membership, IDs, history and metadata separate from the
            // patch. A preview is set up once, but never updated or model-drawn.
            auto registry = std::make_shared<ofxOceanodeNodeRegistry>(*root->getRegistry());
            registry->customGuiFactory = {};
            previewOwner = std::make_shared<ofxOceanodeContainer>(registry, root->getTypesRegistry(),
                std::make_shared<ofxOceanodeTransport>());
            previewOwner->setCanvasID("Node Pool Preview");
            auto model = registry->create(macro ? "Macro" : entry.name);
            if(!model) throw std::runtime_error("Node is no longer available.");
            previewNode = &previewOwner->createNode(std::move(model), macro ? entry.path : "");
            std::function<void(ofxOceanodeContainer&)> pause = [&](ofxOceanodeContainer& owner) {
                for(auto* node : owner.getAllModules()) {
                    node->setActive(false);
                    if(auto* nested = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
                        nested->setWindowVisible(false, false);
                        pause(*nested->getContainer());
                    }
                }
            };
            pause(*previewOwner);
        } catch(const std::exception& exception) {
            previewNode = nullptr;
            previewOwner.reset();
            previewError = std::string("Preview unavailable: ") + exception.what();
        }
    }
    if(!previewError.empty()) ImGui::TextWrapped("%s", previewError.c_str());
    if(previewNode) {
        // NoInputs keeps the actual widgets visually unchanged while preventing
        // edits, node deletion, external-window toggles and parameter popups.
        if(ImGui::BeginChild("##nodePreview", ImVec2(0, 0), false,
                ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground))
            rootCanvas->drawNodePreview(*previewNode);
        ImGui::EndChild();
    }
}

std::string ofxOceanodeNodePool::descriptionForMacro(const std::string& path) {
    const auto cached = macroDescriptions.find(path);
    if(cached != macroDescriptions.end()) return cached->second;
    std::string description;
    const auto file = path + "/description.json";
    if(ofFile::doesFileExist(file)) {
        try {
            const auto json = ofLoadJson(file);
            if(json.is_object() && json.contains("Description") && json["Description"].is_string())
                description = json["Description"].get<std::string>();
        } catch(const std::exception&) {}
    }
    macroDescriptions[path] = description;
    return description;
}
