# Optional Custom GUI addon

Custom panels, their editor, widgets, GUI snapshots and the **CustomGui Snapshots**
node live in the separate `ofxOceanodeCustomGui` addon. Normal node controls,
Inspector controls, Scope, macros and node-provided custom drawing regions stay
in `ofxOceanode`.

Place `ofxOceanodeCustomGui` beside `ofxOceanode` in your openFrameworks `addons`
folder. Add it to the application's `addons.make`, then regenerate the project:

```text
ofxOceanode
ofxImGui
ofxOceanodeCustomGui
```

Include its header and attach it after Oceanode setup, before loading presets:

```cpp
#include "ofxOceanodeCustomGui.h"

void ofApp::setup() {
    oceanode.setup();
    ofxOceanodeCustomGui::setup(oceanode);
    // Register other nodes and load presets here.
}
```

Attachment registers the snapshot node and enables panels for the root canvas,
existing macros and future macros. Repeating setup is harmless. Headless hosts
can call `ofxOceanodeCustomGui::attach(container)` instead.

The panel/snapshot API previously on `ofxOceanodeContainer` is now on its GUI
service:

```cpp
auto& gui = ofxOceanodeCustomGui::get(*oceanode.getContainer());
auto& panel = gui.createCustomGuiPanel("Performance");
gui.openCustomGuiPanel(panel.id, true);
```

Custom widget code includes the addon registry/layout headers. Widget contexts
refer to the GUI service; `context.container.getContainer()` accesses the core
canvas. Generic parameter lookup remains in core as `container.findParameter()`.
The existing `customGuiRegion` and `addCustomRegion()` node APIs stay in core and
work without this addon.

## Without the addon

The graph, connections between available nodes, node editor, Scope, macros,
transport and other addons continue to work. Custom GUI publishing menus,
publication badges, panel windows and GUI snapshot recall are unavailable.
Snapshot recall cannot drive parameter changes while its addon is absent.

Core preserves `custom_guis.json` and `custom_gui_snapshots.json` verbatim when
loading/saving a preset, including Save As and nested local macros. Unavailable
addon nodes retain their module entries, parameter files and cables without
executing. This lets the CustomGui Snapshots node return when the addon is
attached before reloading the saved preset. GUI references are not remapped by
structural edits made while their addon is unavailable.

Existing preset filenames, panel IDs, widget identifiers and snapshot formats
are unchanged. Normal node controls do not require the GUI addon.

## Verification

From the `ofxOceanode` directory:

```sh
python3 tests/custom_gui_addon_regression.py
python3 tests/custom_gui_addon_regression.py --with-addon
python3 tests/macro_undo_regression.py
python3 tests/macro_undo_regression.py --with-custom-gui
python3 tests/integration_compile.py
python3 tests/integration_compile.py --custom-gui ../ofxOceanodeCustomGui --midi-osc
```

Set `OCEANODE_CUSTOMGUI_ROOT` when testing an addon in a different directory.
Native regressions use real openFrameworks/ImGui with ASan/UBSan. Rendering tests
use a headless ImGui context; they do not launch the host graphics backend or test
external audio hardware.
