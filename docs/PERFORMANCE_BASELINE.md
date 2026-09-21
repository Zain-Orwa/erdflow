# Conceptual editor performance baseline

**Recorded:** 2026-09-14  
**Host:** macOS arm64, Apple Clang 21.0.0, Qt 6.11.2, CMake 4.3.4  
**Build:** Release, Qt offscreen platform

Run from the repository root:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
QT_QPA_PLATFORM=offscreen ./build/release/editor_benchmark
```

The deterministic workload is in `tests/editor_benchmark.cpp`. It creates 1,000
entities through normal commands, adds 250 relationships with 500 participants,
projects them into the canvas, dispatches 100 live drag events, commits/renames,
and checks an encode/decode roundtrip. IDs vary between runs; fixture structure
and operations do not.

| Measurement | Observed |
| --- | ---: |
| Create 1,000 entities, including validation and command history | 127.32 ms |
| Initial projection/show of 1,250 nodes and 500 connectors | 109.41 ms |
| Dispatch/process 100 offscreen drag events | 13.24 ms total |
| Rename, full validation, and canvas synchronization | 1.25 ms |
| Encode, decode, validate, and compare full graph | 23.04 ms ¹ |
| Encoded file size | 744,851 bytes |
| Conservative retained history estimate | 1,255,692 bytes |

These are one local baseline run, with other build-verification processes also
active. They are not latency percentiles or release acceptance thresholds.
The drag figure includes synthetic event dispatch and offscreen painting; it
does not measure physical input-to-display latency, native GPU/compositor cost,
or the Explorer/Properties shell. The initial projection includes Qt font/UI
initialization. The history value estimates command storage, not peak process
memory. Larger/richer graphs and native input still need measurement.

The canvas tests separately verify that moving one node updates only incident
connectors during the gesture, preserves model state until release, and commits
one undoable move. Group snapping preserves relative positions and clamps the
whole group inside canvas bounds. Synchronization retains unchanged item identity
and the viewport. These structural checks support the current QGraphicsView
choice without implying that full-scale production engineering is complete.

¹ Re-measured at 17.34 ms after the loader change recorded below. The other
rows in the table are from the original run and were not re-recorded, so they
are not mixed with later measurements.

## Dragging on a large diagram

**Measured:** 2026-09-21, macOS 26.6.2 arm64, Apple Clang 21.0.0, Qt 6.11.1,
CMake 4.3.4, Release, Qt offscreen platform.

The drag row in the table above was recorded before the canvas drew cardinality
and role labels on its connectors, and before a dragged element lined itself up
with its neighbours. Both arrived afterwards on the path a pointer move takes,
and neither was free:

| Dispatch/process 100 offscreen drag events | Observed |
| --- | ---: |
| As recorded 2026-09-14, before either feature | 13.24 ms |
| With both, before the changes below | 379 ms |
| With both, after them | 87 ms |

Two things cost the time, found by sampling rather than by reading:

- **Every visible connector reshaped its labels on every repaint.** Laying text
  out means shaping it, and `EdgeItem::paint` asked for that work again for each
  label each frame; it was 37% of the samples. A label says the same handful of
  characters frame after frame, so what was shaped is now kept until the text or
  the room it has changes. This alone took 379 ms to 138 ms.
- **An alignment guide asked for the whole viewport back whenever it appeared,
  moved or went away.** A guide is a hairline, but repainting it repainted every
  shape behind it. Only the strip a guide was or now is in is asked for now,
  which took 138 ms to 87 ms. Gathering what a drag can line up against once
  when it begins, rather than asking every shape for its rectangle again on
  every pointer move, is part of the same change and is what makes the narrowed
  repaint safe to rely on.

What remains is drawing the diagram itself, which is what the 2026-09-14 figure
measured when the canvas drew less. The other rows of the table above have not
been brought back to their recorded values and are not regressions in the same
sense: the model has grown since it was recorded, which the same fixture shows
directly — its encoded file went from 744,851 to 970,503 bytes and its retained
history from 1,255,692 to 2,563,148. Creating a thousand entities validates the
whole project a thousand times, and there is more of each entity to validate
than there was. Current figures on the host above, for the record rather than as
a target: create 1,000 entities 280 ms, initial projection 168 ms, rename and
synchronize 5.8 ms, encode/decode roundtrip 22.4 ms.

Both suites pass in Debug and Release after these changes, including the canvas
tests that check a dragged element still meets its neighbour's edge and middle
exactly, and a new one that checks it still meets an element far away across the
diagram -- which is the case a search that only looked nearby would have lost.

## Untrusted input rejection

**Measured:** 2026-09-14, same host, Release, after the table above.

A `.erdx` file is the only untrusted input this stage accepts, and the loader's
duplicate-key preflight runs before any format check. Every file therefore paid
that cost, including files that are not ERDFlow projects at all.

The workload builds a maximum-size 8 MiB input of repeated small objects and
times a full `decode` to rejection:

| 8 MiB input | Before | After |
| --- | ---: | ---: |
| ~1.05M plain object keys | 515.81 ms | 216.06 ms |
| ~700k escaped object keys | 368.86 ms | 175.98 ms |
| No object keys (parse-only floor) | 28.90 ms | 28.71 ms |

The preflight previously invoked `QJsonDocument::fromJson` once per key and
built an ordered container per object. It now unescapes key tokens directly and
settles duplicates by linear scan over a pooled buffer, which the sixteen-key
object cap already bounds. The remaining distance from the parse-only floor is
per-key string allocation; it was left in place because the operation is
already bounded and the simpler code is easier to keep correct.

This reduces a worst-case freeze; it does not make the load path asynchronous.
Moving file work off the UI thread is still the ADR-007 decision due at the
first expensive operation, and cancellation and a parser memory budget remain
unimplemented.

## Correctness baseline

All five CTest suites passed in Debug, Release, and a Debug build with
AddressSanitizer + UndefinedBehaviorSanitizer on the same host:

- Core domain/command behavior.
- Real persistence adapter and invalid-file isolation.
- Canvas interaction and projection behavior.
- Integrated desktop editing/open/save workflow.
- Application startup with the example.

The rendered example was visually inspected. Tests use offscreen Qt and do not
establish Windows/Linux support, native dialog behavior, full accessibility, or
leak coverage on platforms without LeakSanitizer. Prebuilt Qt itself is not
sanitizer-instrumented. Source built cleanly with the configured warning flags.

Next measurements should cover native interaction, shell refresh costs, peak
memory and larger graph fixtures before changing container, rendering,
validation, or executor architecture.
