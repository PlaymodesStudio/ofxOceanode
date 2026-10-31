# GUI shell refactor: implementation plan

This plan is written for a coding agent. Follow it literally. If something in the
code contradicts this plan, stop and ask; do not improvise a different design.

## 1. Goal

Move window management, layouts, themes and app configuration out of the
`ofxOceanode` facade into four classes, each a `.h/.cpp` pair in `src/Gui/`.
Each class owns its own files on disk, and no file has two writers.
Old on-disk formats are converted once by a migration script. The runtime keeps
no compatibility readers.

User-visible behaviour stays the same, except for the changes listed in §12.

## 2. Preconditions

1. `git status` must be clean. When this plan was written, the tree had uncommitted
   work in `src/ofxOceanodeDockCollapse.h`, `src/ofxOceanodeProportionalDocking.h`,
   `src/ofxOceanode.cpp`, `docs/layouts.md` and two tests. If the tree is not clean,
   stop and ask the user to commit first.
2. Create branch `gui-shell-refactor` from `megabranch26_A`.
3. Read `CLAUDE.md`, `docs/layouts.md` and `docs/help-and-windows.md` before starting.
4. Commit once per phase (§11), with the message `GUI shell refactor phase N: <title>`.

## 3. Target structure

```
src/Gui/
  ofxOceanodeWindowManager.h/.cpp      dockspace host window, docking, dock IDs, applying an
                                       ImGui ini string, tab cycling, show mode, the window
                                       list, capturing/restoring visibility, the Window menu
  ofxOceanodeLayouts.h/.cpp            global + preset layouts, layout.json, guiLayouts.json,
                                       Layouts menu and popups
  ofxOceanodeThemeManager.h/.cpp       theme editor, fonts, Alt-pick, theme save/load,
                                       themes.json, Theme menu section and popups
  ofxOceanodeAppConfig.h/.cpp          config/config.json, Config menu "Rendering" and "Editor"
                                       sections, Save/Load Config buttons
  ofxOceanodeProportionalDocking.h/.cpp  (moved from src/, split)
  ofxOceanodeDockCollapse.h/.cpp         (moved from src/, split)
  ofxOceanodeScopeProportions.h/.cpp     (moved from src/, split)
  ofxOceanodeFloatingWindowDecor.h/.cpp  (moved from src/, split)
  ofxOceanodeTheme.h/.cpp                (moved from src/, split; class OceanodeTheme)
  ofxOceanodeThemeColorPicker.h/.cpp     (moved from src/, split)
```

Deleted at the end: `src/Layouts/` (all three files), `src/ofxOceanodeWindowVisibility.h`.

### Header/source rule

Every file in `src/Gui/` is a `.h/.cpp` pair. Headers contain declarations, member
initialisers and one-line accessors only. Every function body longer than one statement
goes in the `.cpp`. Templates are not allowed in these classes.

When splitting a moved helper, keep its class name, members and behaviour. Only move
function bodies into the `.cpp` (`ClassName::method`). Static data members such as
`ofxOceanodeFloatingWindowDecor::Enabled` are defined in the `.cpp`.

No change to `addon_config.mk` is needed: OF Makefiles and the Project Generator
pick up `src/**/*.cpp`. Host Xcode projects must be regenerated once (§8).

### Dependency rules (enforced by the acceptance checks in §14)

| Class | May include / call | Must not |
|---|---|---|
| WindowManager | Controls, Container, NodeMacro, Scope, CustomGuiExtension, Shared, ProportionalDocking, FloatingWindowDecor, ofxOceanodeMenu | Read or write any file. Include Layouts, ThemeManager or AppConfig. |
| Layouts | WindowManager, Shared, `PresetSaveCleanup.h` (`oceanodePresetSave::saveJson`), ofxOceanodeMenu | Touch `config/config.json`, themes, controller internals |
| ThemeManager | Canvas (`setupFonts`), OceanodeTheme, ThemeColorPicker, FloatingWindowDecor, OceanodeColors | Touch layouts or config.json |
| AppConfig | Canvas, Shared, TimeController (via Controls), WindowManager (`setShowMode`/`isShowMode`) | Touch layouts, themes, window visibility |
| Container | Shared events | Call any `ImGui::*IniSettings*` |
| Help | — | Draw the Window menu |

Dock IDs stay readable through `ofxOceanodeShared::getDockspaceID/getCentralNodeID/getLeftNodeID`,
because Canvas, Controls and macros read them. WindowManager becomes the only caller of the
matching setters.

## 4. On-disk formats

All paths are relative to the OF data folder. Every JSON file carries an integer `version`.
A reader that finds a different version logs `ofLogError` and treats the file as absent.
Files are written with `ofSavePrettyJson` (config/theme files) or
`oceanodePresetSave::saveJson` (preset folder files).

### 4.1 `<presetFolder>/layout.json` (owner: Layouts)

This file replaces `ImGuiLayout.ini`, `view.json`, `preset_layouts.json` and `workspaces.json`.

```json
{
  "version": 1,
  "current": { "ini": "<ImGui ini text>", "view": View | null },
  "layouts": { "<name>": { "ini": "<non-empty ImGui ini text>", "view": View } },
  "active":  "<name or empty>",
  "default": "<name or empty>"
}
```

`View` is exactly what `oceanodeLayouts::captureWindows` produces today:
`{ "controllers": {name: bool}, "windows": {...tree...}, "scopes": bool }`.
The `windows` tree keeps its current shape: `nodes` keyed by `["<nodeName>",<numId>]`,
each with `parameters`, `inspector`, and for macros `open` and `children`. Custom GUI
state stays under the keys written by `ofxOceanodeCustomGuiExtension::windowState`.

Validation on read: the file is invalid if any of these is false:
- `version == 1`
- `layouts` is an object
- every entry name passes `ofxOceanodePresetLayouts::validName`
- every entry has `ini` as a non-empty string
- every entry has `view` as an object containing a `controllers` object and a `windows` object
- `current` is absent or an object, `current.ini` is absent or a string, and `current.view` is absent, null or an object of the same shape

An `active` or `default` naming a missing entry is reset to `""`, as today.
If the file is invalid: set `readFailed = true`, show the error in the Layouts menu (as
`layoutError` does today), and **never write `layout.json` for that preset** until a preset is
loaded again.

Macros never read or write `layout.json`, which is unchanged from today.

### 4.2 `config/guiLayouts/<name>.ini` and `config/guiLayouts.json` (owner: Layouts)

- Global layouts stay as `<name>.ini` files in `config/guiLayouts/`.
- A layout's name is the file name without `.ini`, and menus show that name.
- `config/guiLayouts.json` contains `{ "version": 1, "default": "<name or empty>" }`. It sits
  next to the folder, not inside it.
- Global layout names must pass `validName` and contain no `/`, `\` or `:`.

### 4.3 `config/themes/<name>.json` and `config/themes.json` (owner: ThemeManager)

- The theme file format is unchanged: `fontFamily`, `colors`, `oceanodeColors`.
- `config/themes.json` contains `{ "version": 1, "default": "<name or empty>" }` and replaces
  the pointer file `config/themes/defaultTheme`.
- Theme names follow the same rule as global layout names.
- Font discovery still scans `config/font/`.

### 4.4 `config/config.json` (owner: AppConfig)

```json
{
  "version": 2,
  "desiredFPS": 60, "vsync": false, "configurationFlags": 0, "showMode": false,
  "colorTexturePrecision": 32, "nodeTextWidth": 90, "nodeWidgetWidth": 150,
  "gridDivisions": 8, "snapToGrid": true, "autoInspectorShowHide": true,
  "selectionCentersCanvas": false, "connectionWidth": 3.0, "spaceTogglesPlay": false
}
```

The values above are examples. The rules:
- **Save** writes all 13 keys plus `version`. Each value is read from its live target (§5.4).
- **Load** applies only the keys that are present. A missing key leaves its target unchanged, so
  defaults remain whatever the target initialised itself to.
- There is no `defaultLayout` and no `fps` key.
- `vsync` becomes a real `AppConfig` member. It fixes today's bug where the checkbox was a
  local `false` on every frame.

### 4.5 Files that disappear

`config/view.json`, `config/themes/defaultTheme`, and per preset `ImGuiLayout.ini`,
`view.json`, `preset_layouts.json` and `workspaces.json`. Only the migration script reads them.

## 5. Class APIs

Names and signatures are binding. Private members are up to you.

### 5.1 `ofxOceanodeWindowManager`

```cpp
class ofxOceanodeWindowManager {
public:
    ofxOceanodeWindowManager();
    ~ofxOceanodeWindowManager();
    void setup(std::shared_ptr<ofxOceanodeContainer> root, ofxOceanodeControls& controls);
    void installImGuiHandlers();          // after gui.setup(): DockCollapse settings handler + FloatingWindowDecor::install()

    // Frame. The caller draws the menu bar and popups between begin/endHostWindow.
    void beginHostWindow();               // reconcile pending controller docking, Begin("DockSpace") with menu bar,
                                          // proportional DockSpace, default left split if missing, setDockspaceID
    void endHostWindow();                 // ImGui::End() of the host, then refresh left/central dock IDs in Shared
    void drawShowModeWindow();            // moved from ofxOceanode::drawShowModeWindow
    void endFrame();                      // cycleDockedTabs() + fold handles (call just before gui.end())

    bool isShowMode() const;
    void setShowMode(bool);
    bool& showModeRef();                  // for the Config checkbox

    // Layout application (called by Layouts only)
    void applyIni(const std::string& iniText); // body of today's ofxOceanode::applyGUILayout
    void clearIni();                            // ImGui::ClearIniSettings()
    std::string captureIni() const;             // ImGui::SaveIniSettingsToMemory

    // Window visibility (called by Layouts only)
    ofJson captureView();                      // today's oceanodeLayouts::captureWindows
    void restoreView(const ofJson& view);      // today's oceanodeLayouts::restoreWindows

    void drawWindowMenu();                     // today's ofxOceanodeHelp::drawWindowMenu, unchanged UI

    static bool isWindowToggle(const std::string& parameterName); // from ofxOceanodeWindowVisibility.h
};
```

**One window walker.** The `.cpp` has one private recursive function that visits every node
window. It provides the node, its model, its menu label path (`"Macro_1 / Phasor_1"`), its key
(`nodeKey`), each window toggle (`ofParameter<bool>*`) from `getParameters()` and
`getInspectorParameters()`, and macros (with their child container). Both `drawWindowMenu`
and `captureView/restoreView` use it.

Preserve today's difference between them:
- `captureView/restoreView` skip nodes where `node->getIsPersistent()` is true.
- `drawWindowMenu` lists them.

Custom GUI entries keep using `extension->windowEntries(path)` and
`extension->windowState(state, restore)`. **Do not change `ofxOceanodeCustomGuiExtension`**,
because the `ofxOceanodeCustomGui` addon implements it.

Moved from the facade into WindowManager unchanged:
- `applyGUILayout`, `resolveControllerDockID`, `reconcileMissingControllerWindows`, and their state (`loadedGUILayoutData`, `loadedGUILayoutLeftDockId`, `pendingControllerDockReconciliation`)
- `cycleDockedTabs`
- the dockspace part of `ShowExampleAppDockSpace` (window flags, style vars, DockGutter colour, split creation)
- `proportionalDocking`

Remove the "MiniMap"/"Presets" anchor fallback in `applyIni`. Keep the "Nodes" anchor and the
loop over controllers.

### 5.2 `ofxOceanodeLayouts`

```cpp
class ofxOceanodeLayouts {
public:
    explicit ofxOceanodeLayouts(ofxOceanodeWindowManager& windows);
    void setup();                          // loads config/guiLayouts.json and queues the default global layout
    void onPresetLoaded();                 // reads <preset>/layout.json, queues application (§6.1)
    void onPresetSaved();                  // writes the whole layout.json, current.view = live view (§6.2)
    void saveDocking(const std::string& presetPath); // writes layout.json with live ini, keeps current.view (§6.2)
    void beforeFrame();                    // applies queued ini / snapshot / view (before ImGui::NewFrame)
    void drawMenu();                       // Layouts menu, same items as today
    void drawPopups();
};
```

- `ofxOceanodePresetLayouts` (the data class from `ofxOceanodePresetLayouts.h`) moves into
  `ofxOceanodeLayouts.h` with the same methods.
- Its `toJson/fromJson` change to the §4.1 shape: entries become `{ini, view}`, and the
  `workspaces` and version-1 handling is removed.
- Its function bodies go in the `.cpp`.
- `Callbacks` is deleted, and Layouts calls `windows.*` directly.

### 5.3 `ofxOceanodeThemeManager`

```cpp
class ofxOceanodeThemeManager {
public:
    void setup(ofxOceanodeCanvas& canvas); // refreshThemeFonts(); load default from config/themes.json
    void beforeFrame();                    // today's applyThemeFont() (font atlas changes before NewFrame)
    void afterRender();                    // today's pickThemeColorUnderMouse()
    void drawMenuSection();                // the Theme section of the Config menu, same items as today
    void drawPopups();                     // "Save Theme##modal"
    void drawWindows();                    // editor window + pick tooltip when open
    void save(const std::string& name);
    void load(const std::string& name);
};
```

Every theme member and method moves here from the facade:
- `showThemeEditor`, `showThemeColorSearch`, `themeColorSearch`
- `themeColorPick*`, `themeEditorHovered`, `themeSelectedColor`, `themeColorRevealPending`
- `ThemeFontFamily`, `themeFontFamilies`, `themeFontFamily`, `themeFontWarning`, `pendingThemeFontChange`
- `currentThemeName`, `openSaveThemePopup`, `themeNameBuf`
- `refreshThemeFonts`, `applyThemeFont`, `drawThemeEditorWindow`, `selectThemeColor`, `pickThemeColorUnderMouse`, `drawThemeColorPickTooltip`, `drawThemeColorPickLayers`
- the anonymous-namespace FreeType helper above `refreshThemeFonts`

The theme list in the menu lists `config/themes/*.json`. "Set default theme" writes `config/themes.json`.

### 5.4 `ofxOceanodeAppConfig`

```cpp
class ofxOceanodeAppConfig {
public:
    void setup(ofxOceanodeCanvas& canvas, ofxOceanodeControls& controls, ofxOceanodeWindowManager& windows);
    void load();               // config/config.json -> targets (§4.4), also called by setup()
    void save();               // targets -> config/config.json
    void drawMenuSections();   // Config menu sections "Rendering" and "Editor"
    void drawConfigButtons();  // Config menu section "Configuration": Save Config / Load Config
};
```

Where each key lives:

| Key | Target |
|---|---|
| `desiredFPS` | AppConfig member, applied with `ofSetFrameRate` |
| `vsync` | AppConfig member, applied with `ofSetVerticalSync` |
| `configurationFlags`, `colorTexturePrecision`, `snapToGrid` (+ `canvas.setSnapToGrid`), `gridDivisions` (+ `canvas.setGridDivisions`, `canvas.updateGridSize`, `Shared::setSnapGridDiv`), `autoInspectorShowHide`, `selectionCentersCanvas`, `connectionWidth` | `ofxOceanodeShared` / canvas, as today |
| `nodeTextWidth`, `nodeWidgetWidth` | canvas |
| `showMode` | `windows.setShowMode` |
| `spaceTogglesPlay` | `controls.get<ofxOceanodeTimeController>()` |

Menu labels and widgets are unchanged. The section today called "GUI" is renamed "Editor".
Canvas Zoom stays in it and is still not persisted.

## 6. Exact runtime behaviour of Layouts

### 6.1 Preset load (`onPresetLoaded`, only for `ofxOceanodePresetLoadType_FullPreset`)

1. Clear all pending state, the collection, `layoutError` and `readFailed`.
2. If the preset path is empty, stop.
3. Read `<preset>/layout.json`. If it is absent, stop. If it is invalid, see §4.1.
4. `pendingIni = current.ini`, if it is non-empty.
5. `pendingSnapshot = collection.layoutOnLoad()` (the default if it exists, otherwise active).
6. `savedView = current.view` (it may be null), and `pendingView = savedView`.

In `beforeFrame()`, in this order:
1. If `pendingIni` is set: `windows.applyIni(pendingIni)`.
2. If `pendingSnapshot` is set, recall it:
   - `windows.restoreView(entry.view)`
   - `windows.clearIni()`
   - `windows.applyIni(entry.ini)`
   - set `active`, and write the file if `active` changed
3. If `pendingView` is set: `windows.restoreView(pendingView)`.

This is the same order as today.

### 6.2 Writes

Every write of `layout.json` writes the whole file:
- `current.ini = windows.captureIni()`
- `current.view = savedView`
- the collection

Writes happen on:

| Trigger | What changes before writing |
|---|---|
| `onPresetSaved` (Save, Save As) | First `savedView = windows.captureView()` |
| Collection edits (create, overwrite, rename, delete, set default, `active` change, choosing a global layout while a snapshot is active) | Nothing extra |
| `saveDocking(path)` | Nothing extra. Called from `ofxOceanode::exit()` and from the docking-save event (§6.3). |

No write happens when:
- the preset path is empty (an unsaved patch keeps everything in memory until its first Save)
- `readFailed` is set
- `path` differs from the current preset path, for `saveDocking`

### 6.3 Removing the Container's ini writer

1. In `ofxOceanodeContainer::flushPendingScopeSave`, delete the `ImGui::SaveIniSettingsToDisk` block.
2. In its place, notify a new event: `ofxOceanodeShared::getDockingSaveRequestEvent()` (type `ofEvent<const std::string>`, payload `presetPath`).
3. Add the event to `ofxOceanodeShared` next to `getPresetWasSavedEvent()`.
4. Layouts subscribes to it in `setup()` and calls `saveDocking(path)`.

### 6.4 Global layouts

- **Load global layout:**
  - if a snapshot is active, clear `active` and write the file (as today)
  - drop any pending snapshot
  - `pendingIni = <file text>`
  - `globalLayoutName = name`
- **Save global layout as…:** writes `config/guiLayouts/<name>.ini` from `windows.captureIni()`.
- **Set default global layout:** writes `config/guiLayouts.json`.

## 7. Facade after the refactor

### 7.1 Members

Add these members to `ofxOceanode`:

```cpp
std::unique_ptr<ofxOceanodeWindowManager> windows;
std::unique_ptr<ofxOceanodeLayouts> layouts;
std::unique_ptr<ofxOceanodeThemeManager> theme;
std::unique_ptr<ofxOceanodeAppConfig> appConfig;
```

Remove from the facade:
- every theme member
- `proportionalDocking`
- `loadedGUILayout*` and `pendingControllerDockReconciliation`
- `savedViewConfig`, `showMode`, `snapToGrid`, `desiredFPS`

### 7.2 Public API

Remove these public methods:
- `ShowExampleAppDockSpace`, `cycleDockedTabs`
- `setShowMode`, `toggleShowMode`
- `saveConfig`, `loadConfig`
- `saveViewConfig`, `loadViewConfig`
- `loadDefaultGUILayout`
- `saveTheme`, `loadTheme`

None of them is used by sibling addons. Host apps use only `loadConfig` (§8).

Everything else in the public API stays.

### 7.3 `setup()` order

```
container, canvas, Shared root, scope->setup(), controls, help       (unchanged)
gui.setup(new OceanodeTheme(), false, Docking|Viewports, false)       (unchanged)
windows->setup(container, *controls); windows->installImGuiHandlers();
ofxOceanodeClipboard::install();
theme->setup(canvas);
layouts->setup();
presetLoadedListener / presetSavedListener                           (unchanged bodies, now call layouts->)
ofxOceanodeShared::readMacros();
oceanodeTime->setup(...)
appConfig->setup(canvas, *controls, *windows);   // loads config.json; replaces hosts' loadConfig()
```

`appConfig->setup` runs last because it applies `spaceTogglesPlay` to the Time controller.

### 7.4 `draw()` order

```
theme->beforeFrame(); layouts->beforeFrame(); preset canvas centring (unchanged);
gui.begin(); PopupBg = WindowBg (unchanged);
windows->beginHostWindow();
  drawMenuBar();
  layouts->drawPopups(); controls->drawPopups(); CleanupReview::instance().draw();
  drawUnusedNodesWindow(); help->draw(); quick-tips popup; theme->drawPopups();
  ofxOceanodeContainer::drawPendingConnectionErrorDialog(); ofxOceanodeMacroMixer::drawPendingBlockedNodeDialog();
windows->endHostWindow();
if(windows->isShowMode()){ windows->drawShowModeWindow(); container->draw(); }
else { scope->draw(); container->draw(); controls->draw(); canvas.draw(); }
theme->drawWindows();
config/utility item draw callbacks (unchanged)
windows->endFrame();
gui.end(); gui.draw(); theme->afterRender();
```

### 7.5 `drawMenuBar()`

A private facade method in `ofxOceanode.cpp`. It holds today's menu bar code with the inline
sections replaced by owner calls.

Order: Presets → Layouts (`layouts->drawMenu()`) → Edit (unchanged, stays in the facade) →
`controls->drawMenus("Presets")` → Config → Window (`windows->drawWindowMenu()`) → Help.

Inside Config:
1. Extensions (unchanged)
2. `appConfig->drawMenuSections()`, which draws Rendering + Editor
3. `theme->drawMenuSection()`
4. `appConfig->drawConfigButtons()`, which draws the Configuration Save/Load buttons

The final visual order is Extensions, Rendering, Editor, Theme, Configuration.

The Help menu is unchanged. The ImGui Metrics toggle moves from a function-local `static` to a
facade member.

### 7.6 Remaining in the facade

These stay in the facade (out of scope):
- registration templates, OSC, `loadPreset`
- unused-nodes window, user manual, quick tips
- `disableRenderAll` / `disableRenderHistograms`

## 8. Host apps

Delete the line `oceanode.loadConfig();` in each of these files (it is the only facade call
being removed that apps use):

```
apps/ASTRES/oceanodeASTRES/src/ofApp.cpp
apps/BLOOBS/BLOOBS_MACAU/src/ofApp.cpp
apps/ESPILLS/AUDITORI_GIRONA_2026/src/ofApp.cpp
apps/ESPILLS/ESPILLS/src/ofApp.cpp
apps/OCEANODE/OCEANODE_2026/src/ofApp.cpp
apps/SIGNES/Lightnet/src/ofApp.cpp
apps/SIGNES/SIGNES_Morera/src/ofApp.cpp
apps/SIGNES/SingesUNNA/src/ofApp.cpp
apps/SILO/SPIN/src/ofApp.cpp
apps/VORTEX/VORTEX/src/ofApp.cpp
```

The paths are relative to the OF root, which is `../../` from this addon.

- These apps live in other repositories. Do not commit there. List the edited files in your final report.
- Tell the user to regenerate each app's Xcode project with the Project Generator, because there are new `.cpp` files.

## 9. Migration script: `scripts/migrate_gui_files.py`

### 9.1 Usage

`python3 scripts/migrate_gui_files.py --data <dataDir> [--apply]`

- Python 3 standard library only.
- Without `--apply` it is a dry run: it prints the report and touches nothing.
- With `--apply`:
  1. Copy every file it will change or delete into `<dataDir>/../gui_migration_backup_<YYYYmmdd-HHMMSS>/`, keeping relative paths. This is the sibling of `data`, outside it.
  2. Write the new files.
  3. Delete the old files.
  4. Print the report.
- The script is idempotent. On a second run it finds nothing to do.
- Exit code: `0` on success, `1` on any error. On an error, abort before writing anything for that unit (one preset folder, or the config folder).

### 9.2 Preset folders

A preset folder is any directory under `<data>/Presets/` that contains at least one of
`ImGuiLayout.ini`, `view.json`, `preset_layouts.json` or `workspaces.json`.

For each preset folder:

1. **If `layout.json` already exists:** report `skip (already migrated)` and do not delete
   the old files. Print them as a warning.
2. **Build `current`:**
   - `ini` = text of `ImGuiLayout.ini`, or `""` if it is absent.
   - `view` = contents of `view.json` without its `version` key, or `null` if absent.
   - Omit `current` entirely if both are absent.
   - If `view.json` exists but its `version != 1` or it has no `controllers` object: error.
3. **Build the collection:**
   - Read `preset_layouts.json` (it must have `version` 2 and use the `layouts` key).
   - Otherwise read `workspaces.json` (it must have `version` 1 and use the `workspaces` key).
   - Otherwise use an empty collection.
4. **Convert each snapshot** `{layout, controllers, windows, scopes?}` to
   `{ini: layout, view: {controllers, windows, scopes?}}`. Keep `scopes` only if it was present.
5. **Copy `active` and `default`.** Reset either one to `""` if it names a missing entry.
6. **Write `layout.json`** with `version: 1`, using `json.dump(indent=4, ensure_ascii=False)`.
   Then delete the four old files.

### 9.3 Config folder (`<data>/config/`)

- **`config.json`** (only if `version` is absent or not 2):
  - Take `defaultLayout` out. Strip a trailing `.ini` and write the name to `config/guiLayouts.json` as `{version: 1, default}`, but only if that `.ini` file exists. Otherwise report it and use `""`.
  - If `desiredFPS` is absent and `fps` is present, set `desiredFPS = int(fps)`.
  - Drop `fps`. Keep the 13 keys of §4.4 that are present, drop any other key and report it, and set `version: 2`.
- **`themes/defaultTheme`:** read `{"theme": name}` and write `config/themes.json` `{version: 1, default: name}`, but only if `themes/<name>.json` exists. Otherwise use `""`. Delete the pointer file.
- **`view.json`:** delete it and report "global view config dropped; per-preset visibility is in layout.json".
- Do not touch anything else in `config/`: `font/`, `help-assistant.json`, `requesty.md`, theme files and `.ini` files.

### 9.4 Report

Print one line per unit (`migrated`, `skipped` or `error`) and a summary of counts.

### 9.5 Real data

The user's data folder is:

```
/Users/eloimaduell/PM Dropbox/PLAYMODES_STUDIO/2026/BLOOBS_MACAU/SHARED_DATA/data
```

When this plan was written it contained:
- 45 `ImGuiLayout.ini`, 5 `view.json`, 11 `preset_layouts.json` (all with empty collections), and 1 `workspaces.json` (`Presets/BLOOBS_MACAU/8--BLOOBS_6`)
- `config/config.json` with `defaultLayout: "init_4.ini"`
- `config/view.json`
- `config/themes/defaultTheme` → `remake_darkie_2`
- 4 global `.ini` files

**Run only the dry run on this folder.** Paste its report to the user. Run `--apply` only after
the user explicitly confirms. The folder is synced by Dropbox and may be shared by several
apps: an app built before this refactor will not find its layouts after migration.

## 10. Tests

Use the conventions in `CLAUDE.md`: Python harnesses, `clang++ -std=c++17`, ASan/UBSan, and
text anchors into `src/`. After every phase, all of these must pass:

```bash
for t in tests/*_regression.py; do python3 "$t" || echo "FAIL $t"; done
```

```bash
python3 tests/integration_compile.py
```

```bash
python3 tests/integration_compile.py --midi-osc
```

### 10.1 Tests to update

| Test | Required change |
|---|---|
| `dock_collapse_regression.py`, `proportional_docking_regression.py` | Include from `src/Gui/`, add `-I src/Gui`, and compile `src/Gui/ofxOceanodeProportionalDocking.cpp`, `ofxOceanodeDockCollapse.cpp` and `ofxOceanodeFloatingWindowDecor.cpp` with the harness. Assertions unchanged. |
| `scope_docking_regression.py` | Same, plus `ofxOceanodeScopeProportions.cpp`. Anchors into `ofxOceanodeScope.cpp` unchanged. |
| `theme_color_picker_regression.py` | Anchors move from `ofxOceanode.cpp` to `src/Gui/ofxOceanodeThemeManager.cpp` (`ofxOceanodeThemeManager::afterRender`, `::pickThemeColorUnderMouse`). Compile `ofxOceanodeThemeColorPicker.cpp`. |
| `layouts_regression.py` | Rewrite it against `ofxOceanodeLayouts.cpp` and `ofxOceanodeWindowManager.cpp`. Keep every scenario. Replace file assertions with `layout.json` equivalents. Delete the `workspaces.json` migration scenario (it moves to the script test). Add: an invalid `layout.json` blocks every write; `saveDocking` keeps `current.view`; the docking-save event writes `current.ini`. |
| `new_preset_regression.py` | `block(layouts, ...)` anchors point at `ofxOceanodeLayouts::onPresetLoaded/onPresetSaved/saveDocking`. Replace `savedLayoutPath=="FreshPreset/ImGuiLayout.ini"` with an assertion that the docking-save event fired with `"FreshPreset"`. |
| `preset_view_center_regression.py` | Anchors `void ofxOceanode::draw(`, `gui.begin(`, `if(presetCanvasCenterPending` stay. Replace `layouts->beforeFrame(` with the new call text if it changed (keep the name `layouts->beforeFrame();`). |
| `inspector_canvas_selection_regression.py` | `config.at("selectionCentersCanvas")` now reads from `ofxOceanodeAppConfig::save` output. Update the anchor to `ofxOceanodeAppConfig.cpp`. |
| `unused_nodes_window_regression.py` | Anchor `void ofxOceanode::drawUnusedNodesWindow(` unchanged. Only fix it if the surrounding code moved. |

### 10.2 New tests

- **`tests/migrate_gui_files_regression.py`.** Build a temporary data folder covering:
  - a preset with all four old files
  - a preset with only `ImGuiLayout.ini`
  - a preset with `workspaces.json` v1 containing one entry
  - a preset that already has `layout.json`
  - a `config/` with `defaultLayout`, `fps`, `view.json` and `defaultTheme`

  Assert:
  - the dry run changes nothing (compare file hashes)
  - `--apply` produces the exact JSON of §4 and creates the backup folder
  - a second `--apply` is a no-op
  - a malformed `view.json` aborts that preset only
- **`tests/app_config_regression.py`.** `save()` then `load()` round-trips all 13 keys, a missing key leaves its target unchanged, and `version != 2` is ignored.
- **`tests/window_manager_regression.py`.** Using real ImGui, the same pattern as `layouts_regression.py`:
  - `captureView` skips persistent nodes but `drawWindowMenu` lists them
  - `restoreView` with a missing node key leaves it unchanged
  - `isWindowToggle` gives the same results as the old header for every label in `docs/layouts.md`

## 11. Phases

Each phase must build and pass §10 before you commit it.

1. **Move docking helpers.** `git mv` the six helper headers to `src/Gui/`, split them into `.h/.cpp`, and fix includes (`#include "ofxOceanodeProportionalDocking.h"` keeps working through include paths). Update the docking/scope/theme-picker tests. No behaviour change.
2. **WindowManager.** Create the class and move code into it (§5.1, §7.4, §7.5 for the Window menu). Move the Window menu out of Help: delete `ofxOceanodeHelp::drawWindowMenu` and its declaration. Temporarily make the old `ofxOceanodeLayoutManager` callbacks call `windows->captureView/restoreView/applyIni`. Delete `ofxOceanodeLayoutWindowState.h` and `ofxOceanodeWindowVisibility.h`. Add `window_manager_regression.py`.
3. **Layouts.** Create `ofxOceanodeLayouts` with the §4.1/§4.2 formats and §6 behaviour. Add the docking-save event and remove the Container's ini write. Delete `src/Layouts/`. Update `layouts_regression.py` and `new_preset_regression.py`.
4. **ThemeManager.** Move the theme code (§5.3) and switch to `config/themes.json`.
5. **AppConfig and facade cleanup.** Add AppConfig (§5.4), remove the facade API (§7.2), add `drawMenuBar()` (§7.5), and auto-load in `setup()`. Delete `saveViewConfig`/`loadViewConfig`. Edit the 10 host apps (§8, not committed here). Add `app_config_regression.py`.
6. **Migration script.** Write §9 and `migrate_gui_files_regression.py`. Dry-run it on the real data folder and report. Do not apply it.
7. **Docs.**
   - Rewrite `docs/layouts.md`: Code organization, file names, and the §12 behaviour changes.
   - Update `docs/help-and-windows.md`: the Window menu is now owned by WindowManager.
   - Update the CLAUDE.md Architecture bullet "Layouts and windows" to describe `src/Gui/` and the four owners.
   - Search `docs/oceanode-user-manual.md` for `ImGuiLayout.ini`, `view.json`, `preset_layouts.json`, `defaultTheme`, `config/view.json`, "Save Config" and update every hit. Do not rebuild the PDF.

## 12. Intended behaviour changes (only these)

1. Config is loaded automatically in `setup()`, so hosts no longer call `loadConfig()`.
2. The V Sync checkbox shows and saves the real state.
3. The Config section "GUI" is renamed "Editor".
4. Global layout names are shown without `.ini`.
5. `config/view.json` no longer exists. Startup visibility comes from the preset that loads.
6. Old on-disk formats are not read at runtime. Run the migration script once.
7. Layouts no longer anchor to MiniMap/Presets for layouts saved before the Nodes window existed.

## 13. Out of scope

Do not change any of these:
- `scope_config.json` (Scope's own window geometry and scoped parameters)
- the `ofxOceanodeCustomGuiExtension` interface
- the Edit menu
- the unused-nodes window
- Help search and the assistant
- `ofxOceanodeColors.h`, `ofxOceanodeMenu.h`
- the preset file format apart from §4.1
- macro-level layouts (none exist)
- `ofxOceanodeControls` inspector auto show/hide logic

## 14. Acceptance checks (run at the end, all must hold)

```bash
grep -rn -E "ImGuiLayout\.ini|preset_layouts|workspaces\.json|themes/defaultTheme|\"defaultLayout\"|config/view\.json|\"view\.json\"|/view\.json" src
```
Expect no output.

```bash
grep -rn -E "SaveIniSettings|LoadIniSettings|ClearIniSettings" src
```
Expect hits only in `src/Gui/ofxOceanodeWindowManager.cpp`.

```bash
grep -rln -E "ofToDataPath|ofLoadJson|ofSavePrettyJson|ofFile|ofDirectory" src/Gui
```
Expect only `ofxOceanodeLayouts.cpp`, `ofxOceanodeThemeManager.cpp` and `ofxOceanodeAppConfig.cpp`.

```bash
ls src/Layouts src/ofxOceanodeWindowVisibility.h src/ofxOceanodeDockCollapse.h 2>&1 | grep -c "No such file"
```
Expect `3`.

Also check:
- `wc -l src/ofxOceanode.cpp` is at most 750.
- Every test passes, including `integration_compile.py` and `--midi-osc`.
- The final report lists the 10 edited app files, the dry-run report, and anything in this plan you could not do as written.
