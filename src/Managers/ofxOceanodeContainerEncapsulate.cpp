//
//  ofxOceanodeContainerEncapsulate.cpp
//  ofxOceanode
//
//  Encapsulating selected nodes into a macro and rebuilding their routers.
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


///////////////////////ENCAPSULATE
///
///


vector<ofxOceanodeContainer::UpstreamSubgraph>
ofxOceanodeContainer::findExclusiveUpstreamSubgraphs(
    const vector<ofxOceanodeAbstractParameter*>& requestedSinks) {
    vector<UpstreamSubgraph> plans;
    unordered_set<ofxOceanodeNode*> sinkOwners;
    auto isPortalNode = [](ofxOceanodeNode* node) {
        return node != nullptr && dynamic_cast<abstractPortal*>(&node->getNodeModel()) != nullptr;
    };
    for(auto* sink : requestedSinks) {
        if(sink == nullptr || !sink->hasInConnection()) continue;
        if(auto* owner = getNodeFromParameter(*sink)) sinkOwners.insert(owner);
    }

    vector<unordered_set<ofxOceanodeNode*>> closures;
    for(auto* sink : requestedSinks) {
        if(sink == nullptr || !sink->hasInConnection()) continue;
        auto* sourceNode = getNodeFromParameter(sink->getInConnection()->getSourceParameter());
        if(sourceNode == nullptr || isPortalNode(sourceNode) || sinkOwners.count(sourceNode) != 0) continue;

        unordered_set<ofxOceanodeNode*> closure;
        vector<ofxOceanodeNode*> pending{sourceNode};
        while(!pending.empty()) {
            auto* node = pending.back();
            pending.pop_back();
            if(node == nullptr || isPortalNode(node) || sinkOwners.count(node) != 0 || !closure.insert(node).second) continue;
            for(auto& rawParameter : node->getParameters()) {
                auto parameter = dynamic_pointer_cast<ofxOceanodeAbstractParameter>(rawParameter);
                if(parameter == nullptr || !parameter->hasInConnection()) continue;
                auto* upstream = getNodeFromParameter(parameter->getInConnection()->getSourceParameter());
                if(upstream != nullptr && !isPortalNode(upstream) && sinkOwners.count(upstream) == 0)
                    pending.push_back(upstream);
            }
        }
        if(!closure.empty()) {
            plans.push_back({sink, {}});
            closures.push_back(std::move(closure));
        }
    }

    // A node reached from more than one mixer inlet is a shared dependency.
    unordered_map<ofxOceanodeNode*, int> claims;
    for(const auto& closure : closures)
        for(auto* node : closure) claims[node]++;

    const auto allNodes = getAllModules();
    for(size_t i = 0; i < plans.size(); ++i) {
        auto kept = closures[i];
        for(auto it = kept.begin(); it != kept.end();) {
            if(claims[*it] > 1) it = kept.erase(it);
            else ++it;
        }

        // Removing a shared/downstream node can expose another outgoing edge.
        // Iterate to a fixed point so every retained node is exclusive to this
        // branch. The one permitted external edge is the selected mixer inlet.
        bool changed = true;
        while(changed) {
            changed = false;
            vector<ofxOceanodeNode*> remove;
            for(auto* node : kept) {
                bool escapes = false;
                for(auto& rawParameter : node->getParameters()) {
                    auto parameter = dynamic_pointer_cast<ofxOceanodeAbstractParameter>(rawParameter);
                    if(parameter == nullptr) continue;
                    for(auto* connection : parameter->getOutConnections()) {
                        if(&connection->getSinkParameter() == plans[i].sink) continue;
                        auto* downstream = getNodeFromParameter(connection->getSinkParameter());
                        if(downstream == nullptr || kept.count(downstream) == 0) {
                            escapes = true;
                            break;
                        }
                    }
                    if(escapes) break;
                }
                if(escapes) remove.push_back(node);
            }
            for(auto* node : remove) changed |= kept.erase(node) != 0;
        }

        // Preserve deterministic node order for copy/paste and for matching
        // recreated nodes inside the macro.
        for(auto* node : allNodes)
            if(kept.count(node) != 0) plans[i].nodes.push_back(node);
    }
    return plans;
}

void ofxOceanodeContainer::encapsulateSelectedNodes(const string& macroName, bool moveTimelineIntoMacro) {
	encapsulateNodes(getSelectedModules(), macroName, moveTimelineIntoMacro);
}

bool ofxOceanodeContainer::hasAutomatedParameters(const vector<ofxOceanodeNode*>& nodes) const {
	auto* automation = const_cast<ofxOceanodeContainer*>(this)->getAutomationExtension();
	if(automation == nullptr) return false;
	std::unordered_set<ofxOceanodeAbstractParameter*> bound;
	automation->collectBoundParameters(bound);
	if(bound.empty()) return false;
	std::function<bool(ofxOceanodeNode&)> automated = [&](ofxOceanodeNode& node) {
		for(auto& abstract : node.getParameters())
			if(auto* parameter = dynamic_cast<ofxOceanodeAbstractParameter*>(abstract.get()); parameter && bound.count(parameter)) return true;
		if(auto* macro = dynamic_cast<ofxOceanodeNodeMacro*>(&node.getNodeModel()); macro && macro->getContainer())
			for(auto* inner : macro->getContainer()->getAllModules()) if(automated(*inner)) return true;
		return false;
	};
	return std::any_of(nodes.begin(), nodes.end(), [&](ofxOceanodeNode* node) { return node != nullptr && automated(*node); });
}

ofxOceanodeNode* ofxOceanodeContainer::encapsulateNodes(
    const vector<ofxOceanodeNode*>& nodes, const string& macroName, bool moveTimelineIntoMacro, bool autoLayout) {
	// 1. Validate and select exactly this precomputed branch. This keeps batch
	// encapsulation independent from the order in which its plans are applied.
	auto selectedNodes = nodes;
	if(selectedNodes.empty()) {
		ofLogWarning("Encapsulation") << "No nodes selected for encapsulation";
		return nullptr;
	}
    const auto ownedNodes = getAllModules();
    std::set<ofxOceanodeNode*> uniqueNodes;
    for(auto* node : selectedNodes){
        if(!node || std::find(ownedNodes.begin(), ownedNodes.end(), node) == ownedNodes.end() ||
           node->getIsPersistent() || !registry->getRegisteredModels().count(node->getNodeModel().nodeName()) ||
           !uniqueNodes.insert(node).second){
            ofLogWarning("Encapsulation") << "Selection must contain distinct dynamic nodes from this canvas";
            return nullptr;
        }
    }
    if(!registry->getRegisteredModels().count("Macro")) return nullptr;
    MacroCanvasEditScope edit(*this);
    try {
	for(auto* node : getAllModules()) node->getNodeGui().setSelected(false);
	for(auto* node : selectedNodes) if(node != nullptr) node->getNodeGui().setSelected(true);
	
	ofLogNotice("Encapsulation") << "Encapsulating " << selectedNodes.size() << " nodes";
	
	// 2. Calculate center position for macro placement
	glm::vec2 centerPos(0, 0);
	for(auto node : selectedNodes) {
		centerPos += node->getNodeGui().getPosition();
	}
	if(!selectedNodes.empty()) {
		centerPos /= selectedNodes.size();
	}
	
    // Preserve keys and source ID order before any nodes can be deleted.
	struct NodeInfo {
		string originalName;
		string originalEscapedName;
		string nodeType;
        int identifier;
		glm::vec2 position;
	};
	
	vector<NodeInfo> originalNodeInfo;
	for(auto node : selectedNodes) {
		NodeInfo info;
		info.originalName = node->getParameters().getName();
		info.originalEscapedName = node->getParameters().getEscapedName();
		info.nodeType = node->getNodeModel().nodeName();
        info.identifier = node->getNodeModel().getNumIdentifier();
		info.position = node->getNodeGui().getPosition();
		originalNodeInfo.push_back(info);
	}
	
    std::sort(originalNodeInfo.begin(), originalNodeInfo.end(), [](const NodeInfo& a, const NodeInfo& b){
        return a.nodeType == b.nodeType ? a.identifier < b.identifier : a.nodeType < b.nodeType;
    });

	// 4. Analyze external connections BEFORE cutting (nodes are still valid)
	auto externalConnections = analyzeExternalConnections(selectedNodes);
	ofLogNotice("Encapsulation") << "Found " << externalConnections.size() << " external connections";
    for(const auto& boundary : externalConnections){
        const string routerType = "Router " + typesRegistry->getTypeNameFromTypeDescription(boundary.routerType);
        if(!registry->getRegisteredModels().count(routerType))
            throw std::runtime_error("Required router factory is unavailable: " + routerType);
    }
	
    // Stage a complete copy before removing the originals. Factory or paste
    // failures can then roll back without reconstructing the user's live nodes.
    const auto selectedComments = getSelectedCommentIndices();
	if(!copySelectedModulesWithConnections()) {
		ofLogError("Encapsulation") << "Failed to copy selected nodes";
		return nullptr;
	}
	
	// 6. Create macro node
	auto macroNode = createNodeFromName("Macro");
	if(!macroNode) {
		ofLogError("Encapsulation") << "Failed to create macro node";
		return nullptr;
	}
	
	macroNode->getNodeGui().setPosition(centerPos);
	macroNode->getNodeGui().setSelected(true);

	// 7. Get macro internals
	auto macroModel = dynamic_cast<ofxOceanodeNodeMacro*>(&macroNode->getNodeModel());
	if(!macroModel) {
		ofLogError("Encapsulation") << "Failed to cast to macro model";
		macroNode->deleteSelf();
		return nullptr;
	}
	if(!macroName.empty() && macroModel->getInspectorParameterGroup().contains("Local Name"))
		macroModel->getInspectorParameter<string>("Local Name").set(macroName);
	
	auto macroContainer = macroModel->getContainer();
	if(!macroContainer) {
		ofLogError("Encapsulation") << "Failed to get macro container";
		macroNode->deleteSelf();
		return nullptr;
	}
	
    // Populate the destination while the originals are still intact.
	if(!macroContainer->pasteModulesAndConnectionsInPosition(glm::vec2(0, 0), false) ||
       macroContainer->getAllModules().size() != selectedNodes.size()) {
        throw std::runtime_error("Failed to paste nodes inside macro");
	}
	
    for(auto* node : selectedNodes) node->deleteSelf();
    for(auto index = selectedComments.rbegin(); index != selectedComments.rend(); ++index)
        comments.erase(comments.begin() + *index);

    // Clipboard loading allocates nodes in source type/ID order. Use that same
    // order to map repeated node types and nested macros deterministically.
	auto macroNodes = macroContainer->getAllModules();
	
    // Nested macros must expose their pins before boundary wires are rebuilt.
	ensureMacroParametersExposed(macroNodes);
	
    // Map both human-readable group names and escaped parameter paths.
	map<string, string> nodeNameMapping;
	map<string, string> escapedNodeNameMapping;
		
	if(!macroNodes.empty()) {
		
		ofLogNotice("Encapsulation") << "Creating node name mapping from stored info...";
		
		// Collect the pasted nodes by type
		map<string, vector<ofxOceanodeNode*>> pastedNodesByType;
		for(auto node : macroNodes) {
			string nodeType = node->getNodeModel().nodeName();
			pastedNodesByType[nodeType].push_back(node);
		}
		
        for(auto& group : pastedNodesByType){
            std::sort(group.second.begin(), group.second.end(), [](ofxOceanodeNode* a, ofxOceanodeNode* b){
                return a->getNodeModel().getNumIdentifier() < b->getNodeModel().getNumIdentifier();
            });
        }
        map<string, size_t> nextOfType;
        for(const auto& original : originalNodeInfo){
            auto& candidates = pastedNodesByType.at(original.nodeType);
            const auto index = nextOfType[original.nodeType]++;
            if(index >= candidates.size()) throw std::runtime_error("Incomplete macro paste");
            auto* pasted = candidates[index];
            nodeNameMapping[original.originalName] = pasted->getParameters().getName();
            escapedNodeNameMapping[original.originalEscapedName] = pasted->getParameters().getEscapedName();
        }
        // Timeline bindings follow what moved: old path -> new path for every
        // parameter of a moved node and, for a moved macro, of all inside it.
        std::map<std::string, std::string> timelinePaths;
        for(const auto& original : originalNodeInfo){
            auto* moved = macroContainer->parameterGroupNodesMap.at(escapedNodeNameMapping.at(original.originalEscapedName));
            collectMovedTimelinePaths(*moved, timelineCanvasPrefix(canvasID) + original.originalEscapedName + "/",
                                      timelineMacroCanvas(canvasID, original.identifier), timelinePaths);
            pendingCanvasEdit->nodeTransfers.push_back({
                {"before", {original.originalEscapedName}},
                {"after", {macroNode->getParameters().getEscapedName(), moved->getParameters().getEscapedName()}},
                {"beforeId", original.identifier}, {"afterId", moved->getNodeModel().getNumIdentifier()}});
#ifdef OFXOCEANODE_USE_MIDI
            const auto originalIdentity = pendingCanvasEdit->before["nodes"][original.originalEscapedName]["identity"].get<std::uint64_t>();
            const auto saved = pendingCanvasEdit->snapshots.find(originalIdentity);
            if(saved != pendingCanvasEdit->snapshots.end()){
                for(const auto& midi : saved->second->midi){
                    auto binding = macroContainer->createMidiBindingFromInfo(moved->getParameters().getEscapedName(),
                        midi["parameter"].get<string>(), false, midi["id"].get<int>(), midi["binding"].value("Keyboard", false));
                    if(binding){ auto json = midi["binding"]; binding->loadPreset(json); }
                }
            }
#endif
        }
        if(customGuiExtension && macroContainer->getCustomGuiExtension())
            customGuiExtension->migrateTo(*macroContainer->getCustomGuiExtension(), escapedNodeNameMapping);
        if(auto* automation = getAutomationExtension(); automation != nullptr && !timelinePaths.empty()) {
            auto record = automation->encapsulateParameters(*this, *macroContainer, timelinePaths, moveTimelineIntoMacro);
            pendingCanvasEdit->parameterMoves.push_back({{"paths", timelinePaths},
                {"macro", macroNode->getParameters().getEscapedName()}, {"record", std::move(record)}});
        }

	}

	if(!externalConnections.empty()) {
		// Update external connections using the mapping
		for(auto& extConn : externalConnections) {
			if(extConn.isIncoming) {
				// Update input router internal connections
				for(auto& internalConn : extConn.internalConnections) {
					string originalName = internalConn.nodeName;
					
					if(nodeNameMapping.find(originalName) != nodeNameMapping.end()) {
						string newName = nodeNameMapping[originalName];
						if(newName != originalName) {
							ofLogNotice("Encapsulation") << "Updated input: " << originalName << " → " << newName;
							internalConn.nodeName = newName;
						}
					}
				}
			} else {
				// Update output router internal connection
				string originalName = extConn.internalConnection.nodeName;
				
				if(nodeNameMapping.find(originalName) != nodeNameMapping.end()) {
					string newName = nodeNameMapping[originalName];
					if(newName != originalName) {
						ofLogNotice("Encapsulation") << "Updated output: " << originalName << " → " << newName;
						extConn.internalConnection.nodeName = newName;
					}
				}
			}
		}
		
		// Compute minPosition (same logic as cutSelectedModulesWithConnections) for router positioning
		glm::vec2 minPos(FLT_MAX, FLT_MAX);
		for(const auto& info : originalNodeInfo) {
			minPos.x = std::min(minPos.x, info.position.x);
			minPos.y = std::min(minPos.y, info.position.y);
		}
		if(minPos.x == FLT_MAX) minPos = glm::vec2(0, 0);

		// 10. Create routers and reconnect
		createRoutersAndReconnect(macroNode, externalConnections, minPos);
	}

	// Only on request, reuse the canvas auto-layout once the pasted nodes and
	// generated boundary routers are all present. The request remains pending
	// while the macro is closed and centers the resulting composition when its
	// canvas is drawn. Otherwise the pasted nodes and routers keep the relative
	// positions they had on this canvas (translated so the selection's top-left
	// corner sits at the origin), and the macro canvas centers its view on
	// first draw.
	if(autoLayout)
		if(auto* macroCanvas = macroModel->getCanvas())
			macroCanvas->requestAutoLayoutCanvas(true);
	
	ofLogNotice("Encapsulation") << "Encapsulation completed successfully";
    edit.success = true;
	return macroNode;
    } catch(const std::exception& error){
        ofLogError("Encapsulation") << error.what();
        return nullptr;
    }
}

vector<ofxOceanodeContainer::ExternalConnection> ofxOceanodeContainer::analyzeExternalConnections(vector<ofxOceanodeNode*> selectedNodes) {
	// For input routers: group by external parameter (one external → multiple internals)
	// Keep the encapsulated destination type at the macro boundary. One external
	// output can legally feed inputs of different types through Oceanode's
	// connection conversions, so destination type is part of the grouping key.
	map<pair<ofxOceanodeAbstractParameter*, string>, ExternalConnection> inputRouters;
	auto inputBoundaryType = [](const string& sourceType, const string& sinkType) {
		auto isNumericFamily = [](const string& type) {
			return type == typeid(float).name() || type == typeid(int).name() ||
			       type == typeid(vector<float>).name() || type == typeid(vector<int>).name();
		};
		if(sourceType != sinkType && isNumericFamily(sourceType) && isNumericFamily(sinkType))
			return string(typeid(vector<float>).name());
		return sinkType;
	};
	
	// For output routers: group by internal parameter (one internal → multiple externals)
	map<pair<string, string>, ExternalConnection> outputRouters; // Key: {nodeName, paramName}
	
	// Track router names to avoid conflicts within this encapsulation
	map<string, int> nameCounters;
	
	ofLogNotice("Encapsulation") << "=== DEBUGGING CONNECTION ANALYSIS ===";
	ofLogNotice("Encapsulation") << "Total connections in container: " << connections.size();
	ofLogNotice("Encapsulation") << "Selected nodes for encapsulation: " << selectedNodes.size();
	
	// First, let's see what nodes exist in the container
	ofLogNotice("Encapsulation") << "ALL NODES in container:";
	for(auto& nodeTypeMap : dynamicNodes) {
		for(auto& node : nodeTypeMap.second) {
			ofLogNotice("Encapsulation") << "  - " << node.second->getParameters().getName()
				<< " (type: " << node.second->getNodeModel().nodeName() << ")";
		}
	}
	
	// Check if any of our selected nodes are macros or routers
	ofLogNotice("Encapsulation") << "SELECTED NODES details:";
	for(auto node : selectedNodes) {
		ofLogNotice("Encapsulation") << "  - " << node->getParameters().getName()
			<< " (type: " << node->getNodeModel().nodeName() << ")";
		
		// Check if this is a router or macro
		if(node->getNodeModel().nodeName().find("Router") != string::npos) {
			ofLogWarning("Encapsulation") << "    WARNING: Selected node is a router!";
		}
		if(node->getNodeModel().nodeName().find("Macro") != string::npos) {
			ofLogWarning("Encapsulation") << "    WARNING: Selected node is a macro!";
		}
	}
	
	ofLogNotice("Encapsulation") << "ANALYZING ALL CONNECTIONS:";
	
	int connectionIndex = 0;
	for(auto& connection : connections) {
		if(!connection) continue;
		
		auto sourceNode = getNodeFromParameter(connection->getSourceParameter());
		auto sinkNode = getNodeFromParameter(connection->getSinkParameter());
		
		if(!sourceNode || !sinkNode) {
			ofLogWarning("Encapsulation") << "Connection " << connectionIndex << ": Could not find nodes";
			connectionIndex++;
			continue;
		}
		
		bool sourceSelected = isNodeInList(sourceNode, selectedNodes);
		bool sinkSelected = isNodeInList(sinkNode, selectedNodes);
		
		string sourceNodeName = sourceNode->getParameters().getName();
		string sinkNodeName = sinkNode->getParameters().getName();
		string sourceParamName = connection->getSourceParameter().getName();
		string sinkParamName = connection->getSinkParameter().getName();
		
		ofLogNotice("Encapsulation") << "Connection " << connectionIndex << ": "
			<< sourceNodeName << "." << sourceParamName << " -> "
			<< sinkNodeName << "." << sinkParamName;
		ofLogNotice("Encapsulation") << "  Source selected: " << (sourceSelected ? "YES" : "NO")
			<< ", Sink selected: " << (sinkSelected ? "YES" : "NO");
		
		// Check if this connection involves existing routers
		bool sourceIsRouter = sourceNodeName.find("Router") != string::npos;
		bool sinkIsRouter = sinkNodeName.find("Router") != string::npos;
		bool sourceIsMacro = sourceNodeName.find("Macro") != string::npos;
		bool sinkIsMacro = sinkNodeName.find("Macro") != string::npos;
		
		if(sourceIsRouter || sinkIsRouter || sourceIsMacro || sinkIsMacro) {
			ofLogWarning("Encapsulation") << "  -> INVOLVES EXISTING ROUTER/MACRO: "
				<< "src_router=" << sourceIsRouter << ", sink_router=" << sinkIsRouter
				<< ", src_macro=" << sourceIsMacro << ", sink_macro=" << sinkIsMacro;
		}
		
		// Skip internal connections (both nodes selected)
		if(sourceSelected && sinkSelected) {
			ofLogNotice("Encapsulation") << "  -> INTERNAL CONNECTION (preserved)";
			connectionIndex++;
			continue;
		}
		
		// Skip unrelated connections (neither node selected)
		if(!sourceSelected && !sinkSelected) {
			ofLogNotice("Encapsulation") << "  -> UNRELATED CONNECTION (ignored)";
			connectionIndex++;
			continue;
		}
		
		// This is an external connection we need to handle
		if(sourceSelected && !sinkSelected) {
			ofLogNotice("Encapsulation") << "  -> OUTGOING CONNECTION (needs output router)";
			
			// Check if sink is a router from previous encapsulation
			if(sinkIsRouter) {
				ofLogError("Encapsulation") << "    ERROR: Trying to connect to existing router! This might cause issues.";
				ofLogError("Encapsulation") << "    Sink: " << sinkNodeName << "." << sinkParamName;
			}
			
			string fullInternalNodeName = sourceNodeName;
			string originalInternalParamName = sourceParamName;
			auto internalKey = make_pair(fullInternalNodeName, originalInternalParamName);
			
			auto it = outputRouters.find(internalKey);
			if(it != outputRouters.end()) {
				it->second.externalConnections.push_back(&connection->getSinkParameter());
			} else {
				ExternalConnection extConn;
				extConn.routerType = connection->getSourceParameter().valueType();
				extConn.isIncoming = false;
				extConn.routerName = generateRouterName(originalInternalParamName, nameCounters);
				extConn.internalConnection.nodeName = fullInternalNodeName;
				extConn.internalConnection.paramName = originalInternalParamName;
				extConn.externalConnections.push_back(&connection->getSinkParameter());
				
				outputRouters[internalKey] = extConn;
				ofLogNotice("Encapsulation") << "    Created output router: " << extConn.routerName;
			}
		}
		else if(!sourceSelected && sinkSelected) {
			ofLogNotice("Encapsulation") << "  -> INCOMING CONNECTION (needs input router)";
			
			// Check if source is a router from previous encapsulation
			if(sourceIsRouter) {
				ofLogError("Encapsulation") << "    ERROR: Source is existing router! This might cause issues.";
				ofLogError("Encapsulation") << "    Source: " << sourceNodeName << "." << sourceParamName;
			}
			
			auto externalParam = &connection->getSourceParameter();
			const string routerType = inputBoundaryType(connection->getSourceParameter().valueType(),
			                                                connection->getSinkParameter().valueType());
			string fullInternalNodeName = sinkNodeName;
			string originalInternalParamName = sinkParamName;
			auto inputKey = make_pair(externalParam, routerType);
			
			auto it = inputRouters.find(inputKey);
			if(it != inputRouters.end()) {
				it->second.internalConnections.push_back({fullInternalNodeName, originalInternalParamName});
			} else {
				// An inlet fed by a portal takes the portal's name: that is the
				// name the value is known by on this canvas. Any other source
				// names the inlet after the parameter it feeds inside the macro.
				string inletBaseName = originalInternalParamName;
				if(auto* sourcePortal = dynamic_cast<abstractPortal*>(&sourceNode->getNodeModel())) {
					const string portalName = ofTrim(sourcePortal->getName());
					if(!portalName.empty()) inletBaseName = portalName;
				}
				ExternalConnection extConn;
				extConn.routerType = routerType;
				extConn.externalParam = externalParam;
				extConn.isIncoming = true;
				extConn.routerName = generateRouterName(inletBaseName, nameCounters);
				extConn.internalConnections.push_back({fullInternalNodeName, originalInternalParamName});
				
				inputRouters[inputKey] = extConn;
				ofLogNotice("Encapsulation") << "    Created input router: " << extConn.routerName;
			}
		}
		
		connectionIndex++;
	}
	
	// Convert maps to vector
	vector<ExternalConnection> externals;
	for(auto& pair : inputRouters) {
		externals.push_back(pair.second);
	}
	for(auto& pair : outputRouters) {
		externals.push_back(pair.second);
	}
	
	ofLogNotice("Encapsulation") << "=== ANALYSIS COMPLETE ===";
	ofLogNotice("Encapsulation") << "Will create " << externals.size() << " routers";
	
	return externals;
}

void ofxOceanodeContainer::createRoutersAndReconnect(ofxOceanodeNode* macroNode, vector<ExternalConnection>& connections, glm::vec2 minPosition) {
	auto macroModel = dynamic_cast<ofxOceanodeNodeMacro*>(&macroNode->getNodeModel());
	if(!macroModel) {
		ofLogError("Encapsulation") << "Invalid macro model for router creation";
		return;
	}
	
	auto macroContainer = macroModel->getContainer();
	if(!macroContainer) {
		ofLogError("Encapsulation") << "Invalid macro container for router creation";
		return;
	}
	
	ofLogNotice("Encapsulation") << "=== CREATING ROUTERS INSIDE MACRO ===";
	ofLogNotice("Encapsulation") << "Creating " << connections.size() << " routers inside macro...";
	
	// Get all nodes inside the macro
	auto macroNodes = macroContainer->getAllModules();
	ofLogNotice("Encapsulation") << "Found " << macroNodes.size() << " nodes inside macro:";
	for(auto node : macroNodes) {
		ofLogNotice("Encapsulation") << "  - " << node->getParameters().getName()
			<< " (type: " << node->getNodeModel().nodeName() << ")";
	}
	
	int routerIndex = 0;
	vector<ofxOceanodeNode*> createdRouters;
	
	// Create routers and connect them appropriately
	for(auto& extConn : connections) {
		try {
            string routerTypeName = "Router " + macroContainer->getTypesRegistry()->getTypeNameFromTypeDescription(extConn.routerType);
			
			ofLogNotice("Encapsulation") << "Creating router " << (routerIndex + 1) << "/" << connections.size()
				<< ": " << routerTypeName << " named '" << extConn.routerName << "'";
			
			auto routerNode = macroContainer->createNodeFromName(routerTypeName);
			if(!routerNode) {
				ofLogError("Encapsulation") << "Failed to create router: " << routerTypeName;
				continue;
			}
			
			createdRouters.push_back(routerNode);
			
			// Set router name
			auto routerModel = dynamic_cast<abstractRouter*>(&routerNode->getNodeModel());
			if(routerModel) {
				routerModel->getNameParam() = extConn.routerName;
				ofLogNotice("Encapsulation") << "Set router name to: " << extConn.routerName;
			}
			
						glm::vec2 routerPos(0, 0);
						bool posFound = false;

						if(extConn.isIncoming) {
							// Input router: position at the external source node's location, translated to macro canvas space
							if(extConn.externalParam) {
								auto externalNode = getNodeFromParameter(*extConn.externalParam);
								if(externalNode) {
									routerPos = externalNode->getNodeGui().getPosition() - minPosition;
									posFound = true;
								}
							}
							if(!posFound) {
								// Fallback: average of target internal nodes, offset left
								int count = 0;
								for(const auto& internalConn : extConn.internalConnections) {
									for(auto node : macroNodes) {
										if(node->getParameters().getName() == internalConn.nodeName) {
											routerPos += node->getNodeGui().getPosition();
											count++;
											break;
										}
									}
								}
								if(count > 0) { routerPos /= count; routerPos.x -= 200; }
								else routerPos = glm::vec2(-200, routerIndex * 80);
							}
						} else {
							// Output router: position at the external destination node's location, translated to macro canvas space
							if(!extConn.externalConnections.empty() && extConn.externalConnections[0]) {
								auto externalNode = getNodeFromParameter(*extConn.externalConnections[0]);
								if(externalNode) {
									routerPos = externalNode->getNodeGui().getPosition() - minPosition;
									posFound = true;
								}
							}
							if(!posFound) {
								routerPos = glm::vec2(400, routerIndex * 80);
							}
						}

						routerNode->getNodeGui().setPosition(routerPos);

						ofLogNotice("Encapsulation") << "Positioned " << (extConn.isIncoming ? "input" : "output")
							<< " router '" << extConn.routerName << "' at (" << routerPos.x << ", " << routerPos.y << ")";

			// Get router's Value/Val parameter (void routers use "Val", typed routers use "Value")
			auto& routerParams = routerNode->getParameters();
			string valParamName = "";
			if(routerParams.contains("Value"))    valParamName = "Value";
			else if(routerParams.contains("Val")) valParamName = "Val";

			if(valParamName.empty()) {
				ofLogError("Encapsulation") << "Router does not have Value/Val parameter!";
				ofLogError("Encapsulation") << "Available parameters:";
				for(auto& param : routerParams) {
					ofLogError("Encapsulation") << "  - " << param->getName();
				}
				continue;
			}

			auto routerParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&routerParams.get(valParamName));
			if(!routerParam) {
				ofLogError("Encapsulation") << "Could not cast " << valParamName << " parameter to ofxOceanodeAbstractParameter";
				continue;
			}
			
			if(extConn.isIncoming) {
				// Input router: connect to multiple internal parameters
				ofLogNotice("Encapsulation") << "Connecting input router to " << extConn.internalConnections.size() << " internal parameters:";
				for(const auto& internalConn : extConn.internalConnections) {
					ofxOceanodeAbstractParameter* internalParam = findInternalParameter(macroNodes, internalConn);
					if(internalParam) {
						// Check if target parameter already has a connection
						if(internalParam->hasInConnection()) {
							ofLogError("Encapsulation") << "ERROR: Target parameter " << internalConn.nodeName
								<< "." << internalConn.paramName << " already has an input connection!";
							ofLogError("Encapsulation") << "This should not happen - investigating connection analysis...";
							continue;
						}
						
						auto conn = macroContainer->createConnection(*routerParam, *internalParam);
						if(conn) {
							ofLogNotice("Encapsulation") << "  ✓ Connected input router '" << extConn.routerName
								<< "' → " << internalConn.nodeName << "." << internalConn.paramName;
						} else {
							ofLogError("Encapsulation") << "  ✗ Failed to connect input router '" << extConn.routerName
								<< "' → " << internalConn.nodeName << "." << internalConn.paramName;
						}
					} else {
						ofLogError("Encapsulation") << "  ✗ Could not find internal parameter: "
							<< internalConn.nodeName << "." << internalConn.paramName;
					}
				}
			} else {
				// Output router: connect from single internal parameter
				ofLogNotice("Encapsulation") << "Connecting output router from internal parameter:";
				ofxOceanodeAbstractParameter* internalParam = findInternalParameter(macroNodes, extConn.internalConnection);
				if(internalParam) {
					auto conn = macroContainer->createConnection(*internalParam, *routerParam);
					if(conn) {
						ofLogNotice("Encapsulation") << "  ✓ Connected " << extConn.internalConnection.nodeName
							<< "." << extConn.internalConnection.paramName << " → output router '" << extConn.routerName << "'";
					} else {
						ofLogError("Encapsulation") << "  ✗ Failed to connect " << extConn.internalConnection.nodeName
							<< "." << extConn.internalConnection.paramName << " → output router '" << extConn.routerName << "'";
					}
				} else {
					ofLogError("Encapsulation") << "  ✗ Could not find internal parameter: "
						<< extConn.internalConnection.nodeName << "." << extConn.internalConnection.paramName;
				}
			}
			
			routerIndex++;
			
		} catch(const std::exception& e) {
			ofLogError("Encapsulation") << "Exception creating/connecting router: " << e.what();
		}
	}
	
	// Update everything to process the new routers
	ofEventArgs args;
	for(auto router : createdRouters) {
		router->update(args);
	}
	macroNode->update(args);
	
	// Give time for macro to process routers and expose them as parameters
	ofSleepMillis(100);
	macroNode->update(args);
	
	// Now reconnect external connections to macro parameters
	ofLogNotice("Encapsulation") << "=== RECONNECTING EXTERNAL CONNECTIONS ===";
	
	auto& macroParams = macroNode->getParameters();
	ofLogNotice("Encapsulation") << "Available macro parameters:";
	for(auto& param : macroParams) {
		ofLogNotice("Encapsulation") << "  - " << param->getName() << " (" << param->valueType() << ")";
	}
	
	int reconnectedCount = 0;
	for(auto& extConn : connections) {
		try {
			if(macroParams.contains(extConn.routerName)) {
				auto macroParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&macroParams.get(extConn.routerName));
				if(!macroParam) {
					ofLogError("Encapsulation") << "Could not cast macro parameter: " << extConn.routerName;
					continue;
				}
				
				if(extConn.isIncoming) {
					// Incoming: external → macro input (single connection)
					if(extConn.externalParam) {
						auto conn = createConnection(*extConn.externalParam, *macroParam);
						if(conn) {
							ofLogNotice("Encapsulation") << "  ✓ Reconnected incoming: external → " << extConn.routerName;
							reconnectedCount++;
						} else {
							ofLogError("Encapsulation") << "  ✗ Failed to reconnect incoming to " << extConn.routerName;
						}
					}
				} else {
					// Outgoing: macro output → multiple externals
					for(auto externalParam : extConn.externalConnections) {
						if(externalParam) {
							auto conn = createConnection(*macroParam, *externalParam);
							if(conn) {
								ofLogNotice("Encapsulation") << "  ✓ Reconnected outgoing: " << extConn.routerName << " → external";
								reconnectedCount++;
							} else {
								ofLogError("Encapsulation") << "  ✗ Failed to reconnect outgoing from " << extConn.routerName;
							}
						}
					}
				}
			} else {
				ofLogError("Encapsulation") << "Macro parameter not found: " << extConn.routerName;
			}
		} catch(const std::exception& e) {
			ofLogError("Encapsulation") << "Exception reconnecting: " << e.what();
		}
	}
	
	ofLogNotice("Encapsulation") << "=== ENCAPSULATION COMPLETE ===";
    size_t expected = 0;
    for(const auto& boundary : connections) expected += boundary.isIncoming ? 1 : boundary.externalConnections.size();
    if(createdRouters.size() != connections.size() || reconnectedCount != expected)
        throw std::runtime_error("Could not reconstruct every macro boundary connection");
	ofLogNotice("Encapsulation") << "Successfully reconnected: " << reconnectedCount << " connections";
}

bool ofxOceanodeContainer::isNodeInList(ofxOceanodeNode* node, vector<ofxOceanodeNode*>& nodeList) {
	return std::find(nodeList.begin(), nodeList.end(), node) != nodeList.end();
}

string ofxOceanodeContainer::generateRouterName(const string& paramName, map<string, int>& nameCounters) {
	// Clean parameter name
	string cleanParamName = paramName;
	ofStringReplace(cleanParamName, ".", "_");
	ofStringReplace(cleanParamName, " ", "_");
	
	// Remove consecutive underscores and trim
	while(cleanParamName.find("__") != string::npos) {
		ofStringReplace(cleanParamName, "__", "_");
	}
	if(!cleanParamName.empty() && cleanParamName[0] == '_') {
		cleanParamName = cleanParamName.substr(1);
	}
	if(!cleanParamName.empty() && cleanParamName.back() == '_') {
		cleanParamName = cleanParamName.substr(0, cleanParamName.length() - 1);
	}
	if(cleanParamName.empty()) {
		cleanParamName = "Param";
	}
	
	// Check if this name already exists in this encapsulation
	if(nameCounters.find(cleanParamName) == nameCounters.end()) {
		nameCounters[cleanParamName] = 1;
		return cleanParamName;
	} else {
		nameCounters[cleanParamName]++;
		return cleanParamName + "_" + ofToString(nameCounters[cleanParamName]);
	}
}

// Fix the getParameterTypeName method to properly handle nodePort:

ofxOceanodeAbstractParameter* ofxOceanodeContainer::findInternalParameter(
	const vector<ofxOceanodeNode*>& macroNodes,
	const InternalConnection& internalConn) {
	
	ofLogVerbose("Encapsulation") << "Looking for internal parameter: " << internalConn.nodeName << "." << internalConn.paramName;
	
	for(auto node : macroNodes) {
		if(node->getParameters().getName() == internalConn.nodeName) {
			auto& nodeParams = node->getParameters();
			
			ofLogVerbose("Encapsulation") << "Found matching node: " << internalConn.nodeName;
			
			// MACRO-AWARE FIX: Check if this is a macro node
			bool isMacroNode = (node->getNodeModel().nodeName() == "Macro");
			
			if(isMacroNode) {
				ofLogNotice("Encapsulation") << "Node is a macro, looking for exposed parameter: " << internalConn.paramName;
				
				// For macro nodes, the exposed router parameters appear directly in the parameter group
				// after the macro has processed its internal routers
				
				// Force an update to ensure all router parameters are exposed
				ofEventArgs args;
				node->update(args);
				
				// Small delay to allow parameter exposure to complete
				ofSleepMillis(50);
				node->update(args);
				
				// Now check for the parameter
				if(nodeParams.contains(internalConn.paramName)) {
					auto param = dynamic_cast<ofxOceanodeAbstractParameter*>(&nodeParams.get(internalConn.paramName));
					if(param) {
						ofLogNotice("Encapsulation") << "✓ Found macro exposed parameter: " << internalConn.paramName;
						return param;
					} else {
						ofLogError("Encapsulation") << "✗ Found parameter but failed to cast: " << internalConn.paramName;
					}
				} else {
					ofLogError("Encapsulation") << "✗ Macro parameter not found: " << internalConn.paramName;
					ofLogError("Encapsulation") << "Available macro parameters:";
					for(auto& param : nodeParams) {
						ofLogError("Encapsulation") << "  - '" << param->getName() << "'";
					}
				}
			} else {
				// Regular node logic (unchanged)
				ofLogVerbose("Encapsulation") << "Regular node, available parameters:";
				for(auto& param : nodeParams) {
					ofLogVerbose("Encapsulation") << "  - '" << param->getName() << "'";
				}
				
				if(nodeParams.contains(internalConn.paramName)) {
					auto param = dynamic_cast<ofxOceanodeAbstractParameter*>(&nodeParams.get(internalConn.paramName));
					if(param) {
						ofLogVerbose("Encapsulation") << "✓ Found and cast parameter: " << internalConn.paramName;
						return param;
					} else {
						ofLogError("Encapsulation") << "✗ Found parameter but failed to cast: " << internalConn.paramName;
					}
				} else {
					ofLogError("Encapsulation") << "✗ Parameter not found in node: " << internalConn.paramName;
				}
			}
			break;
		}
	}
	
	ofLogError("Encapsulation") << "Could not find internal parameter: "
		<< internalConn.nodeName << "." << internalConn.paramName;
	return nullptr;
}

string ofxOceanodeContainer::extractNodeType(const string& nodeName) {
	// Extract base type from node name (e.g., "Vector Item Operations 2" → "Vector Item Operations")
	string nodeType = nodeName;
	auto lastSpacePos = nodeType.find_last_of(' ');
	if(lastSpacePos != string::npos) {
		string possibleId = nodeType.substr(lastSpacePos + 1);
		bool isNumeric = !possibleId.empty() && std::all_of(possibleId.begin(), possibleId.end(), ::isdigit);
		if(isNumeric) {
			nodeType = nodeType.substr(0, lastSpacePos);
		}
	}
	return nodeType;
}

void ofxOceanodeContainer::ensureMacroParametersExposed(const vector<ofxOceanodeNode*>& macroNodes) {
	ofLogNotice("Encapsulation") << "Ensuring macro parameters are exposed...";
	
	// Force multiple updates to ensure all macro router parameters are exposed
	for(int i = 0; i < 3; i++) {
		ofEventArgs args;
		for(auto node : macroNodes) {
			if(node->getNodeModel().nodeName() == "Macro") {
				node->update(args);
			}
		}
		ofSleepMillis(25); // Small delay between updates
	}
	
	// Log final parameter state
	for(auto node : macroNodes) {
		if(node->getNodeModel().nodeName() == "Macro") {
			ofLogNotice("Encapsulation") << "Macro " << node->getParameters().getName() << " exposed parameters:";
			for(auto& param : node->getParameters()) {
				ofLogNotice("Encapsulation") << "  - " << param->getName() << " (" << param->valueType() << ")";
			}
		}
	}
}
