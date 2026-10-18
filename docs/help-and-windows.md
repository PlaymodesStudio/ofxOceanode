Help and window menus
====================

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

Window lists controllers, node Show toggles, macro canvases, custom GUIs and
utility windows (Help, Theme Editor, User Manual and Metrics). Canvas focuses
the root canvas.
It includes nodes inside nested macros and uses their canvas path to distinguish
instances. Toggles named Show, Show.Win, Show Window, Show GUI, Show Editor,
Show Snapshot?, Show All and Show Split are discovered in normal and Inspector
parameters. Display options such as Show Names and Show Clipping are excluded.
Macros use an explicit canvas visibility API. External OS windows using Show.Win
also appear. Custom GUIs and controllers use their existing visibility state.

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
