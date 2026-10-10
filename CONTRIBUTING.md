# Contributing to ERDFlow

Thank you for your interest in ERDFlow!

ERDFlow is a desktop-first visual database design application. It keeps a
**Conceptual Design** and the **Relational Schema** it becomes connected as two
views of the same project, guided by one principle:

> **Draw once. Progressively refine.**

Contributions of every size are welcome, from a typo fix to a new capability.
This guide explains how to take part without stepping on settled work.

## Ways to contribute

- **Bug reports**: [report a bug](https://github.com/Zain-Orwa/erdflow/issues/new?template=01-bug-report.yml)
- **Feature ideas**: [request a feature](https://github.com/Zain-Orwa/erdflow/issues/new?template=02-feature-request.yml)
- **Performance reports**: [report a performance problem](https://github.com/Zain-Orwa/erdflow/issues/new?template=03-performance.yml)
- **UI / UX improvements**: [suggest an improvement](https://github.com/Zain-Orwa/erdflow/issues/new?template=04-ui-ux.yml)
- **Documentation**: [report a documentation problem](https://github.com/Zain-Orwa/erdflow/issues/new?template=05-documentation.yml)
- **Tests and code**: [propose a contribution](https://github.com/Zain-Orwa/erdflow/issues/new?template=06-contribution-proposal.yml), or open a Pull Request for a small fix

Please do not report security vulnerabilities in public issues; see
[SECURITY.md](SECURITY.md).

## Before coding

- **Search first.** Check existing issues and Pull Requests for the same
  problem or idea.
- **Small fixes can go straight to a Pull Request.** A typo, a broken link, or
  a clear bug with a small, contained fix does not need a discussion first.
- **Discuss significant changes first.** Changes to behavior, architecture,
  the `.erdx` project format, Conceptual ↔ Schema conversion, or the user
  experience should start with a
  [Contribution Proposal](https://github.com/Zain-Orwa/erdflow/issues/new?template=06-contribution-proposal.yml),
  so the scope is agreed before you spend time on it.
- **One Pull Request, one coherent task.** Two changes share a Pull Request
  only when one genuinely cannot work without the other.

## Development setup

ERDFlow is built with **C++20**, **Qt 6** (Widgets), **CMake** and **CTest**.
The [development guide](docs/DEVELOPMENT.md) is the source of truth for exact
versions, platform notes and build commands. In short, from the repository
root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Architecture

ERDFlow follows a layered architecture with one-way dependencies; see the
[architecture document](docs/2.ERDFlow_ARCHITECTURE.md) and the
[architecture decision records](docs/adr/).

| Directory | Holds |
|---|---|
| `app/desktop` | The Qt Widgets desktop application: windows, canvas, panels and the composition root (`main.cpp`) |
| `application` | Editing commands, undo/redo history and coordination (`Editor`) |
| `domain` | The project model, validation and conversion rules |
| `infrastructure` | Adapters such as `.erdx` persistence and identity generation |
| `tests` | The CTest suites |
| `docs` | Product, architecture, ADRs, the `.erdx` format and implementation status |
| `assets` | Icons and images |

`domain` and `application` have no Qt dependency, and domain code stays
independent of UI concerns. Persistent edits go through `Editor`; the views
reflect them rather than changing the model themselves. What is actually built
today is recorded in [IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md).

## ERDFlow conventions

- **Write M:M, never M:N**, for many-to-many relationships.
- **Preserve typed IDs and level boundaries.** Identities are stable and
  strongly typed, and a Conceptual object and the Schema object it becomes are
  different objects linked by provenance, never the same ID reused
  ([ADR-001](docs/adr/ADR-001-STABLE-IDENTITY-STRATEGY.md),
  [ADR-008](docs/adr/ADR-008-CROSS-LEVEL-MAPPING-AND-PROVENANCE.md)).
- **Never silently destroy downstream or manual edits**
  ([ADR-010](docs/adr/ADR-010-CHANGE-PROPAGATION-AND-REGENERATION.md)).
- **Keep conversion deterministic**: the same input produces the same output.
- **Do not change unrelated behavior or UI while fixing something else.** Much
  of ERDFlow is settled work arrived at deliberately. If your change genuinely
  needs to alter existing behavior, say so in the Pull Request and explain what
  will look different afterwards.
- **Visual reference changes must be intentional and explained** (see below).
- **Preserve cross-platform behavior** on Linux, Windows and macOS.
- **Protect user and project data**: never include private databases, client
  data or confidential `.erdx` projects in issues, tests or examples.

## Making a change

1. Branch from the current `main` (in your fork or in a branch of your own).
2. Make the smallest coherent change that solves the problem.
3. Add or update tests that cover the behavior you changed.
4. Run the appropriate tests locally.
5. Review `git diff` before committing.
6. Open a Pull Request using the template, and link the related issue.

## Testing

Test behavior at the lowest useful layer: domain rules in `core_tests`, project
files in `persistence_tests`, and the desktop application in the desktop
suites, which run offscreen. The [development guide](docs/DEVELOPMENT.md)
explains the full CMake / CTest workflow, including Release builds and the
sanitizer configuration. To run one suite:

```sh
ctest --test-dir build -R desktop_window --output-on-failure
```

Continuous integration builds and tests every Pull Request on
**Linux x86-64**, **Windows x86-64**, **macOS Apple Silicon** and
**macOS Intel**. Please test locally where you can; you do not need all four
machines yourself.

## UI and rendering changes

- Screenshots, and before/after comparisons, make UI changes much easier to
  review.
- The `visual` suite compares rendered pages with reference pictures kept per
  platform in `tests/visual`. Reference pictures are never updated just to make
  a test pass: a change to one must be intentional, explained in the Pull
  Request, and agreed.
- Fonts and text rendering differ between platforms. When a check behaves
  differently on one platform, diagnose the cause before introducing a
  production workaround, and keep any fix platform-neutral.

## Documentation changes

Use ERDFlow's product terminology consistently: Conceptual Design, Relational
Schema, Convert to Schema, Convert to Conceptual, Import and Export, M:M.
Describe only what is actually built as available; planned capabilities belong
in the roadmap.

## Commit and Pull Request quality

- Keep commits focused, with meaningful messages that say what changed and why.
- Do not commit generated build output, logs, local screenshots or editor files.
- Never commit credentials, tokens or private data.
- Leave out unrelated files and unrelated cleanup.
- Third-party assets must come with a compatible licence recorded beside them,
  as the bundled Lucide icons do (`assets/icons-outline/LICENSE-lucide.txt`).

## License

ERDFlow is released under the [MIT License](LICENSE). By contributing, you
agree that your contributions are submitted under the same license.
