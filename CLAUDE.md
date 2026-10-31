# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ofxOceanode is an openFrameworks addon for node-based ("dataflow") patching of `ofParameter`s, drawn with ofxImGui (docking branch). Patches (nodes, connections, layout) are saved and recalled as JSON presets. It lives at `openFrameworks/addons/ofxOceanode`; sibling addons (`ofxOceanodeMidi`, `ofxOceanodeOSC`, `ofxOceanodeSuperCollider`, `ofxOceanodeTimeline`, …) extend it and are often affected by core API changes.

It needs PlaymodesStudio's openFrameworks fork (branch `bugfix-ofParameter_change_name`), not upstream OF. Development happens mainly on macOS.

## Build

There is no standalone build. The addon is compiled inside a host OF project, either an `example-*` app or a downstream app. Use that project's Xcode project or the OF Makefile (`make -C example-basic`, then `make -C example-basic RunRelease`). After changing `addons.make`, regenerate the project with the OF Project Generator. `addon_config.mk` lists extra sources explicitly, for example the profiling controller.

Optional features are compile-time defines: `OFXOCEANODE_USE_MIDI`, `OFXOCEANODE_USE_OSC` and `OFXOCEANODE_USE_BPM_DETECTION`. Timeline support comes from the separate `ofxOceanodeTimeline` addon, which exports `OFX_OCEANODE_HAS_TIMELINE=1`. Transport/clock/scheduling is core and always compiled; don't add conditional guards for it (see `docs/timeline-addon-dependencies.md`).

## Tests

Tests are standalone Python 3 scripts in `tests/`. Each one builds a small C++ harness with `clang++ -std=c++17`, usually with `-fsanitize=address,undefined`, and runs it. Nothing launches a GUI. Run one with:

```bash
python3 tests/canvas_undo_regression.py
```

Run all of them with `for t in tests/*_regression.py; do python3 "$t" || echo "FAIL $t"; done`.

- `tests/integration_compile.py` syntax-checks (`-fsyntax-only`) every `src/**/*.cpp` against the real OF and addon headers. Options: `--midi-osc`, `--timeline <dir>`, `--jobs N`. Run it after any header change.
- `tests/of_environment.py` locates the OF root from `../../`, or from `OCEANODE_TEST_OF_ROOT` when you work in a git worktree outside the OF tree. `OCEANODE_TEST_IMGUI_SRC` overrides the ImGui source path. `CXX` overrides the compiler.
- **Many tests pull production code out of `src/` by text anchors.** They use `source.index("<signature>")` and the `block()` brace-matcher in `new_preset_regression.py`, then splice the result into mocked OF/graph types. Some also assert that particular substrings or orderings exist in the source. Renaming, reordering or reformatting a function can therefore break a test without any change in behaviour. When that happens, update the anchors in the test along with the code.

## Generated files and docs

- `src/ofxOceanodeNodeHelpCatalog.h` is generated; don't edit it by hand. Edit `docs/node-help-catalog.json` or `docs/oceanode-assistant.md`, then run `python3 scripts/generate_node_help.py`. `scripts/audit_node_help.py` checks catalog coverage.
- The user manual PDF (`output/pdf/`) is built from `docs/oceanode-user-manual.md` by `scripts/build_user_manual.py`, which needs reportlab.
- `docs/` holds design notes and specs for features such as layouts, workspaces, macro mixer, custom GUI and profiling. Read the matching doc before changing one of those features.

## Architecture

- **`ofxOceanode`** (`src/ofxOceanode.h/.cpp`) is the facade the host app owns. It calls `setup/update/draw` and registers node models (`registerModel<T>(category, args...)`) and data types. `registerType<T>` automatically creates `router<T>`, `portal<T>` and `portalSelector<T>` nodes for the type. `registerTypeWithBufferAndHeader` also adds buffer, header, headerPro and vector types. Built-in registration is split across `ofxOceanodeDefaultNodes.cpp`, `ofxOceanodeDefaultTypes.cpp` and `ofxOceanodeDefaultRouting.cpp` to keep compile times down.
- **Node model and node.** `ofxOceanodeNodeModel` (`src/Nodes/`) is the base class for user nodes. A node declares ports with `addParameter` and `addOutputParameter`, which wrap `ofParameter`s in `ofxOceanodeParameter<T>` (`src/ofxOceanodeParameter.*`, with flags such as `DisableInConnection`). It overrides lifecycle and persistence hooks such as `presetSave`, `presetRecall*`, `loadBeforeConnections` and `macroSave/macroLoad`. `ofxOceanodeNode` wraps a model in the graph, and `ofxOceanodeNodeGui` draws it. `example-basic/src/testNode.*` shows how to write a custom node.
- **Container and canvas** (`src/Managers/`). `ofxOceanodeContainer` owns the nodes and connections of one graph level. It handles creation and deletion, preset save and load, copy/paste, and canvas undo/redo snapshots. `ofxOceanodeCanvas` is the ImGui view and editing UI for one container. Connections (`src/Connections/`) are typed parameter links.
- **Macros.** `ofxOceanodeNodeMacro` is a node that holds a child container, so patches nest. Each macro has its own canvas, and Cmd+Up navigates to the parent. The `Macro*` files in `src/Nodes/` implement macro presets and snapshots, morphing, router management, and the Macro Mixer.
- **Registries.** `ofxOceanodeNodeRegistry` and `ofxOceanodeTypesRegistry` map names to factories. Connection compatibility and router/portal creation are driven by registered types.
- **Controllers** (`src/Controls/`). These are subclasses of `ofxOceanodeBaseController` and appear as ImGui windows: presets, inspector, nodes, hierarchy, time/transport, profiling, global variables, minimap and log. Host apps add their own with `addController<T>()`.
- **GUI shell** (`src/Gui/`). Four owners, each a `.h/.cpp` pair, and no file on disk has two writers. `ofxOceanodeWindowManager` hosts the dockspace window and docking IDs, applies/captures ImGui ini text, owns show mode, window visibility and the Window menu, and does no file IO. `ofxOceanodeLayouts` owns preset `layout.json` and global layouts (`config/guiLayouts/<name>.json`, `config/guiLayouts.json`) plus the Layouts menu; every layout stores docking (ini) and window visibility (view). `ofxOceanodeThemeManager` owns the theme editor, fonts and `config/themes*`. `ofxOceanodeAppConfig` owns `config/config.json` and the Config menu's Rendering/Editor/Configuration sections. The docking and theme helpers (proportional docking, dock collapse, scope proportions, floating-window decor, `OceanodeTheme`, theme colour picker) live beside them. Old file formats are converted once by `scripts/migrate_gui_files.py`; see `docs/layouts.md`.
- **Custom GUI** (`src/CustomGui/`). User-built control panels made of widgets bound to node parameters, with a widget registry.
- **Transport and time** (`ofxOceanodeTime`, `Managers/ofxOceanodeTransport`, `ofxOceanodeScheduling`, `ofxOceanodeMidiClock`). The core clock, BPM and scheduling. `ofxOceanodeAutomationExtension.h` is the interface that optional addons such as the timeline attach to, so core never includes their concrete types.
- **Help and AI assistant** (`Managers/ofxOceanodeHelp*`, `ofxOceanodeAIAssistant.h`, `ofxOceanodeRequesty.h`). Node search, the embedded catalog, and an LLM assistant that uses Requesty. It can propose patch plans, which `ofxOceanodeHelpPlan.h` validates.

Everything runs on the OF update/draw thread. Execution order follows the order in which connections were created.
