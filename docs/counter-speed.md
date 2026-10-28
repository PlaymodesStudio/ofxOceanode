# Counter Speed

Counter includes the time scaling and phase compensation normally supplied by
Counter → Mod Time. `Speed` defaults to `{1}`. `Out` is now a float vector with
one value for each speed: `{1, 2, 0.5}` produces normal, double and half speed
clocks. Zero holds a lane. An empty Speed vector falls back to `{1}`.

Speed edits take effect at the next clock sample. That sample uses the old
rate to reach the change point; subsequent samples use the new rate. Editing
Speed alone does not emit Out. Several edits between samples are combined, so
no phase compensation is lost. A new lane inherits the first lane's phase
before continuing at its own rate. Removing lanes discards their state.

`Reset` restarts the free-running source and clears all lane offsets. At the
next update each lane starts from zero at its configured speed. This makes
Reset useful after speed changes: the old separate Mod Time retained its
compensation offset when only Counter's Reset was triggered.

A global phase reset clears speed compensation, as Mod Time's `resetPhase()`
did. With `RstWPhs` disabled, the source time keeps running and Out returns to
source time × Speed. With `RstWPhs` enabled, the free-running source also
restarts at zero.

`Sync To Transport` keeps its existing meaning: the source becomes transport
seconds and follows play/pause, seeks, stops, loops and frame stepping. Speed
scales this source with the same compensation, including backward jumps.
Local Reset and the source-reset part of RstWPhs remain ignored in this mode.
Disabling sync preserves every lane's phase and resumes free-running motion.

## Existing patches

Old presets without Speed use `{1}`, preserving Counter's previous timing.
Out keeps its name. Oceanode reads a saved scalar as a one-element vector and
existing connections to scalar inputs use the first output value. Code that
accesses Counter's Out directly as `ofParameter<float>` must use
`ofParameter<vector<float>>` instead.

To replace a Counter → Mod Time pair, transfer Mod Time's Speed setting or
connection to Counter's Speed, and reconnect consumers to Counter's Out.
Counter's new Speed must stay at `{1}` if the existing Mod Time remains in
the chain and should continue to supply all speed scaling. Mod Time itself
is unchanged and remains available for other time sources.

## Verification

`python3 tests/counter_speed_regression.py` runs the production Counter and
Mod Time with real openFrameworks parameters/events and ASan/UBSan. It checks
legacy scalar timing, independent vector rates, speed continuity, hold,
multiple edits, resets, resizing, empty vectors, transport play/pause/seeks/
loops, sync switching, and precision after long-running clock resets.
The host services are adapted; this does not launch or validate the host UI.
