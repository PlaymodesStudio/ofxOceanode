# Custom Node GUIs for ofxOceanode — Full Feature Specification

> **Purpose of this document:** Complete specification for an AI agent (or human developer) to implement the Custom Node GUI feature in ofxOceanode. Covers architecture, data structures, persistence, entry points, UI flows, widget library, phase plan, and all relevant existing code to reuse or extend.

---

## 1. Feature Overview

Any node or macro in Oceanode can have a **Custom GUI Panel**: a user-designed floating window with interactive widgets (sliders, knobs, toggle grids, multisliders, XY pads, etc.) mapped to the node's parameters. The user designs the layout interactively via a built-in designer mode.

**Key properties:**
- The custom GUI is a **separate floating window**, not part of the node box. Node box pins, connections, and standard parameter GUI are unaffected.
- The panel is **scopable** — it behaves like an `ofxOceanodeScope` window (independent, floating, resizable, persisted).
- **Dual-control**: if a parameter is connected (receiving values from another node), the widget reflects the live incoming value. If the user interacts with the widget, it calls `.set()` on the parameter directly, overriding the incoming connection — identical to scRhythmBox's behavior.
- **Layout is grid-based** (configurable columns × rows, widgets occupy 1 or more cells).
- **Panel-local zoom** (scroll wheel inside the panel scales all widgets independently of canvas zoom).
- Custom GUIs are **saved with the node type or macro**, not just with the preset, so they are reusable across projects.

---

## 2. Existing Codebase to Understand First

Before implementing, read these files thoroughly:

### 2.1 Scope system (primary template)
- `src/Managers/ofxOceanodeScope.h`
- `src/Managers/ofxOceanodeScope.cpp`

The custom GUI panel is architecturally a scope window with interactive widgets and a 2D grid layout instead of 1D read-only visualizations. Reuse:
- Window management (open/close, position/size persistence, ImGui window flags)
- `notifyScopeChanged()` auto-save callback pattern
- `registerScope<T>()` renderer registry pattern → replicate as widget registry
- `ofxOceanodeScopeItem` struct pattern → replicate as `CustomGuiWidget`

### 2.2 Node GUI (widget rendering to reuse)
- `src/Nodes/ofxOceanodeNodeGui.h`
- `src/Nodes/ofxOceanodeNodeGui.cpp` (~1000 lines)

All widget rendering code (SliderFloat, DragFloat, SliderInt, Checkbox, Button, Combo, PlotHistogram) lives here. **Do not rewrite these — extract and reuse the ImGui calls directly.**

Right-click parameter context menu is here — extend it with "Add to Custom GUI" (search for "Add to Scope" to find the exact location, around line 565).

### 2.3 Inspector controller (add "Custom GUI" section)
- `src/Controls/ofxOceanodeInspectorController.h`
- `src/Controls/ofxOceanodeInspectorController.cpp`

The inspector renders a collapsible "Scopes" section (around line 289). Add a parallel "Custom GUI" section with the same structure: overview of placed widgets, Open/Edit/Delete buttons.

### 2.4 Canvas (wire up node body right-click)
- `src/Managers/ofxOceanodeCanvas.cpp`

Node body right-click is detected at line ~732 (`open_context_menu |= ImGui::IsMouseClicked(1)`) but **the flag is never used to open a menu**. This is where "Open/Edit/Create/Delete Custom GUI" menu items must be added.

### 2.5 Node / persistence hooks
- `src/Nodes/ofxOceanodeNode.h`
- `src/Nodes/ofxOceanodeNode.cpp`

Persistence hooks available in `ofxOceanodeNodeModel`:
```cpp
void loadCustomPersistent(ofJson &json){}   // load node-level data
// and the save equivalent (check .cpp for the save call that pairs with it)
void macroSave(ofJson &json, string path){}
void macroLoad(ofJson &json, string path){}
```
Use these to load/save the custom GUI layout at the node level.

### 2.6 Container (preset-level save/load)
- `src/Managers/ofxOceanodeContainer.h`
- `src/Managers/ofxOceanodeContainer.cpp`

`saveScope()` / `loadScope()` (around line 257 / 285) show the exact pattern for saving/loading a panel's window state per preset. Add `saveCustomGuis()` / `loadCustomGuis()` alongside them.

### 2.7 Parameter system
- `src/ofxOceanodeParameter.h`

All parameters are `ofxOceanodeParameter<T>` wrapping `ofParameter<T>`. To read a typed value for rendering, cast: `p->cast<float>().getParameter().get()`. To write (user interaction), call `.set(value)` on the underlying `ofParameter`.

### 2.8 Parameter reference/resolution
Parameters are referenced by `canvasID + nodeName + paramName` (not by pointer) so they survive reload. The existing `resolveParameterFromPath()` in `ofxOceanodeContainer.cpp` handles lookup. Use this exact mechanism.

### 2.9 scRhythmBox (dual-control reference)
- Located in `ofxOceanodeSuperCollider` addon: `src/scRhythmBox.cpp`

Shows the dual-control pattern in practice: the custom ImGui sequencer window calls `paramObject.set(newValue)` on user interaction. The scope window reads `paramObject.get()` every frame. Incoming connections also call `.set()`. No special handling needed — the pattern is free.

### 2.10 Expanded/collapsed toggle (per-node bool persistence pattern)
In `ofxOceanodeNodeGui.h` (line ~80): `bool expanded;` with getter/setter.
In `ofxOceanodeNode.cpp` (line ~98): `json["expanded"] = nodeGui->getExpanded();` / load equivalent.
Replicate this pattern for `bool customGuiOpen` and any other per-node panel state.

---

## 3. Data Structures

```cpp
// Widget types — filtered by parameter type compatibility (see Section 6)
enum class CustomGuiWidgetType {
    Slider,         // float, int
    Knob,           // float
    DragNumber,     // float, int
    Toggle,         // bool
    Button,         // void
    MultiSlider,    // vector<float>
    ToggleGrid,     // vector<int>, vector<bool>
    XYPad,          // vector<float> (2-element), glm::vec2
    Waveform,       // vector<float> — read-only display
    TextDisplay,    // string — read-only display
    Label,          // no parameter — static text
    Dropdown        // int (reuse existing Combo widget)
};

struct CustomGuiWidget {
    string parameterName;        // parameter name as registered in the node's ofParameterGroup
                                 // empty string if type == Label
    CustomGuiWidgetType type;
    int gridX, gridY;            // top-left cell (0-based)
    int spanW, spanH;            // cell span (minimum 1×1)
    string label;                // display label — empty = use parameterName
    ofColor color;               // tint color for the widget
    ofJson config;               // widget-specific settings, e.g.:
                                 //   ToggleGrid: {"rows": 4, "cols": 8}
                                 //   XYPad: {"paramY": "someOtherParam"}
                                 //   Label: {"text": "Section Title"}
};

struct CustomGuiLayout {
    int columns = 4;
    int rows = 3;
    float cellWidth = 80.0f;     // base pixel width per cell (before zoom)
    float cellHeight = 60.0f;    // base pixel height per cell (before zoom)
    float zoom = 1.0f;           // panel-local zoom multiplier
    vector<CustomGuiWidget> widgets;
};

// Saved per-preset (window state only — not layout)
struct CustomGuiWindowState {
    float posX = 100.0f;
    float posY = 100.0f;
    float width = 400.0f;
    float height = 300.0f;
    bool isOpen = false;
};
```

---

## 4. Persistence Architecture

### 4.1 Layout persistence (node-level — reusable across presets)

| Node type | Layout file location |
|---|---|
| Regular node (addon-defined) | `data/nodeGUIs/<NodeTypeName>.json` |
| Local macro | `<presetFolder>/macros/<macroName>/customGui.json` |
| Global macro | `<globalMacroFolder>/customGui.json` |

The layout JSON is loaded/saved via `loadCustomPersistent` / the paired save call for regular nodes, and via `macroLoad` / `macroSave` for macros.

**Layout JSON format:**
```json
{
  "columns": 4,
  "rows": 3,
  "cellWidth": 80.0,
  "cellHeight": 60.0,
  "zoom": 1.0,
  "widgets": [
    {
      "parameterName": "tempo",
      "type": "Slider",
      "gridX": 0, "gridY": 0,
      "spanW": 2, "spanH": 1,
      "label": "Tempo",
      "color": [255, 200, 100, 255],
      "config": {}
    },
    {
      "parameterName": "steps",
      "type": "ToggleGrid",
      "gridX": 0, "gridY": 1,
      "spanW": 4, "spanH": 2,
      "label": "Steps",
      "color": [100, 200, 255, 255],
      "config": {"rows": 2, "cols": 8}
    }
  ]
}
```

### 4.2 Window state persistence (preset-level)

Saved alongside `scope_config.json` in the preset folder as `customGui_windowState.json`:

```json
{
  "nodes": [
    {
      "canvasID": "0",
      "nodeName": "MyNode",
      "posX": 320.0,
      "posY": 150.0,
      "width": 480.0,
      "height": 360.0,
      "isOpen": true
    }
  ]
}
```

Add `saveCustomGuiWindowStates(presetPath)` / `loadCustomGuiWindowStates(presetPath)` to `ofxOceanodeContainer`, called from the same place as `saveScope` / `loadScope`.

### 4.3 Parameter references

Use the **same reference scheme as scopes**: `canvasID + nodeName + paramName`. Use the existing `resolveParameterFromPath()` for restoration on load. Never store raw pointers in persisted data.

### 4.4 Macro-specific rule

Macro custom GUIs only expose **router parameters** (published parameters with routers). Do not attempt to reference internal node parameters of a macro from its custom GUI. This keeps the macro's public interface stable.

---

## 5. Entry Points (UI Flows)

### 5.1 Node body right-click menu (primary creation entry point)

In `ofxOceanodeCanvas.cpp`, wire up the currently-unused `open_context_menu` flag (line ~732) to open an ImGui popup:

```
Right-click node body or title bar
  ├─ [if no custom GUI layout exists for this node type]
  │    └─ "Create Custom GUI..."
  │         → creates empty CustomGuiLayout (default 4×3 grid)
  │         → saves layout to data/nodeGUIs/<NodeTypeName>.json
  │         → opens panel in DESIGN MODE
  │
  └─ [if custom GUI layout exists]
       ├─ "Open Custom GUI"    → opens panel in RUN MODE
       ├─ "Edit Custom GUI"    → opens panel in DESIGN MODE
       └─ "Delete Custom GUI"  → confirmation modal → removes layout file, closes panel
```

### 5.2 Parameter right-click menu (fast-path entry point)

In `ofxOceanodeNodeGui.cpp`, extend the parameter right-click context menu (near line 565, alongside "Add to Scope"):

```
Right-click parameter
  ├─ Add to Scope
  ├─ Add to Timeline
  ├─ Add to Custom GUI ▶
  │    ├─ <DefaultWidget>      ← bold, shown first (see compatibility matrix §6)
  │    ├─ ─────────────
  │    └─ [all compatible widget types for this param's type]
  └─ ...
```

**Behavior when "Add to Custom GUI" is triggered:**
1. If no layout exists for this node → create a default empty `CustomGuiLayout`, save it.
2. Find the next available grid cell (scan left-to-right, top-to-bottom for first empty 1×1 cell; if grid is full, add a new row).
3. Create a `CustomGuiWidget` with the chosen type, place it at that cell with default span (1×1 for most widgets; 2×1 for Slider; full-width for MultiSlider/ToggleGrid).
4. Save updated layout.
5. Open the panel (in RUN MODE if it was already populated; in DESIGN MODE if this was the first widget added).

### 5.3 Inspector "Custom GUI" section (management entry point)

In `ofxOceanodeInspectorController.cpp`, add a collapsible section after the existing "Scopes" section:

```
▾ Custom GUI
  ┌─────────────────────────────────┐
  │  Layout: 4 × 3  │  Zoom: 1.0×  │
  │  [Open]  [Edit]  [Delete]       │
  └─────────────────────────────────┘
  stepVol_0     MultiSlider   (2×1)
  gate          ToggleGrid    (4×2)
  tempo         Slider        (2×1)
```

If no layout exists, show a single `[Create Custom GUI]` button instead.

---

## 6. Widget Compatibility Matrix

| Parameter C++ type | Default widget | All compatible widgets |
|---|---|---|
| `float` | Slider | Slider, Knob, DragNumber, Waveform (read-only as single value) |
| `int` | SliderInt | SliderInt (shown as Slider), DragInt (shown as DragNumber), Dropdown |
| `bool` | Toggle | Toggle |
| `void` | Button | Button |
| `string` | TextDisplay | TextDisplay |
| `vector<float>` | MultiSlider | MultiSlider, Waveform, XYPad (only if size == 2) |
| `vector<int>` | ToggleGrid | ToggleGrid, MultiSlider |
| `vector<bool>` | ToggleGrid | ToggleGrid |
| `glm::vec2` | XYPad | XYPad |

**Label** widget has no parameter — it is always available as a static decoration element.

---

## 7. The Custom GUI Panel Class

Create `ofxOceanodeCustomGuiPanel` (one instance per node that has a layout). Model it on `ofxOceanodeScope`.

### 7.1 Key responsibilities
- Owns a `CustomGuiLayout` (loaded from file, modified in designer mode)
- Owns a `CustomGuiWindowState` (position/size, received from container on load)
- Renders in two modes: **Run** and **Design**
- Manages a widget renderer registry (see §7.3)
- Calls a "layout changed" callback on any modification (for auto-save, same as scope's `notifyScopeChanged()`)
- Resolves `parameterName` strings to live `ofxOceanodeAbstractParameter*` pointers at load time

### 7.2 Panel rendering — Run mode

```
BeginPanel(title, &isOpen, ImGuiWindowFlags_NoCollapse)
  draw title bar with: node name, lock icon (click → enter Design mode), zoom display
  
  panelWidth  = layout.columns * layout.cellWidth  * layout.zoom
  panelHeight = layout.rows    * layout.cellHeight * layout.zoom
  
  for each widget in layout.widgets:
    pixelX = widget.gridX * layout.cellWidth  * layout.zoom
    pixelY = widget.gridY * layout.cellHeight * layout.zoom
    pixelW = widget.spanW * layout.cellWidth  * layout.zoom
    pixelH = widget.spanH * layout.cellHeight * layout.zoom
    
    ImGui::SetCursorPos({pixelX, pixelY + titleBarHeight})
    ImGui::PushID(widgetIndex)
    renderWidget(widget, {pixelW, pixelH})
    ImGui::PopID()
  
  handle scroll wheel → adjust layout.zoom (clamp 0.25 – 4.0), save
EndPanel()
```

### 7.3 Widget renderer registry

```cpp
// Registry maps (paramType, widgetType) → render function
// Render function signature:
using CustomGuiRenderFn = function<void(ofxOceanodeAbstractParameter*, ImVec2 size, const ofJson& config)>;

map<pair<type_index, CustomGuiWidgetType>, CustomGuiRenderFn> widgetRenderers;
```

Registration example:
```cpp
registerWidget<float>(CustomGuiWidgetType::Slider,
    [](ofxOceanodeAbstractParameter* p, ImVec2 size, const ofJson& config) {
        auto& param = p->cast<float>().getParameter();
        float val = param.get();
        ImGui::SetNextItemWidth(size.x);
        if (ImGui::SliderFloat("##w", &val, param.getMin(), param.getMax())) {
            param.set(val);   // this is the dual-control write
        }
    }
);
```

The `param.set(val)` call is all that's needed for dual-control — it writes through to the ofParameter, which notifies any listeners (synths, connections, etc.).

### 7.4 Panel rendering — Design mode

```
BeginPanel(title + " [DESIGN]", ...)
  draw title bar with: unlock icon (click → return to Run mode), grid controls
  
  // Draw grid overlay
  for col in 0..columns:
    for row in 0..rows:
      draw cell border rect at (col*cw*z, row*ch*z) size (cw*z, ch*z)
      if cell is empty: draw light fill on hover (drop target hint)
  
  // Draw placed widgets with handles
  for each widget:
    draw widget (same as run mode but slightly dimmed)
    draw drag handle (top-left corner, 8×8 px grab zone)
    draw resize handle (bottom-right corner, 8×8 px grab zone)
    if widget is selected: draw selection highlight border
  
  // Handle interactions
  if drag handle held:
    follow mouse, snap to grid cell on release → update gridX/gridY, save
  if resize handle held:
    follow mouse, snap to grid → update spanW/spanH (min 1×1), save
  if widget clicked:
    set as selected widget → show properties in sidebar
  
  // Left sidebar: parameter list
  BeginChild("params", {150, 0})
    for each param in node:
      show icon for type, show name
      if DragDropSource: allow dragging onto grid cells
      double-click → add with default widget to next free cell
  EndChild()
  
  // Right sidebar: selected widget properties
  BeginChild("props", {150, 0})
    if selectedWidget:
      InputText "Label", selectedWidget.label
      ColorEdit4 "Color", selectedWidget.color
      Combo "Widget Type" (filtered to compatible types for param's type)
      InputInt "Span W/H"
      // widget-specific config fields (e.g. rows/cols for ToggleGrid)
      if Button "Remove":
        remove widget from layout, save
  EndChild()
  
  // Top bar: grid controls
  InputInt "Columns", layout.columns  → on change, clamp widgets, save
  InputInt "Rows",    layout.rows     → on change, clamp widgets, save
  InputFloat "Cell W/H"               → on change, save
  Button "Reset Zoom"                 → layout.zoom = 1.0, save
EndPanel()
```

---

## 8. Widget Implementation Details

### 8.1 Slider (float / int)
Reuse exactly from `ofxOceanodeNodeGui.cpp`. Use `ImGui::SliderFloat` with `param.getMin()` / `param.getMax()`. Call `param.set(val)` on change.

### 8.2 Knob (float)
Draw a circular dial using ImGui DrawList arcs. Map angle range (e.g. -135° to +135°) to `[min, max]`. Drag vertically to change value (same as ImGui's DragFloat interaction). Call `param.set(val)` on change.

### 8.3 Toggle (bool)
`ImGui::Checkbox` or a custom colored button that toggles. Call `param.set(!param.get())` on click.

### 8.4 Button (void)
`ImGui::Button(label, size)`. On click: `p->cast<void>().trigger()` (or however void params are triggered in the existing codebase — check NodeGui.cpp).

### 8.5 MultiSlider (vector<float>)
Draw N vertical bars, one per element. Each bar is an `ImGui::SliderFloat` rendered vertically (rotate via ImGui DrawList) or horizontal bars stacked. Allow individual bar drag. On change to element i: copy vector, set element i, call `param.set(newVec)`.

### 8.6 ToggleGrid (vector<int> / vector<bool>)
`config` contains `"rows"` and `"cols"`. Draw `rows × cols` buttons in a grid. Button at (r, c) maps to index `r * cols + c`. Highlight if value > 0. On click: toggle that index, call `param.set(newVec)`.

### 8.7 XYPad (vector<float> size 2, or glm::vec2)
Draw a square area. A crosshair dot at position `(val[0]-min)/(max-min), (val[1]-min)/(max-min)` within the area. On drag inside area: update both components, call `param.set(newVec)`.

For two separate float parameters: store the second param name in `config["paramY"]`, resolve it separately at load time.

### 8.8 Waveform / Display (vector<float>, read-only)
`ImGui::PlotLines` or `ImGui::PlotHistogram` filling the widget rect. No interaction. Read `param.get()` every frame.

### 8.9 TextDisplay (string, read-only)
`ImGui::TextWrapped(param.get().c_str())`. No interaction.

### 8.10 Dropdown (int)
Reuse the existing Combo widget from NodeGui. Requires that the parameter has associated option strings (stored in `config["options"]` as JSON array).

---

## 9. New Files to Create

| File | Purpose |
|---|---|
| `src/CustomGui/ofxOceanodeCustomGuiPanel.h` | Panel class declaration |
| `src/CustomGui/ofxOceanodeCustomGuiPanel.cpp` | Panel rendering, designer mode, widget registry |
| `src/CustomGui/ofxOceanodeCustomGuiLayout.h` | `CustomGuiLayout`, `CustomGuiWidget`, `CustomGuiWindowState` structs + JSON serialization |
| `src/CustomGui/ofxOceanodeCustomGuiWidgets.h` | Individual widget render functions (callable standalone) |
| `src/CustomGui/ofxOceanodeCustomGuiWidgets.cpp` | Widget implementations (Knob, MultiSlider, ToggleGrid, XYPad) |

---

## 10. Existing Files to Modify

| File | Change |
|---|---|
| `src/Managers/ofxOceanodeCanvas.cpp` | Wire up node body right-click menu (line ~732). Add "Create/Open/Edit/Delete Custom GUI" items. |
| `src/Nodes/ofxOceanodeNodeGui.cpp` | Extend parameter right-click menu (~line 565) with "Add to Custom GUI ▶" submenu. |
| `src/Controls/ofxOceanodeInspectorController.cpp` | Add "Custom GUI" collapsible section after "Scopes" section (~line 289). |
| `src/Managers/ofxOceanodeContainer.cpp` | Add `saveCustomGuiWindowStates()` / `loadCustomGuiWindowStates()` calls alongside scope save/load. |
| `src/Nodes/ofxOceanodeNode.h` / `.cpp` | Add `customGuiPanel` member, load/save layout in `loadCustomPersistent` / save equivalent. |
| `src/Nodes/ofxOceanodeNodeMacro.h` / `ofxOceanodeNodeMacroGui.cpp` | Add layout load/save in `macroSave` / `macroLoad`. Expose only router parameters to designer. |

---

## 11. Implementation Phases

### Phase 0 — Data structures & persistence (1 week)
- Implement `CustomGuiWidget`, `CustomGuiLayout`, `CustomGuiWindowState` in `ofxOceanodeCustomGuiLayout.h`
- Implement JSON serialization/deserialization for all three structs
- Create `data/nodeGUIs/` directory convention and file I/O helpers
- Hook into node `loadCustomPersistent` / save and macro `macroLoad` / `macroSave`
- Hook into container preset save/load for window state
- **Deliverable**: round-trip test — save a hardcoded layout, reload it, verify all fields survive

### Phase 1 — Runtime panel (2 weeks)
- Create `ofxOceanodeCustomGuiPanel` class (model on `ofxOceanodeScope`)
- Implement grid renderer (cell positioning with zoom, span support)
- Implement widget renderer registry
- Implement core widgets reusing NodeGui code: Slider, Toggle, Button, DragNumber, TextDisplay, Waveform
- Wire "Open Custom GUI" to node right-click (basic version)
- Panel-local zoom via scroll wheel
- Window position/size save on close, restore on open
- **Deliverable**: a manually-specified layout renders and is interactive with dual-control working

### Phase 2 — Right-click parameter integration (1 week)
- Extend parameter right-click menu with "Add to Custom GUI ▶"
- Implement compatible widget filtering by parameter type
- Implement auto-placement (next free cell scan)
- "Create Custom GUI" implicit path if no layout exists
- **Deliverable**: full right-click workflow from parameter to placed widget

### Phase 3 — Designer mode (2–3 weeks)
- Design/Run toggle in panel title bar
- Grid overlay rendering (cell borders, empty cell hover highlight)
- Widget drag-to-reposition (snap on release)
- Resize handle (bottom-right, snap on release)
- Widget selection highlight
- Parameter sidebar (all node params, type icons, drag-to-place or double-click)
- Properties sidebar (label, color, widget type dropdown, span spinners, widget-specific config)
- Grid dimension controls (columns/rows spinners, add/remove)
- Delete selected widget (Delete key)
- Auto-save layout on every design change
- **Deliverable**: fully interactive designer matching Max presentation mode UX

### Phase 4 — Remaining widgets (2 weeks, can overlap Phase 3)
- Interactive MultiSlider (drag individual bars, `vector<float>`)
- ToggleGrid (`vector<int>` / `vector<bool>`, configurable rows × cols)
- Knob (circular drag, `float`)
- XYPad (2D drag, `vector<float>` 2-element or two separate params)
- **Deliverable**: full widget library available in designer

### Phase 5 — Macro integration (1 week)
- Macro right-click: "Edit Custom GUI" → opens panel with only router parameters available in sidebar
- Layout saved to correct macro folder (local or global)
- On macro instantiation, panel loads from macro folder
- Macro window state saved per-preset
- **Deliverable**: macros fully supported with same UX as regular nodes

### Phase 6 — Polish (1 week)
- Inspector "Custom GUI" section (overview, Open/Edit/Delete buttons, widget list)
- Import/export layout as standalone JSON file (right-click panel title)
- Copy layout to another node of same type
- "Delete Custom GUI" → confirmation → removes layout file, closes panel
- Keyboard shortcuts in designer: Delete (remove widget), Escape (exit design mode), Ctrl+Z (undo last placement — simple stack)
- **Deliverable**: production-quality feature

---

## 12. Critical Implementation Notes

1. **Never store raw `ofxOceanodeAbstractParameter*` pointers in persisted data.** Always use `canvasID + nodeName + paramName` strings and resolve via `resolveParameterFromPath()` on load. Pointers are invalid after reload.

2. **Dual-control is free.** Do not add any special connection-override logic. Just call `param.set(val)` in widget interaction handlers. The existing `ofParameter` / connection system handles the rest — this is proven by scRhythmBox.

3. **Read parameter values every frame** in widget renderers. Do not cache or subscribe to change events for rendering. Call `p->cast<T>().getParameter().get()` on every render call. This is the scope pattern and it works.

4. **Widget renderer functions must be pure ImGui** — no state of their own except what's in `CustomGuiWidget.config`. All state lives in the `ofParameter` they are bound to.

5. **Designer mode must be completely inert in run mode.** No drag handles, no hit-testing, no sidebar code paths execute when `designMode == false`. Use a single bool flag.

6. **Auto-save layout on every designer change** using the same `notifyScopeChanged()` callback pattern used in `ofxOceanodeScope`. Never require the user to explicitly save the layout.

7. **Macro custom GUIs expose only router parameters.** When populating the designer's parameter sidebar for a macro, iterate only over the macro's router parameters, not all internal node parameters. This keeps macro encapsulation intact.

8. **Panel-local zoom is independent of canvas zoom.** The canvas zoom system (5 quantized font levels in `ofxOceanodeShared`) does not apply to custom GUI panels. Panel zoom is stored in `CustomGuiLayout.zoom` and applied as a simple multiplier to all cell dimensions.

9. **The `data/nodeGUIs/` folder** must be created at application startup if it doesn't exist. Store one JSON file per node type name (e.g., `data/nodeGUIs/testNode.json`). If the file doesn't exist, the node has no custom GUI.

10. **For the XYPad with two separate parameters**, store the second parameter's name in `config["paramY"]` and resolve it independently during panel initialization (same `resolveParameterFromPath()` call). Both parameters must belong to the same node.
