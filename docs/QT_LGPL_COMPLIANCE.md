# Qt LGPL distribution compliance plan

> **Settled work.** Do not change, replace or re-style anything in this
> document to suit something new you have been asked to build. If what you are
> building genuinely contradicts what is written here, stop and ask Zain, who
> owns this project: say what you want to change, what the application will
> look like afterwards, and whether it is a gain or a loss. He decides.
> Full rule: [CLAUDE.md](../CLAUDE.md).

**Status:** Pre-distribution checklist, not legal advice. Completion requires a
review of the exact Qt binaries and plugins placed in each final package.

ERDFlow currently uses Qt Core, Gui, Widgets, and SVG. The project does not have
a commercial Qt licence and therefore intends to use the open-source licence
route. Being free, non-commercial, or source-available does not by itself
satisfy Qt's licence conditions.

The intended route for the initial release is dynamic linking to Qt components
that are available under LGPL version 3. Static Qt builds and GPL-only Qt
modules are outside the approved initial distribution design.

Authoritative references:

- <https://www.qt.io/development/open-source-lgpl-obligations>
- <https://www.qt.io/faq/qt-open-source-licensing>
- <https://doc.qt.io/qt-6/licensing.html>
- <https://doc.qt.io/qt-6/lgpl.html>
- <https://doc.qt.io/qt-6/qtsvg-index.html>

## Obligations before distributing a binary

### 1. Verify every distributed component's licence

Create an inventory from the actual Windows, macOS, and Linux packages, not
only from `find_package()` in CMake. It must include:

- every Qt shared library or framework;
- every Qt platform, image-format, style, TLS, and other plugin;
- compiler runtimes and other bundled native libraries;
- installer and updater components;
- icons, fonts, images, and other assets; and
- transitive third-party code carried inside the Qt distribution.

Record the exact component name, version, origin, licence, copyright notice,
source archive, and whether it was modified. Do not ship a GPL-only module
unless ERDFlow is deliberately released under compatible GPL terms. Any module
whose licence cannot be established blocks release.

### 2. Use replaceable shared Qt libraries

The release builds must dynamically link Qt. Packaging may place Qt DLLs,
frameworks, or shared objects beside the application, but it must not prevent a
recipient from replacing those libraries with a compatible modified build.

Do not:

- statically link Qt into the ERDFlow executable;
- cryptographically bind ERDFlow to only the supplied Qt library build;
- add integrity checks that reject a user-replaced compatible Qt library;
- prohibit reverse engineering when it is needed to debug a modification of
  the LGPL-covered component; or
- use an installer/update policy that makes lawful replacement practically
  impossible without first obtaining legal review.

If static linking ever becomes necessary, stop and perform a new legal and
technical review. It can require relinkable ERDFlow object code and installation
information and may be incompatible with the intended distribution model.

### 3. Give recipients the licence texts and notices

Every distribution must carry, in a durable and accessible location:

- the complete LGPL version 3 text;
- the GPL version 3 text referenced by the LGPL;
- Qt copyright and attribution notices;
- the licence and notices for every shipped Qt component and plugin;
- notices for Qt's bundled third-party components; and
- ERDFlow's own licence plus the Lucide/Feather and other asset notices.

The desktop application's About/Licences interface should identify Qt and link
to the locally installed notices. The installer/package should install the
same material. The website should also provide a third-party notices page, but
the website alone is not a substitute for notices travelling with the binary.

Application terms must say that their restrictions do not override recipients'
rights under the listed open-source licences.

### 4. Provide the complete corresponding Qt source

For the precise Qt version/configuration distributed with ERDFlow, retain and
make available the complete corresponding source for the LGPL-covered Qt
libraries, including:

- Qt source for the shipped version;
- source for LGPL-covered bundled third-party components where required;
- every ERDFlow-applied patch or modification;
- build/configuration information needed to produce the shipped libraries; and
- the applicable licence and notice files.

Qt's licensing FAQ states that this source availability must be under the
distributor's control; merely linking to a Qt Project or Qt Company download is
not sufficient. The safest operational approach is to archive the exact source
bundle and patches under ERDFlow-controlled storage and link that offer from the
installed notices and download site.

Before release, legal review must decide whether to distribute the source bundle
alongside every GitHub Release or use a written offer. If a written offer is
used, it must remain valid for the period required by the applicable licence,
and ERDFlow must have an operational process to fulfil it.

### 5. Preserve modification and installation rights

Recipients must be able to run ERDFlow with a compatible modified version of
the LGPL-covered Qt libraries. Document enough information to exercise that
right, including:

- the Qt version and ABI/compiler used for each platform;
- which libraries and plugins may be replaced;
- where the package installs them;
- relevant build configuration and platform requirements; and
- how to start a locally reassembled/test build.

LGPLv3 installation-information requirements need particular review if a
platform, signature policy, application sandbox, or installer prevents the user
from installing and running modified Qt components. Developer ID signing and
Windows code signing improve provenance, but the distribution design must not
use them as a contractual or technical ban on LGPL-authorized modification.

### 6. Publish modifications to LGPL components

If ERDFlow modifies Qt or another LGPL-covered library, publish the complete
corresponding source for those modifications under the applicable LGPL terms.
Keep patches in a reproducible form and retain the scripts/configuration used to
build the modified library.

The preferred initial policy is to use unmodified official open-source Qt
components. Any Qt patch must trigger a compliance review.

### 7. Keep compliance material reproducible and available

For each released ERDFlow version, archive:

- the binary artifact and checksum;
- an SPDX SBOM;
- the exact Qt/component inventory;
- source archives and patches;
- build configuration and toolchain versions;
- licence and notice bundle; and
- the release-specific written source offer or source download location.

Do not delete this material when a newer ERDFlow release is published. Release
retention must cover the licence's source-availability obligations.

## Platform packaging implications

### Windows

Use `windeployqt` to identify and copy shared Qt DLLs and plugins. Audit its
output before signing. Keep the DLLs replaceable and install the notice/source
instructions with the application.

### macOS

Use `macdeployqt` to embed shared Qt frameworks and plugins in the `.app`
bundle. Signing and notarization are still required for normal Gatekeeper
behavior. Because replacing a signed nested framework invalidates the original
signature, the relinking/replacement and installation-information position must
receive specific legal review before release; notarization alone does not waive
LGPL rights.

### Linux

An AppImage may bundle shared Qt libraries, but the final image must be audited
to confirm that they remain dynamically linked. Provide practical extraction,
replacement, and rebuild instructions rather than assuming that the AppImage
format itself demonstrates compliance.

## Evidence required to close this gate

The LGPL distribution gate is complete only when all of the following exist for
each release target:

- [ ] Final package contents have been enumerated.
- [ ] Every component has an approved licence classification.
- [ ] No GPL-only component is present unintentionally.
- [ ] Qt is dynamically linked and replaceability has been tested.
- [ ] Required licence texts and notices ship with the package.
- [ ] The About/Licences UI exposes the installed notices.
- [ ] Exact corresponding Qt source, patches, and build information are under
      ERDFlow's control.
- [ ] The source-delivery mechanism and retention period have been reviewed.
- [ ] Application terms preserve open-source modification and reverse-
      engineering rights.
- [ ] An SPDX SBOM is generated from the release package.
- [ ] Windows, macOS, and Linux replacement/relinking instructions have been
      tested.
- [ ] macOS signing/notarization implications have received professional legal
      review.
- [ ] A release owner has signed off on the completed evidence.

Until those boxes are supported by release-specific evidence, a successful CI
build is not approval to distribute ERDFlow binaries.
