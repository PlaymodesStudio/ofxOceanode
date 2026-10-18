# Remove unused nodes

Edit → Remove unused nodes reviews the active canvas. The popup lists the
nodes, connected groups and cables to remove, plus protected groups and the
reasons they are retained. Nothing is deleted until Remove is clicked. A graph
change requires Refresh; changes to bindings are checked again at confirmation.
The native deletion path releases cables, listeners and node resources.

A whole connected group is retained if any member has a Custom GUI widget,
snapshot value, Timeline binding, MIDI binding, Scope, persistent status or a
standalone/unclassified model. Bindings in closed panels and other canvases are
included. A protected group without a detected host binding is labelled
“Possibly unused, protected” and expanded for manual review.

This deliberately does not infer side effects from node names or parameter
counts. Portals, Macro interfaces, output/recording nodes and unknown addon
models are protected. A group connected to a used group is also retained,
including branches that might not contribute to an output. Run the command
inside a Macro to review that canvas; the command does not prune Macro internals
from its parent canvas.

Audited core numeric generators/processors and the Collection nodes declare that
they only do parameter/cable work through `registerProcessingModel<T>()`. Other
models can use this registration method or override
`bool canRemoveWhenUnused() const`. Return true only when the model has no
independent window, output, wireless link, global effect or externally-held
resource not represented by host bindings. The default is false. This explicit
policy lets addons extend coverage without silently deleting unknown work.

Validation: `python3 tests/unused_nodes_regression.py` runs the production
analysis and deletion functions with isolated host services under ASan/UBSan.
It covers cycles/chains, each host binding, protected models, stale previews,
cross-canvas references, comment pruning and a 2,000-node cleanup. Production
syntax checks cover the real host APIs; live host and hardware behavior still
need an application rebuild.
