#!/usr/bin/env python3
"""Writes an SPDX 2.3 software bill of materials (JSON) for one finished ERDFlow
package, from the inventory package_notices.py made of it and the package file
itself (docs/QT_LGPL_COMPLIANCE.md, section 7).

    inventory_to_spdx.py --inventory build/package/packaging/inventory.json \\
        --package build/package/dist/ERDFlow-0.1.0-macos-arm64.dmg \\
        --version 0.1.0 --output ERDFlow-0.1.0-macos-arm64.spdx.json

The package is described with its SHA-256; each Qt component, the third-party
code compiled into Qt, every other declared component and each operating-system
library package is a package it contains, with its version and licence as the
inventory records them. A licence the SPDX list has no identifier for is given
its text from notices/licenses when there is one there.
"""
import argparse
import datetime
import hashlib
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPOSITORY = "https://github.com/Zain-Orwa/erdflow"


def spdx_id(text):
    return re.sub(r"[^A-Za-z0-9.-]+", "-", text).strip("-")


def licence_refs(expression):
    return set(re.findall(r"LicenseRef-[A-Za-z0-9.-]+", expression or ""))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--inventory", required=True)
    parser.add_argument("--package", required=True, help="the finished package file")
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--copyright", default="NOASSERTION", help="ERDFlow's copyright line")
    parser.add_argument("--created", help="creation time, UTC (default: now)")
    args = parser.parse_args()

    inventory = json.loads(Path(args.inventory).read_text(encoding="utf-8"))
    package_file = Path(args.package)
    digest = hashlib.sha256(package_file.read_bytes()).hexdigest()
    created = args.created or datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    release = f"{REPOSITORY}/releases/tag/v{args.version}"

    erdflow = {
        "SPDXID": "SPDXRef-Package-ERDFlow",
        "name": "ERDFlow",
        "versionInfo": args.version,
        "packageFileName": package_file.name,
        "downloadLocation": release,
        "homepage": REPOSITORY,
        "filesAnalyzed": False,
        "checksums": [{"algorithm": "SHA256", "checksumValue": digest}],
        "licenseConcluded": "MIT",
        "licenseDeclared": "MIT",
        "copyrightText": args.copyright,
        "primaryPackagePurpose": "APPLICATION",
    }
    packages = [erdflow]
    relationships = [{"spdxElementId": "SPDXRef-DOCUMENT", "relationshipType": "DESCRIBES",
                      "relatedSpdxElement": erdflow["SPDXID"]}]
    used = set()

    def contained(kind, name, version, declared, concluded, copyright_text, location, comment):
        identifier = f"SPDXRef-{kind}-{spdx_id(name)}"
        if any(p["SPDXID"] == identifier for p in packages):
            return
        declared = declared or "NOASSERTION"
        concluded = concluded or declared
        used.update(licence_refs(declared) | licence_refs(concluded))
        packages.append({
            "SPDXID": identifier,
            "name": name,
            "versionInfo": version or "NOASSERTION",
            "downloadLocation": location or "NOASSERTION",
            "filesAnalyzed": False,
            "licenseConcluded": concluded,
            "licenseDeclared": declared,
            "copyrightText": copyright_text or "NOASSERTION",
            "comment": comment,
        })
        relationships.append({"spdxElementId": erdflow["SPDXID"], "relationshipType": "CONTAINS",
                              "relatedSpdxElement": identifier})

    sources = {r["name"]: r["source"] for r in inventory.get("qt", {}).get("repositories", [])}
    for component in inventory.get("qt", {}).get("components", []):
        contained("Qt", f"Qt {component['name']}", component.get("version"), component.get("licence"),
                  component.get("elected"), component.get("copyright"),
                  sources.get("qtsvg" if component["name"].lower().startswith("svg") else "qtbase"),
                  "Official Qt binaries, unmodified: " + ", ".join(component.get("files", [])))
    for component in inventory.get("built_into_qt", []):
        contained("InQt", component["name"], component.get("version"), component.get("licence"),
                  component.get("elected"), component.get("copyright"), None,
                  "Compiled into " + ", ".join(sorted(component.get("in", []))) +
                  ", as recorded in The Qt Company's SBOM")
    for component in inventory.get("declared", []):
        licence = component.get("licence")
        if not licence or licence == "-":
            licence = "LicenseRef-" + spdx_id(component["name"])
        files = component.get("files", [])
        versions = component.get("versions", {})
        detail = ", ".join(f + (f" {versions[f]}" if f in versions else "") for f in files)
        contained("Declared", component["name"], None, licence, licence, None, None,
                  (component.get("note") or "") + (f" Files: {detail}" if detail else ""))
    for name, record in sorted(inventory.get("system_libraries", {}).items()):
        contained("Ubuntu", name, record.get("version"), "NOASSERTION", "NOASSERTION", None, None,
                  "Ubuntu 24.04 package, unmodified; its copyright file is in the package. Files: " +
                  ", ".join(record.get("files", [])))

    extracted = []
    for reference in sorted(used):
        text_file = HERE / "licenses" / f"{reference}.txt"
        text = text_file.read_text(encoding="utf-8") if text_file.is_file() else \
            "See THIRD-PARTY-LICENSES.txt and THIRD-PARTY-NOTICES.md in the package."
        extracted.append({"licenseId": reference, "extractedText": text,
                          "name": reference[len("LicenseRef-"):]})

    document = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": package_file.name,
        "documentNamespace": f"{REPOSITORY}/spdx/{package_file.name}-{digest}",
        "creationInfo": {"created": created,
                         "creators": ["Tool: erdflow-inventory_to_spdx", "Organization: ERDFlow"],
                         "comment": inventory.get("scope", "")},
        "packages": packages,
        "relationships": relationships,
    }
    if extracted:
        document["hasExtractedLicensingInfos"] = extracted
    Path(args.output).write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
