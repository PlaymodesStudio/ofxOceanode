# Remove unused nodes, sink-based liveness: implementation plan

This plan is written for coding agents working in parallel. Follow it literally.
If the code contradicts this plan, stop and ask; do not improvise a different
design. Each work package (WP) names the files it may write. No file has two
writers. The contract in §3 is frozen before any WP starts: it is what lets the
packages run at the same time.

## 1. Goal

Replace the opt-in allowlist behind Edit → Remove unused nodes with a
sink-based liveness analysis:

- A **sink** is a node that does useful work on its own: it renders, sends,
  records, plays, drives hardware, or changes global state. Sinks are declared
  by the node (registration flag or `isSink()` override), inferred from shape
  (no written outputs), or implied by the host (bindings, scope, persistence).
- A node is **live** when it is a sink or when one of its outlets leads, through
  cables and wireless links, to a live node.
- Every node that is not live is **dead**. The command lists dead nodes in
  connected groups, lets the user untick groups, and removes the rest in one
  undoable canvas edit.

The old `CanRemoveWhenUnused` flag, `canRemoveWhenUnused()` and
`setUnusedCleanupSafe()` disappear from core. The enumerator stays for one
release as an ignored, deprecated value so sibling addons keep compiling.

## 2. Preconditions

1. Branch from `megabranch26_A`. One branch per WP, named `unused-nodes/wpN-<slug>`,
   all based on the WP0 commit (§4). The integrator merges them in the order of §6.
2. Read `CLAUDE.md`, `docs/unused-node-cleanup.md` (the current design, to be
   replaced) and this file.
3. Tests pull production code by text anchors (see `CLAUDE.md`, Tests). A WP that
   changes a function a test anchors on also owns updating that test.
4. Run `python3 tests/integration_compile.py` before every commit that touches a
   header.

## 3. Frozen contract

### 3.1 Registration flag (`src/Managers/ofxOceanodeNodeRegistry.h`)

```cpp
enum class ofxOceanodeNodeRegistrationFlags : unsigned int {
    None = 0,
    // Deprecated. Accepted and ignored so existing registrations compile.
    // Delete after the sibling-addon audit (WP5) has landed everywhere.
    CanRemoveWhenUnused = 1u << 0,
    // The model does useful work even when nothing consumes its outputs.
    // Dead-node cleanup never removes a sink or anything that feeds it.
    Sink = 1u << 1
};
```

`registerModel` calls `model->setDeclaredSink(true)` when `Sink` is set and does
nothing for `CanRemoveWhenUnused`. The `|` and `&` operators stay.

### 3.2 Node model (`src/Nodes/ofxOceanodeNodeModel.h`)

Replace the `canRemoveWhenUnused` / `setUnusedCleanupSafe` pair (around line 84)
with:

```cpp
// A sink does useful work on its own: it renders, sends, records, plays,
// drives hardware or changes global state. Declare one with the Sink
// registration flag, or override when the status is fixed for the class.
// The default also applies the shape rule (R2): a node with no output pin
// (isNodeOutput()) is a sink. A pure model whose output is an ordinary
// parameter overrides this and returns false.
virtual bool isSink() const;   // default body in ofxOceanodeNodeModel.h
void setDeclaredSink(bool sink){ declaredSink = sink; }
// ...
bool declaredSink = false;
```

### 3.3 Plan data (`src/Managers/ofxOceanodeContainer.h`, replaces `UnusedNodePlan`)

```cpp
struct UnusedNodePlan {
    struct Group {              // one connected component of dead nodes
        vector<string> nodes;   // escaped node names, sorted
        size_t connections = 0; // cables removed with the group
        bool selected = true;   // the window toggles this; removal honours it
    };
    struct Kept {
        string node;            // escaped node name
        vector<string> reasons; // see §3.5, sorted, unique
    };
    std::uint64_t revision = 0; // getNodesRevision() when computed
    vector<Group> groups;       // sorted by nodes
    vector<Kept> kept;          // sorted by node
};
UnusedNodePlan findUnusedNodes();
// Removes the selected groups only. Recomputes the plan first and refuses
// (returns false) when the revision or the dead set changed. Runs inside one
// beginCanvasEdit/endCanvasEdit so Cmd+Z restores everything.
bool removeUnusedNodes(const UnusedNodePlan& plan);
```

### 3.4 Liveness rules (implemented by WP2, declared by WP4/WP5)

Let N be the nodes of the reviewed container (`getAllModules()`, dynamic and
persistent). Compute `live ⊆ N` as the least fixed point of:

- **R1 Declared sink.** `node.getNodeModel().isSink()` is true. This covers the
  `Sink` flag, class overrides, and the shape rule below, because the default
  `isSink()` implements it.
- **R2 Shape sink (inside the default `isSink()`).** The node has no port for
  which `isNodeOutput()` is true: the `Output` flag, or an outlet without an
  inlet. Inspector parameters are not ports. This errs toward keeping: a node
  that writes outputs through plain `addParameter` is a sink until its author
  marks the outputs or overrides `isSink()` to return false. Routers return
  true, and portals and macros return false, by override, so R5, R6 and R7
  decide those.
- **R3 Host sink.** Any port parameter is scoped, timelined, bound by Custom GUI,
  snapshot or Timeline (`collectBoundParameters`, resolved through the root
  container and every macro, exactly as the current `findUnusedNodes` does), or
  has a MIDI binding (`OFXOCEANODE_USE_MIDI`). A persistent node is a sink.
- **R4 Cable.** A node is live when any out-connection from any of its ports
  (`getOutConnections()`) reaches a port whose owning node is live. Connections
  whose other end is not in N (cross-canvas, macro boundary) count as reaching
  a live node.
- **R5 Router.** `abstractRouter` overrides `isSink()` to return true. Routers are
  the macro's interface pins; removing one changes the parent's pins.
- **R6 Portal.** `abstractPortal` overrides `isSink()` to return false. A portal
  is live when R4 holds, or when it has an in-connection and any portal with the
  same name and matching scope (`match()`, honouring Local) either lives outside
  N or is live inside N, or when any `portalSelectorBase` in N or outside N has
  it selected. Enumerate peers with `ofxOceanodeShared::getPortalsNamed(name)`.
- **R7 Macro.** `ofxOceanodeNodeMacro` is live when R4 holds or when its child
  container contains a *hard sink*: a node satisfying R1, R2, R3 or R6-with-outer-peer,
  excluding routers, or a nested macro with a hard sink. Implement as
  `static bool containerHasHardSink(ofxOceanodeContainer&)`, recursive.
- **R8 Fixed point.** Iterate R4/R6/R7 until no node changes. Cycles of dead
  nodes stay dead. Dead nodes feeding other dead nodes stay dead.

Groups are the connected components of the dead set over cables. Order of
evaluation must not change the result; the test in WP2 checks a cycle, a chain
of five, a dead branch off a live chain, and a dead subgraph feeding a dead sink
chain that ends nowhere.

### 3.5 Reason strings (shown in the window, asserted by tests)

Exact prefixes, one reason per line:

- `sink: declared` (R1), `sink: no outputs` (R2), `sink: router` (R5)
- `sink: scope <param>`, `sink: timeline <param>`, `sink: binding <param>`,
  `sink: MIDI <param>`, `sink: persistent` (R3)
- `feeds: <node name>` (R4, first live consumer found), `feeds: outside canvas`
- `portal: <name> used by <node or 'another canvas'>` (R6)
- `macro: contains <node name>` (R7)

### 3.6 Window behaviour (`ofxOceanode::drawUnusedNodesWindow`)

Same window, same non-modal rules, same click-to-navigate. Changes:

- Header: `Remove N nodes in G groups and C connections. Keep K nodes.`
- Each dead group is a `Checkbox` + `TreeNode` ("Group i: <first node> (n nodes)")
  listing its nodes. Unticking sets `Group::selected = false`; counts update.
- Kept nodes are one collapsed `TreeNode` per node showing its reasons.
- Remove is disabled when no group is selected, the canvas changed, or the
  canvas is closed. After Remove the window stays open and refreshes.
- Drop the paragraph about protected models. Replace with one line:
  `Sinks, bound parameters and everything feeding them are kept. Cmd+Z restores a removal.`

## 4. WP0: contract (sequential, lands first)

Owner: the integrator, before any other WP branches. About 30 minutes.

Files: `src/Managers/ofxOceanodeNodeRegistry.h`, `src/Nodes/ofxOceanodeNodeModel.h`,
`src/Managers/ofxOceanodeContainer.h`, `tests/node_registration_flags_regression.py`.

1. Apply §3.1, §3.2 and §3.3 exactly. Leave `findUnusedNodes`,
   `removeUnusedNodes` and the window compiling by the smallest stub edit
   (fill `groups` with the old `groups` names, leave `kept` empty). This commit
   may list the wrong nodes; it must compile and pass `integration_compile.py`.
2. In `ofxOceanodeDefaultNodes.cpp` replace every `CanRemoveWhenUnused` with
   nothing (the plain overload). This is a text substitution only; WP4 does the
   real audit.
3. Rewrite `tests/node_registration_flags_regression.py` for `Sink`: flag sets
   `declaredSink`, `CanRemoveWhenUnused` is ignored, `isSink()` override wins,
   legacy constructor arguments still pass through, `|` combines.
4. Commit: `Unused nodes: freeze sink contract`. Tag the commit; every WP
   branches from it.

## 5. Work packages (parallel after WP0)

### WP1: core sink overrides (small)

Files: `src/Nodes/Default_Nodes/router.h`, `src/Nodes/Default_Nodes/portal.h`,
`src/Nodes/ofxOceanodeNodeMacro.h`, `src/Nodes/Default_Nodes/portalSelector.h`.

- `abstractRouter::isSink() const override { return true; }` (R5).
- `abstractPortal::isSink() const override { return false; }` (R6).
- `portalSelectorBase`: add `abstractPortal* getSelectedPortal() const` if no
  accessor exists (WP2 needs it for R6).
- `ofxOceanodeNodeMacro::isSink() const override { return false; }`; R7 is
  computed by WP2. Without the override the default shape rule would keep
  every macro whose pins are not flagged as outputs.
- Done when `integration_compile.py` passes. No test of its own; WP2's test
  covers the rules.

### WP2: liveness analysis and undoable removal

Files: `src/Managers/ofxOceanodeContainer.cpp` (only `findUnusedNodes`,
`removeUnusedNodes` and new static helpers next to them),
`tests/unused_nodes_regression.py`.

- Rewrite `findUnusedNodes` to §3.4 and §3.5. Keep the binding collection code
  as is. Keep the "never retain node pointers" rule: the plan holds names.
- Add `containerHasHardSink` (R7) and a portal peer lookup (R6).
- `removeUnusedNodes`: recompute, compare `revision` and the dead groups
  (ignoring `selected`), refuse on mismatch, then
  `beginCanvasEdit(); delete selected groups via deleteSelf(); endCanvasEdit();`.
  Comment pruning stays.
- Rewrite `tests/unused_nodes_regression.py`. Its mocks must expose
  `isSink()`, `getFlags()` with the Output bit, `getPortalsNamed`, a
  `portalSelectorBase` with `getSelectedPortal`, and `beginCanvasEdit`/
  `endCanvasEdit` counters. Cases: each rule R1–R8, the four graph shapes in
  §3.4, `selected=false` groups survive removal, a stale revision refuses,
  comments lose removed nodes, undo brackets exactly one edit, 2,000-node
  graph finishes under one second with ASan.
- Done when the test passes under ASan/UBSan and `integration_compile.py` passes.

### WP3: review window

Files: `src/ofxOceanode.cpp` (only `drawUnusedNodesWindow` and the menu item
that opens it), `src/ofxOceanode.h` (the unused-nodes members),
`tests/unused_nodes_window_regression.py`.

- Implement §3.6 against the §3.3 struct. Do not touch the analysis.
- Update `tests/unused_nodes_window_regression.py`: group checkbox toggles
  `selected`, Remove disabled with nothing selected, kept-node tree shows
  reasons, window stays open and refreshes after Remove, existing navigation and
  stale-canvas cases still pass.
- Done when the window test passes and `integration_compile.py` passes.

### WP4: core node audit

Files: `src/ofxOceanodeDefaultNodes.cpp`, `src/ofxOceanodeDefaultRouting.cpp`,
`src/ofxOceanodeDefaultTypes.cpp`, and `isSink()` overrides inside
`src/Nodes/Default_Nodes/*.h` other than the four files WP1 owns.

For every registered core model decide one of: pure (no change), `Sink` flag,
or `isSink()` override. Write the decision table into
`docs/unused-node-cleanup.md` (WP6 merges it). Known answers:

- Sink by flag: `midiClockNode` (Drive Transport can change the transport),
  `ofxOceanodeMacroMixer` (morphs macros).
- Sink by shape, no change needed: `globalPitch` (no outputs).
- Pure, no change needed: `beatTransport` and `presetLoadTrigger` (they only
  publish values), `globalVariables` (a source), every generator and modifier,
  `curve`, `portalSelector` (R4 decides it).
- Pure by override: `simpleNumberGenerator` and `simpleNormalizedNumberGenerator`
  (a plain `Value` parameter is their output).
- Check each model's `addParameter` calls: a pure model whose written outputs
  lack the Output flag must switch them to `addOutputParameter`, otherwise R2
  keeps it forever. List those changes in the table.
- Done when `integration_compile.py` passes and the table is complete.

### WP5: sibling addon audit (one agent per addon, separate repositories)

Addons and registered model counts: ofxSantiNodes 202, ofxOceanodeSuperCollider 60,
ofxOceanodeTextures 48, ofxEloiNodes 39, ofxOceanodeVectorGraphics 39,
ofxOceanodePlaymodes 32, ofxOceanodeAstres 31, ofxOceanodeCollection 31,
ofxOceanodeMidi 10, ofxOceanodeLights 9, ofxOceanodeGraaf 7, ofxOceanodeLaser 6,
ofxOceanodeForms 5, ofxOceanodeOsc 3, ofxOceanodeNDI 2, ofxOceanodePonk 1,
ofxOceanodeTimeline 1, ofxOceanodeCustomGui 1, ofxPersonSegmentation 1.

Per addon, in its own repository and branch `unused-nodes-sinks`:

1. Remove every `ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused`.
2. For each model with side effects and written outputs add `Sink`. Typical:
   anything that outputs to screen, Syphon, NDI, Art-Net, DMX, MIDI, OSC, audio
   (every SuperCollider synth, mixer, recorder and output), files (recorders,
   snapshots), lasers, or that mutates global state.
3. Models with no `addOutputParameter` are already kept by R2; add `Sink` only
   if they also have outputs, and convert plain `addParameter` outputs of pure
   models to `addOutputParameter` so they can be cleaned.
4. Build against the WP0 commit of ofxOceanode with that addon's example or a
   downstream app, or at least `tests/integration_compile.py` with the addon's
   headers on the include path.
5. Deliver the decision table (model → pure/sink/override, and why) as the
   commit body.

These branches merge independently of §6 and can land after it.

### WP6: documentation

Files: `docs/unused-node-cleanup.md` (rewrite), `docs/oceanode-user-manual.md`
(add a short "Remove unused nodes" section if the manual has an Edit menu
section), `docs/node-help-catalog.json` only if it mentions the old flag
(it does not today).

- Rewrite the doc around §1, §3.4 and §3.5 in the house style of the current
  file: behaviour, rules, how an addon declares a sink, how to run the tests.
- Merge WP4's decision table.
- Done when the doc matches the merged code; the integrator reviews it last.

## 6. Integration order and verification

Merge in this order, running the checks after each merge:

1. WP0 (already on the base).
2. WP1, then WP4 (header overrides first, registrations second).
3. WP2.
4. WP3.
5. WP6.

After each merge:

```bash
python3 tests/integration_compile.py
```

```bash
for t in tests/*_regression.py; do python3 "$t" || echo "FAIL $t"; done
```

After the final merge, rebuild `example-basic` (`make -C example-basic` then
`RunRelease`) and run the manual scenarios in §7. Then delete
`docs/unused-nodes-liveness-plan.md` or move it under a `docs/plans/` folder.

## 7. Manual scenarios (example-basic or a Playmodes app)

1. Oscillator → mapper → nothing: both listed in one group, removed, Cmd+Z brings both back with the cable.
2. Oscillator → mapper → node with a Custom GUI widget: all kept, reasons show the binding and two `feeds:` lines.
3. Oscillator → textureDisplay (no outputs): kept with `sink: no outputs`.
4. A dead cycle of two nodes: listed, removed.
5. Portal "A" fed by an oscillator in canvas 1, portal "A" read in canvas 2: reviewing canvas 1 keeps both with `portal: A used by another canvas`; delete the reader, refresh, both listed.
6. Macro containing a sink, no outer cables: macro kept with `macro: contains …`. Macro containing only pure nodes and no outer cables: listed.
7. Inside a macro: input router with nothing attached is kept (`sink: router`); a dead chain hanging off it is listed.
8. Untick one of two groups, Remove: only the other goes; window refreshes and shows the remaining group.
9. Edit the canvas while the window is open: Remove disables until Refresh.

## 8. Out of scope

- Reviewing a whole patch tree from the root in one pass (today the command
  reviews the active canvas; a macro's internals are reviewed from inside it).
- Dynamic sinks whose status depends on runtime state (a recorder only while
  recording). `isSink()` is consulted at review time and may read state, but no
  core node does so in this plan.
- Removing the deprecated `CanRemoveWhenUnused` enumerator: a follow-up after
  every addon in WP5 has merged.
