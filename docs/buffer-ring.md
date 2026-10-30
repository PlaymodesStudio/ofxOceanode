# Buffer storage and Header reads

The `buffer<T1, T2>` template (`src/Nodes/Default_Nodes/Buffer/`) is the
history behind the Buffer, Header and HeaderPro nodes. This note records the
performance design so later changes keep it.

## Storage

- Frames sit in a ring. `head` is the physical index of the oldest frame, and
  logical index 0 is always the oldest. When the buffer is full, a new sample
  overwrites the oldest slot and advances `head`; no other frame moves. The
  frame's `T2` container is reused in place, which is what lets the texture
  buffer (`ofxOceanodeTextures`, `T2 = ofFbo`) keep its fbos allocated.
- Frames are kept in a `std::deque` for stable addresses. Only `setMaxSize`
  moves frames, once, to put logical and physical order back together.
- A frame stores the container, the `Timestamp`, and a pointer to the buffer's
  `bufferFunctions` (assign and return). The functions are owned once by the
  buffer, so a float frame is 24 bytes instead of 80.
- Timestamps are mirrored as `int64_t` microseconds in a contiguous vector with
  the same physical layout. All lookups read that vector, never the frames.

## Lookups

- The buffer counts `inversions`: adjacent pairs recorded out of timestamp
  order. It is maintained in O(1) on insert and recounted on `setMaxSize`.
  While it is zero (`isOrdered()`), `getClosestFrame`, the relative branch of
  `getClosestFrameDelayMs` and `getAvailableHistoryMs` binary search or read
  the ends directly. After a transport seek timestamps can go backwards; the
  original newest-first linear scans then run on the microsecond vector with
  identical results.
- Tie rules that the binary search reproduces: equal distances and equal
  timestamps prefer the newer frame; the oldest-by-time frame used on overflow
  is the newest among frames sharing the oldest timestamp; a NaN or negative
  delay reads the newest frame; a delay beyond the history sets overflow.
- `tests/buffer_ring_regression.py` runs random histories (seeks, duplicates,
  resizes, clears) through the ring and the previous deque implementation and
  requires the same frame for every query.

## Header and HeaderPro

- `buffer::revision()` changes on every insert, clear and resize. The readers
  recompute their samples only when a parameter or the revision changed. The
  Output is still assigned on every update and on every frame notification, so
  downstream nodes see the same events as before; what is skipped is the
  second computation of an identical result in the same tick.
- With `Calculate On Update` off, parameter listeners still recompute at once,
  as before.

## Transport-indexed mode

`buffer::setTransportIndexed(true)` turns the same slots into a sparse
timeline keyed by transport time. The Buffer node enables it with the
`Sync To Transport` inspector option.

- `order` lists the occupied physical slots sorted by stamp (equal stamps keep
  the later-recorded one last); `freeSlots` holds slots whose sample was
  replaced. The ring's `head` and inversion count are unused in this mode, and
  `isOrdered()` is true.
- `addFrame(value, stamp, replaceFromExclusive)` frees every sample stamped in
  `(replaceFromExclusive, stamp]`, plus any at exactly `stamp`, then stores
  the sample in a freed slot, a new slot while under `Max Size`, or a slot
  taken from a sample ahead of `stamp` (the farthest first) and otherwise from
  the oldest. Continuous playback passes the previous sample's stamp, so a
  second pass over a loop or a scrub refreshes slots in place; after a seek,
  stop or loop wrap the node passes one frame earlier so only the frame at the
  new position is replaced. The memory cost is therefore the same as the ring:
  `Max Size` frames, whatever is played.
- `setPlayhead(stamp)` anchors the reads. `getSize`, `getFrame(i, fromTail)`,
  `getClosestFrameDelayMs` and `getAvailableHistoryMs` see only samples at or
  before the playhead; a delay read targets `playhead - delay` and returns the
  nearest sample. The playhead bumps `revision()`, so Header and HeaderPro
  recompute as it moves.
- `setGapToleranceMs` (the node uses two frames of transport time) decides
  overflow: a read whose nearest sample is farther than that from its target
  sets it, and consecutive samples farther apart than that end the available
  history. So a seek into unplayed material reads the nearest sample with
  Overflow on, and `Fit Available History` compresses to what is contiguous
  behind the playhead.
- `setMaxSize` evicts by the same rule relative to the playhead and compacts
  the physical storage; switching the mode, or `clear`, empties the history.
- `bufferTransportClock` (`bufferNode.h`) derives the stamp and the replaced
  span from the frame-latched transport state: `seconds` as microseconds, with
  `generation` and `loopCount` marking discontinuities, and the last positive
  per-frame advance as the frame estimate. The node records on every Input
  change and once per frame while the transport plays; the Timestamp input is
  ignored.
- `tests/buffer_transport_regression.py` checks these rules on the buffer,
  compares random play/seek/loop sessions with a `std::map` model of them, and
  drives the Buffer node and HeaderPro through play, pause, seek, loop wraps,
  Rec and preset loading.

## Measuring

`python3 tests/buffer_benchmark.py` builds the production headers with the
mocks from `header_pro_regression.py` and prints microseconds per operation
for inserts, millisecond reads and HeaderPro ticks at several history sizes and
copy counts. Run it before and after a change.
