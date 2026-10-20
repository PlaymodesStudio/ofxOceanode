# Layouts

The **Layouts** top menu brings together named **Preset layouts** and reusable
**Global layouts**. The former Workspaces menu and Config layout actions now
live here.

## Preset layouts

Arrange your windows for sound, laser, images, or another task, then choose
**Layouts → Save preset layout as…** and enter a name. Select a saved name in
that menu to recall it.

Each snapshot stores docking splits and sizes, window positions, tab order and
selected tabs, controller visibility, node window toggles offered by the Window
menu, macro canvas visibility, and Custom GUI visibility, including inside nested
macros. It leaves signal values, connections, transport, and output settings alone.
New nodes or panels added after a snapshot keep their current visibility.

Use **Update current preset layout** to replace the selected snapshot with your
current arrangement. **Rename preset layout…** changes its name;
**Delete preset layout…** removes its saved snapshot and keeps the windows
currently on screen. Rearranging windows or saving the preset does not replace a
snapshot: use Update to keep those changes in the named layout.

The collection and last selected preset layout are saved inside the preset folder.
Existing `workspaces.json` files remain supported and keep that filename so layouts
saved before the menu rename continue to work. Changes are saved immediately for
an existing preset; an unsaved patch keeps them in memory until its first Save.
Save As carries the collection into the new preset. **Set default preset layout**
chooses which named layout opens whenever that preset is loaded. Switching layouts
while working does not change this default. Choose **Last selected layout** in
that submenu to return to remembering the last selection. Existing presets keep
that behavior until you choose a default. Renaming a default layout preserves its
selection; deleting it returns to the last selected layout. New starts an empty
collection. The global startup default remains independent.

A named preset layout spans all canvas tabs, so focusing a macro does not replace
it with the macro's individual layout.

## Global layouts

**Load global layout**, **Save global layout as…**, and **Set default global
layout** are the existing global layout actions, moved from Config. They retain
their `.ini` files in `config/guiLayouts/` and the existing default preference.
These layouts store docking and window geometry; preset layouts also capture
window visibility.

Loading a global layout deselects the active preset layout, keeping all saved
preset snapshots available. To include a global arrangement in the preset, load
it, adjust window visibility, and choose **Save preset layout as…**.

The **GUI layout changes with macros** preference is also in Layouts. It applies
when no named preset layout is active, including after loading a global layout.

Node window toggles use the same recognition as the Window menu (`Show`, `Show
Win`, `Show Window`, `Show GUI`, `Show Editor`, `Show Snapshot`, `Show All`, `Show
Split`). A window controlled only by private addon state or a differently named
toggle retains its visibility; ImGui restores its saved docking when it opens.
