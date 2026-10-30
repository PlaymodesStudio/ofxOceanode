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

## Measuring

`python3 tests/buffer_benchmark.py` builds the production headers with the
mocks from `header_pro_regression.py` and prints microseconds per operation
for inserts, millisecond reads and HeaderPro ticks at several history sizes and
copy counts. Run it before and after a change.
