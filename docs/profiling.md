# Profiling

Open the **Profiling** controller from the controllers menu. Profiling is off
by default and is not restored as an active capture when loading a project.

1. Choose an averaging window (60, 120 or 300 frames).
2. Press **Profile!** to start a fresh capture.
3. Run the state you want to inspect. The list refreshes four times per second.
4. Press **Stop profiling** to freeze the latest completed samples. Results stay
   available for sorting and filtering, including after node/preset changes,
   until the next capture. Changing the window while stopped applies to the
   next capture. Changing it while recording starts a fresh averaging window.

The default ranking is descending **Self ms/frame**. Click any column heading
to sort, or filter by node name or canvas/Macro path. Click a row to select and
center the live node. Deleted nodes and private MacroMixer source graphs can
still have captured results, but cannot be located on a visible canvas.
The selected node's entire row and its expanded output rows stay highlighted,
including while sorting, filtering or refreshing a live capture.

**Save** opens a native save dialog for a CSV file; **Copy** puts the same CSV
text on the system clipboard. Both export the current snapshot in table order,
respecting the node/path filter and including output rows for expanded nodes.
The suggested filename includes the local date and time when Save is clicked,
for example `Oceanode-Profiling-2026-10-30_15-42-08.csv`.
The CSV includes all table metrics, with additional **Output pin** and **Pin
type** columns. Output rows repeat their owning node's name and canvas path;
their CPU fields are empty. Unavailable rates/intervals are empty cells rather
than `--`. Numbers use decimal points with extra precision for analysis, and
names containing commas, quotes or newlines are escaped as CSV text.

| Column | Meaning |
| --- | --- |
| Self ms/frame | Node CPU wall time excluding nested instrumented work; includes its GUI. |
| Share % | Share of the measured nodes' self time, before filtering. Not OS CPU utilization or a percentage of the entire frame. |
| Inclusive ms/frame | Time including downstream processing and nested Macro contents. Do not sum this column across nodes. |
| Peak self ms/frame | Highest per-frame self time within the averaging window. |
| Update / Draw / Listeners / GUI ms | Average exclusive time in each category. These sum to self time. |
| Calls/frame | Timed lifecycle, GUI, parameter and group notification scopes per frame. Not the number of value changes or calculations. |
| Out events/frame | Average emitted notifications from output-capable pins per sampled frame. The node row sums its pins. |
| Out events/s | Notifications divided by the actual elapsed duration of the retained frames; follows the real FPS. |
| Out interval ms | Mean spacing between publications of one pin. `--` means fewer than two comparable publications, or multiple active pins on a node row. |

Expand a node with its arrow to see each output pin's publication metrics.
For a pin publishing once per frame at 60 FPS, expect approximately **1/frame,
60/s, and 16.67 ms**. With two such pins, the node totals are **2/frame and
120/s**; expand it to read the separate **16.67 ms** intervals. These intervals
describe publication frequency; the CPU timing columns describe processing cost.

Output-capable means a registered node parameter with an enabled output pin,
including disconnected pins. Dedicated outputs are marked **Output pin**.
Bidirectional pins are marked **I/O pin** and also count notifications caused by
incoming values or UI edits. Input-only pins, Inspector parameters, custom GUI
regions without output pins, and group notifications do not add publication
events. Each local parameter notification counts once, regardless of cable
fanout. Assigning the same value again counts if OF emits a notification;
assignments with events disabled do not. Void output triggers are included.

Publication samples use the same completed-frame window and stop/freeze controls
as CPU measurements. Idle frames reduce events/s and events/frame. Intervals use
the actual timestamps of consecutive observed publications within the window;
at least two are needed, and discarded frames break interval continuity. Short
bursts can have a near-zero interval even when the overall events/s is low.
Texture/audio content changing in place without a parameter notification is
not a publication here; these are Oceanode message rates, not GPU frame rates
or audio sample rates.

Idle frames count toward averages. A frame starts at the root Oceanode update;
its draw and subsequent GUI events are committed at the next root update.
Starting/stopping discards the partially recorded frame. Macro containers share
the root session, including active private MacroMixer source graphs. A shared
timing stack avoids charging downstream processing to upstream self time and
counts each node's inclusive duration only at its outermost entry.

## Coverage and cost

All instrumentation is in Oceanode core. Addon nodes need no changes:

- Node-model `update()` and `draw()` use scoped timers.
- Registered Oceanode parameters, including custom types and void triggers,
  acquire first/last-priority timing listeners only during capture. Parameter
  wrapper copies share a single listener pair.
- Typed Inspector parameters and the node/Inspector parameter-group events are
  covered. The type-erased `addInspectorParameter(ofAbstractParameter&)` overload
  has group-level coverage; raw individual callbacks through that overload are
  not separately intercepted.
- Custom parameter `receiveParameter()` callbacks are timed at the receiving
  node, even when they do not assign a standard OF parameter.
- Node GUI construction and the selected node's Inspector rendering are timed.

When stopped, timing listeners are unsubscribed, no clock readings or sample
aggregation occur, and the controller stops refreshing its snapshots. Lifecycle
entry points retain a cheap disabled flag check. Profiling does add overhead
while recording, especially in graphs with many very small notifications.
The controller's own rendering/aggregation is not charged to a node.

These are **main-thread CPU elapsed times**, including waits within measured
callbacks. They do not measure GPU execution, audio/worker thread processing,
external SuperCollider CPU, arbitrary global event handlers, or custom GUI
panels outside the instrumented node/Inspector paths. GPU submission or a GPU
wait can appear as CPU time. No GPU synchronization is introduced.

OF callbacks returning `true` can consume a parameter event before the final
timing listener runs. Unbalanced scopes discard the affected frame and show an
incomplete-scope count, rather than attributing subsequent work to the wrong
node. Exceptions and graph teardown can produce the same diagnostic. Callbacks
using the absolute first/last integer priorities, and parameters shared across
different node owners, can prevent precise listener attribution.

## Validation

`python3 tests/profiling_regression.py` uses real OF parameter events and ImGui
with ASan/UBSan. It covers disabled capture, listener installation/removal,
nested accounting, rolling idle frames and peaks, wrapper-copy lifetime,
recursive group events, void triggers, custom receive callbacks, ignored worker
notifications, consumed-event recovery, session reassignment, capture buttons,
frozen snapshots, sorting, filtering and a narrow controller window.
Publication coverage includes 60 Hz and 30 Hz signals, idle-window eviction,
same-value assignments, multiple pins/fanout, bursts, missing intervals,
direction flags, void triggers, discarded frames, worker exclusion, copied
wrappers, node totals, expanded rows and frozen rates.
Export checks cover CSV escaping, decimal-point formatting, filtered/expanded
rows, matching saved/copied contents, cancellation and filename extensions.
Selection checks cover clicking across the row, full-width backgrounds,
expanded output backgrounds and selection surviving live refreshes. Native
dialogs and clipboard writes are substituted in this headless harness.

`python3 tests/profiling_regression.py --native` reuses the native Macro fixture
to exercise production node creation, lifecycle dispatch, typed/type-erased
node parameters, Inspector parameters, persistent nodes, nodes created during
capture, nested Macro sessions, stopping and graph teardown under ASan/UBSan.

`python3 tests/integration_compile.py --midi-osc` checks the production core
translation units. Regenerate the host project if its source list does not yet
include `src/Controls/ofxOceanodeProfilingController.cpp`, then rebuild to use the
new controller. Headless checks do not establish windowed host behavior or
real-world overhead in a particular patch.
