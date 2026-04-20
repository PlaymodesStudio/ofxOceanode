//
//  ofxOceanodeNodeMacro.cpp
//  example-basic
//
//  Created by Eduard Frigola Bagué on 20/06/2019.
//  Snapshot management added by Santi Vilanova on January 2025
//
//  Main implementation file.  GUI rendering lives in ofxOceanodeNodeMacroGui.cpp.
//

#include "ofxOceanodeNodeMacro.h"
#include "ofxOceanodeShared.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

ofxOceanodeNodeMacro::ofxOceanodeNodeMacro() : ofxOceanodeNodeModel("Macro"){
	color = ofColor::black;
	description = "Encapsulation of a graph";
	showWindow = false;
	lastActiveState = true;
	isLoadingPreset = false;

	matrixRows.set("Snapshot Matrix Rows", 2, 1, 8);
	matrixCols.set("Snapshot Matrix Cols", 8, 1, 8);
	showSnapshotNames.set("Show Names", true);
	buttonSize.set("Button Size", 28.0f, 15.0f, 60.0f);
	showSnapshotMatrix = false;
	retriggerSnapshotOnActive.set("Retrigger Snapshot on Active", false);
	
	// Add minimized view update callback (body in ofxOceanodeNodeMacroGui.cpp)
	minimizedViewCallback = [this](ImVec2 size) { renderMinimizedView(size); };
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::update(ofEventArgs &a){
	if(presetManager.getNextPresetPath() != ""){
		// Clear snapshots when changing presets to avoid data persistence
		snapshotSystem.clearAll();
		
		presetManager.setLocal(false);
		isLoadingPreset = true;

		// Load router sort order from the macro folder BEFORE loadPreset() so
		// allNodesCreated() can apply it (same logic as macroLoad global branch).
		{
			string sortOrderFile = presetManager.getNextPresetPath() + "/router_sort_order.json";
			ofJson sortJson;
			if(ofFile::doesFileExist(sortOrderFile)) {
				sortJson = ofLoadJson(sortOrderFile);
			}
			loadRouterSortFromJson(sortJson);
		}

		//if(clearContainerOnLoad) container->clearContainer();
		presetManager.setInnerPresetLoadingPath(presetManager.getNextPresetPath());
		container->loadPreset(presetManager.getNextPresetPath());
		presetManager.setInnerPresetLoadingPath("");

		// Explicitly load snapshots from the global macro path
		snapshotSystem.loadFromPath(presetManager.getNextPresetPath());
		if(!snapshotSystem.isEmpty()) {
			showSnapshotMatrix = true;
		}

		isLoadingPreset = false;
		presetManager.clearNextPresetPath();
	}
	if(active){
		container->update();

		morphEngine.update(routerManager.getRouterNodes());
	}
}

void ofxOceanodeNodeMacro::draw(ofEventArgs &a){
	if(showWindow){
		canvas.draw(&showWindow, color, presetManager.isLocal() ? localName.get() : presetManager.getCurrentMacro());
	}
	if(active){
		container->draw();
	}
}

void ofxOceanodeNodeMacro::setContainer(ofxOceanodeContainer* container){
	registry = container->getRegistry();
	typesRegistry = container->getTypesRegistry();
	canvasParentID = container->getCanvasID();
}

// ─── Setup ───────────────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::setup(string additionalInfo){
	// node parameter : "Active"
	//--------------------------
	addParameter(active.set("Active", true));
	activeListener = active.newListener([this](bool &b){
		if(lastActiveState != b){
			if(b){
				container->activate();
				if(resetPhaseOnActive) container->resetPhase();
				if(retriggerSnapshotOnActive && snapshotSystem.getCurrentSlot() >= 0) {
					// Retrigger the current snapshot to fire its parameters again
					loadRouterSnapshot(snapshotSystem.getCurrentSlot());
				}
			}else{
				container->deactivate();
			}
		}
		lastActiveState = b;
	});
	
	// node parameter : "Macro Preset"
	//--------------------------------
	auto presetControlRef = addParameter(presetControl.set("Preset Control Gui", [this](){
		renderPresetControlGui();
	}));
		

	
	// Inspector parameters : "Local name"
	//-------------------------------------
	addInspectorSeparator("Macro Info",ofColor(250,250,250,128));
	addInspectorParameter(localName.set("Local Name", "Local"));
	// Inspector parameters : "Color"
	//-------------------------------------
	addInspectorParameter(colorParam.set("Color", color));
	colorListener = colorParam.newListener([this](ofColor &c){
		color = c;
	});
	// Inspector parameters : "Reset Ph. on Active"
	//---------------------------------------------
	addInspectorParameter(resetPhaseOnActive.set("Reset Ph on Active", false));
	//addInspectorParameter(clearContainerOnLoad.set("Clear Container on Load Preset", false));

	// node parameter : "Snapshot"
	//--------------------------------
	activeSnapshotSlotParam = addParameter(activeSnapshotSlot.set("Snapshot", -1, -1, 64));
	activeSnapshotSlotParam->setFlags(activeSnapshotSlotParam->getFlags() | ofxOceanodeParameterFlags_NoGuiWidget);

	// Inspector parameters : "Show Snapshots, Show Names, Retrigger On Active"
	//-------------------------------------------------------------------------
	addInspectorSeparator("Snapshots",ofColor(0,200,0,128));
	addInspectorParameter(showSnapshotMatrix.set("Show Snapshot?", false));
	addInspectorParameter(showSnapshotNames.set("Show Names", true));
	addInspectorParameter(retriggerSnapshotOnActive.set("Retrigger Snapshot on Active", false));

	// Listener "Show Snapshot?"
	//--------------------------
	showSnapshotMatrixListener = showSnapshotMatrix.newListener([this](bool &show){
		if(activeSnapshotSlotParam) {
			if(show) {
				activeSnapshotSlotParam->setFlags(activeSnapshotSlotParam->getFlags() & ~ofxOceanodeParameterFlags_NoGuiWidget);
			} else {
				activeSnapshotSlotParam->setFlags(activeSnapshotSlotParam->getFlags() | ofxOceanodeParameterFlags_NoGuiWidget);
			}
		}
	});
	
	addInspectorParameter(matrixRows.set("Snapshot Rows", 2, 1, 8));
	addInspectorParameter(matrixCols.set("Snapshot Cols", 8, 1, 8));
	addInspectorParameter(buttonSize.set("Button Size", 28.0f, 15.0f, 60.0f));
	addInspectorParameter(addSnapshotButton.set("Add Snapshot"));
	addInspectorParameter(clearSnapshotsButton.set("Clear All Snapshots"));
	
	morphEngine.getMorphMs() = 0.f;
	morphEngine.getMorphBiPow() = 0.f;

	// Add snapshot inspector
	addInspectorParameter(snapshotInspector.set("Snapshot Names", [this]() {
		renderInspectorInterface();
	}));
	
	// Setup snapshot event listeners
	addSnapshotListener = addSnapshotButton.newListener([this](){
		int newSlot = snapshotSystem.count();
		storeRouterSnapshot(newSlot);
	});
	
	clearSnapshotsListener = clearSnapshotsButton.newListener([this](){
		guiState.showClearAllSnapshotsPopup = true;
	});
	
	matrixSizeListener = matrixRows.newListener([this](int& value){
		onMatrixSizeChanged(value);
	});
	
	activeSnapshotSlotListener = activeSnapshotSlot.newListener([this](int& slot){
		if(slot >= 0) {
			loadRouterSnapshot(slot);
		}
	});
	
	// Setup preset control
	auto presetNamingRef = addParameter(presetNaming.set("Preset Naming Gui", [this](){
		renderPresetNamingGui();
	}));
	
	presetControlRef->addReceiveFunc<int>([this](const int &i){
		presetManager.loadMacroInsideCategory(i);
	});
	
	presetControlRef->addReceiveFunc<float>([this](const float &f){
		presetManager.loadMacroInsideCategory(floor(f));
	});
	
	presetControlRef->addReceiveFunc<vector<int>>([this](const vector<int> &vi){
		presetManager.loadMacroInsideCategory(vi[0]);
	});
	
	presetControlRef->addReceiveFunc<vector<float>>([this](const vector<float> &vf){
		presetManager.loadMacroInsideCategory(floor(vf[0]));
	});
	
	// Initialize container
	container = make_shared<ofxOceanodeContainer>(registry, typesRegistry);
	newNodeListener = container->newNodeCreated.newListener(this, &ofxOceanodeNodeMacro::newNodeCreated);
	allNodesCreatedListener = container->allNodesCreated.newListener(this, &ofxOceanodeNodeMacro::allNodesCreated);
	
	canvas.setContainer(container);
	canvas.setup("Macro " + ofToString(getNumIdentifier()), canvasParentID);
	
	// get Snap params from shared
	bool b = ofxOceanodeShared::getSnapToGrid();
	canvas.setSnapToGrid(b);
	int i = ofxOceanodeShared::getSnapGridDivs();
	canvas.setGridDivisions(ofxOceanodeShared::getSnapGridDivs());
	canvas.updateGridSize();
	
	if(additionalInfo != ""){
		presetManager.setLocal(false);
		isLoadingPreset = true;
		// Load router sort order from macro folder before loadPreset()
		{
			string sortOrderFile = additionalInfo + "/router_sort_order.json";
			ofJson sortJson;
			if(ofFile::doesFileExist(sortOrderFile)) sortJson = ofLoadJson(sortOrderFile);
			loadRouterSortFromJson(sortJson);
		}
		presetManager.setInnerPresetLoadingPath(additionalInfo);
		container->loadPreset(additionalInfo);
		presetManager.setInnerPresetLoadingPath("");
		isLoadingPreset = false;
		presetManager.updateCategoryFromPath(additionalInfo);
		presetManager.setCurrentMacroPath(additionalInfo);
		
		// Load snapshots if they exist
		snapshotSystem.loadFromPath(additionalInfo);
		if(!snapshotSystem.isEmpty()) {
			showSnapshotMatrix = true;
		}
	}
	
	snapshotUpdatedListener = ofxOceanodeShared::getSnapshotUpdatedEvent().newListener([this](string &path){
		
		if(!presetManager.isLocal() && path == presetManager.getCurrentMacroPath()) {
			snapshotSystem.loadFromPath(presetManager.getCurrentMacroPath());
			if(!snapshotSystem.isEmpty()) {
				showSnapshotMatrix = true;
			}
		}
	});
	
	macroUpdatedListener = ofxOceanodeShared::getMacroUpdatedEvent().newListener([this](string &s){
		if(s == presetManager.getCurrentMacroPath() && !presetManager.isLocal()){
			// Load router sort order from macro folder before reloading
			{
				string sortOrderFile = presetManager.getCurrentMacroPath() + "/router_sort_order.json";
				ofJson sortJson;
				if(ofFile::doesFileExist(sortOrderFile)) sortJson = ofLoadJson(sortOrderFile);
				loadRouterSortFromJson(sortJson);
			}
//			if(clearContainerOnLoad) container->clearContainer();
			container->loadPreset(presetManager.getCurrentMacroPath());
		}
	});
	
	// Router sort order inspector region
	addInspectorSeparator("Router Management",ofColor(255,128,0,128));

	memset(guiState.routerSortSepNameBuf, 0, sizeof(guiState.routerSortSepNameBuf));
	addInspectorParameter(routerSortInspector.set("Router Order", [this]() {
		renderRouterSortInterface();
	}));
}

// ─── Router node processing ──────────────────────────────────────────────────

void ofxOceanodeNodeMacro::processRouterNode(ofxOceanodeNode* node){
	shared_ptr<ofAbstractParameter> newCreatedParam;

	// Check for routerDropdown first — it doesn't go through the type registry
	auto* dropdownRouter = dynamic_cast<routerDropdown*>(&node->getNodeModel());
	if(dropdownRouter != nullptr){
		// Build a synced int parameter
		auto paramRef = dynamic_pointer_cast<ofParameter<int>>(dropdownRouter->getValue().newReference());
		auto param = make_shared<ofParameter<int>>();
		param->set(dropdownRouter->getNameParam().get(), paramRef->get(), 0,
		           max(0, (int)dropdownRouter->getCurrentOptions().size() - 1));

		deleteListeners.push(paramRef->newListener([paramRef, param](int &val){ param->set(paramRef->get()); }));
		deleteListeners.push(param->newListener([paramRef, param](int &val){ paramRef->set(param->get()); }));
		deleteListeners.push(dropdownRouter->getNameParam().newListener([param](string &s){
			param->setName(s);
		}));

		newCreatedParam = param;

		string paramName = newCreatedParam->getName();
		while(getParameterGroup().contains(paramName)) paramName = "_" + paramName;
		newCreatedParam->setName(paramName);

		auto addedAbstract = addParameter(*newCreatedParam.get());

		// Set initial dropdown options on the macro pin
		auto opts = dropdownRouter->getCurrentOptions();
		if(!opts.empty()){
			addedAbstract->cast<int>().setDropdownOptions(opts);
		}

		// Watch the options parameter on the router node for changes, keep macro pin in sync
		auto& routerParams = node->getParameters();
		if(routerParams.contains("Options")){
			auto& optionsAbstract = routerParams.get("Options");
			auto* optionsOceaParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&optionsAbstract);
			if(optionsOceaParam && optionsOceaParam->valueType() == typeid(vector<string>).name()){
				// Capture shared_ptr to keep addedAbstract alive in lambda
				routerManager.getDropdownListeners()[paramName + "_opts"] =
					optionsOceaParam->cast<vector<string>>().getParameter().newListener(
						[addedAbstract, param](vector<string> &newOpts){
							addedAbstract->cast<int>().setDropdownOptions(newOpts);
							param->setMax(max(0, (int)newOpts.size() - 1));
							if(param->get() >= (int)newOpts.size()) param->set(0);
						}
					);
			}
		}

		ofParameter<string> nameParamFromRouter = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam();
		nameParamFromRouter = paramName;

		parameterGroupChanged.notify(this);
		deleteListeners.push(node->deleteModule.newListener([this, nameParamFromRouter, paramName](){
			getParameterGroup().remove(nameParamFromRouter);
			routerManager.getDropdownListeners().erase(paramName);
			routerManager.getDropdownListeners().erase(paramName + "_opts");
		}, 0));

		routerManager.updateInfo(node);

		// Auto-register in sort order and set up rename/delete tracking.
		// shared_ptr<string> currentName is shared between the rename listener
		// and the delete listener so that a rename keeps both in sync.
		{
			string rName = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam().get();
			if(!isRouterInSortOrder(rName))
				sortOrder.getOrder().push_back(rName);

			auto currentName = make_shared<string>(rName);
			auto& nameRef = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam();

			deleteListeners.push(nameRef.newListener([this, currentName](string& newName) mutable {
				// Update sort order entry
				for(auto& e : sortOrder.getOrder())
					if(!isSortSeparatorEntry(e) && e == *currentName){ e = newName; break; }
				// Keep routerManager's map in sync so the inspector "isUnknown" check
				// reflects the new name immediately after a runtime rename.
				routerManager.renameRouter(*currentName, newName);
				*currentName = newName;
			}));

			deleteListeners.push(node->deleteModule.newListener([this, currentName](){
				sortOrder.getOrder().erase(
					remove(sortOrder.getOrder().begin(), sortOrder.getOrder().end(), *currentName),
					sortOrder.getOrder().end());
			}));
		}
		return;
	}

	// Generic path for all other router types.
	// NOTE: createRouterFromType() returns nullptr for router<void> because void
	// has no typed value to sync. We still register the node for rename/sort
	// tracking; we just skip creating a macro pin for it.
	newCreatedParam = typesRegistry->createRouterFromType(node);
	if(newCreatedParam != nullptr){
		string paramName = newCreatedParam->getName();
		while(getParameterGroup().contains(paramName)) paramName = "_" + paramName;
		newCreatedParam->setName(paramName);
		addParameter(*newCreatedParam.get());

		ofParameter<string> nameParamFromRouter = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam();
		nameParamFromRouter = paramName;

		parameterGroupChanged.notify(this);
		deleteListeners.push(node->deleteModule.newListener([this, nameParamFromRouter](){
			getParameterGroup().remove(nameParamFromRouter);
		}, 0));
	}

	routerManager.updateInfo(node);

	// Auto-register in sort order and set up rename/delete tracking.
	// shared_ptr<string> currentName is shared between the rename listener
	// and the delete listener so that a rename keeps both in sync.
	{
		string rName = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam().get();
		if(!isRouterInSortOrder(rName))
			sortOrder.getOrder().push_back(rName);

		auto currentName = make_shared<string>(rName);
		auto& nameRef = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam();

		deleteListeners.push(nameRef.newListener([this, currentName](string& newName) mutable {
			// Update sort order entry
			for(auto& e : sortOrder.getOrder())
				if(!isSortSeparatorEntry(e) && e == *currentName){ e = newName; break; }
			// Keep routerManager's map in sync so the inspector "isUnknown" check
			// reflects the new name immediately after a runtime rename.
			routerManager.renameRouter(*currentName, newName);
			*currentName = newName;
		}));

		deleteListeners.push(node->deleteModule.newListener([this, currentName](){
			sortOrder.getOrder().erase(
				remove(sortOrder.getOrder().begin(), sortOrder.getOrder().end(), *currentName),
				sortOrder.getOrder().end());
		}));
	}
}

// ─── Node lifecycle callbacks ────────────────────────────────────────────────

void ofxOceanodeNodeMacro::newNodeCreated(ofxOceanodeNode* &node){
	string nodeName = node->getParameters().getName();
	if(ofSplitString(nodeName, " ")[0] == "Router"){
		if(isLoadingPreset){
			// Pre-load nameParam from the router's preset JSON so that
			// allNodesCreated() can build routerByName with the correct saved
			// names instead of the default type labels ("Float", "Int", …).
			// This mirrors what loadPresetBeforeConnections does (step 3), but
			// we need the name to be correct already at allNodesCreated() time
			// (step 2) so our sort-order lookup works.
			if(!presetManager.getInnerPresetLoadingPath().empty()){
				string escapedName = node->getNodeModel().nodeName();
				ofStringReplace(escapedName, " ", "_");
				string filePath = presetManager.getInnerPresetLoadingPath() + "/" + escapedName
				    + "_" + ofToString(node->getNodeModel().getNumIdentifier()) + ".json";
				if(ofFile::doesFileExist(filePath)){
					ofJson routerJson = ofLoadJson(filePath);
					if(!routerJson.empty()){
						// Call abstractRouter::loadBeforeConnections which does
						// ofDeserialize(json, nameParam) — sets the name param
						// to the saved value before allNodesCreated() fires.
						static_cast<abstractRouter*>(&node->getNodeModel())
						    ->loadBeforeConnections(routerJson);
					}
				}
			}
			toCreateRouters.push_back(node);
			return;
		}
		// Interactive creation (not during preset load): use processRouterNode()
		// so the router is immediately added to sortOrder.getOrder() and tracked for
		// renaming/deletion — making it appear in the inspector reorder list right away.
		processRouterNode(node);
	}
	ofEventArgs args;
    node->setActive(active);
    if(active) node->update(args);
}

void ofxOceanodeNodeMacro::allNodesCreated(){
	// ── Collect pre-existing router nodes ────────────────────────────────────
	// Routers that were already in the inner container (from a previous load
	// pass) are merely repositioned by loadPreset_loadNodes() — they do NOT
	// fire newNodeCreated() and therefore are NOT in toCreateRouters.  Without
	// this extra step, sortOrder.getOrder().clear() below would lose those routers
	// from the inspector list even though their parameters are still alive.
	// We must re-add their names to sortOrder.getOrder() WITHOUT calling
	// processRouterNode() again (which would create duplicate parameters and
	// duplicate listeners).
	set<ofxOceanodeNode*> toCreateSet(toCreateRouters.begin(), toCreateRouters.end());
	vector<ofxOceanodeNode*> preExistingRouters;
	for(auto& typeMap : container->getDynamicNodes()){
		if(!typeMap.first.empty() && ofSplitString(typeMap.first, " ")[0] == "Router"){
			for(auto& nodePair : typeMap.second){
				auto* node = nodePair.second.get();
				if(toCreateSet.find(node) == toCreateSet.end()){
					preExistingRouters.push_back(node);
				}
			}
		}
	}

	// Nothing newly created → decide whether we still need to do separator work.
	// If no router was newly created AND neither the parameter group nor the
	// pending sort order contains any separator entries, there is truly nothing
	// to rebuild — preserve the existing sortOrder.getOrder() and bail out early.
	// We must NOT bail when separators are involved: old ones may need to be
	// removed (they have no deleteModule listener) and/or the new preset may
	// bring a different separator layout via sortOrder.getPendingOrder().
	if(toCreateRouters.empty()){
		bool separatorWorkNeeded = false;
		for(int i = 0; i < getParameterGroup().size() && !separatorWorkNeeded; i++){
			const string& pn = getParameterGroup().get(i).getName();
			if(pn.size() > 11 && pn.substr(0, 11) == "SEPARATOR:|") separatorWorkNeeded = true;
		}
		for(const auto& e : sortOrder.getPendingOrder()){
			if(isSortSeparatorEntry(e)){ separatorWorkNeeded = true; break; }
		}
		if(!separatorWorkNeeded){
			sortOrder.getPendingOrder().clear();
			return;
		}
	}

	// Remove any separator parameters left over from a previous load.
	// Router parameters must NOT be removed here — their ofxOceanodeAbstractParameter
	// objects are referenced by existing outer connections and by the type-registry
	// listeners. Recreating them would break those connections. Separator ordering
	// is corrected at the end via syncParameterGroupToSortOrder().
	{
		vector<string> toRemove;
		for(int i = 0; i < getParameterGroup().size(); i++){
			const string& pName = getParameterGroup().get(i).getName();
			if(pName.size() > 11 && pName.substr(0, 11) == "SEPARATOR:|")
				toRemove.push_back(getParameterGroup().get(i).getEscapedName());
		}
		for(const auto& n : toRemove) getParameterGroup().remove(n);
	}

	// Rebuild sortOrder.getOrder() from scratch.
	// processRouterNode() pushes each NEW router name; pre-existing routers are
	// pushed inline below; separators are pushed inline in the sorted path.
	sortOrder.getOrder().clear();

	if(!sortOrder.getPendingOrder().empty()){
		// ── Sorted path ─────────────────────────────────────────────────────
		// Build name→node maps for both new routers (need processRouterNode)
		// and pre-existing ones (already fully set up, just need sort-order entry).
		// Router nameParams have been pre-loaded by newNodeCreated(), so the
		// map keys are the correct saved names, not the default type labels.
		map<string, ofxOceanodeNode*> newRouterByName;
		for(auto* node : toCreateRouters){
			string name = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam().get();
			newRouterByName[name] = node;
		}
		map<string, ofxOceanodeNode*> preExistingByName;
		for(auto* node : preExistingRouters){
			string name = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam().get();
			preExistingByName[name] = node;
		}

		set<ofxOceanodeNode*> processed;

		for(const auto& entry : sortOrder.getPendingOrder()){
			if(isSortSeparatorEntry(entry)){
				// Inject separator into the parameter group AND into the active order.
				addSeparator(getSortSeparatorLabel(entry), getSortSeparatorColor(entry));
				sortOrder.getOrder().push_back(entry);
			} else {
				// New router: needs full processRouterNode() setup.
				auto it = newRouterByName.find(entry);
				if(it != newRouterByName.end()){
					processRouterNode(it->second);   // pushes entry to sortOrder.getOrder()
					ofEventArgs args;
					it->second->update(args);
					processed.insert(it->second);
				} else {
					// Pre-existing router: parameter already exists in the group.
					// Just restore the sort-order entry; syncParameterGroupToSortOrder()
					// at the end will reorder the group correctly (including separators).
					auto it2 = preExistingByName.find(entry);
					if(it2 != preExistingByName.end()){
						if(!isRouterInSortOrder(entry))
							sortOrder.getOrder().push_back(entry);
						processed.insert(it2->second);
					}
					// Entries not found in either map (router was deleted): silently dropped.
				}
			}
		}

		// New routers not listed in the sort order: append in Y-position order
		// (backward compatibility — new routers added to the inner macro).
		vector<ofxOceanodeNode*> remainingNew;
		for(auto* node : toCreateRouters)
			if(processed.find(node) == processed.end()) remainingNew.push_back(node);
		sort(remainingNew.begin(), remainingNew.end(), [](ofxOceanodeNode* a, ofxOceanodeNode* b){
			return a->getNodeGui().getPosition().y < b->getNodeGui().getPosition().y;
		});
		for(auto* node : remainingNew){
			processRouterNode(node);
			ofEventArgs args;
			node->update(args);
		}

		// Pre-existing routers not listed in the sort order: append at the end.
		for(auto* node : preExistingRouters){
			if(processed.find(node) == processed.end()){
				string name = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam().get();
				if(!isRouterInSortOrder(name))
					sortOrder.getOrder().push_back(name);
			}
		}
	} else {
		// ── Unsorted path (no saved order) ──────────────────────────────────
		// Combine all routers (new + pre-existing) and sort by Y-position.
		// Call processRouterNode() only for truly new ones.
		vector<pair<ofxOceanodeNode*, bool>> allRoutersTagged; // {node, isNew}
		for(auto* n : toCreateRouters)    allRoutersTagged.push_back({n, true});
		for(auto* n : preExistingRouters) allRoutersTagged.push_back({n, false});
		sort(allRoutersTagged.begin(), allRoutersTagged.end(),
		     [](const pair<ofxOceanodeNode*, bool>& a, const pair<ofxOceanodeNode*, bool>& b){
			return a.first->getNodeGui().getPosition().y < b.first->getNodeGui().getPosition().y;
		});
		for(auto& tagged : allRoutersTagged){
			ofxOceanodeNode* node = tagged.first;
			bool isNew            = tagged.second;
			if(isNew){
				processRouterNode(node);
				ofEventArgs args;
				node->update(args);
			} else {
				string name = static_cast<abstractRouter*>(&node->getNodeModel())->getNameParam().get();
				if(!isRouterInSortOrder(name))
					sortOrder.getOrder().push_back(name);
			}
		}
	}

	sortOrder.getPendingOrder().clear();
	toCreateRouters.clear();

	// Reorder the parameter group in-place to match sortOrder.getOrder().
	// This places separators at the correct positions relative to router parameters
	// without removing or recreating any parameter objects, so outer connections
	// (which reference specific ofxOceanodeAbstractParameter objects) remain valid.
	syncParameterGroupToSortOrder();
}

// ─── Macro save/load ─────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::macroSave(ofJson &json, string path){
	if(presetManager.isLocal()){
		string localPath = path + "/" + nodeName() + "_" + ofToString(getNumIdentifier());
		container->savePreset(localPath);
		json["LocalPreset"] = true;
		
		// Save snapshots to a separate file in the local macro folder
		if(!snapshotSystem.isEmpty()) {
			string snapshotsFilePath = localPath + "/snapshots.json";
			ofJson snapshotsJson;
			for(const auto& pair : snapshotSystem.getSnapshots()) {
				ofJson slotJson;
				slotJson["name"] = pair.second.name;
				slotJson["morphTimeMs"] = pair.second.morphTimeMs;
				slotJson["morphBiPow"] = pair.second.morphBiPow;
				slotJson["routerValues"] = MacroSnapshotSystem::routerValuesToJson(pair.second.routerValues);
				snapshotsJson[ofToString(pair.first)] = slotJson;
			}
			ofSavePrettyJson(snapshotsFilePath, snapshotsJson);
			
			// Keep saving to JSON for backward compatibility
			ofJson jsonSnapshots;
			for(const auto& pair : snapshotSystem.getSnapshots()) {
				ofJson slotJson;
				slotJson["name"] = pair.second.name;
				slotJson["morphTimeMs"] = pair.second.morphTimeMs;
				slotJson["morphBiPow"] = pair.second.morphBiPow;
				slotJson["routerValues"] = MacroSnapshotSystem::routerValuesToJson(pair.second.routerValues);
				jsonSnapshots[ofToString(pair.first)] = slotJson;
			}
			json["Snapshots"] = jsonSnapshots;
		}
	}
	else
	{
		json["LocalPreset"] = false;
		json["CategoryStruct"] = presetManager.getCurrentCategory();
		json["Macro"] = presetManager.getCurrentMacro();
		json["MacroPath"] = presetManager.getCurrentMacroPath();

		// For global macros: save router sort order to the macro folder so
		// it persists independently of which preset loads this macro (and so
		// "reload macro" from the popup also picks it up).
		if(!presetManager.getCurrentMacroPath().empty()) {
			string sortOrderFile = presetManager.getCurrentMacroPath() + "/router_sort_order.json";
			ofJson sortJson;
			if(!sortOrder.getOrder().empty()) {
				ofJson sortArr = ofJson::array();
				for(const auto& e : sortOrder.getOrder()) sortArr.push_back(e);
				sortJson["RouterSortOrder"] = sortArr;
			}
			ofSavePrettyJson(sortOrderFile, sortJson);
		}
	}

	json["RetriggerSnapshotOnActive"] = retriggerSnapshotOnActive.get();

	// Save router sort order for local macros (stored in the preset json).
	// Global macro sort order is saved to the macro folder above.
	if(presetManager.isLocal()) {
		if(!sortOrder.getOrder().empty()) {
			ofJson sortArr = ofJson::array();
			for(const auto& e : sortOrder.getOrder()) sortArr.push_back(e);
			json["RouterSortOrder"] = sortArr;
		}
	}
}

void ofxOceanodeNodeMacro::macroLoad(ofJson &json, string path){
//	if(json.count(clearContainerOnLoad.getEscapedName()) == 0){
//		clearContainerOnLoad = false;
//	}else{
//		deserializeParameter(json, clearContainerOnLoad);
//	}
//	deserializeParameter(json, clearContainerOnLoad);

	if(json.count("RetriggerSnapshotOnActive") == 0){
		retriggerSnapshotOnActive = false;
	}else{
		retriggerSnapshotOnActive = json["RetriggerSnapshotOnActive"].get<bool>();
	}
	
//	if(clearContainerOnLoad) container->clearContainer();
	isLoadingPreset = true;

	// Clear existing snapshots to prevent old data persisting
	snapshotSystem.clearAll();

	try {
		presetManager.setLocal(json.value("LocalPreset", true));

		if(presetManager.isLocal()){
			// For local macros: router sort order is stored in the preset json.
			// Load it before container->loadPreset() so allNodesCreated() can use it.
			loadRouterSortFromJson(json);

			string localPath = path + "/" + nodeName() + "_" + ofToString(getNumIdentifier());
			presetManager.setInnerPresetLoadingPath(localPath);
			container->loadPreset(localPath);
			presetManager.setInnerPresetLoadingPath("");
			
			// Store the local path for future snapshot saving
			presetManager.setPresetPath(localPath);
			
			// First check for snapshots.json file in the local macro folder
			string snapshotsFilePath = localPath + "/snapshots.json";
			if(ofFile::doesFileExist(snapshotsFilePath)) {
				snapshotSystem.loadFromPath(localPath);
				if(!snapshotSystem.isEmpty()) {
					showSnapshotMatrix = true;
				}
			}
			// If no separate file, check for embedded snapshots (backward compatibility)
			else if(json.contains("Snapshots") && !json["Snapshots"].is_null()) {
				snapshotSystem.loadFromJson(json["Snapshots"]);
				if(!snapshotSystem.isEmpty()) {
					showSnapshotMatrix = true;
				}
			}
		}else{
			// Reset local path since we're now using a global macro
			presetManager.setPresetPath("");
			
			auto currentCategoryVec = json["CategoryStruct"].get<deque<string>>();
			presetManager.getCurrentCategory() = currentCategoryVec;
			presetManager.setCurrentMacro(json["Macro"]);
			presetManager.setCurrentMacroPath("");
			
			if(presetManager.getCurrentMacroPath().empty()) {
				auto macroDirectoryStructure = ofxOceanodeShared::getMacroDirectoryStructure();
				for(int i = 0 ; i < presetManager.getCurrentCategory().size(); i++){
					string categoryNameToCompare = presetManager.getCurrentCategory()[i];
					macroDirectoryStructure = *std::find_if(macroDirectoryStructure->categories.begin(), macroDirectoryStructure->categories.end(),
															[categoryNameToCompare](shared_ptr<macroCategory> &mc){return mc->name == categoryNameToCompare;});
				}
				presetManager.setCurrentCategoryMacro(macroDirectoryStructure);
				auto iter = std::find_if(presetManager.getCurrentCategoryMacro()->macros.begin(), presetManager.getCurrentCategoryMacro()->macros.end(), [this](const std::pair<string, string> &pair){
					return pair.first == presetManager.getCurrentMacro();
				});
				if(iter != presetManager.getCurrentCategoryMacro()->macros.end()){
					// For global macros: router sort order is stored in the macro
					// folder (not the preset json). Load it before loadPreset() so
					// allNodesCreated() can apply it. Fall back to the preset json
					// for backward compatibility with old presets that stored it there.
					{
						string sortOrderFile = iter->second + "/router_sort_order.json";
						ofJson sortJson;
						if(ofFile::doesFileExist(sortOrderFile)) {
							sortJson = ofLoadJson(sortOrderFile);
						}
						if(!sortJson.empty()) {
							loadRouterSortFromJson(sortJson);
						} else {
							// Backward compat: try the preset json
							loadRouterSortFromJson(json);
						}
					}
					presetManager.setInnerPresetLoadingPath(iter->second);
					container->loadPreset(iter->second);
					presetManager.setInnerPresetLoadingPath("");
					presetManager.setCurrentMacroPath(iter->second);
				}
			} else {
				presetManager.setInnerPresetLoadingPath(presetManager.getCurrentMacroPath());
				container->loadPreset(presetManager.getCurrentMacroPath());
				presetManager.setInnerPresetLoadingPath("");
			}
			
			// Load snapshots if they exist
			if(!presetManager.getCurrentMacroPath().empty()) {
				snapshotSystem.loadFromPath(presetManager.getCurrentMacroPath());
				if(!snapshotSystem.isEmpty()) {
					showSnapshotMatrix = true;
				}
			}
		}
	} catch (ofJson::exception) {
		ofLog() << "Cannot get local preset";
		presetManager.setLocal(true);

		string localPath = path + "/" + nodeName() + "_" + ofToString(getNumIdentifier());
		presetManager.setInnerPresetLoadingPath(localPath);
		container->loadPreset(localPath);
		presetManager.setInnerPresetLoadingPath("");

		// Store the local path for future snapshot saving
		presetManager.setPresetPath(localPath);
	}
	isLoadingPreset = false;
}

// ─── Preset lifecycle overrides ──────────────────────────────────────────────

void ofxOceanodeNodeMacro::loadBeforeConnections(ofJson &json){
}

void ofxOceanodeNodeMacro::presetRecallBeforeSettingParameters(ofJson &json){
}

void ofxOceanodeNodeMacro::presetRecallAfterSettingParameters(ofJson &json){
}

void ofxOceanodeNodeMacro::presetWillBeLoaded(){
	container->loadPreset_presetWillBeLoaded();
	snapshotSystem.clearAll();
}

void ofxOceanodeNodeMacro::presetHasLoaded(){
}

void ofxOceanodeNodeMacro::activateConnections(){
}

void ofxOceanodeNodeMacro::deactivateConnections(){
}

// ─── Snapshot store/load ─────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::loadRouterSnapshot(int slot) {
	const SnapshotData* snapData = snapshotSystem.getSnapshot(slot);
	if(snapData == nullptr) return;

	routerManager.updateAllConnections(container.get());
	const auto& values = snapData->routerValues;

	activeSnapshotSlotListener.unsubscribe();
	activeSnapshotSlot = slot;
	activeSnapshotSlotListener = activeSnapshotSlot.newListener([this](int& slot){
		if(slot >= 0) {
			loadRouterSnapshot(slot);
		}
	});

	// Use the destination snapshot's morph time (and update the displayed sliders)
	morphEngine.getMorphMs() = snapData->morphTimeMs;
	morphEngine.getMorphBiPow() = snapData->morphBiPow;

	if(morphEngine.getMorphMs() > 0.f) {
		// Stop any ongoing interpolation
		morphEngine.stop();

		// Capture current router values as interpolation start
		std::map<std::string, RouterSnapshot> startValues;
		for(auto& routerPair : routerManager.getRouterNodes()) {
			if(!routerPair.second.isInput) continue;
			auto& router = routerPair.second;
			auto& params = router.node->getParameters();
			if(!params.contains("Value")) continue;
			auto valueParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get("Value"));
			if(!valueParam) continue;
			abstractRouter* absRouter = dynamic_cast<abstractRouter*>(&router.node->getNodeModel());
			if(absRouter == nullptr || absRouter->isExcludeFromSnapshot()) continue;
			if(!shouldInterpolateType(valueParam->valueType())) continue;

			try {
				RouterSnapshot startSnap = MacroRouterValueDispatch::captureForInterpolation(valueParam);
				startValues[router.routerName] = startSnap;
			} catch(const std::exception& e) {
				ofLogError("Snapshot") << "Error capturing start value: " << e.what();
			}
		}

		// Set target values
		std::map<std::string, RouterSnapshot> targetValues;
		for(const auto& pair : values) {
			targetValues[pair.first] = pair.second;
		}

		// Apply non-interpolatable types immediately (bool, string, void)
		for(auto& routerPair : routerManager.getRouterNodes()) {
			if(!routerPair.second.isInput) continue;
			auto valueIt = values.find(routerPair.second.routerName);
			if(valueIt == values.end()) continue;
			if(shouldInterpolateType(valueIt->second.type)) continue;

			auto& router = routerPair.second;
			auto& params = router.node->getParameters();
			if(!params.contains("Value")) continue;
			auto valueParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get("Value"));
			if(!valueParam) continue;
			abstractRouter* absRouter = dynamic_cast<abstractRouter*>(&router.node->getNodeModel());
			if(absRouter == nullptr || absRouter->isExcludeFromSnapshot()) continue;

			try {
				MacroRouterValueDispatch::applyValue(valueParam, valueIt->second);
			} catch(const std::exception& e) {
				ofLogError("Snapshot") << "Error applying immediate value: " << e.what();
			}
		}

		// Start interpolation
		morphEngine.startMorph(morphEngine.getMorphMs(), morphEngine.getMorphBiPow(),
		                       startValues, targetValues);

	} else {
		// Immediate apply
		morphEngine.setComplete();

		for(auto& routerPair : routerManager.getRouterNodes()) {
			if(!routerPair.second.isInput) continue;

			auto valueIt = values.find(routerPair.second.routerName);
			if(valueIt == values.end()) continue;

			auto& router = routerPair.second;
			auto& params = router.node->getParameters();
			if(!params.contains("Value")) continue; // void routers have no restorable value
			auto valueParam = dynamic_cast<ofxOceanodeAbstractParameter*>(&params.get("Value"));
			if(!valueParam) continue;
			abstractRouter* absRouter = dynamic_cast<abstractRouter*>(&router.node->getNodeModel());
			if(absRouter == nullptr) continue;
			if(absRouter->isExcludeFromSnapshot()) continue;

			try {
				MacroRouterValueDispatch::applyValue(valueParam, valueIt->second);
			} catch(const std::exception& e) {
				ofLogError("Snapshot") << "Error loading value: " << e.what();
			}
		}
	}

	snapshotSystem.setCurrentSlot(slot);
}


// ─────────────────────────────────────────────────────────────────────────────
// Snapshot methods — delegated to snapshotSystem
// ─────────────────────────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::storeRouterSnapshot(int slot) {
	routerManager.updateAllConnections(container.get());
	snapshotSystem.storeSnapshot(slot, routerManager.getRouterNodes(), morphEngine.getMorphMs(), morphEngine.getMorphBiPow());

	// Save snapshots to disk immediately for both local and global presets
	snapshotSystem.save(presetManager.isLocal(), presetManager.getCurrentMacroPath(), presetManager.getPresetPath(), nodeName(), getNumIdentifier());
}

void ofxOceanodeNodeMacro::syncSnapshotsFromDisk() {
	if(!presetManager.isLocal() && !presetManager.getCurrentMacroPath().empty()) {
		ofLogNotice("SnapshotSync") << "Manual sync requested for: " << presetManager.getCurrentMacroPath();
		snapshotSystem.syncFromDisk(presetManager.getCurrentMacroPath());
		if(!snapshotSystem.isEmpty()) {
			showSnapshotMatrix = true;
		}
	}
}

void ofxOceanodeNodeMacro::onMatrixSizeChanged(int& value) {
	matrixRows.set(ofClamp(matrixRows.get(), 1, 8));
	matrixCols.set(ofClamp(matrixCols.get(), 1, 8));
}

// ─── changeRouterType ────────────────────────────────────────────────────────

void ofxOceanodeNodeMacro::changeRouterType(const std::string& routerName, const std::string& newTypeName) {
	auto it = routerManager.getRouterNodes().find(routerName);
	if(it == routerManager.getRouterNodes().end()) return;
	auto* routerNode = it->second.node;

#ifndef OFXOCEANODE_HEADLESS
	glm::vec2 oldPos = routerNode->getNodeGui().getPosition();
#endif

	// 1. Find where this router sits in the sort order so we can restore it later.
	int sortIdx = -1;
	for(int i = 0; i < (int)sortOrder.getOrder().size(); ++i){
		if(!isSortSeparatorEntry(sortOrder.getOrder()[i]) && sortOrder.getOrder()[i] == routerName){
			sortIdx = i; break;
		}
	}

#ifndef OFXOCEANODE_HEADLESS
	// 2. Capture inner-container connection info as plain strings before the node
	//    is deleted (pointers become invalid after deleteSelf()).
	string oldGroupEscaped = routerNode->getParameters().getEscapedName();
	struct ConnInfo { string srcGroup, srcParam, sinkGroup, sinkParam; };
	vector<ConnInfo> savedConns;
	for(auto& conn : container->getAllConnections()){
		string srcG = conn->getSourceParameter().getGroupHierarchyNames()[0];
		string snkG = conn->getSinkParameter().getGroupHierarchyNames()[0];
		if(srcG == oldGroupEscaped || snkG == oldGroupEscaped){
			savedConns.push_back({srcG, conn->getSourceParameter().getName(),
			                      snkG, conn->getSinkParameter().getName()});
		}
	}
#endif

	// 3. Delete the old router.
	//    Fires deleteModule listeners: removes the published param from
	//    getParameterGroup(), removes routerName from sortOrder.getOrder(),
	//    and removes the node from the container's dynamicNodes map.
	routerNode->deleteSelf();
	routerNode = nullptr;

	// 4. Create the new router.
	//    Because isLoadingPreset == false, the newNodeCreated event listener
	//    calls processRouterNode() immediately, which adds a new published
	//    parameter and pushes the default type-label name onto sortOrder.getOrder().
	auto* newNode = container->createNodeFromName(newTypeName);
	if(!newNode) return;

#ifndef OFXOCEANODE_HEADLESS
	newNode->getNodeGui().setPosition(oldPos);
#endif

	// 5. Rename the new router back to the original name.
	//    The rename listener installed by processRouterNode() will synchronise
	//    sortOrder.getOrder() (default label → routerName) and routerManager's map.
	//    It does NOT rename the published macro parameter for generic (non-dropdown)
	//    routers, so we do that manually afterwards.
	string newNodeDefaultParamName = static_cast<abstractRouter*>(&newNode->getNodeModel())->getNameParam().get();
	static_cast<abstractRouter*>(&newNode->getNodeModel())->getNameParam().set(routerName);
	// Rename the published macro parameter to match (generic routers only – dropdown
	// installs its own listener that already does this).
	if(!routerName.empty() && getParameterGroup().contains(newNodeDefaultParamName)){
		getParameterGroup().get(newNodeDefaultParamName).setName(routerName);
	}

	// 6. Restore sort-order position.
	//    After steps 4+5 routerName sits at the end of sortOrder.getOrder().
	//    Move it back to the saved index.
	if(sortIdx >= 0){
		auto entryIt = std::find(sortOrder.getOrder().begin(), sortOrder.getOrder().end(), routerName);
		if(entryIt != sortOrder.getOrder().end()){
			sortOrder.getOrder().erase(entryIt);
			int insertAt = std::min(sortIdx, (int)sortOrder.getOrder().size());
			sortOrder.getOrder().insert(sortOrder.getOrder().begin() + insertAt, routerName);
		}
	}

#ifndef OFXOCEANODE_HEADLESS
	// 7. Reconnect inner-container connections (best-effort).
	//    Incompatible type pairs will silently fail — that is expected when
	//    switching between fundamentally different types.
	string newGroupEscaped = newNode->getParameters().getEscapedName();
	for(auto& ci : savedConns){
		string srcG = (ci.srcGroup == oldGroupEscaped) ? newGroupEscaped : ci.srcGroup;
		string snkG = (ci.sinkGroup == oldGroupEscaped) ? newGroupEscaped : ci.sinkGroup;
		container->createConnectionFromInfo(srcG, ci.srcParam, snkG, ci.sinkParam);
	}
#endif

	// 8. Sync the macro's parameter-group order to the (restored) sort order
	//    and broadcast the structural change to the outer container.
	syncParameterGroupToSortOrder();
	parameterGroupChanged.notify(this);
}
