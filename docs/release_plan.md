# ERDFlow release plan

> **Settled work.** Do not change or replace an agreed decision in this plan
> while implementing something new. If new work genuinely contradicts it, stop,
> explain the user-visible change and its trade-off, and ask Zain to decide.
> See [CLAUDE.md](../CLAUDE.md).

- **Plan date:** 2026-09-22
- **Status:** Agreed direction; staged implementation is incomplete
- **Release authority:** Zain
- **Source of truth:** Private GitHub repository

This document controls how ERDFlow is prepared, built, approved, published, and
later presented publicly. Supporting gates are documented in
[Release readiness](RELEASE_READINESS.md) and
[Qt LGPL compliance](QT_LGPL_COMPLIANCE.md).

Every major implementation stage stops for Zain's approval. Nothing here
authorizes an automatic public release, purchase, DNS change, or account change.

## 1. Agreed product direction

- ERDFlow remains a native C++20/Qt desktop application.
- There is no browser ERD editor and no initial WebAssembly target.
- Existing desktop behavior and active development must not be rewritten merely
  to enable distribution.
- GitHub is the source of truth for source, CI, tags, release metadata, and
  release artifacts.
- The repository is private. It was previously public.
- ERDFlow is currently free and non-commercial. Future commercial use requires
  a new cost, terms, licensing, privacy, and consumer-law review.
- GitHub Releases will distribute native installers and packages.
- Vercel will later host only the marketing, documentation, legal, release-note,
  and download website—not the native application runtime.
- The website will initially use a subdomain of an existing domain controlled
  by Zain. Do not purchase `erdflow.app` or change DNS yet.
- The legal operator is an individual in Austria. Private operator/contact
  details will be provided separately for legal pages and must not be casually
  committed to source.
- Accounts are optional. Local editing must remain available without an account
  or network connection.
- Planned login methods are Google, GitHub, Microsoft, and passwordless email.
- Analytics may be added later, but must never collect ERD/project contents,
  entity or attribute names, SQL, filenames, filesystem paths, database
  credentials, or other project information.
- Accounts and analytics do not block the first downloadable release.
- Qt will initially use the LGPL distribution route. ERDFlow has no commercial
  Qt licence.

## 2. Initial release targets

| Platform | Architecture | Initial package | Trust requirement |
| --- | --- | --- | --- |
| Windows | x86-64 | Signed per-user `.exe` installer | Stable publisher signature |
| macOS | arm64 | Signed/notarized `.app` in `.dmg` | Developer ID and notarization |
| macOS | x86-64 | Signed/notarized `.app` in `.dmg` | Developer ID and notarization |
| Linux | x86-64 | AppImage | Checksums and detached signature |

Windows will target currently supported 64-bit Windows versions compatible
with the selected Qt release and MSVC 2022 runtime. Exact minimum versions are
declared only after clean-machine testing.

Both macOS architectures are required. Separate labelled DMGs are acceptable
initially. A universal binary is considered only after both architectures and
all embedded components are proven compatible.

Linux ARM64, Windows ARM64, `.deb`, Flatpak, Snap, MSIX, and app-store
distribution are later options.

## 3. Release architecture

```text
local development
      |
feature branch -> pull request -> cross-platform CI -> review -> main
                                                          |
                                            protected SemVer tag only
                                                          |
                                               release workflow
                                       Windows / macOS / Linux
                                                          |
                                      package, sign, test, verify
                                                          |
                                            draft GitHub Release
                                                          |
                                               manual approval
                                                          |
                                          published GitHub Release
                                                          |
                                      future Vercel download website
```

Responsibilities remain separated:

- `domain/` and `application/` remain independent of website deployment.
- `app/desktop/` remains the Qt presentation layer.
- `infrastructure/` owns native adapters such as `.erdx` persistence.
- Future `packaging/` directories own OS-specific distribution definitions.
- `.github/workflows/` owns repeatable CI and release orchestration.
- A future `web/` directory owns the Vercel website.
- Future identity, API, and telemetry components remain optional services and
  do not become dependencies of local editing.

## 4. Development and release control

Normal development follows:

```text
feature branch -> pull request -> required CI -> review -> main
```

Rules:

- Merging to `main` never creates a desktop release.
- Release preparation occurs in a focused pull request based on `main`.
- The preparation PR updates version, changelog, release notes, compatibility
  notes, dependency evidence, and the release checklist.
- A protected annotated tag such as `v0.1.0` identifies the exact tested commit.
- Release tags are never moved or reused.
- Published artifacts are never silently replaced. Corrections receive a new
  patch version.
- Beta releases use tags such as `v0.2.0-beta.1` and remain separate from the
  latest stable release.

Recommended manual GitHub settings after CI is proven:

1. Require pull requests and required checks for `main`.
2. Require branches to be current before merge.
3. Prevent force pushes and deletion of `main`.
4. Protect tags matching `v*`.
5. Keep default workflow permissions read-only.
6. Protect a `release` environment with manual approval.
7. Limit signing credentials to that environment.
8. Enable immutable GitHub Releases after automation is verified.

These settings require Zain's manual authorization.

## 5. Continuous integration

The prepared `.github/workflows/ci.yml` matrix contains:

- Windows x86-64 on `windows-2025`;
- Linux x86-64 on `ubuntu-24.04`;
- macOS arm64 on `macos-15`; and
- macOS x86-64 on `macos-15-intel`.

Each job installs Qt 6.11.2 with Qt SVG, configures a Release build, compiles
the application and tests, and runs the complete CTest suite including the
offscreen desktop smoke test.

The workflow has read-only permissions, uses no ERDFlow secrets, pins external
actions to commit SHAs, and does not package, sign, upload, deploy, or publish.
Because the repository is private, it consumes GitHub Actions allowances;
Windows and macOS have higher billing multipliers than Linux.

The matrix must pass on GitHub before becoming a required merge gate.

## 6. Versions and `.erdx` compatibility

ERDFlow uses Semantic Versioning during initial development:

- `0.y.0` for planned feature releases;
- `0.y.z` for compatible fixes; and
- `-beta.N` for prereleases.

The exact first public version is confirmed in its preparation PR. Before
tagging, one application-version source must feed CMake, executable metadata,
Windows installer metadata, macOS bundle metadata, Linux metadata, artifact
names, release manifest, and website display.

Application version and `.erdx` format version remain separate. The current
implementation writes format version 25. Documentation and fixtures must
accurately describe supported versions and migrations before release. A file
migration must be deliberate, tested, documented, and safe for user work.

## 7. Platform packaging

### Windows x86-64

1. Build Release with MSVC 2022 and the selected supported Qt release.
2. Use `windeployqt` for shared Qt DLLs and plugins.
3. Audit every bundled file and licence.
4. Install the supported Visual C++ runtime correctly.
5. Produce a per-user `.exe` installer with normal install, upgrade, and
   uninstall behavior.
6. Include licences, notices, Qt source instructions, and support links.
7. Sign the executable and installer using one stable publisher identity.
8. Verify signatures and behavior on clean supported Windows systems.
9. Treat SmartScreen reputation as a launch risk; never fall back to unsigned
   production artifacts.

MSIX is a later option, not the first package.

### macOS arm64 and x86-64

1. Configure a proper `.app` bundle and stable bundle identifier.
2. Build and test both architectures independently.
3. Use `macdeployqt` to embed shared frameworks and plugins.
4. Audit every embedded component and licence.
5. Sign nested components and the application with Developer ID Application.
6. Enable hardened runtime and timestamp signatures.
7. Notarize with `notarytool` and staple the ticket.
8. Place each application in an architecture-labelled `.dmg`.
9. Test first launch, file operations, upgrade, and removal on clean Intel and
   Apple Silicon systems.

A `.pkg` is unnecessary unless privileged/shared system components are added.

### Linux x86-64

1. Select a conservative documented glibc/runtime baseline.
2. Build Release on that baseline.
3. Produce an AppImage with needed Qt libraries and X11/Wayland plugins.
4. Audit dynamic dependencies and licences.
5. Test Ubuntu-family and Fedora-family systems and applicable X11/Wayland
   sessions.
6. Verify launch, file operations, desktop integration, replacement, and
   removal instructions.
7. Publish SHA-256 checksums and a detached signature.

AppImage is not a promise of compatibility with every Linux distribution.

## 8. Qt LGPL distribution gate

Free/non-commercial distribution does not waive Qt obligations. The initial
release dynamically links LGPL-eligible Qt components. No static Qt build or
unintended GPL-only module may ship.

Before distribution ERDFlow must:

1. Inventory every library, plugin, runtime, installer component, asset, and
   transitive dependency in the final packages.
2. Record versions, origin, licence, notices, source archive, configuration,
   and modifications.
3. Keep shared Qt components replaceable by recipients.
4. Avoid technical or contractual restrictions that override LGPL modification
   and necessary reverse-engineering rights.
5. Ship LGPLv3, referenced GPLv3, Qt, Qt third-party, ERDFlow,
   Lucide/Feather, and all other required notices with each package.
6. Keep exact corresponding Qt source, patches, and build information under
   ERDFlow-controlled availability; an upstream-only link is insufficient.
7. Provide tested replacement/relinking instructions for each platform.
8. Publish source for any LGPL component modified by ERDFlow.
9. Generate and retain an SPDX SBOM and release-specific evidence.
10. Obtain professional review of the source-offer method, retention, terms,
    and macOS signing/replacement interaction.

The complete checklist is in [Qt LGPL compliance](QT_LGPL_COMPLIANCE.md).

ERDFlow currently uses an MIT licence. Before public binary distribution, Zain
must confirm whether MIT remains intended. Making the repository private does
not revoke rights previously granted for MIT-licensed copies.

## 9. Automated release workflow

The future workflow is triggered only by an intentional protected SemVer tag:

1. Verify the tag points to an allowed `main` commit.
2. Verify tag, source version, package versions, changelog, and manifest agree.
3. Run the complete test matrix again.
4. Build every platform in a clean environment.
5. Package all four initial targets.
6. Sign Windows and sign/notarize/staple macOS.
7. Verify packages, signatures, dependencies, and launch behavior.
8. Generate checksums, detached signature, SPDX SBOM, provenance, manifest,
   and release notes.
9. Create a draft GitHub Release and upload artifacts.
10. Stop at a protected manual approval gate.
11. Publish only after Zain approves the evidence.

One failed platform blocks a stable release. The workflow must not publish an
unsigned or partial stable release.

## 10. Release artifacts

```text
ERDFlow-<version>-windows-x86_64-setup.exe
ERDFlow-<version>-macos-arm64.dmg
ERDFlow-<version>-macos-x86_64.dmg
ERDFlow-<version>-linux-x86_64.AppImage
SHA256SUMS
SHA256SUMS.sig
ERDFlow-<version>.spdx.json
release-manifest.json
```

Each release also provides release notes, upgrade information, supported
systems, known issues, install/removal instructions, support/security contacts,
Qt source/notice instructions, and provenance information.

GitHub Releases is the initial distribution service. A separate CDN is added
only when measured scale, regional performance, SLA, or updater needs justify
it.

## 11. Credentials and secrets

No token, key, certificate password, OAuth secret, or provider credential is
committed. A protected release environment may later require:

- Windows signing-service identity or certificate reference;
- an encrypted certificate/password only if managed signing is unavailable;
- Apple Developer ID certificate and import password;
- Apple Team ID;
- App Store Connect API issuer ID, key ID, and private key, or equivalent
  notarization credentials;
- a dedicated release/update metadata signing key; and
- an optional private deployment hook for the later website.

Prefer GitHub OIDC and cloud/HSM-backed signing to exportable long-lived keys.
Pull-request workflows never receive production secrets. Zain enters credentials
directly into GitHub/provider secret stores, never source, issues, logs, or chat.

## 12. Release acceptance

Every candidate must pass:

- all required CI jobs on the exact release commit;
- Debug and Release suites where release changes apply;
- clean-machine installation and first launch;
- correct version/About information;
- representative create, edit, save, reopen, and export workflows;
- safe older `.erdx` loading/migration;
- safe rejection of malformed/unsupported `.erdx` files;
- spaces and non-ASCII paths;
- upgrade from the previous release;
- uninstall/removal without deleting user projects;
- offline editing with optional services unavailable;
- signature/notarization verification;
- package/dependency/licence audit;
- installed notices, source instructions, SBOM, checksums, and manifest; and
- re-download and verification of every draft artifact.

Security, data-loss, signature, licensing, and compatibility failures cannot be
waived for a stable release.

## 13. Publication and rollback

Zain manually approves publication. Once published, tags/assets are immutable,
release notes remain available, and compliance evidence is retained.

If a release is defective:

- never replace assets under the same version;
- clearly mark the affected release when necessary;
- remove it from the recommended path only when justified;
- publish a tested new patch version; and
- retain previous evidence unless a documented security response requires
  restrictions.

## 14. Updates

The first release will not silently self-install updates. ERDFlow may perform a
documented privacy-respecting check, show an unobtrusive notice, and open the
official download page when the user chooses. It must not interrupt unsaved
work or force ordinary updates.

Later updates require HTTPS, signed metadata, artifact hashes, platform
signatures, interrupted-download recovery, atomic application where possible,
rollback protection, and stable/beta channels. Candidate technologies are
WinSparkle or MSIX/App Installer, Sparkle, and AppImageUpdate/manual replacement.

## 15. Vercel website and subdomain

The website comes after desktop release foundations. A static-first `web/`
project will use:

```text
main -> Vercel production -> approved existing-domain subdomain
feature/PR branch -> Vercel preview -> temporary preview URL
```

Desktop-only changes should not rebuild the site. Website merges may deploy
automatically; desktop releases remain protected tag/manual approval operations.

The site obtains latest stable release metadata through a cached, allowlisted
GitHub adapter. Downloads point to GitHub Release assets. Binaries are never
manually uploaded to Vercel.

Initial content: Home, current features, screenshots, Download, system
requirements, documentation, changelog, privacy, open-source notices,
security/contact, and Austrian Impressum.

The exact subdomain/DNS remain manual and private. No DNS change occurs until
the production site is approved. Vercel plan eligibility must be reviewed again
before commercial use.

## 16. Optional accounts and analytics

Accounts are not a first-release prerequisite. When justified:

- users can continue without an account;
- web and desktop share one ERDFlow identity;
- providers are Google, GitHub, Microsoft, and passwordless email;
- native auth uses the system browser and Authorization Code with PKCE;
- no client secret is embedded in ERDFlow;
- refresh tokens use the OS credential vault/keychain; and
- project files remain local unless cloud storage is separately approved.

Auth0 is the current native-first candidate; Supabase Auth/PostgreSQL is the
one-vendor alternative. Selection requires a Qt integration spike, pricing and
DPA review, and Zain's approval. No tenant/database is created during desktop
release work.

Initial metrics use GitHub download counts. Later telemetry must be
data-minimized, consent-aware, separate from account identity, and separately
approved. Crash reporting is also separate because dumps may expose paths or
project information.

## 17. Legal and documentation gates

Before release prepare/review:

- accurate README and current-versus-planned features;
- installation, requirements, getting-started, and user guides;
- changelog, release notes, known issues, and update policy;
- security policy and reporting route;
- support/contact information;
- privacy notice for processing actually performed;
- Austrian Impressum using information Zain supplies privately;
- Terms/EULA if adopted;
- third-party notices and Qt source instructions; and
- account deletion/export and analytics controls only when those features ship.

Professional review remains necessary for LGPL mechanics, consumer terms,
liability, Austrian/EU privacy and device-storage rules, international
transfers, children/education use, and future commercial/cloud/AI features.

## 18. Staged implementation order

Each stage ends with review and explicit approval.

1. **Architecture assessment — complete.** Repository, release gaps, desktop
   direction, GitHub Releases, and Vercel boundaries established.
2. **Cross-platform CI foundation — prepared locally.** Four-target workflow,
   readiness plan, and LGPL plan added. Still requires branch/PR, GitHub run,
   genuine platform fixes, and approval as required checks.
3. **Version and file-format consistency — next.** Centralize application
   versioning and reconcile format-25 code, documentation, and fixtures.
4. **Packaging foundations.** Add isolated Windows/macOS/Linux definitions and
   create unsigned candidates for clean-system testing.
5. **Licence evidence.** Inventory dependencies, assemble notices/source/SBOM,
   test replaceability, and confirm ERDFlow's own licence.
6. **Signing environment.** Obtain/authorize Windows and Apple identities,
   protect credentials, and test on non-public candidates.
7. **Release automation.** Add tag validation, clean builds, packaging,
   signing, checksums, SBOM, provenance, draft release, and manual gate.
8. **Release-candidate acceptance.** Run the full clean-system matrix and close
   security, data-loss, compatibility, signature, and licence gates.
9. **First GitHub Release.** Zain approves and publishes immutable artifacts;
   every public download is reverified.
10. **Website/subdomain.** Build the static site, connect GitHub/Vercel, approve
    previews/production, then manually configure the selected subdomain.
11. **Update notification.** Add signed metadata and user-controlled notices;
    native updating requires another review.
12. **Optional accounts/analytics.** Proceed only after separate provider,
    security, privacy, consent, retention, and deletion approval.

## 19. Manual approvals required

Zain must approve or perform:

- preserving active unfinished desktop work before release branches change;
- private-repository GitHub Actions allowance/cost and public action access;
- branch protection, protected tags/environment, and immutable releases;
- final minimum OS versions;
- ERDFlow's MIT or future licence decision;
- LGPL source-delivery method and legal review;
- Apple Developer Program and signing identity;
- Windows signing identity/service;
- every public release;
- Vercel GitHub authorization and plan;
- subdomain and DNS records;
- private legal operator/contact details through an appropriate channel;
- auth/database/analytics providers and DPAs; and
- any purchase or recurring paid service.

## 20. Definition of ready to publish

- [ ] Clean, reviewed release commit on protected `main`.
- [ ] All required CI checks pass on that commit.
- [ ] One consistent application version.
- [ ] `.erdx` documentation and fixtures match code.
- [ ] Supported platforms are accurately documented and tested.
- [ ] All four packages install, launch, edit, save, reopen, upgrade, and remove
      correctly on clean systems.
- [ ] Windows artifacts are signed.
- [ ] Both macOS artifacts are signed, notarized, and stapled.
- [ ] Linux runtime baseline, checksum, and signature are verified.
- [ ] Dependency licences, notices, Qt source availability, replacement
      instructions, and SBOM pass review.
- [ ] No secret appears in source, artifacts, logs, or release notes.
- [ ] Installation, support, security, privacy, legal, and release documents are
      complete for features actually enabled.
- [ ] Draft GitHub Release contains every verified artifact.
- [ ] Zain explicitly approves publication.

Until all non-deferrable items have evidence, the build is a candidate, not a
supported public ERDFlow release.
