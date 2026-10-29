# Layouts

The **Layouts** top menu, just after **Window**, brings together named **Preset
layouts** and reusable **Global layouts**. Every layout stores both the docking
arrangement and which windows are shown (the Window menu's choices).

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

Changes are saved immediately for an existing preset; an unsaved patch keeps
them in memory until its first Save. Save As carries the collection into the new
preset. **Set default preset layout** chooses which named layout opens whenever
that preset is loaded. Switching layouts while working does not change this
default. Choose **Last selected layout** in that submenu to return to remembering
the last selection. Renaming a default layout preserves its selection; deleting
it returns to the last selected layout. New starts an empty collection. The
global startup default remains independent.

The GUI arrangement spans all canvas tabs. Focusing or navigating into a macro
keeps the current layout. Macros do not load or save individual GUI layouts.

Divider-arrow folds are saved with the layout, along with the expanded sizes.
Loading restores the folds once the saved windows have joined their docks;
clicking an arrow again recovers the expanded size. This works for ordinary
preset saves, named preset layouts, and global layouts.

Every **Save Preset** also captures the current Window-menu visibility,
independently of named snapshots. Loading applies the named default's
arrangement first, then the visibility from the last preset save. Explicitly
recalling a named layout afterward still restores that snapshot's visibility.
Save As carries the current visibility into the new preset. Exiting stores the
docking but does not overwrite the visibility from the last explicit preset save.

## Global layouts

**Load global layout**, **Save global layout as…**, and **Set default global
layout** keep reusable arrangements outside any preset. Like preset layouts, a
global layout stores docking and window geometry together with the Window menu's
visibility: controllers, Scopes, node windows, Macro canvases and Custom GUIs.
Menus show layout names without an extension; names cannot contain `/`, `\` or `:`.

The default global layout is applied when Oceanode starts, before any preset is
loaded, and again by **Presets → New Preset**, so it decides which controllers are
shown at startup and in every new patch. To choose them,
arrange and show/hide the windows, choose **Save global layout as…**, then select
it in **Set default global layout**. Loading a preset afterwards applies that
preset's own layout and visibility.

Loading a global layout deselects the active preset layout, keeping all saved
preset snapshots available. To include a global arrangement in the preset, load
it, adjust window visibility, and choose **Save preset layout as…**.

Node window toggles use the same recognition as the Window menu (`Show`, `Show
Win`, `Show Window`, `Show GUI`, `Show Editor`, `Show Snapshot`, `Show All`, `Show
Split`). A window controlled only by private addon state or a differently named
toggle retains its visibility; ImGui restores its saved docking when it opens.

## Files

All paths are relative to the data folder. Each JSON file has an integer
`version`; a file with another version is reported and ignored.

| File | Owner | Contents |
|---|---|---|
| `<preset>/layout.json` | Layouts | `current` (`ini`: the ImGui docking text, `view`: visibility from the last preset save, or null), `layouts` (named snapshots, each `{ini, view}`), `active`, `default` |
| `config/guiLayouts/<name>.json` | Layouts | One global layout: `{version, ini, view}`; `view` may be null (docking only) |
| `config/guiLayouts.json` | Layouts | `{version, default}`: the global startup layout |
| `config/themes/<name>.json` | ThemeManager | One theme: `fontFamily`, `colors`, `oceanodeColors` |
| `config/themes.json` | ThemeManager | `{version, default}`: the theme loaded at startup |
| `config/config.json` | AppConfig | Version 2: the Config menu's Rendering and Editor settings |

In Theme Editor > ImGui Colors, `TabSelectedText` controls the label of the tab
whose contents are visible; `TabText` controls the other tab labels. Selection
is independent of window focus. These apply to docked panels and regular tab
bars, are saved with themes, and reset to `Text` / `TextDisabled` respectively.
Older themes without these entries inherit their own `Text` / `TextDisabled`
palette when loaded. This requires the matching ofxImGui extension that adds
`ImGuiCol_TabSelectedText` and `ImGuiCol_TabText` to its bundled ImGui sources;
rebuild the host application and ofxImGui together after updating.

`layout.json` is written whole on Save Preset, Save As, every change to the
collection, the Scopes autosave and exit. It is never written for an unsaved
patch, and never while the preset's file is invalid: the Layouts menu shows the
error until a preset is loaded again, so a broken file is never replaced.

**Save Config** writes every Config setting; **Load Config** applies the keys
present. The configuration is loaded automatically when Oceanode starts, so host
apps do not call a load function. Canvas Zoom is not saved.

## Changes from earlier versions

- Config is loaded automatically in `setup()`; hosts no longer call `loadConfig()`.
- The V Sync checkbox shows and saves the real state.
- The Config section "GUI" is now "Editor".
- Global layouts are `config/guiLayouts/<name>.json` files that also store window
  visibility; menus show their names without an extension.
- `config/view.json` no longer exists. Startup visibility comes from the default
  global layout, then from the preset that loads.
- The Layouts menu sits after Window.
- Old files (`ImGuiLayout.ini`, `view.json`, `preset_layouts.json`, `workspaces.json`,
  `config/guiLayouts/*.ini`, `config/themes/defaultTheme`, `defaultLayout` and `fps`
  in `config.json`) are not read at runtime. The migration gives the default global
  layout the controller visibility of the old `config/view.json` when it is present. Convert a data folder once with:

  ```bash
  python3 scripts/migrate_gui_files.py --data <path/to/bin/data>
  ```

  This is a dry run that prints a report. Add `--apply` to convert; changed and
  deleted files are first copied to `gui_migration_backup_<date>-<time>/` next to
  the data folder. Apps built before this change do not find layouts in a
  migrated folder.
- Controllers missing from a layout dock beside **Nodes**; layouts saved before
  the Nodes window existed no longer anchor to MiniMap or Presets.

## Code organization

The GUI shell lives in `src/Gui/`. Four owners each own their files on disk;
no file has two writers.

- `ofxOceanodeWindowManager` hosts the dockspace window and its docking IDs,
  applies and captures ImGui ini text, cycles docked tabs, owns show mode, and
  captures/restores window visibility. It draws the Window menu. One walker over
  node windows serves both the menu and captured views. It reads and writes no
  files.
- `ofxOceanodeLayouts` owns preset and global layouts: `layout.json`,
  `config/guiLayouts/*.json`, `config/guiLayouts.json`, the Layouts menu and its dialogs.
  `ofxOceanodePresetLayouts` in the same header is the named-snapshot collection.
  It calls WindowManager to apply or capture docking and visibility. The
  Container requests docking saves through
  `ofxOceanodeShared::getDockingSaveRequestEvent()` and never writes ini files.
- `ofxOceanodeThemeManager` owns the theme editor, fonts, Alt-pick colour lookup
  and `config/themes*`.
- `ofxOceanodeAppConfig` owns `config/config.json` and the Config menu's
  Rendering, Editor and Configuration sections.

The facade `ofxOceanode` creates them, calls them in a fixed frame order and
draws the menu bar (`drawMenuBar()`). The docking helpers
(`ofxOceanodeProportionalDocking`, `ofxOceanodeDockCollapse`,
`ofxOceanodeScopeProportions`, `ofxOceanodeFloatingWindowDecor`) and the theme
helpers (`ofxOceanodeTheme`, `ofxOceanodeThemeColorPicker`) sit beside them.
Host Xcode projects must be regenerated once with the Project Generator to pick
up the new `.cpp` files.

`tests/layouts_regression.py` covers both scopes, preset defaults, `layout.json`
persistence, invalid files, Save As and repeated docking restoration using real
ImGui. `tests/window_manager_regression.py` covers the window walker,
`tests/app_config_regression.py` the config file, and
`tests/migrate_gui_files_regression.py` the migration script.
