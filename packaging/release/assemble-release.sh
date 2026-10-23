#!/usr/bin/env bash
# Gathers everything an ERDFlow release publishes from one run of the Desktop
# Packages workflow, checks it, and prints the command that would create the
# GitHub release as a draft. It creates nothing on GitHub itself.
#
#   packaging/release/assemble-release.sh <run-id> [<folder>]
#
# The run must be of the release commit, with the release owner's approval set
# (docs/RELEASE_0.1.0.md), so that every package says it was approved.
set -euo pipefail

run=${1:?usage: assemble-release.sh <run-id> [<folder>]}
out=${2:-release-assets}
repo=Zain-Orwa/erdflow
source_root=$(cd "$(dirname "$0")/../.." && pwd)

if [ -e "$out" ]; then
    echo "$out already exists; give an empty folder" >&2
    exit 1
fi
mkdir -p "$out/download"
gh run download "$run" --repo "$repo" --dir "$out/download"

assets="$out/assets"
mkdir -p "$assets"
find "$out/download" -type f \( -name 'ERDFlow-*' -o -name 'Qt-*-build-information.tar.gz' \
    -o -name 'SHA256SUMS' -o -name 'SHA256SUMS.asc' -o -name 'SHA256SUMS.ed25519' \
    -o -name 'SHA256SUMS-sources' -o -name 'SOURCES.md' \
    -o -name '*-everywhere-src-*.tar.xz' -o -name 'fuse-*.tar.xz' -o -name 'squashfuse-*.tar.gz' \
    -o -name 'type2-runtime-*.tar.gz' \) -exec cp {} "$assets/" \;

cd "$assets"
version=$(ls ERDFlow-*-windows-x86_64-setup.exe | sed -E 's/^ERDFlow-(.+)-windows-x86_64-setup\.exe$/\1/')
shasum -a 256 -c SHA256SUMS
shasum -a 256 -c SHA256SUMS-sources
if [ -f SHA256SUMS.asc ]; then
    gpg --verify SHA256SUMS.asc SHA256SUMS
else
    echo "warning: SHA256SUMS is not signed" >&2
fi
# The signature an installed ERDFlow checks before it updates itself (ADR-024),
# checked against the key ERDFlow carries. Needs OpenSSL 3 (Homebrew's on a Mac).
if [ -f SHA256SUMS.ed25519 ]; then
    key=$(sed -n 's/.*release_key_hex\[\] = "\([0-9a-f]\{64\}\)".*/\1/p' "$source_root/infrastructure/updater.cpp")
    printf '302a300506032b6570032100%s' "$key" | xxd -r -p | openssl pkey -pubin -inform DER -out ../update-key.pub
    openssl pkeyutl -verify -pubin -inkey ../update-key.pub -rawin -in SHA256SUMS -sigfile SHA256SUMS.ed25519
else
    echo "warning: SHA256SUMS.ed25519 is missing; installed copies will not update to this release" >&2
fi
for inventory in ERDFlow-*-inventory.json; do
    python3 -c 'import json, sys
d = json.load(open(sys.argv[1]))
if not (d["inventory_complete"] and d["redistribution_approved"] and not d["release_blockers"]):
    sys.exit(f"{sys.argv[1]}: not approved for distribution")' "$inventory"
done
ls -l

echo
echo "Everything checks. To create the release as a draft (nothing is published until it is):"
echo "  gh release create v$version --repo $repo --draft --verify-tag --title 'ERDFlow $version' \\"
echo "    --notes-file docs/RELEASE_NOTES_$version.md $assets/*"
