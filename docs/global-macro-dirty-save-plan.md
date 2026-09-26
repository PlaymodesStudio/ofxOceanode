# Global macro dirty state and Save Preset

Status: implementation in progress on `codex/global-macro-save-live`; build and
interactive behavior still need validation in the host application.

## Goal

When the user chooses **Save Preset**, inspect every global macro reachable from
the current canvas, including macros nested in local and global macros. Ask
whether to save each global macro that has unsaved **user edits**. Save the
preset regardless of the answers. A clean global macro needs no prompt, and
skipping one must leave its `Macros/` folder untouched.

Here, **dirty** means that an edit made by the user would change the macro's
restorable definition compared with its existing global save. A transient
value produced by the running graph does not make a macro dirty. Returning an
edit to its saved state makes it clean again. A successful global-macro save or
reload establishes a new clean baseline; clicking Save Preset alone does not
clear the dirty state of a skipped macro.

## Ownership and nesting

- Each global macro has its own dirty state, keyed by its resolved path in
  `Macros/`. Its internal canvas and any **local** macros embedded in that
  canvas belong to that saved definition.
- A global macro used *inside* another macro is a separate saved definition.
  Editing its internal canvas dirties the child global macro. It does not by
  itself dirty the parent.
- The Macro **node** that selects or refers to a child belongs to the parent
  canvas. Changing that node's macro selection, local/global setting, saved
  parameters, inspector settings, position, or connections dirties the parent
  if the parent is global.
- Editing a local child embedded in a global macro dirties the containing
  global macro. Editing a local macro embedded in the root preset is handled
  by the ordinary preset save.
- Traversal must visit children even when their parent global macro is clean
  or the user declines to save it. Avoid cycles and identify repeated uses of
  the same global path. Two instances of the same path may have different
  in-memory edits; never silently choose one to overwrite the other.

## Dirty signals to include

All signals below apply to an edit on a global macro's owned canvas, including
an embedded local child's canvas. Record the *resulting saved state*, not merely
that a UI action occurred.

| User action | What counts |
| --- | --- |
| Move a node | Its final saved canvas position differs. Mark after the move completes, rather than on every drag frame. |
| Add, delete, paste, or replace a node | The saved node set, type, identity, or embedded local content differs. This also covers creating or deleting a nested Macro node. |
| Edit a node parameter or inspector parameter | A user-entered value that is saved and restored differs, including edits from a node widget, Inspector, or Custom GUI control. A reset-to-default action counts only if the resulting value differs. |
| Edit a connection | A user creates, removes, or reconnects a saved connection. The resulting connection set differs. |
| Change a nested Macro node | The selected global macro, local/global mode, or another saved property of that node differs. The parent owns this edit. |
| Edit saved node-specific data | A user changes data stored by a node's `presetSave()` hook, such as curve points, even if no ordinary parameter event fires. |
| Edit a comment | A user adds, removes, moves, resizes, recolors, or changes the text of a saved comment. |
| Edit the macro interface | A user changes routers, their names/types, or their saved order/separators. Node and connection changes may already detect part of this; the order file must also be covered. |
| Edit a Custom GUI or its stored snapshots | A user changes saved panels, widgets, geometry, styling, or captured values, provided the change has not already been persisted automatically. |
| Edit MIDI bindings, when enabled | A user changes a binding saved with this macro. |

Structural changes also include a user's changes to a local child macro's
contents. An action that is undone back to the saved state must not leave a
permanent dirty result.

## Changes that do not count

- Values arriving through connections, including values forwarded through
  routers or portals. A connection itself changing is dirty; data flowing
  through an unchanged connection is not.
- Values written by a node's own update/calculation loop, including output
  values and automatically updated inspector diagnostics.
- Values or graph mutations applied while loading or reloading a preset or
  macro. Suppress dirty signals during load and establish the baseline after
  loading finishes.
- Parameters marked `DisableSavePreset`, because the macro save does not store
  them. A connected input's current value should not be treated as an editable
  saved value: the loader does not restore it while that input is connected.
- Selection, hover, canvas pan/zoom, open windows, and other navigation state.
- Snapshot capture/name/delete when it has already been written immediately to
  the global macro's snapshot file. Do not prompt to save the same change again.

**Proposed initial boundary:** node expanded/collapsed state and ImGui window
layout do not trigger a prompt. These are saved incidentally by current save
paths but are presentation state. Revisit this if the user wants macro layout
edits to be part of the dirty definition.

**Needs review:** a user-triggered snapshot *recall*, MIDI/OSC control, and
automation can change parameter values without being an edit to the macro's
definition. The proposed default is to treat these as live operation, not dirty
signals. Explicit editing of a saved value in a Custom GUI does count.

## Detection design

1. Separate the macro's **saveable definition** from the file-writing action.
   Build a read-only representation of graph structure, positions, eligible
   values, comments, node-specific data, router order, and other included
   persisted data. Do not call the existing save method merely to check dirty
   state: saving has side effects.
2. Identify user edit origins at the interaction boundary. An ordinary
   `ofParameter` change callback cannot by itself distinguish a widget edit
   from a connection or calculation. Record user-edit candidates for value
   changes and structural edits; suppress load and live-operation changes.
3. Before prompting, compare the candidate's current *eligible* state with
   the last saved global definition. A candidate that has been undone becomes
   clean. Check the actual disk state at Save Preset time, or invalidate a
   cached baseline when a file is reloaded or changed externally.
4. Keep `isDirty()` as a query derived from these signals and the comparison.
   A bare boolean set by parameter callbacks is insufficient: it creates
   false positives for live values and false negatives for edits that bypass
   callbacks.
5. Compare parsed/normalized saved data, rather than JSON text formatting.
   Account for missing or removed saved files. Do not use file timestamps as
   the definition of an edit.

The initial implementation can do the final comparison on Save Preset; this
action is infrequent. Cache or optimize only if measurement shows a problem.

## Save Preset review UX

The review applies to **Save Preset** and **Save Preset As**. If no global macro
is dirty, save immediately as today. Otherwise open a persistent modal after
the menu closes. Gather the changes before showing it; merely opening the
review must not write to `Presets/` or `Macros/`.

### One macro at a time

Show the dirty global macros in a stable order, with progress such as
**Macro 2 of 4** and a count of changes. For each macro, show:

- The preset name and bank being saved.
- The global macro's name and resolved `Macros/` folder.
- A readable path from the root canvas through every enclosing local/global
  Macro node, with both the node's visible name and numeric ID where available.
  This disambiguates repeated instances with the same macro name.
- A scrollable, grouped list of **all detected user edits** belonging to this
  macro, including edits inside its embedded local macros. Group by nested
  canvas and node; keep the controls visible when the list is long.
- A clear statement of the effect: **Save this global macro** writes its
  entire current definition to the shown folder; **Skip this macro** leaves
  that folder unchanged. Skipping lasts for this save attempt only.

Each change row should identify the owning canvas, node type/name and numeric
ID, the field or parameter name, and the kind of change. For a nested Macro
node, also show its local name and the old/new referenced global macro paths
when the selected macro changed. Connection rows name both endpoint nodes and
parameters. Node add/delete rows identify the node; movement rows show old and
new coordinates. Comment rows identify the comment by text excerpt and
location. Router, Custom GUI, and MIDI rows identify the relevant item.

Where a value can be represented use **Saved -> Current** (for example,
`Threshold: 0.25 -> 0.40`, `Active: false -> true`, or
`Position: (120, 80) -> (180, 80)`). Use the value stored in the target
global folder as **Saved**, not a value that has since arrived through a
connection. For added/removed fields show **Added** or **Removed**. Show enum
labels, readable colors, and short vectors directly. For long arrays or
structured values, show type, size, a short preview, and an expandable diff.
If a safe value preview is unavailable, say **Changed; value preview
unavailable** and still identify the node, field, and change type. Avoid
flooding the dialog with continuously changing or calculated values.

Example of the information hierarchy (illustrative names and values):

```text
Save Preset: Bank_A / Scene_1             Global macro 1 of 2
Macro: Image_Treatment                    Macros/Effects/Image_Treatment
Used at: Canvas > Local Wrapper [Macro 3] > Image_Treatment [Macro 7]

  Image_Treatment canvas
    Threshold [Node 4] > Level            0.25 -> 0.40
    Blur [Node 8] > Position              (120, 80) -> (180, 80)
    Blur [Node 8] > Radius -> Output      Connection added
  Embedded local macro: Mask [Macro 2]
    Gate [Node 1] > Enabled               false -> true

              Save this macro   Skip this macro   Cancel preset save
```

**Agreed decision:** the user reviews **each change**, then explicitly
confirms **each macro** with **Save this macro** or **Skip this macro**. The
confirmation covers every listed change for that macro. Change rows are
informational; they do not have individual Save/Skip controls. The current
macro save operation writes a complete definition, so the UI must not imply
that a single parameter can be saved independently.

### Flow and exceptional cases

1. Traverse the live macro tree and collect dirty global macro **instances**,
   their readable nesting paths, global save paths, and structured changes.
   Do not present a global child as merely a change inside its global parent.
2. For each dirty macro, show the review and record **Save this macro** or
   **Skip this macro**. **Cancel preset save** exits without writing either
   the preset or a macro; defer writes until all decisions have been made.
3. If two instances reference the same global path with identical current
   definitions, show the locations and ask once. If their definitions differ,
   show both instance paths and a conflict: saving either would overwrite the
   same folder. Require an explicit choice of which instance, or skip both;
   never silently pick the last instance visited.
4. After the decisions, save selected globals through the same complete
   operation as their own **Save** button, then save the root preset once.
   The exact write order and notification timing must ensure that a
   `macroUpdated` broadcast cannot reload or destroy a pending instance.
5. Only successful writes clear the corresponding dirty state. A skipped or
   failed macro remains dirty. Show a save failure with **Retry** or **Stop**;
   a partial write may already have changed the macro folder, so continuing
   to save the preset would give an ambiguous result. If the graph or macro path changes while
   the review is open, refresh the changes and require review of the updated
   list before writing.

The final result should say which global macros were saved and which were
skipped, while making clear whether the preset itself saved successfully.

## Current-code constraints

- `ofxOceanodeContainer::savePreset()` writes `modules.json`,
  `connections.json`, node JSON, comments, optional MIDI, scope data, Custom
  GUIs, and Custom GUI snapshots. Node positions and node membership are in
  `modules.json`.
- `ofxOceanodeNode::savePreset()` writes parameter and inspector values,
  model-specific `presetSave()` data, and `macroSave()` data. Parameter saving
  filters `DisableSavePreset`; loading skips a value with an incoming
  connection.
- A local Macro node saves its internal container under the enclosing preset
  or macro folder. A global Macro node saves a reference. Its current
  `macroSave()` also writes router order and layout into the global folder.
  That incidental write must be removed or deferred so **Skip macro** really
  means no global write.
- The global Macro node's own **Save** button also saves its container,
  layout, snapshots, and router order, then broadcasts `macroUpdated`. The
  preset-triggered save should share this operation rather than reimplement a
  partial version.
- `Save Preset As` currently registers the new preset before calling the save
  method. If the review can be cancelled, defer that registration until the
  review succeeds so cancellation leaves no phantom preset in the menu.
- There is no single container-wide event for every graph edit. Position,
  connection, parameter, comment, and Custom GUI changes currently enter
  through separate code paths. Existing Custom GUI dirty flags cover only
  their own subsystem.

Relevant source: `src/Controls/ofxOceanodePresetsController.cpp`,
`src/Managers/ofxOceanodeContainer.cpp`, `src/Managers/ofxOceanodeCanvas.cpp`,
`src/Nodes/ofxOceanodeNode.cpp`, `src/Nodes/ofxOceanodeNodeMacro.cpp`, and
`src/Nodes/ofxOceanodeNodeMacroGui.cpp`.

## Review cases before implementation

- Move, add/delete, parameter edit, Inspector edit, connection edit, and nested
  Macro selection change: only the owning global macro prompts.
- Change a connected input or calculated output for many frames: no prompt.
- Edit then undo or restore a value to its saved value: no prompt.
- Edit inside a local child of a global macro: parent global prompts.
- Edit inside a global child: child prompts; unchanged parent does not.
- Two instances of one global path with different live contents: conflict is
  visible and no implicit overwrite occurs.
- Save one macro, skip another, then click Save Preset again: only the skipped
  dirty macro prompts.
- Load or reload a preset/macro: no dirty signal from the load itself.
- Save Preset without dirty global macros: current one-click behavior remains.
