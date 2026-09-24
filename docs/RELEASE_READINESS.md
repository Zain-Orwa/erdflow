# Release readiness

> **Settled work.** Do not change, replace or re-style anything in this
> document to suit something new you have been asked to build. If what you are
> building genuinely contradicts what is written here, stop and ask Zain, who
> owns this project: say what you want to change, what the application will
> look like afterwards, and whether it is a gain or a loss. He decides.
> Full rule: [CLAUDE.md](../CLAUDE.md).

**Status:** CI foundation only; ERDFlow is not yet packaged or ready for public
distribution.

This document records the boundary between normal development, continuous
integration, and an intentional public desktop release. It does not authorize a
release or make a support claim by itself.

## Supported release targets

The first release is intended to support:

| Platform | Architecture | Current CI target | Release status |
| --- | --- | --- | --- |
| Windows | x86-64 | Windows Server 2025 runner, MSVC 2022 Qt kit | Build verification pending |
| macOS | arm64 | macOS 15 Apple Silicon runner | Build verification pending |
| macOS | x86-64 | macOS 15 Intel runner | Build verification pending |
| Linux | x86-64 | Ubuntu 24.04 runner | Build verification pending |

Passing CI means only that the current source configures, compiles, and passes
the automated test suite on the runner. It does not yet prove that an installer
works on every supported operating-system version.

The exact minimum Windows, macOS, and Linux versions remain a packaging-stage
decision. They must be based on the selected Qt release, compiler/runtime
requirements, package baseline, and installation tests on clean systems.

## Continuous integration

`.github/workflows/ci.yml` runs on pull requests, pushes to `main`, and manual
dispatch. Each target:

1. checks out the source with read-only repository permissions;
2. installs the same pinned Qt 6.11.2 release and the Qt SVG module;
3. configures a Release build with tests enabled;
4. compiles the complete application and tests; and
5. runs the CTest suite, including the offscreen desktop smoke test.

The workflow does not package, sign, upload, deploy, or create a GitHub Release.
It uses no ERDFlow credentials. Third-party actions are pinned to immutable
commit SHAs so their code cannot change merely because a tag moves.

CMake continues to express Qt 6.9 as the source-level minimum. CI uses a
currently supported Qt line for security and release-readiness testing. Before
packaging, the selected Qt release and Qt's separately published security
patches must be reviewed; a passing build is not evidence that a dependency has
no outstanding security update.

Because this is a private repository, GitHub-hosted runner usage consumes the
account's included Actions minutes and storage allowance. macOS and Windows
jobs have higher billing multipliers than Linux. The four-platform matrix is
deliberate because all four outputs are intended release targets.

## Development and release separation

Normal development remains:

```text
feature branch -> pull request -> Desktop CI -> review -> main
```

A future release must be a separate, explicit operation:

```text
release preparation PR -> tested main commit -> protected vX.Y.Z tag
  -> package/sign/notarize -> draft GitHub Release -> manual approval -> publish
```

No merge to `main` should create a public desktop release. A later release
workflow must validate that the tag version, CMake version, executable version,
package version, release manifest, and changelog agree before it handles any
signing credentials.

## Gates before packaging work

The following remain open after this CI foundation:

- Observe a successful run of every CI matrix job on GitHub.
- Fix platform-specific compiler or test failures without weakening warnings or
  disabling tests globally.
- Make one source of truth for the application version. The `.erdx` format
  version must remain separate.
- Bring the `.erdx` format document and compatibility fixtures up to the format
  version written by the application.
- Select exact minimum OS versions and test clean installations on them.
- Add OS-specific packaging without changing the domain or application layers.
- Complete the Qt and third-party distribution audit described in
  `QT_LGPL_COMPLIANCE.md`.
- Decide whether ERDFlow's existing MIT licence remains the intended licence
  for public binary releases. Making the repository private does not revoke
  rights already granted for copies released under MIT.
- Establish Windows signing and Apple Developer ID/notarization identities.
- Add checksums, an SPDX SBOM, provenance, release notes, and a protected manual
  publication gate.

## Repository settings to apply manually

After the first CI run is proven stable, repository administration should:

1. require pull requests for `main`;
2. require all four `Desktop CI` matrix checks before merge;
3. require branches to be current before merge;
4. prevent force pushes and deletion of `main`;
5. restrict creation or movement of release tags matching `v*`;
6. keep workflow permissions read-only by default; and
7. later protect a `release` environment with manual approval and access to
   signing credentials.

These settings are intentionally not changed by this repository commit.

## Next implementation stage

After the CI matrix succeeds, the next bounded stage should resolve version and
file-format documentation consistency. Packaging and signing should follow only
after that foundation is merged and stable.
