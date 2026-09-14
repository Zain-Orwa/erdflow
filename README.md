<p align="center">
  <img src="assets/readme/hero-banner.png" alt="ERDFlow Hero Banner" width="100%">
</p>

<h1 align="center">ERDFlow</h1>

<p align="center">
  <b>Draw once. Progressively refine.</b>
</p>

<p align="center">
  A desktop-first visual database design environment for moving from
  <b>Conceptual ERD</b> → <b>Relational Schema</b> → <b>Physical Design</b> → <b>SQL</b> → <b>Data</b>.
</p>

<p align="center">
  <img alt="Status" src="https://img.shields.io/badge/status-conceptual%20prototype-blue">
  <img alt="Platform" src="https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20Windows-0a84ff">
  <img alt="Desktop First" src="https://img.shields.io/badge/focus-desktop--first-1f9d55">
  <img alt="License" src="https://img.shields.io/badge/license-MIT-green">
</p>

---

## Current implementation

A working single-page Chen Conceptual ERD editor: entities, attributes,
relationships, cardinality/participation, Explorer/Properties, undo/redo,
and versioned `.erdx` save/load. Advanced conceptual semantics and downstream
Schema, Physical, SQL, and Data workspaces are still planned.

Build with CMake 3.21+, C++20, and Qt 6.9+:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/erdflow --example
```

Read the [implemented features and remaining work](docs/IMPLEMENTATION_STATUS.md),
[development guide](docs/DEVELOPMENT.md), and [project format](docs/ERDX_FORMAT.md).
The product vision and artwork below include future capabilities.

## Overview

**ERDFlow** is being designed as a professional, cross-platform desktop application for understanding, designing, refining, and transforming database models.

It follows one connected workflow:

- **Conceptual ERD** — understand the business
- **Relational Schema** — structure the data
- **Physical Design** — optimize implementation details
- **SQL** — generate the code
- **Data** — explore and validate data

The core idea is simple:

> **Draw once. Progressively refine.**

Instead of redrawing the same project at every stage, ERDFlow keeps the stages connected as different representations of the same project.

---

## Visual Showcase

<p align="center">
  <img src="assets/readme/core-workflow.png" alt="ERDFlow Core Workflow" width="100%">
</p>

---

## Why ERDFlow?

ERDFlow is being built to combine:

- **Chen conceptual modeling**
- **Correct M:M modeling**
- **Basic and Convertible conceptual modes**
- **Deterministic downstream conversion**
- **Safe schema refinement**
- **Structured SQL generation**
- **Live Data Grid**
- **Cross-platform desktop experience**
- **Future-ready architecture for AI, plugins, and ecosystem integration**

---

## Feature Highlights

<p align="center">
  <img src="assets/readme/feature-highlights.png" alt="ERDFlow Feature Highlights" width="100%">
</p>

### Planned core capabilities

- **Chen Conceptual Modeling**
  - entities
  - attributes
  - relationships
  - weak entities
  - associative entities
  - ISA / generalization-specialization

- **Correct relationship modeling**
  - `1:1`
  - `1:M`
  - `M:1`
  - `M:M`

- **Basic vs Convertible Modes**
  - learn with a clean conceptual view
  - switch to a richer engineering-oriented mode
  - keep the same underlying model

- **Safe downstream refinement**
  - generated models stay reviewable
  - manual edits are protected
  - regeneration is controlled

- **Live Data Grid**
  - browse
  - sort
  - filter
  - validate
  - inspect imported/project data

---

## Architecture Overview

<p align="center">
  <img src="assets/readme/architecture-overview.png" alt="ERDFlow Architecture Overview" width="100%">
</p>

ERDFlow follows a clean layered architecture with one-way dependencies:

- **Presentation Layer**
  - Qt / C++ desktop UI
  - canvas
  - panels
  - dialogs
  - workspace views

- **Application Layer**
  - commands
  - undo / redo
  - task coordination
  - review / apply flows

- **Domain Layer**
  - pure domain models
  - validation
  - deterministic conversion rules
  - mapping and provenance
  - baselines
  - diff / review logic

- **Infrastructure Layer**
  - `.erdx` persistence
  - import / export
  - SQL parsing and generation
  - external data sources

### Important architectural principles

- **Stable IDs**
  - identities survive rename and refinement

- **Deterministic conversion**
  - same input should produce predictable output

- **Protected manual edits**
  - downstream user changes must not be destroyed silently

- **Background work**
  - imports, validation, generation, and export should not freeze the UI

- **Future Rust boundary**
  - performance-critical parts may later be implemented behind a stable boundary

---

## Safe Regeneration Philosophy

ERDFlow is designed around safe refinement.

Instead of replacing downstream work blindly, it will follow a review-oriented approach:

```text
Previous Generated Baseline
        +
Current User-Edited Model
        +
New Candidate
        ↓
Reviewable Change Proposal
```


## Documentation

- [Product and system quality requirements](docs/1.ERDFlow_PRODUCT.md)
- [Architecture](docs/2.ERDFlow_ARCHITECTURE.md)
- [Target domain model](docs/3.ERDFlow_DOMAIN_MODEL.md)
- [Scale strategy](docs/4.ERDFlow_SCALE.md)
- [Roadmap](docs/5.ERDFlow_ROADMAP.md)
- [Architecture review](docs/PHASE_0_REVIEW.md)
- [Current implementation choices](docs/adr/ADR-014-CONCEPTUAL-EDITOR-FOUNDATION.md)

The current development build has been verified on macOS arm64. Windows/Linux
support and release packaging remain unverified. The project is released under
the [MIT License](LICENSE); a dependency/module license review for
distribution has not yet been performed.
