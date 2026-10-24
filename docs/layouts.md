# Layouts

The **Layouts** top menu brings together named **Preset layouts** and reusable
**Global layouts**. Both scopes share one layout manager and the same docking mechanism.

## Preset layouts

Arrange your windows for sound, laser, images, or another task, then choose
**Layouts → Save preset layout as…** and enter a name. Select a saved name in
that menu to recall it.

Each snapshot stores docking splits and sizes, window positions, tab order and
selected tabs, controller visibility, node window toggles offered by the Window
menu, macro canvas visibility, and Custom GUI visibility, including inside nested
macros, plus the Scopes window's visibility. It leaves signal values, connections,
transport, and output settings alone. Deprecated persistent-node windows and
utility dialogs (Help, Theme Editor, confirmations) are excluded.
New nodes or panels added after a snapshot keep their current visibility.

Use **Update current preset layout** to replace the selected snapshot with your
current arrangement. **Overwrite preset layout** lists every saved name: pick one
and confirm to replace that snapshot with your current arrangement and select it.
**Rename preset layout…** changes its name;
**Delete preset layout…** removes its saved snapshot and keeps the windows
currently on screen. Rearranging windows or saving the preset does not replace a
snapshot: use Update to keep those changes in the named layout.

The collection and last selected preset layout are saved inside the preset folder.
New saves use `preset_layouts.json` (schema version 2, with a `layouts` collection).
The manager also reads the original `workspaces.json` (version 1) when the new file
does not exist. Saving writes the new file and leaves the old file untouched;
subsequent loads prefer the new file. Invalid current files are reported and never
silently replaced with older data. Changes are saved immediately for
an existing preset; an unsaved patch keeps them in memory until its first Save.
Save As carries the collection into the new preset. **Set default preset layout**
chooses which named layout opens whenever that preset is loaded. Switching layouts
while working does not change this default. Choose **Last selected layout** in
that submenu to return to remembering the last selection. Existing presets keep
that behavior until you choose a default. Renaming a default layout preserves its
selection; deleting it returns to the last selected layout. New starts an empty
collection. The global startup default remains independent.

The GUI arrangement spans all canvas tabs. Focusing or navigating into a macro
keeps the current layout. Preset saves and exit store it in the preset's root
`ImGuiLayout.ini`; macros do not load or save individual GUI layouts.

Divider-arrow folds are saved with the layout, along with the expanded sizes.
Loading restores the folds once the saved windows have joined their docks;
clicking an arrow again recovers the expanded size. This works for ordinary
preset saves, named preset layouts, and global layouts. Older layouts lack fold
metadata: collapse the desired regions and save again to remember their folds.

Every **Save Preset** also captures the current Window-menu visibility in the
preset's `view.json`, independently of named snapshots. Loading applies the named
default's arrangement first, then the visibility from the last preset save.
Explicitly recalling a named layout afterward still restores that snapshot's
visibility. Save As carries the current visibility into the new preset. Exiting
does not overwrite the visibility from the last explicit preset save.

## Global layouts

**Load global layout**, **Save global layout as…**, and **Set default global
layout** are the existing global layout actions, moved from Config. They retain
their `.ini` files in `config/guiLayouts/` and the existing default preference.
These layouts store docking and window geometry; preset layouts also capture
window visibility.

Loading a global layout deselects the active preset layout, keeping all saved
preset snapshots available. To include a global arrangement in the preset, load
it, adjust window visibility, and choose **Save preset layout as…**.

Node window toggles use the same recognition as the Window menu (`Show`, `Show
Win`, `Show Window`, `Show GUI`, `Show Editor`, `Show Snapshot`, `Show All`, `Show
Split`). A window controlled only by private addon state or a differently named
toggle retains its visibility; ImGui restores its saved docking when it opens.

## Code organization

All layout ownership lives in `src/Layouts/ofxOceanodeLayoutManager.h`: global and
preset selection, defaults, persistence, Save As, deferred
application, menus and dialogs. It is header-only so existing host projects do not
need to register another compiled source file.

`ofxOceanodePresetLayouts.h` is the manager's pure preset snapshot data structure,
including naming, default tracking and schema compatibility.
`ofxOceanodeLayoutWindowState.h` adapts node/controller visibility to snapshots.
`ofxOceanode` supplies these adapters and the low-level ImGui docking restoration;
it delegates layout lifecycle and UI to the manager.

`tests/layouts_regression.py` covers both scopes, preset defaults, legacy migration,
shared layout persistence, Save As, and repeated docking restoration using real ImGui.
