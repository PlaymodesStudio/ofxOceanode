//
//  ofxOceanodeContainerUnused.cpp
//  ofxOceanode
//
//  Unused-node liveness analysis and removal.
//  Split out of ofxOceanodeContainer.cpp; bodies are unchanged.
//


#include "ofxOceanodeContainer.h"
#include "ofxOceanodeAutomationExtension.h"
#include "ofxOceanodeNodeRegistry.h"
#include "ofxOceanodeTypesRegistry.h"
#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeScope.h"
#include "Nodes/MacroSnapshotSystem.h"
#include "Nodes/MacroRouterValueDispatch.h"
#include "portal.h"
#include "portalSelector.h"
#include "imgui.h"
#include <cmath>
#include <cfloat>
#include <deque>
#include <filesystem>
#include <stdexcept>
#include "PresetSaveCleanup.h"

#ifdef OFXOCEANODE_USE_MIDI
#include "ofxOceanodeMidiClock.h"
#include "ofxOceanodeMidiBinding.h"
#include "ofxOceanodeMidiKeyboardBinding.h"
#include "ofxMidiIn.h"
#include "ofxMidiOut.h"
#endif


ofxOceanodeContainer::UnusedNodePlan ofxOceanodeContainer::findUnusedNodes(){
    // A node is live when it is a sink, or when something live consumes it
    // through a cable or a wireless link. Everything else is dead. See
    // docs/unused-node-cleanup.md for the rules; plans hold names, never pointers.
    UnusedNodePlan plan;
    plan.revision = nodesRevision;
    const auto nodes = getAllModules();
    std::unordered_map<ofxOceanodeNodeModel*, ofxOceanodeNode*> owners;
    for(auto* node : nodes) owners[&node->getNodeModel()] = node;
    const auto nameOf = [](ofxOceanodeNode* node){ return node->getParameters().getEscapedName(); };
    const auto abstractParameters = [](ofxOceanodeNode* node){
        vector<ofxOceanodeAbstractParameter*> result;
        for(auto& abstract : node->getParameters())
            if(auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get())) result.push_back(parameter);
        return result;
    };
    const auto hasInConnection = [&](ofxOceanodeNodeModel* model){
        for(auto& abstract : model->getParameterGroup()){
            auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get());
            if(parameter != nullptr && !parameter->getInConnections().empty()) return true;
        }
        return false;
    };
    const auto hasOutConnection = [&](ofxOceanodeNodeModel* model){
        for(auto& abstract : model->getParameterGroup()){
            auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get());
            if(parameter != nullptr && !parameter->getOutConnections().empty()) return true;
        }
        return false;
    };

    // Bindings may live in a different canvas (including a closed Custom GUI).
    // Resolve their actual targets, rather than guessing from node names. The same
    // walk finds portal selectors elsewhere, which keep their portal in use.
    std::unordered_set<ofxOceanodeAbstractParameter*> published;
    std::unordered_set<const abstractPortal*> outsideSelected;
    auto* root = this;
    while(root->timelineHost != nullptr) root = root->timelineHost;
    std::function<void(ofxOceanodeContainer*)> walk = [&](ofxOceanodeContainer* current){
        if(auto* extension = current->getCustomGuiExtension()) extension->collectBoundParameters(published);
        for(auto* node : current->getAllModules()){
            if(current != this){
                if(auto* selector = dynamic_cast<portalSelectorBase*>(&node->getNodeModel()))
                    if(selector->getSelectedPortal() != nullptr) outsideSelected.insert(selector->getSelectedPortal());
            }
            if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node->getNodeModel()))
                walk(macro->getContainer().get());
        }
    };
    walk(root);
    if(auto* extension = root->getAutomationExtension()) extension->collectBoundParameters(published);

    // Host bindings make a node a sink: its work is represented outside the graph.
    const auto hostSinkReasons = [&](ofxOceanodeContainer& owner, ofxOceanodeNode* node){
        vector<string> found;
        if(node->getIsPersistent()) found.push_back("sink: persistent");
        for(auto* parameter : abstractParameters(node)){
            const string name = parameter->getName();
            if(parameter->isScoped()) found.push_back("sink: scope " + name);
            if(parameter->isTimelined()) found.push_back("sink: timeline " + name);
            if(published.count(parameter)) found.push_back("sink: binding " + name);
#ifdef OFXOCEANODE_USE_MIDI
            const string key = parameter->getGroupHierarchyNames()[0] + "-|-" + parameter->getEscapedName();
            const auto midi = owner.midiBindings.find(key);
            const auto persistentMidi = owner.persistentMidiBindings.find(key);
            if((midi != owner.midiBindings.end() && !midi->second.empty()) ||
               (persistentMidi != owner.persistentMidiBindings.end() && !persistentMidi->second.empty()))
                found.push_back("sink: MIDI " + name);
#endif
        }
        return found;
    };

    // The first node inside a Macro that does useful work whatever its pins
    // connect to; empty when there is none. Router pins never count.
    std::function<string(ofxOceanodeContainer&, int)> hardSinkInside = [&](ofxOceanodeContainer& inner, int depth) -> string {
        if(depth > 32) return {};
        const auto members = inner.getAllModules();
        std::unordered_set<const ofxOceanodeNodeModel*> insideModels;
        for(auto* member : members) insideModels.insert(&member->getNodeModel());
        for(auto* member : members){
            auto& model = member->getNodeModel();
            if(dynamic_cast<abstractRouter*>(&model) != nullptr) continue;
            if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&model)){
                if(!hardSinkInside(*macro->getContainer(), depth + 1).empty()) return nameOf(member);
            }
            if(model.isSink() || !hostSinkReasons(inner, member).empty()) return nameOf(member);
            if(auto* portal = dynamic_cast<abstractPortal*>(&model)){
                if(!hasInConnection(portal)) continue;
                for(auto* peer : ofxOceanodeShared::getPortalsNamed(portal->getName()))
                    if(peer != portal && portal->isMatching(peer) && insideModels.count(peer) == 0) return nameOf(member);
            }
        }
        return {};
    };

    std::unordered_map<ofxOceanodeNode*, vector<ofxOceanodeNode*>> feeders, neighbors;
    std::unordered_map<ofxOceanodeNode*, std::unordered_set<ofxOceanodeAbstractConnection*>> nodeConnections;
    std::unordered_set<ofxOceanodeNode*> feedsOutside, fedFromOutside;
    for(auto* node : nodes){
        for(auto* parameter : abstractParameters(node)){
            for(auto* link : parameter->getOutConnections()){
                nodeConnections[node].insert(link);
                auto other = owners.find(link->getSinkParameter().getNodeModel());
                if(other == owners.end()){ feedsOutside.insert(node); continue; }
                feeders[other->second].push_back(node);
                neighbors[node].push_back(other->second);
                neighbors[other->second].push_back(node);
            }
            for(auto* link : parameter->getInConnections()){
                nodeConnections[node].insert(link);
                if(owners.find(link->getSourceParameter().getNodeModel()) == owners.end()) fedFromOutside.insert(node);
            }
        }
    }

    std::unordered_map<ofxOceanodeNode*, vector<string>> reasons;
    std::unordered_set<ofxOceanodeNode*> live;
    std::deque<ofxOceanodeNode*> queue;
    const auto markLive = [&](ofxOceanodeNode* node, string reason){
        reasons[node].push_back(std::move(reason));
        if(live.insert(node).second) queue.push_back(node);
    };
    // A portal another canvas reads, or a selector there uses, is consumed.
    const auto consumedOutside = [&](abstractPortal* peer){
        return outsideSelected.count(peer) != 0 || hasOutConnection(peer);
    };

    for(auto* node : nodes){
        auto& model = node->getNodeModel();
        if(model.isSink()){
            if(dynamic_cast<abstractRouter*>(&model) != nullptr) markLive(node, "sink: router");
            else if(!model.hasOutputPin()) markLive(node, "sink: no outputs");
            else markLive(node, "sink: declared");
        }
        for(auto& reason : hostSinkReasons(*this, node)) markLive(node, reason);
        if(feedsOutside.count(node)) markLive(node, "feeds: outside canvas");
        if(fedFromOutside.count(node)) markLive(node, "connection: outside canvas");
        if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&model)){
            const string inside = hardSinkInside(*macro->getContainer(), 0);
            if(!inside.empty()) markLive(node, "macro: contains " + inside);
        }
        if(auto* portal = dynamic_cast<abstractPortal*>(&model)){
            if(!hasInConnection(portal)) continue;
            for(auto* peer : ofxOceanodeShared::getPortalsNamed(portal->getName())){
                if(peer == portal || !portal->isMatching(peer) || owners.count(peer) != 0) continue;
                if(consumedOutside(peer)){
                    markLive(node, "portal: " + portal->getName() + " used by another canvas");
                    break;
                }
            }
        }
    }

    while(!queue.empty()){
        auto* node = queue.front();
        queue.pop_front();
        const string name = nameOf(node);
        for(auto* feeder : feeders[node]) markLive(feeder, "feeds: " + name);
        auto& model = node->getNodeModel();
        if(auto* portal = dynamic_cast<abstractPortal*>(&model)){
            // A live portal keeps the writers that send to it.
            for(auto* peer : ofxOceanodeShared::getPortalsNamed(portal->getName())){
                if(peer == portal || !portal->isMatching(peer)) continue;
                auto found = owners.find(peer);
                if(found != owners.end() && hasInConnection(peer))
                    markLive(found->second, "portal: " + portal->getName() + " used by " + name);
            }
        }
        if(auto* selector = dynamic_cast<portalSelectorBase*>(&model)){
            if(auto* selected = selector->getSelectedPortal()){
                auto found = owners.find(selected);
                if(found != owners.end()) markLive(found->second, "portal: " + selected->getName() + " used by " + name);
            }
        }
    }

    for(auto* node : nodes){
        if(live.count(node) == 0) continue;
        auto& found = reasons[node];
        std::sort(found.begin(), found.end());
        found.erase(std::unique(found.begin(), found.end()), found.end());
        plan.kept.push_back({nameOf(node), found});
    }

    // Dead nodes form groups wherever cables join them.
    std::unordered_set<ofxOceanodeNode*> visited;
    for(auto* node : nodes){
        if(live.count(node) != 0 || !visited.insert(node).second) continue;
        vector<ofxOceanodeNode*> component{node};
        vector<string> names;
        std::unordered_set<ofxOceanodeAbstractConnection*> links;
        for(size_t i = 0; i < component.size(); ++i){
            auto* member = component[i];
            names.push_back(nameOf(member));
            links.insert(nodeConnections[member].begin(), nodeConnections[member].end());
            for(auto* neighbor : neighbors[member])
                if(live.count(neighbor) == 0 && visited.insert(neighbor).second) component.push_back(neighbor);
        }
        std::sort(names.begin(), names.end());
        plan.groups.push_back({std::move(names), links.size(), true});
    }
    std::sort(plan.groups.begin(), plan.groups.end(),
              [](const auto& a, const auto& b){ return a.nodes < b.nodes; });
    std::sort(plan.kept.begin(), plan.kept.end(),
              [](const auto& a, const auto& b){ return a.node < b.node; });
    return plan;
}

bool ofxOceanodeContainer::removeUnusedNodes(const UnusedNodePlan& plan){
    const auto current = findUnusedNodes();
    if(current.revision != plan.revision || current.groups.size() != plan.groups.size()) return false;
    for(size_t i = 0; i < plan.groups.size(); ++i)
        if(current.groups[i].nodes != plan.groups[i].nodes) return false;
    // One canvas edit, so a single undo restores every removed group. Join an
    // edit already in progress rather than closing it early.
    const bool standalone = !pendingCanvasEdit;
    beginCanvasEdit();
    // Use native deletion so cables, listeners and model resources are released.
    for(const auto& group : plan.groups){
        if(!group.selected) continue;
        for(const auto& name : group.nodes){
            auto found = parameterGroupNodesMap.find(name);
            if(found == parameterGroupNodesMap.end()) continue;
            auto* node = found->second;
            for(auto& comment : comments){
                comment.nodes.erase(std::remove(comment.nodes.begin(), comment.nodes.end(), node), comment.nodes.end());
            }
            node->deleteSelf();
        }
    }
    if(standalone) endCanvasEdit();
    return true;
}
