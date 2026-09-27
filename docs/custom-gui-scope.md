# Scope widgets in Custom GUI

Choose **Add to Custom GUI > panel name (or New Custom GUI) > Scope** from a
parameter's context menu. Scope is available when the Scope registry can render
the parameter's type, including types registered by addons with `registerScope<T>()`.

Each panel can contain one ordinary widget and one Scope for the same parameter.
The widget-type picker also respects this limit. Remove an individual widget in
Edit mode; the node menu's **Remove from Custom GUI** removes all representations
of that parameter from the selected panel and labels the action accordingly.

## Appearance and interaction

- Move, resize, select, lock, and zoom a Scope like other Custom GUI widgets.
- Edit its label, hint, label visibility/font/color, body color, and accent color.
- Scope Controls enables the renderer's own controls in Run mode, such as a
  texture scope's aspect-ratio button. These controls are disabled in Edit mode.
- Standard ImGui scope drawings inherit the widget colors. Addon renderers that
  draw hardcoded colors or source-content colors retain them.
- Value editing, default-value reset, custom ranges, and value-display settings
  are not provided by the Scope widget. Its visualization is the registered
  renderer's existing behavior; it does not add signal history or recording.

Adding a Custom GUI Scope does not add the parameter to the Scopes window.
The two views have independent membership and layouts. Renderer settings stored
on the parameter itself (such as texture aspect ratio) are shared between views.

## Persistence and snapshots

The widget type, parameter reference, geometry, and appearance use the existing
Custom GUI JSON format. Existing widget type names and numeric enum values are
unchanged. A missing renderer displays `Scope unavailable`.

Scope-only parameters are excluded from snapshot capture and update. If the same
parameter also has an ordinary widget in the panel, it participates as before.
Recall also skips parameters currently represented only by a Scope, including
values stored before converting or removing their ordinary widget.

## Implementation and verification

The signal-widget registration supplies a Scope adapter around
`ofxOceanodeScope::drawParameter()`. A child region gives each instance a bounded
content area, clipping, and its own ImGui ID scope. Renderer registration and
the Scopes window's docking/persistence are unchanged.

Interactive checks when building a host application:

1. Add float/vector scopes to new and existing panels. Add a control for the same
   parameter in either order; check duplicate prevention and widget-type changes.
2. Resize and zoom, edit labels/colors, overlap widgets, lock/unlock, and remove
   either representation. Confirm the other representation remains usable.
3. Open the Scopes window and Custom GUI together. Check texture aspect ratio,
   an addon renderer using `GetContentRegionAvail()`, and multiple Scope widgets.
4. Check Edit/Run interaction and the Scope Controls toggle. Confirm double-click
   on the Scope does not reset the parameter.
5. Save/reload a preset and a macro containing a panel, and restart the host.
   Check missing/empty parameter data and a renderer unavailable at load time.
6. Capture/recall snapshots with control-only, Scope-only, and combined layouts.
   Convert a captured control to Scope and confirm old snapshots do not write it.
