# Remove unused nodes

Edit → Remove unused nodes reviews the active canvas in a non-modal window.
It lists the nodes that nothing live uses, in groups of connected nodes, plus
the nodes it keeps and why. Nothing is deleted until Remove is clicked, and one
undo (Cmd/Ctrl+Z) restores the whole removal.

## What counts as unused

A node is **live** when it does useful work, or when something live consumes its
output. Every other node is **dead**. The analysis marks live nodes backward
from the nodes that do useful work, which is why it finds a whole dead chain,
a dead cycle or a dead branch in one pass.

A node does useful work on its own, and is called a **sink**, when any of these
hold:

| Reason shown | Rule |
| --- | --- |
| `sink: declared` | The model is registered with the `Sink` flag, or overrides `isSink()`. |
| `sink: no outputs` | The node publishes no output pin. It exists for its effect (display, sender, recorder, hardware). |
| `sink: router` | Routers are a Macro's interface pins. |
| `sink: persistent` | The node is persistent. |
| `sink: scope <p>`, `sink: timeline <p>` | A parameter is in the Scope or driven by a Timeline. |
| `sink: binding <p>` | A Custom GUI widget, snapshot or Timeline track is bound to a parameter, in any canvas, including a closed panel. |
| `sink: MIDI <p>` | A parameter has a MIDI binding. |

A node is also kept for these reasons:

| Reason shown | Rule |
| --- | --- |
| `feeds: <node>` | A cable leads to that live node. The first one found is shown. |
| `feeds: outside canvas` | A cable leaves this canvas. |
| `connection: outside canvas` | A cable arrives from another canvas. Deleting the node would delete a cable owned elsewhere. |
| `portal: <name> used by <node>` | A portal and its matching portals, through the portals' name and scope rules, or a portal selector that reads it. A writer portal is live when a matching portal elsewhere is consumed, in this canvas or another. |
| `macro: contains <node>` | A Macro whose inside holds a sink that is not a router pin, or a writer portal that broadcasts outside the Macro. Nested Macros count. |

Everything else is dead. Dead nodes that are joined by cables form one group.
A dead node may still have cables from live nodes: those cables go with it.

Run the command inside a Macro to review that canvas. The command does not look
into a Macro from its parent beyond the rules above.

## The review window

- Each dead group has a checkbox. Untick a group to keep it. Ticks survive
  Refresh and the refresh after Remove while the group stays the same.
- Remove deletes the ticked groups only. It is disabled when nothing is ticked,
  when the canvas changed since the plan was made, or when the canvas is closed.
- Before deleting, Remove recomputes the plan and refuses if the canvas or the
  dead set changed, then shows the revised plan.
- The window stays open after Remove so you can review the next candidates.
- Click any listed node, kept nodes included, to open its canvas, select it and
  center the view. Macro canvases open automatically.

## Writing a node

Most nodes need nothing. Use `addOutputParameter` for the values a node writes,
as nodes already do, and the shape rule does the rest.

| Your node | What to do |
| --- | --- |
| Computes outputs from inputs (generator, mapper, filter) | Nothing. Mark written pins with `addOutputParameter`. It is removed when nothing consumes it. |
| Has no output pins (display, sender, recorder, hardware out) | Nothing. It is a sink by shape. |
| Does something **and** has output pins (a synth that plays audio and exposes a bus, a recorder that passes data through) | Register it with the `Sink` flag, or override `isSink()`. |
| Is pure but its output is a plain `addParameter` (a number box) | Override `isSink()` to return false, or switch the pin to `addOutputParameter`. Otherwise the shape rule keeps it. |

```cpp
o.registerModel<mySynth>("Audio", ofxOceanodeNodeRegistrationFlags::Sink);
```

Flags can be combined with `|` and go after the category and before any
constructor arguments. The flag is remembered per model name, so refreshing or
copying the registry keeps it.

The default errs toward keeping. A node that writes outputs through plain
`addParameter` is treated as a sink until its author marks the outputs or
overrides `isSink()`. The risk in the other direction is a node that has output
pins and hidden side effects but no `Sink` flag: it would be listed as removable.
The window lists it before anything is removed, and undo brings it back.

`ofxOceanodeNodeRegistrationFlags::CanRemoveWhenUnused` is deprecated. It still
compiles and is ignored. Remove it from registrations when convenient.

## Core audit

| Model | Decision |
| --- | --- |
| Oscillator, Chaotic Oscillator, Phasor, Counter, Ramp, Noise, Random, Indexer, Mapper, Ranger, Reindexer, Smoother, Switcher, Curve | Pure. |
| Number, Number01 | Pure by `isSink()` override: their only output is a plain parameter. |
| Beat Transport, Preset Load Trigger, Global Variables | Pure: they only publish values. |
| Global Pitch | Sink by shape (no outputs). |
| MIDI Clock | `Sink` flag: Drive Transport can change the transport. |
| Macro Mixer | `Sink` flag: it morphs Macros. |
| Routers | Sink (`isSink()` override). |
| Portals | Never a sink by themselves: live through cables and matching portals. |
| Portal selectors | Pure: live through cables. They keep the portal they read. |
| Macros | Never a sink by themselves: live through cables or a sink inside. |

Sibling addons need the same audit. Until it is done, their nodes with output
pins and external effects (SuperCollider synths and recorders, texture senders,
NDI, Art-Net, laser, MIDI and OSC output) can be listed as removable. Review
the window before clicking Remove.

## Limits

- Dynamic sinks whose status depends on runtime state (a recorder only while
  recording) are not modelled. `isSink()` is read at review time.
- A portal in another canvas counts as consumed when it has an out-cable or a
  selector reads it, even if that canvas is itself dead. Review that canvas
  next, then run this one again.
- Models registered with refreshModel keep their registered flags. A model that
  was never registered has none.

## Tests

- `python3 tests/unused_nodes_regression.py` runs the production analysis and
  removal with mocked host types under ASan/UBSan. It covers every rule above,
  chains, cycles, dead branches, stale plans, unticked groups, the undo bracket,
  order independence and a 2,000-node graph.
- `python3 tests/unused_nodes_window_regression.py` drives the review window
  with real ImGui mouse events: non-modal use, navigation, ticking, the window
  staying open after Remove, stale canvases and expired canvases.
- `python3 tests/node_registration_flags_regression.py` checks the registration
  overloads, the `Sink` flag, the ignored deprecated flag, overrides, copies and
  refresh paths.

Live host behaviour and hardware still need an application rebuild.
