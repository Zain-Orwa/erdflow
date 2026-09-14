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
