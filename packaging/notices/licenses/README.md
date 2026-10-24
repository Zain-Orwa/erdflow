# Licence texts

`package_notices.py` copies these texts into each package's
`THIRD-PARTY-LICENSES.txt`, for the licences of the components the package
actually contains. Each file is named by its SPDX licence identifier and is an
unedited copy of its source below. A licence with no file here stops the
package build; add the text from its source rather than writing one.

## From the Qt 6.11.2 source

Copied from the `LICENSES/` folder of the official Qt 6.11.2 source archives,
which are the source of the Qt binaries the package workflow uses (their `.tag`
files name the same commits as the binaries' SBOMs):

| Archive | SHA-256 | Commit |
| --- | --- | --- |
| `qtbase-everywhere-src-6.11.2.tar.xz` | `5b2e00eccaf5a4d8c14134ffa0ea8dfd0a35ae1ffc7f8d87fa4305a1ed23cf22` | `ef55f427f2c8b410d34f8a7681020a3000cf6866` |
| `qtsvg-everywhere-src-6.11.2.tar.xz` | `d594337feca84c26fb67fe87b85e6a5c12fda404b611d905f9d138210c311876` | `17ca512f903f935282ebeca496aac5d11ba4199a` |

Both from <https://download.qt.io/official_releases/qt/6.11/6.11.2/submodules/>;
the hashes match the `.sha256` files published there.

- From qtbase: `AFL-2.1`, `Apache-2.0`, `BSD-2-Clause`, `BSD-3-Clause`,
  `CC0-1.0`, `FTL`, `HPND`, `IJG`, `Imlib2`, `Libpng`, `libpng-2.0`,
  `LicenseRef-BSD-3-Clause-with-PCRE2-Binary-Like-Packages-Exception`,
  `LicenseRef-ICC-License`, `LicenseRef-Lcs-Telegraphics`,
  `LicenseRef-SHA1-Public-Domain`, `MIT`, `MIT-open-group`, `Unicode-3.0`,
  `X11`, `Zlib`.
- From qtsvg: `HPND-sell-variant`.

## From other projects

- `LicenseRef-ICU-73.2.txt`: `icu4c/LICENSE` of ICU at tag `release-73-2`
  (<https://github.com/unicode-org/icu/blob/release-73-2/icu4c/LICENSE>). Qt
  for Linux ships ICU 73.2 (`libicuuc.so.73.2`) without a licence file.
- `LicenseRef-AppImage-type2-runtime.txt`: `LICENSE` of the AppImage
  `type2-runtime` at tag `20251108`
  (<https://github.com/AppImage/type2-runtime/blob/20251108/LICENSE>).

## Beside these, installed as they are

Unedited copies, installed into the package as files of their own rather than
merged into `THIRD-PARTY-LICENSES.txt` (2026-10-15):

- `../windows/Inno-Setup-6.7.1-license.txt`: `license.txt` of Inno Setup at tag
  `is-6_7_1` (<https://github.com/jrsoftware/issrc/blob/is-6_7_1/license.txt>);
  it covers the uninstaller installed beside `erdflow.exe`.
- `../appimage-runtime/`, the components the AppImage runtime `20251108` is
  statically linked with (its `scripts/common/install-dependencies.sh` and the
  versions embedded in the runtime binary), and linuxdeploy-plugin-qt:
  - `libfuse-3.15.0-LGPL2.txt`: `LGPL2.txt` of libfuse at tag `fuse-3.15.0`;
  - `squashfuse-0.5.2-LICENSE.txt`: `LICENSE` of squashfuse at tag `0.5.2`;
  - `zstd-1.5.6-LICENSE.txt`: `LICENSE` of zstd at tag `v1.5.6`;
  - `zlib-1.3.1-LICENSE.txt`: `LICENSE` of zlib at tag `v1.3.1`;
  - `mimalloc-2.1.7-LICENSE.txt`: `LICENSE` of mimalloc at tag `v2.1.7` (the
    runtime takes mimalloc 2 from Alpine Linux 3.21; the terms are the same);
  - `musl-1.2.5-COPYRIGHT.txt`: `COPYRIGHT` of musl at tag `v1.2.5`;
  - `linuxdeploy-plugin-qt-LICENSE.txt`: `LICENSE.txt` of linuxdeploy-plugin-qt
    at tag `1-alpha-20250213-1`, for the start-up hook it adds.

## When Qt is updated

A different Qt version can bring different components and licences. Download
that version's source archives, check their `.tag` files against the SBOMs in
`<Qt>/sbom/`, and copy any newly needed texts from their `LICENSES/` folders.
