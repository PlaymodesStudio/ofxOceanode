# Workspaces

Workspaces are named window arrangements belonging to one preset. Arrange your
windows for sound, laser, images, or another task, then choose **Workspaces →
Save current as…** and give the arrangement a name. Select a name in the same
menu to recall it.

Each snapshot stores docking splits and sizes, window positions, tab order and
selected tabs, controller visibility, node window toggles offered by the Window
menu, macro canvas visibility, and Custom GUI visibility, including inside nested
macros. It leaves signal values, connections, transport, and output settings alone.
New nodes or panels added after a snapshot keep their current visibility.

Use **Update current workspace** to replace the selected snapshot with your
current arrangement. **Rename current…** changes its name; **Delete current…**
removes its saved snapshot and keeps the windows currently on screen.
Rearranging windows or saving the preset does not replace a snapshot: use Update
when you want those changes to become part of the named workspace.

The collection and last selected workspace live in `workspaces.json` inside the
preset folder. Changes are saved immediately for an existing preset; an unsaved
patch keeps them in memory until its first Save. Save As carries the collection
into the new preset. Loading a preset restores its last selected snapshot. New
starts an empty collection, and presets without workspaces continue to use their
existing layout.

While a named workspace is active, one arrangement spans all canvas tabs, so
focusing a macro does not replace it with the macro's individual layout. The
configured “GUI layout changes with macros” preference resumes when there is no
active named workspace.

Window toggles use the same recognition as the Window menu (`Show`, `Show Win`,
`Show Window`, `Show GUI`, `Show Editor`, `Show Snapshot`, `Show All`, `Show Split`).
An addon's window controlled only by private state or a differently named toggle
retains its visibility; ImGui still restores its saved docking when it opens.
