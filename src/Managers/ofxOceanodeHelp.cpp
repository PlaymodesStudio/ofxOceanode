#include "ofxOceanodeHelp.h"
#include "ofxOceanodeWindowVisibility.h"
#include "ofxOceanodeHelpPlan.h"
#include "ofxOceanodeHelpPrompt.h"
#include "ofxOceanodeNodeHelpCatalog.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeNodeGui.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeMenu.h"
#include "ofxOceanodeScope.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace {
std::shared_ptr<ofxOceanodeContainer> ownerForCanvas(
    const std::shared_ptr<ofxOceanodeContainer>& owner, const std::string& id) {
    if(owner->getCanvasID() == id) return owner;
    for(auto* node : owner->getAllModules())
        if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel()))
            if(auto found = ownerForCanvas(macro->getContainer(), id)) return found;
    return nullptr;
}
bool matchesKeywords(const std::string& text, const std::string& query) {
    const auto haystack = ofToLower(text);
    std::istringstream words(ofToLower(query));
    std::string word;
    while(words >> word) if(haystack.find(word) == std::string::npos) return false;
    return true;
}
std::vector<std::string> sortedTypes(ofxOceanodeNodeRegistry& registry) {
    std::vector<std::string> names;
    for(const auto& entry : registry.getRegisteredModels()) names.push_back(entry.first);
    std::sort(names.begin(), names.end());
    return names;
}
std::string descriptionFor(ofxOceanodeNodeRegistry& registry, const std::string& name) {
    const auto& descriptions = registry.getModelDescriptions();
    const auto item = descriptions.find(name);
    return item != descriptions.end() && !item->second.empty()
        ? item->second : ofxOceanodeNodeHelpDescription(name);
}
std::string readableType(const std::string& type) {
    if(type == typeid(float).name()) return "float";
    if(type == typeid(int).name()) return "int";
    if(type == typeid(bool).name()) return "bool";
    if(type == typeid(void).name()) return "trigger";
    if(type == typeid(std::string).name()) return "string";
    if(type == typeid(std::vector<float>).name()) return "float vector";
    if(type == typeid(std::vector<int>).name()) return "int vector";
    return type;
}
}

ofxOceanodeHelp::ofxOceanodeHelp(std::shared_ptr<ofxOceanodeContainer> container) : root(std::move(container)) {
    ofxOceanodeAIAssistant::Spec spec;
    spec.title = "Oceanode assistant";
    spec.subtitle = [](){ return "Ask about nodes, or propose a simple patch for the active canvas."; };
    spec.generateModeLabel = "Ask";
    spec.fixModeLabel = "Build patch";
    spec.generateButton = "Ask";
    spec.fixButton = "Propose patch";
    spec.generateHint = "Ask how Oceanode or an installed node works.";
    spec.fixHint = "Describe a simple node structure and its connections.";
    spec.pendingHint = "The answer or proposed patch will appear in Help.";
    spec.readyMessage = "Response ready in Help";
    spec.systemPrompt = [](){ return std::string(ofxOceanodeHelpPrompt()); };
    spec.buildPrompt = [this](const std::string& request, bool patch){ return buildPrompt(request, patch); };
    spec.apply = [this](const std::string& response){ return acceptResponse(response); };
    assistant.setup(spec);
    const auto settings = ofToDataPath("config/help-assistant.json");
    if(ofFile::doesFileExist(settings)) {
        try { assistant.load(ofLoadJson(settings)); } catch(const std::exception&) {}
    }
}

void ofxOceanodeHelp::update() { assistant.update(); }

void ofxOceanodeHelp::drawMenu() {
    ofxOceanodeMenu::section("Utilities");
    if(ImGui::MenuItem("Search nodes...")) { visible = true; requestedTab = 0; }
    if(ImGui::MenuItem("AI assistant...")) { visible = openAssistant = true; requestedTab = 1; }
}

std::string ofxOceanodeHelp::buildPrompt(const std::string& request, bool patch) {
    const auto previousProposal = proposal;
    proposal = ofJson();
    patchRequested = patch;
    targetCanvas = ofxOceanodeShared::getActiveCanvasUniqueID();
    auto target = ownerForCanvas(root, targetCanvas);
    if(!target) { target = root; targetCanvas = root->getCanvasID(); }
    targetOwner = target;
    targetRevision = target->getNodesRevision();
    ofJson catalog = ofJson::array();
    auto registry = root->getRegistry();
    for(const auto& name : sortedTypes(*registry)) {
        ofJson entry = {{"type", name}, {"description", descriptionFor(*registry, name)}};
        const auto category = registry->getRegisteredModelsCategoryAssociation().find(name);
        if(category != registry->getRegisteredModelsCategoryAssociation().end()) entry["category"] = category->second;
        const auto* hint = ofxOceanodeNodeHelpEntry(name);
        if(hint) entry["parameter_hints"] = (*hint)["parameters"];
        catalog.push_back(std::move(entry));
    }
    ofJson live = ofJson::array();
    // Schemas contain names/types only: parameter values, files and scripts are not sent.
    std::set<std::string> seen;
    for(auto* node : target->getAllModules()) {
        auto& model = node->getNodeModel();
        if(!seen.insert(model.nodeName()).second) continue;
        ofJson pins = ofJson::array();
        for(auto& parameter : node->getParameters()) {
            auto* pin = dynamic_cast<ofxOceanodeAbstractParameter*>(parameter.get());
            if(!pin) continue;
            pins.push_back({{"name", pin->getName()}, {"type", readableType(pin->valueType())},
                {"input", !(pin->getFlags() & ofxOceanodeParameterFlags_DisableInConnection)},
                {"output", !(pin->getFlags() & ofxOceanodeParameterFlags_DisableOutConnection)}});
        }
        live.push_back({{"type", model.nodeName()}, {"pins", pins}});
    }
    return ofJson({{"mode", patch ? "patch" : "answer"}, {"request", request},
        {"previous_answer", answer}, {"previous_proposal", previousProposal}, {"last_error", error},
        {"registered_nodes", catalog}, {"live_schemas", live}}).dump();
}

std::string ofxOceanodeHelp::acceptResponse(const std::string& response) {
    try {
        auto parsed = ofJson::parse(response);
        ofxOceanodeHelpPlan::validate(parsed, [this](const std::string& type){
            return root->getRegistry()->getRegisteredModels().count(type) != 0;
        });
        if(!patchRequested && (!parsed["nodes"].empty() || !parsed["connections"].empty()))
            throw std::runtime_error("Ask mode cannot propose a patch. Select Build patch.");
        answer = parsed["answer"].get<std::string>();
        proposal = std::move(parsed);
        error.clear();
        visible = true;
        requestedTab = 1;
        ofJson settings;
        assistant.save(settings);
        ofDirectory::createDirectory(ofToDataPath("config"), true, true);
        ofSavePrettyJson(ofToDataPath("config/help-assistant.json"), settings);
        return "";
    } catch(const std::exception& e) { return std::string("Invalid assistant response: ") + e.what(); }
}

void ofxOceanodeHelp::applyProposal() {
    error.clear();
    auto target = targetOwner.lock();
    if(!target || root->getContainerForCanvasID(targetCanvas) != target.get() ||
        target->getNodesRevision() != targetRevision) {
        error = "The target canvas changed. Generate a new proposal for the current canvas.";
        return;
    }
    std::vector<ofxOceanodeNode*> created;
    std::vector<ofxOceanodeAbstractConnection*> connections;
    std::map<std::string, ofxOceanodeNode*> ids;
    try {
        ofxOceanodeHelpPlan::validate(proposal, [this](const std::string& type){
            return root->getRegistry()->getRegisteredModels().count(type) != 0;
        });
        float x = 40, y = 40;
        for(auto* node : target->getAllModules())
            y = std::max(y, node->getNodeGui().getPosition().y + 220.0f);
        for(const auto& entry : proposal["nodes"]) {
            auto* node = target->createNodeFromName(entry["type"].get<std::string>());
            if(!node) throw std::runtime_error("A proposed node is no longer registered.");
            created.push_back(node);
            ids[entry["id"].get<std::string>()] = node;
            const size_t i = created.size() - 1;
            node->getNodeGui().setPosition({x + float(i % 4) * 260, y + float(i / 4) * 260});
        }
        // Resolve every pin before adding a single connection.
        for(const auto& edge : proposal["connections"]) {
            auto& source = ids.at(edge["from"].get<std::string>())->getParameters();
            auto& sink = ids.at(edge["to"].get<std::string>())->getParameters();
            const auto output = edge["output"].get<std::string>(), input = edge["input"].get<std::string>();
            if(!source.contains(output) || !sink.contains(input))
                throw std::runtime_error("Missing pin: " + output + " -> " + input + ". Ask the assistant to correct the pin names.");
            auto* out = dynamic_cast<ofxOceanodeAbstractParameter*>(&source.get(output));
            auto* in = dynamic_cast<ofxOceanodeAbstractParameter*>(&sink.get(input));
            if(!out || !in || (out->getFlags() & ofxOceanodeParameterFlags_DisableOutConnection) ||
                (in->getFlags() & ofxOceanodeParameterFlags_DisableInConnection))
                throw std::runtime_error("A proposed pin does not allow this connection: " + output + " -> " + input);
        }
        for(const auto& edge : proposal["connections"]) {
            std::string reason;
            auto* connection = target->createConnectionFromInfo(
                ids.at(edge["from"].get<std::string>())->getParameters().getEscapedName(), edge["output"].get<std::string>(),
                ids.at(edge["to"].get<std::string>())->getParameters().getEscapedName(), edge["input"].get<std::string>(), false, &reason);
            if(!connection) throw std::runtime_error(reason);
            connections.push_back(connection);
        }
        for(auto* connection : connections) {
            const auto& current = target->getAllConnections();
            if(std::none_of(current.begin(), current.end(), [connection](const auto& item){ return item.get() == connection; }))
                throw std::runtime_error("Activating the patch changed its pins. Use a simpler structure.");
            connection->setActive(true);
        }
        for(auto* node : target->getAllModules()) node->getNodeGui().setSelected(false);
        for(auto* node : created) node->getNodeGui().setSelected(true);
        // Focus and center the result, also when the request targeted a macro.
        std::function<void(ofxOceanodeContainer*, ofxOceanodeCanvas*)> focus =
            [&](ofxOceanodeContainer* owner, ofxOceanodeCanvas* canvas){
                if(owner == target.get() && canvas && !created.empty()) {
                    canvas->requestFocus();
                    canvas->requestCenterOnNode(created.front(), 2);
                }
                for(auto* node : owner->getAllModules())
                    if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel())) {
                        if(macro->getContainer().get() == target.get()) macro->activateWindow();
                        focus(macro->getContainer().get(), macro->getCanvas());
                    }
            };
        focus(root.get(), ofxOceanodeShared::getRootCanvas());
        answer += "\n\nCreated " + ofToString(created.size()) + " nodes in " + targetCanvas + ".";
        proposal = ofJson();
    } catch(const std::exception& e) {
        // Remove connections first, avoiding callbacks into already destroyed nodes.
        for(auto it = connections.rbegin(); it != connections.rend(); ++it) {
            const auto& current = target->getAllConnections();
            if(std::none_of(current.begin(), current.end(), [it](const auto& item){ return item.get() == *it; })) continue;
            (*it)->prepareForDestruction();
            (*it)->deleteSelf();
        }
        for(auto it = created.rbegin(); it != created.rend(); ++it) (*it)->deleteSelf();
        targetRevision = target->getNodesRevision(); // Retrying a corrected response remains possible.
        error = std::string("Could not create patch: ") + e.what();
    }
}

void ofxOceanodeHelp::draw() {
    if(!visible) return;
    ImGui::SetNextWindowSize(ImVec2(720, 560), ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Help##oceanode_reference", &visible)) {
        if(ImGui::BeginTabBar("##help_tabs")) {
            if(ImGui::BeginTabItem("Node reference", nullptr, requestedTab == 0 ? ImGuiTabItemFlags_SetSelected : 0)) {
                ImGui::InputTextWithHint("##help_keywords", "Search names, descriptions, categories, parameters...", keywords, sizeof(keywords));
                ImGui::TextDisabled("All keywords must match. Search is case insensitive.");
                ImGui::Separator();
                auto registry = root->getRegistry();
                int matches = 0;
                if(ImGui::BeginChild("##help_results")) {
                    for(const auto& name : sortedTypes(*registry)) {
                        const auto category = registry->getRegisteredModelsCategoryAssociation().at(name);
                        const auto description = descriptionFor(*registry, name);
                        const auto* hint = ofxOceanodeNodeHelpEntry(name);
                        const auto parameters = hint ? (*hint)["parameters"].dump() : "";
                        if(!matchesKeywords(name + " " + category + " " + description + " " + parameters, keywords)) continue;
                        ++matches;
                        ImGui::TextUnformatted(name.c_str());
                        ImGui::SameLine();
                        ImGui::TextDisabled("(%s)", category.c_str());
                        ImGui::TextWrapped("%s", description.empty() ? "No description supplied by this addon." : description.c_str());
                        ImGui::Separator();
                    }
                    if(!matches) ImGui::TextDisabled("No matching nodes. Try fewer keywords.");
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("AI assistant", nullptr, requestedTab == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
                if(ImGui::Button("Ask or build a patch...")) openAssistant = true;
                ImGui::TextWrapped("Uses the same Requesty credentials as Formula and DynGen. Sends your question, installed node descriptions and pin schemas.");
                ImGui::Separator();
                if(!answer.empty()) ImGui::TextWrapped("%s", answer.c_str());
                if(!assistant.getStatus().empty()) ImGui::TextWrapped("%s", assistant.getStatus().c_str());
                if(!proposal.is_null() && !proposal["nodes"].empty()) {
                    ImGui::Separator();
                    ImGui::Text("Target canvas: %s", targetCanvas.c_str());
                    for(const auto& node : proposal["nodes"]) ImGui::BulletText("%s: %s", node["id"].get<std::string>().c_str(), node["type"].get<std::string>().c_str());
                    for(const auto& edge : proposal["connections"]) ImGui::BulletText("%s.%s -> %s.%s",
                        edge["from"].get<std::string>().c_str(), edge["output"].get<std::string>().c_str(),
                        edge["to"].get<std::string>().c_str(), edge["input"].get<std::string>().c_str());
                    if(assistant.isPending()) ImGui::BeginDisabled();
                    if(ImGui::Button("Create in canvas")) applyProposal();
                    ImGui::SameLine();
                    if(ImGui::Button("Discard proposal")) proposal = ofJson();
                    if(assistant.isPending()) ImGui::EndDisabled();
                }
                if(!error.empty()) ImGui::TextWrapped("%s", error.c_str());
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if(openAssistant) { assistant.openPopup(); openAssistant = false; }
        assistant.drawPopup();
    }
    ImGui::End();
    // SetSelected is only needed for the frame in which a menu item opens Help.
    requestedTab = -1;
}

void ofxOceanodeHelp::drawWindowMenu(ofxOceanodeControls& controls) {
    if(ImGui::MenuItem("Canvas"))
        if(auto* canvas = ofxOceanodeShared::getRootCanvas()) canvas->requestFocus();
    bool scopesVisible = ofxOceanodeScope::getInstance()->isVisible();
    if(ImGui::Checkbox("Scopes", &scopesVisible))
        ofxOceanodeScope::getInstance()->setVisible(scopesVisible);
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Show or hide Scopes without removing scoped parameters.");
    ImGui::TextDisabled("Visibility is saved with the preset.");
    ofxOceanodeMenu::section("Controllers");
    auto& visibility = controls.getControllersVisibility();
    std::vector<std::string> names;
    for(const auto& controller : controls.getControllers()) {
        const auto& name = controller->getControllerName();
        if(controller->isMenuController()) {
            visibility.erase(name);
            continue;
        }
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    for(const auto& name : names) {
        if(!visibility.count(name)) visibility[name] = true;
        ImGui::Checkbox(name.c_str(), &visibility[name]);
        if(name == "Inspector" && ImGui::IsItemHovered())
            ImGui::SetTooltip("Unchecked: always hidden. When enabled, automatic show/hide still follows node selection.");
    }
    using Entry = ofxOceanodeCustomGuiExtension::WindowEntry;
    std::vector<Entry> nodes, macros, panels;
    std::function<void(ofxOceanodeContainer*, const std::string&)> collect =
        [&](ofxOceanodeContainer* owner, const std::string& path){
            for(auto* node : owner->getAllModules()) {
                auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel());
                const auto label = path + node->getParameters().getName();
                if(macro) {
                    macros.push_back({label, [macro,label](){
                        bool show = macro->isSharedWindowVisible();
                        ImGui::BeginDisabled(macro->isWindowShownByOtherInstance());
                        if(ImGui::Checkbox(label.c_str(), &show)) macro->setWindowVisible(show);
                        ImGui::EndDisabled();
                    }});
                    collect(macro->getContainer().get(), label + " / ");
                }
                auto addToggles = [&](ofParameterGroup& group){
                    for(auto& parameter : group) {
                        ofParameter<bool>* toggle = nullptr;
                        if(auto* pin = dynamic_cast<ofxOceanodeParameter<bool>*>(parameter.get())) toggle = &pin->getParameter();
                        else toggle = dynamic_cast<ofParameter<bool>*>(parameter.get());
                        if(!toggle || !oceanodeWindowVisibility::isWindowToggle(parameter->getName())) continue;
                        const auto title = label + " / " + parameter->getName();
                        nodes.push_back({title, [toggle,title](){
                            bool show = toggle->get();
                            if(ImGui::Checkbox(title.c_str(), &show)) toggle->set(show);
                        }});
                    }
                };
                addToggles(node->getParameters());
                addToggles(node->getInspectorParameters());
            }
            if(auto* extension = owner->getCustomGuiExtension()) {
                auto entries = extension->windowEntries(path);
                panels.insert(panels.end(), entries.begin(), entries.end());
            }
        };
    collect(root.get(), "");
    auto section = [](const char* name, std::vector<Entry>& entries){
        ofxOceanodeMenu::section(name);
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b){ return a.label < b.label; });
        if(entries.empty()) ImGui::TextDisabled("None in this patch");
        for(const auto& entry : entries) { ImGui::PushID(entry.label.c_str()); entry.draw(); ImGui::PopID(); }
    };
    section("Nodes", nodes);
    section("Macros", macros);
    if(root->getCustomGuiExtension()) section("Custom GUIs", panels);
    if(root->getCustomGuiExtension() && ImGui::MenuItem("New Custom GUI")) {
        auto* owner = root->getContainerForCanvasID(ofxOceanodeShared::getActiveCanvasUniqueID());
        if(!owner) owner = root.get();
        if(auto* extension = owner->getCustomGuiExtension()) extension->requestCreate();
    }
}
