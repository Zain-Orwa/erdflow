# ERDFlow AI LinkedIn demo — isolated experiment

This folder is an **experimental harness**, not an ERDFlow product feature. All work is isolated on branch `experiment/ai-linkedin-demo`; do not merge without explicit approval.

## Objective

Compare **Claude alone** with **Jev + Claude**, build an approved Conceptual Design using ERDFlow's native model, convert it using the existing schema engine, and produce a visually compelling, repeatable recording for LinkedIn.

## Workflow design

1. **Live experiment (future integration):** call Claude to propose typed conceptual facts. Jev may perform supported bounded decisions (routing/classification), not freeform ERD generation. Validate all generated facts locally against native ERDFlow semantics. Record latency, errors, and cost. API credentials must come from environment variables and must never enter repository files.
2. **Approve:** serialize an actual, successfully opened native `.erdx` project. Store an immutable approved fixture separately from transient outputs; never produce guessed ERDX JSON.
3. **Replay (future integration):** reset a disposable in-memory project, progressively apply approved native editor commands with timer-driven animation, then invoke ERDFlow's existing Conceptual → Schema pathway. Preserve ordinary app behavior and user files.
4. **Capture:** use an external screen recorder for the initial prototype; MP4 export is not currently implemented by this harness. Label recordings accurately as live or replayed.
5. **Repeat:** user can reset and replay any number of times with consistent layout and timings.

## Current implementation status

The initial scaffold supplies a configurable storyboard and repeatable dry-run validation only, using Python standard library. It does **not** call Jev/Claude, animate the desktop editor, convert to Schema, or record video yet. Those require explicit native integration and tests; do not claim those features work.

From repository root:

```sh
python3 tests/ai_demo/run_demo.py --check
python3 tests/ai_demo/run_demo.py --storyboard
```

Next implementation must inspect `app/desktop/home_live_demo.cpp`, `app/desktop/conceptual_examples.cpp`, `application/editor.cpp`, `infrastructure/project_store.cpp`, and the schema conversion entrypoints. Only add functionality; do not alter settled UX. Unit and desktop tests, both Debug and Release, must pass before claiming done.
