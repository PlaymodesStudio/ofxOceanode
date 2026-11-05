# Architecture review — 2026-11-01

A pass over the core after the October 2026 feature work (GUI shell refactor,
MacroMixer, timeline transports, unused-node liveness, Global Variables, node
snapshots, profiling). It records what was changed in this pass, what was
found and left alone, and the order in which the remaining work pays off.

Scope: `src/` of ofxOceanode only. Sibling addons were read where the core API
they use was in question.

## 1. Summary

The four-owner GUI shell (`src/Gui/`) holds up: no dependency-rule violation
from `docs/gui-shell-refactor-plan.md` §3 was found, and no file has two
writers. The node/parameter/connection core is sound. The problems are
elsewhere:

- **Per-frame work that scales with the patch** rather than with what changed:
  linear node scans per cable, JSON serialisation of every editable parameter
  every frame, whole-graph walks in controllers, string-typed dispatch on
  every value pass.
- **Two files that were too large to navigate**: `ofxOceanodeContainer.cpp`
  (4750 lines, 118 functions, eight concerns; split in this pass, see §4) and
  `ofxOceanodeCanvas::draw` (one function of 3062 lines; not yet).
- **Duplicated walks**: the recursive "visit every node of every macro" loop
  is written eleven times across controllers, the facade and the GUI shell;
  the dynamic/persistent double loop is written seventeen times in the
  container.
- **Leaks that only show in long sessions**: listener lists that grow by one
  entry per node, connection, binding or macro router ever created.

## 2. Changed in this pass

### Performance (per frame or per value change)

| Where | Before | After |
| --- | --- | --- |
| `ofxOceanodeContainer::getGuiFromModel` | Linear scan of every node, called twice per cable per frame from the canvas: O(cables × nodes). | `nodesByModel` hash map kept beside `parameterGroupNodesMap`. |
| `ofxOceanodeNodeGui::constructGui`, Inspector | Every unconnected editable parameter serialised to JSON before and after its widget, every frame, for user-edit tracking (whole vectors included). | Skipped on frames with no mouse button, wheel, key, text input or active item (`ofxOceanodeNodeGui::userInputPossibleThisFrame`). |
| `ofxOceanodeAbstractParameter::refreshCombinedInput` / `allowsMultipleInputs` | Built a `std::string` from `typeid().name()` and did a map lookup on every value passed through a same-type connection. | Answer cached per parameter until another input combiner is registered. |
| `ofxOceanodeInspectorController::draw` | Copied the root node map (string keys) and recursed every macro with `dynamic_cast` each frame to find the selection. | Reads `ofxOceanodeNodeGui::selectedGuis()`, a registry kept by `setSelected`. |
| `ofxOceanodeTime::update` | Copied the phasor list through `ofThreadChannel` to the audio thread every frame. | Sent only when the graph scan rebuilt the list. |
| `MacroMixerSource::Graph::parameter` | Scanned every node of the source graph with `dynamic_cast` on every read, several times per router per frame. | Cached per router id, validated by the container's node revision and the router's name. |
| `ofxOceanodeMacroMixer::publish` | Assigned the status parameter every update and draw; `ofParameter::set` notifies on every assignment. | Assigned only when the text changes. |
| MiniMap, Hierarchy controllers | Rebuilt their scope/entry lists with a recursive `dynamic_cast` walk every frame. | Rebuilt when `ofxOceanodeNode::graphGeneration()` changes (Hierarchy also every 15 frames, for renames). |
| Layouts and Theme menus | Listed `config/guiLayouts` and `config/themes` (twice) from disk every frame while open. | Listed every 30 frames while open. |
| `oceanodeGlDiagnostics::Scope` | Built a `std::function` identity per node per phase even with tracing off. | Built only while `OCEANODE_GL_TRACE=1`. |
| Canvas node z-order sort | Comparator took both pairs by value (two string copies per comparison). | By const reference. |

### Structure

`ofxOceanodeContainer.cpp` is now eight files by concern (§4), header
unchanged, every function body moved verbatim. The tests that read the
container source by text anchor now read the files concatenated through
`tests/of_environment.py::container_source()`; two tests in the sibling
`ofxOceanodeTimeline` addon read the same path and were repointed the same
way (uncommitted there).

### Bugs

- `ofxOceanode::disableRenderHistograms` set `DisableRenderAll` instead of
  `DisableHistograms`.
- `ofxOceanodeLogController` drained its thread channel only in `draw()`,
  which hidden windows and show mode skip, so the channel grew for the length
  of a show. It now drains in `update()`.
- OSC relative int: `castedParam += (castedParam + arg)` doubled the value.
- OSC address parsing used `find` where the other six copies use `rfind`:
  a type whose name contains the id digits ("Vec3_3") was cut to "Ve".
- `loadPresetNumEvent` with preset number 0 indexed `[-1]`.
- `ofxOceanodeTypesRegistry`: a `vector<T>` source feeding a custom sink was
  cast to the scalar wrapper (`source.cast<T>()`), so the connection stored a
  mistyped reference and listener. Now `cast<std::vector<T>>()`.
- `MacroRouterManager` kept a dangling `RouterInfo::node` after a router inside
  a macro was deleted; the minimised view and a running snapshot morph
  dereferenced it. The delete listener now removes the entry.
- Canvas Shift+[C] on an empty canvas divided by zero and set the scroll to NaN.
- Hierarchy: a deferred single click indexed an entry list that may have been
  rebuilt shorter in the meantime.

## 3. Found and not changed

Ranked within each group by expected gain. Line numbers are as of this commit.

### 3.1 Per-frame cost

1. **Canvas node list** (`ofxOceanodeCanvas.cpp` ~1415-1500): the whole
   `parameterGroupNodesMap` is copied into a `vector<pair<string,Node*>>`
   each frame, keys are copied into two more sets, `nodesDrawingOrder`
   (`std::map<string,int>`) is cleared and rebuilt, and the visible list is
   copied again; `for(auto nodePair : …)` copies each pair (anchored by
   `canvas_nudge` and `canvas_undo` tests). A persistent `vector<Node*>` in
   z-order, invalidated by `getNodesRevision()`, with the z-index stored on
   the node, removes roughly 6N allocations per frame.
2. **Three passes over every parameter of every visible node** with
   `dynamic_pointer_cast` (~1664-1775, 2858): the input and output pin loops
   are mirror images; the parameter vector could be cached per node.
3. **Node GUI type dispatch** (`ofxOceanodeNodeGui.cpp` 395-718): a chain of
   up to 11 `valueType() == typeid(X).name()` comparisons per parameter per
   frame, each building a string; `getDropdownOptions()` returns a vector by
   value and is called to test emptiness; `isParameterShownInGui` escapes the
   name per call; `getParameters().getPosition(name)` per cable endpoint. A
   `std::type_index` (or enum) stored on the wrapper in `bindParameter` would
   turn every `valueType()` compare in the GUI, the registry, the scope,
   snapshots and `receiveParameter` into a pointer compare. This touches a
   template header used by every addon, so it is a deliberate API step.
4. **`ofxOceanodeNode::syncSnapshotSelector`** runs per node per frame; when
   the selector is on it walks all parameters with `dynamic_cast` and builds
   the option strings to compare with the cached ones.
5. **`beginCanvasEdit()` on every canvas click** (`ofxOceanodeCanvas.cpp:504`)
   serialises the entire graph twice (begin and end) even for a click on
   empty space or the zoom slider.
6. **MacroMixerRuntime `enforceWindows`/`adoptWindows`** walk the source graphs
   three times per frame building a label string per node; they only need to
   run when a graph, the window map or the transition state changes.
7. **`ofxOceanodeNodeMacroGui::renderPresetControlGui`** scans
   `liveMacros()` with path compares three times per drawn macro: O(M²) per
   frame across M macro nodes.
8. **Window menu** (`ofxOceanodeWindowManager.cpp:464-490`): two
   `dynamic_cast` per parameter of every node and an `ofJson::dump()` per
   node, every frame the menu is open.
9. `ofxOceanodeNodesController` rebuilds the display strings of every cached
   node each frame to detect renames; the Profiling filter concatenates
   `name + " " + path` per row per frame.
10. `ofxOceanodeScope::draw` does 3-4 `FindWindowByName` per scoped parameter
    per frame.

### 3.2 Structure and readability

1. **`ofxOceanodeCanvas::draw`** (3062 lines): the natural members are the
   portalize lambdas (531-881), `autoLayoutNodes` (567-652), the comments pass
   (1861-2231), the New Node modal (2449-2792) and the Alt+arrow portal
   cycling (3260-3422, already outside `Begin/End`). Six tests anchor on
   exact indented text inside this function, so each extraction re-indents
   anchored lines and must update the tests with it.
2. **One `forEachNode` walker.** The recursive macro walk with
   `dynamic_cast<ofxOceanodeNodeMacro*>` is written in Inspector, Nodes,
   Hierarchy, MiniMap, Profiling, Presets, PresetFileCleanupReview,
   WindowManager, NodePool and twice in the facade. One
   `ofxOceanodeContainer::visitNodes(visitor, recursive)` replaces eleven
   copies. Inside the container, the dynamic/persistent double loop appears
   seventeen times; `clearContainer` already shows the
   `for(auto* nodes : {&dynamicNodes, &persistentNodes})` idiom.
3. **Facade** (`ofxOceanode.cpp`): `drawUnusedNodesWindow` and its nine
   members are a full window with keyboard navigation and belong in a
   controller; `showHelpPopUp`/`openQuickTips`/`userManualPath` belong in
   `ofxOceanodeHelp`; `new OceanodeTheme()` is never freed and should be
   owned by the ThemeManager; `resetPresetCanvasViews` and
   `beginTopLevelMenu` duplicate helpers in `ofxOceanodeControls.cpp`.
4. **`ofxOceanodeShared`** is a single header holding dock ids, the macro
   directory, the portal registry, preset events, configuration flags, canvas
   zoom/font render context and root pointers. It is used by every addon, so
   its API should stay; grouping the state into named sub-structs inside the
   singleton would make the header readable without an API change.
5. **Connection templates** (`ofxOceanodeConnection.h`): nine classes
   re-declare listener, source/sink references, destructor, `restoreValue`
   and `serializeBeforeConnectionValue`; a shared typed base with one
   `assign` customisation point removes most of it.
   `ofxOceanodeConnection.cpp` is entirely commented-out code.
   `ofxOceanodeParameter<void>` re-declares every override without
   `override`.
6. **Macro family**: the "load contents from folder" sequence appears five
   times in `ofxOceanodeNodeMacro.cpp`; `MacroMixerSource.cpp` holds file
   analysis, graph I/O, morph planning and morph signals (four concerns);
   `RouterInfo` belongs in `MacroRouterManager.h` (a test slices it from the
   macro header, so move both together).
7. **Controllers** share four copy-pasted idioms: "select node in its
   canvas", deferred centre-on-node with a raw pointer (where
   `ofxOceanodeCanvas::requestCenterOnNode` already exists), the macro label
   "Name [Type]", and the name-prompt modal (eleven instances).
8. **Dead code** worth deleting: `ofxOceanodeCanvas` `screenToCanvas`,
   `canvasToScreen`, `getMatrixScale`, `translateMatrixWithoutScale` (only a
   commented block uses them), "Example 1/2/3" placeholder menus
   (2723-2767); container `extractNodeType` (never called; anchored by
   `macro_undo_regression`), `fullPath` built and discarded in
   `loadScope` (`ofxOceanodeContainerPresets.cpp`), the 47 per-node
   `ofLogNotice` dumps and three `ofSleepMillis` calls on the main thread in
   `ofxOceanodeContainerEncapsulate.cpp`; `MacroPresetManager` bank/preset
   members, `clearContainerOnLoad`,
   `MacroSnapshotSystem::calculateHash`; unused members listed in the
   controller headers (`ofxOceanodeControls::listeners`,
   `TimeController::bpmListener/phase/phaseListener`,
   `PresetsController::newPresetCreated`, MiniMap entry fields).

### 3.3 Correctness

1. **Listener lists that never shrink**: `destroyNodeListeners`,
   `destroyConnectionListeners`, `midiUnregisterlisteners`,
   `midiSenderListeners` in the container, `deleteListeners` in
   `ofxOceanodeNodeMacro` (which also captures the published `pin`
   `shared_ptr`, keeping every deleted router's parameter alive), and the
   router listeners pushed into the `ofxOceanodeTypesRegistry` singleton
   (five listeners and two parameters per router created, for the process
   lifetime). They are emptied only by `clearContainer`. A long show with
   many preset loads grows them linearly. The fix is a keyed container with
   deferred pruning on the next update, because the callback that would
   remove the entry runs from inside the event being destroyed.
2. **Raw node pointers kept across frames in the canvas**:
   `customGuiContextNode`, `pendingCenterNode`, `ofxOceanodeComment::nodes`
   are cleared only on a canvas-history revision change; a node removed by an
   OSC preset load, the Unused Nodes window or the Nodes controller between
   a right-click and the popup frame is dereferenced. `getNodesRevision()`
   is the right trigger.
3. **`loadPreset_loadNodes`** (`ofxOceanodeContainerPresets.cpp`) clears
   `parameterGroupNodesMap` when `modules.json` is present but empty,
   dropping persistent nodes from the map; the TODO beside it describes this.
   In `receiveOscMessage` the `if(size() != 1) return` guards make the
   following vector branches unreachable.
4. **Macro load failure** (`ofxOceanodeNodeMacro.cpp:121-152`): snapshots,
   local flag and contents are cleared before `container->loadPreset` can
   fail, leaving an empty node flagged global that still points at the
   previous library path.
5. `ofxOceanodeNodeGui.cpp:801` dereferences an `ofTexture*` without a null
   check for a folded node with `ScopeKeepAspectRatio`;
   `ofxOceanodeNodeGui.cpp:1008` passes a parameter name as a printf format.
6. `ofxOceanodeTimeController.cpp:109-125` lets ImGui write through
   `(bool*)&fm.get()` into parameter storage.
7. `ofxOceanodeAbstractParameter::removeOutConnection` uses single-argument
   `erase(std::remove(…))`, which is UB when the connection is absent; the
   wrapper's implicit copy constructor copies connection vectors, so copying
   a connected wrapper would double-delete.
8. Canvas comment header hit-test uses `15 / zoom` world height while the
   header is drawn 15 world units tall (right-click misses at zoom ≠ 1);
   `recenterCanvas` is only consumed inside `if(show_grid)`.
9. `ofxOceanodeDefaultRouting.cpp:39` registers `portal<Timestamp>` under
   category "Router" (every other portal is "Portal").
10. `getOutConnections()` returns its vector by value, and that copy is load
    bearing: `scMixerPro.cpp:793` in the SuperCollider addon calls
    `deleteSelf()` on each connection while iterating the result, which
    erases from the live vector. Returning a reference (the obvious per-frame
    saving) needs those callers changed first.

## 4. Container split: resulting files

Header unchanged; functions moved verbatim. Tests slice ranges of the
concatenated source (order: core, presets, paths, clipboard, encapsulate,
unused, midiosc, history), so the orders marked "keep" still matter.

| New file | Contents |
| --- | --- |
| `ofxOceanodeContainer.cpp` | anonymous ns: `containerClearDepth`, `ContainerClearGuard`, `ConnectionFailureInfo`, `ConnectionErrorDialogData`, `pendingConnectionErrorDialogs`, `drawConnectionErrorDialogContents` (keep this order: `new_preset`, `macro_mixer` slice it); ctor, dtor, `isClearingContainers`, `clearContainer`, `update`, `draw`, `drawPendingConnectionErrorDialog`, `activate`, `deactivate`, `createNodeFromName`, `replacePortalTypeInAllInstances`, `createNode`, `getAllModules`, `getGuiFromModel`, `getSelectedModules`, `getSelectedCommentIndices`, `deselectAllComments`, `deleteSelectedModules`, `setBpm`, `resetPhase`, `getTransportState`, `getFrameTransportState`, `saveCurrentPreset`, `createConnectionFromInfo`, `createConnection`, `getNodeFromParameter`, `userEditedValueKey`, `recordUserEditedValues`, `wasValueUserEdited`, `clearUserEditedValues`, `customGuiContainsParameterAnywhere` |
| `ofxOceanodeContainerPresets.cpp` | `loadPreset`, the nine `loadPreset_*`, `savePreset`, `savePersistent`, `loadPersistent`, `updatePersistent`, `saveScope`, `scheduleScopeSave`, `flushPendingScopeSave`, `loadScope`, `get*JsonForReview`; file-local `savedNodeKey`, `removeSkippedNodes`, `removeSkippedConnections`. Shares `pendingConnectionErrorDialogs` with core → internal header. |
| `ofxOceanodeContainerHistory.cpp` | `CanvasNodeSnapshot::~CanvasNodeSnapshot` first, (the two `getCustomGui*JsonForReview` getters sit here because the `[~CanvasNodeSnapshot, EOF)` test slice includes them) then capture/begin/end/measure/trim/`clearCanvasHistory`/replay/step/undo/redo/root/visit/finish/next/can*/revision/request/process, `MacroCanvasEditScope`, `rollbackMacroCanvasEdit`, then `extractSelectedMacro`, `extractMacro`, `retargetCanvasNodeSnapshot`, `synchronizeCanvasTransfers`, `captureCanvasAuxiliary`, `restoreCanvasAuxiliary` (keep: `canvas_undo` slices `[~CanvasNodeSnapshot, extractSelectedMacro)`; `macro_mixer`/`macro_undo` take `[~CanvasNodeSnapshot, EOF)`). |
| `ofxOceanodeContainerEncapsulate.cpp` | `findExclusiveUpstreamSubgraphs`, `encapsulateSelectedNodes`, `hasAutomatedParameters`, `encapsulateNodes`, `analyzeExternalConnections`, `createRoutersAndReconnect`, `isNodeInList`, `generateRouterName`, `findInternalParameter`, `extractNodeType`, `ensureMacroParametersExposed`, `planMacroPortalReduction`, `countReducibleMacroPortals`, `removeReducedMacroPortals`, `mergeReducedMacroRouters`, `reduceMacroPortals` |
| `ofxOceanodeContainerClipboard.cpp` | `loadClipboardModulesAndConnections`, `saveClipboardModulesAndConnections`, `copy/cut/pasteModules…` |
| `ofxOceanodeContainerUnused.cpp` | `findUnusedNodes`, `removeUnusedNodes` |
| `ofxOceanodeContainerPaths.cpp` | anonymous ns `kTimelineCanvasSeparator`, `isRootCanvasID` (must immediately precede `getTimelineParameterPath`: `macro_undo` slices that range), both `getTimelineParameterPath`, `collectMovedTimelinePaths`, `timelineCanvasPrefix`, `timelineMacroCanvas`, `findTimelineParameter`, `findParameter`, `parseParameterPath`, `resolveParameterFromPath`, both `getContainerForCanvasID`, `showParameterInCanvas`, `showNodeInCanvas`, `showNodesInCanvas` |
| `ofxOceanodeContainerMidiOsc.cpp` | `setMidiClockSync*`, `setIsListeningMidi`, `createMidiBinding`, `drawMidiKeyboardMenu`, `removeLastMidiBinding`, `midiBindingBound`, `createMidiBindingFromInfo`, `addNewMidiMessageListener`, `receiveOscMessage` |

`ofxOceanodeContainerInternal.h` declares the one thing two files share:
`queueConnectionErrorDialog`, which presets call and core drains. Tests
reading the source: `custom_gui_test_support`, `new_preset`,
`preset_file_cleanup`, `unused_nodes`, `layouts`, `macro_mixer` (and every
`macro_mixer_*`), `macro_undo`, `canvas_undo`, `clipboard_parameter`,
`unused_nodes_window`, `canvas_navigation`, plus `attachment_regression`
and `encapsulate_timeline_regression` in ofxOceanodeTimeline.

## 5. Suggested order

1. The `forEachNode` walker (§3.2.2) and the controller idioms (§3.2.7):
   pure readability, mechanical.
2. Canvas node list and parameter passes (§3.1.1-2): the next largest
   per-frame cost in big patches, inside one function, no API change.
3. Listener pruning (§3.3.1): the leak that matters for long shows.
4. Typed parameter dispatch (§3.1.3): the API step that removes most
   remaining string work from the GUI and value paths; coordinate with the
   sibling addons.
5. Canvas `draw` extraction (§3.2.1), one block at a time, each with its
   test anchors.
