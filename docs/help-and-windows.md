Help and window menus
====================

Window > Node Pool (Cmd+N on macOS, Ctrl+N elsewhere) opens a floating
browser with Nodes and Macros tabs. Each tab has its own search and selection,
a scrollable category list on the left, and a description on the right. Node
details include a parameter preview when catalog hints are available; these
hints do not claim live inlet/outlet directions. Macro descriptions come from
the library's description.json. Browsing never constructs or runs a node.
Add creates the selected item in the active canvas (including macro canvases),
selects it and brings it into view. The destination is shown above Add. Repeated
adds are offset so their headers remain reachable, and grid snapping is honored.
Adding participates in canvas undo/redo. The pool stays open for further adds;
close it with its title-bar close button. It is a utility dialog, excluded from
saved layout visibility like Help and Theme Editor.

Help > Search nodes opens the dockable node reference. Space-separated keywords
must all match, case insensitively, across a node's name, category, description
and parameter hints. Only models registered in the running application appear.
Searching does not construct models or run their setup functions.

The shared catalog supplies descriptions for core and sibling addon nodes that
have none, including Collection, SantiNodes, SuperCollider, Textures, Playmodes,
VectorGraphics, EloiNodes, Astres, Lights, Laser, MIDI, OSC and other local addons.
A model's own nonempty description takes precedence in its tooltip. Registry
metadata captures constructor descriptions, updates after setup, and can receive
metadata directly through setModelDescription. Named imported-model factories
also retain a supplied spec's public description field without constructing a
throwaway model. Catalog keys ending in a space describe dynamic type families.

Window lists controllers, node Show toggles, macro canvases and custom GUIs.
It is drawn and owned by ofxOceanodeWindowManager (src/Gui/), which also
captures and restores this visibility for Layouts; Help no longer draws it.
Canvas focuses the root canvas.
Visibility checkboxes appear to the left of every window name and take effect
immediately, keeping the menu open. There is no Apply step. Window replaces the
former View menu. Save Preset (or Save Preset As) stores these choices; loading
that preset restores them, including after restarting the app. Preset and global
layouts (the Layouts menu, after Window) store them too; the default global
layout sets which windows are shown at startup.
Scopes can be hidden without removing its scoped parameters. An unchecked
Inspector stays hidden; when checked, Config's automatic show/hide option can
still hide it while no node is selected. Automatic hiding does not uncheck it.
Custom GUIs includes New Custom GUI for the active canvas and an Options menu
beside each panel for Edit and Delete.
It includes nodes inside nested macros and uses their canvas path to distinguish
instances. Toggles named Show, Show.Win, Show Window, Show GUI, Show Editor,
Show Snapshot?, Show All and Show Split are discovered in normal and Inspector
parameters. Display options such as Show Names and Show Clipping are excluded.
Macros use an explicit canvas visibility API. External OS windows using Show.Win
also appear. Custom GUIs and controllers use their existing visibility state.

The preset's `layout.json` (`current.view`) stores controller, Scopes, node,
Macro and Custom GUI visibility, including nested canvases. It is applied after an automatically
recalled named layout, so an older layout cannot undo the saved choices.
Explicitly selecting a named layout still applies its visibility. Save the preset
afterward to keep those choices. Presets without visibility data keep the
current visibility until saved again.
Help, Theme Editor, confirmations and other utility dialogs are excluded, as is
the deprecated persistent-node feature. Canvas is a focus action, not a toggle.

Config includes Extensions, Rendering, Editor, Theme and Configuration sections.
Rendering, Editor and Configuration (Save Config / Load Config) belong to
ofxOceanodeAppConfig and config/config.json; Theme belongs to
ofxOceanodeThemeManager. Theme contains the
editor, theme Save/Load, default selection and built-in reset; there is no
separate Theme top menu. Help has two sections: Documentation (User Manual and
Quick tips) and Utilities (Search nodes, AI assistant, Metrics, Reload Macros and addon actions
such as Textures' Update Effects). Each action appears once. Theme Editor is
available in Config's Theme section.

While the Theme Editor is open, hold Alt and hover any GUI element to find its
color. A tooltip lists the colors drawn under the cursor, topmost first, and the
editor selects the topmost theme color, switches to its tab (ImGui Colors or
Oceanode Colors), scrolls to it and outlines it. The last lookup stays listed at
the top of the editor; click a name there to choose another candidate when
several colors share a value. The lookup reads ImGui's rendered draw data after
gui.draw() (src/Gui/ofxOceanodeThemeColorPicker.h, called by
ofxOceanodeThemeManager::afterRender): it skips user images and the
Theme Editor itself, and matches by RGB with "(other alpha)" when only alpha
differs, for example disabled widgets. Colors not owned by the theme, such as
node colors, appear as hex values.

Presets starts with the current Bank and Preset status, then New Preset, New Bank,
Save Preset, Save Preset As and Delete Preset, followed by the LOAD PRESET section.

All menu subsection headings use ofxOceanodeMenu::section in
src/ofxOceanodeMenu.h. It capitalizes titles and uses ImGui's titled separator
style, shared by Config, Edit, Presets, Window, Help, Global Variables and OSC
Variables. Change that
helper to restyle their headings together. Addons can register Help > Utilities
actions with registerUtilityMenuItem; update/draw callbacks run even while the
menu is closed, and its optional enabled callback controls availability.

The assistant
-------------

Help > AI assistant uses the shared Requesty helper, model catalog and credentials
already used by Formula and DynGen. Configure data/config/requesty.md or the
REQUESTY_API_KEY environment variable, then select a provider/model in its popup.
Its chosen model is stored in data/config/help-assistant.json after a response.

Ask mode answers questions. Build patch mode produces a reviewable list of nodes
and connections. Create in canvas adds that proposal to the canvas active when
the request was sent, places the nodes below existing content, selects them and
centers the view. A changed or deleted target canvas requires a fresh proposal.
The previous answer, proposal and last application error accompany follow-ups.

The assistant receives the embedded Oceanode guide, registered node descriptions,
source-derived parameter hints, and pin names/types/directions from existing node
types in the target canvas. It does not receive parameter values, source scripts,
API keys or preset file contents as prompt context. The credentials are used by
Requesty for authentication through the existing request helper.

Proposals contain at most 24 new nodes and 48 connections. Validation rejects
unknown types, duplicate IDs/inputs, references to existing nodes, feedback cycles,
missing pins, forbidden pin directions and unsupported type conversions. Nodes
use their normal defaults; parameter values and executable code are not part of
the proposal format. Connections are created inactive, then enabled after all
have been resolved. Failed creation removes the newly added graph. Node setup
still has its usual addon behavior, including contact with external services.
There is no general undo stack; successfully created nodes are selected so they
can be deleted together. An MCP server is not needed for this in-process helper.

Maintaining the reference
------------------------

Edit node-help-catalog.json and oceanode-assistant.md in this directory, then run:

    python3 scripts/generate_node_help.py
    python3 scripts/audit_node_help.py
    python3 tests/help_regression.py

The generated headers embed the catalog and guide, so host applications do not
need to copy documentation into their data folder. The audit checks statically
named constructors and dynamic family prefixes in the locally available sibling
addons. Runtime-imported effects keep their supplied descriptions. Parameter
hints are an aid to discovery; actual pins after setup remain authoritative.

When integrating this change into an existing Xcode/IDE project, regenerate its
addon source list to include src/Managers/ofxOceanodeHelp.cpp. The openFrameworks
makefile build and tests discover it automatically.
