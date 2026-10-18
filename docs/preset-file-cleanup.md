# Preset and Macro file cleanup

Saving a canvas now removes generated node JSON files for nodes removed from
that saved canvas. It also removes the complete saved folder of a deleted local
Macro, including nested local Macros. This runs for preset saves, explicit local
or Global Macro saves, and `savePersistent()`.

Deletion from the live canvas alone does not touch the saved files. Reloading
without saving can still restore the previously saved graph. A preset save does
not change a skipped Global Macro definition, and deleting a Global Macro
instance never deletes its shared library folder.

Each saved canvas contains `.oceanode-node-files.json`, an ownership journal for
generated node files and embedded Macro directories. Existing canvases seed this
journal from their previous `modules.json` and local Macro references. Cleanup
uses these explicit ownership records rather than a filename sweep. Files for
unavailable addon types stay protected, including on later saves.

Core JSON writers check stream completion and replace individual files through
temporary files. Nested saves share their success state. Node lists are published
only after the participating writers succeed; obsolete files are pruned only
after all node lists have been published. Ownership is journaled before writing,
so a later successful save can retry cleanup after a failed save. This is not an
atomic transaction across the entire directory: a failure may leave some newer
files alongside the old graph, but it does not trigger obsolete-node cleanup.
Addon asset hooks retain their existing error reporting behavior.

Samples, SVGs, wavetables, layout files, snapshots and unrelated JSON beside the
node files are retained. Media inside a removed embedded Macro directory goes
with that directory. Shared media outside that directory is not collected.
Cleanup does not follow symbolic links. Failed removals are logged and retained
in the journal for a later attempt.

## Older leftovers

Use **Edit → Review leftover node files** on the active saved canvas. The review
checks that canvas and its saved local Macros. It offers unlisted JSON files with
canonical registered node names and recognizable obsolete embedded Macro
folders. Unknown addon files, unrelated files, shared Global Macro definitions,
and files used by the live graph are excluded. All candidates start unselected.

Select the files to remove, then choose **Remove selected files**. A Macro folder
entry removes its entire directory. Before removal, the saved graph, candidate
files and live ownership are checked again. Changes require a fresh review.
Opening or refreshing the review does not write files. Save pending patch edits
first to make the saved graph reflect the work you want to keep.

## Validation

`python3 tests/preset_file_cleanup_regression.py` exercises production node and
Macro persistence and the cleanup/review helpers with real OF JSON/filesystem
behavior under ASan/UBSan. It covers upgrade from existing saves, deletion and ID
reuse, Save As, nested saves, writer failures and retries, local-to-global changes,
shared library preservation, persistent nodes, missing addons, media, symlinks,
invalid journals, stale reviews and a headless ImGui modal render.

The Macro GUI, clipboard, New Preset and Timeline regressions cover the affected
save callers. Production syntax checks include MIDI/OSC. A rebuilt host is still
needed to verify windowed menu and dialog interaction.
