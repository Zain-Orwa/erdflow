#!/usr/bin/env python3
"""Writes THIRD-PARTY-LICENSES.txt for one ERDFlow package.

The file is made from what the package actually contains, not from a list kept
by hand. Every program and library in the package (each Mach-O, PE or ELF file)
must be accounted for:

  * ERDFlow itself (--application);
  * a Qt library or plugin, identified through the SPDX SBOMs that come with
    the official Qt binaries (<Qt prefix>/sbom/*.spdx.json). The SBOMs also say
    which third-party code is compiled into each Qt library, with its version,
    licence and copyright;
  * a library declared on the command line (--declared), such as the Visual C++
    runtime or the ICU libraries that come with Qt for Linux;
  * on Linux, with --debian-system-libraries, a library copied from the build
    machine, whose Debian copyright file is then included.

Anything else is reported and stops the build, because a component whose
licence is not established blocks release (docs/QT_LGPL_COMPLIANCE.md). With
--allow-incomplete the file is still written but says, at the top, that the
package is not fit to distribute; that is for local test builds only.

Licence texts are read from the licenses/ folder beside this script, copied
from the Qt source the binaries were built from (licenses/README.md). A licence
with no text there is reported, never filled in.

Uses only the Python standard library.
"""

import argparse
import fnmatch
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
LICENSE_TEXTS = HERE / "licenses"

# Qt's own modules are offered under several licences; ERDFlow uses them under
# the LGPL, whose texts travel as separate files beside this one.
QT_ELECTED = "LGPL-3.0-only"
SEPARATE_TEXTS = {
    "LGPL-3.0-only": "LGPL-3.0.txt",
    "GPL-3.0-only": "GPL-3.0.txt",
}


def is_binary(path):
    try:
        with open(path, "rb") as f:
            head = f.read(4)
    except OSError:
        return False
    return (head[:2] == b"MZ"
            or head == b"\x7fELF"
            or head in (b"\xfe\xed\xfa\xce", b"\xfe\xed\xfa\xcf",
                        b"\xce\xfa\xed\xfe", b"\xcf\xfa\xed\xfe",
                        b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca",
                        b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca"))


def comparable_name(name):
    """libQt6Gui.so.6.11.2 and libQt6Gui.so.6 are the same library."""
    return re.sub(r"\.so(\.[0-9]+)*$", ".so", name)


def split_or(expression):
    expression = expression.strip()
    while expression.startswith("(") and expression.endswith(")"):
        depth = 0
        for i, ch in enumerate(expression):
            depth += ch == "("
            depth -= ch == ")"
            if depth == 0:
                break
        if i != len(expression) - 1:
            break
        expression = expression[1:-1].strip()
    parts, depth, start = [], 0, 0
    for i, ch in enumerate(expression):
        depth += ch == "("
        depth -= ch == ")"
        if depth == 0 and expression.startswith(" OR ", i):
            parts.append(expression[start:i])
            start = i + 4
    parts.append(expression[start:])
    return [p.strip() for p in parts]


def licence_ids(expression):
    return [t for t in re.findall(r"[A-Za-z0-9.+-]+", expression)
            if t not in ("AND", "OR", "WITH")]


def elect(expression, qt_module):
    """The licence ERDFlow uses a component under, when it is offered under
    several. Qt itself: the LGPL. Anything else: the first alternative that is
    neither GPL nor Qt's commercial licence."""
    choices = split_or(expression)
    if qt_module and QT_ELECTED in choices:
        return QT_ELECTED
    for choice in choices:
        ids = licence_ids(choice)
        if not any(i.startswith(("GPL-", "LGPL-", "AGPL-")) or i == "LicenseRef-Qt-Commercial"
                   for i in ids):
            return choice
    for choice in choices:
        if "LicenseRef-Qt-Commercial" not in licence_ids(choice):
            return choice
    return expression


class QtSbom:
    def __init__(self, prefix):
        self.packages = {}
        self.depends = {}
        self.owner_of = {}
        self.repositories = []
        for path in sorted(Path(prefix, "sbom").glob("*.spdx.json")):
            if path.name.endswith(".source.spdx.json"):
                continue
            doc = json.loads(path.read_text(encoding="utf-8"))
            files = {f["SPDXID"]: f["fileName"] for f in doc.get("files", [])}
            for package in doc["packages"]:
                self.packages[package["SPDXID"]] = package
            for rel in doc["relationships"]:
                kind, a, b = rel["relationshipType"], rel["spdxElementId"], rel["relatedSpdxElement"]
                if kind == "DEPENDS_ON":
                    self.depends.setdefault(a, []).append(b)
                elif kind == "CONTAINS" and b in files:
                    self.owner_of[comparable_name(Path(files[b]).name)] = a
            top = doc["packages"][0]
            # The document's own package carries the git revision; the modules
            # carry the Qt version.
            module = next((p for p in doc["packages"] if "-qt-module-" in p["SPDXID"]), top)
            tools = [p for p in doc["packages"]
                     if "-system-build-tool-Compiler-" in p["SPDXID"] or "-system-build-tool-Linker-" in p["SPDXID"]]
            self.repositories.append({
                "name": top["name"],
                "version": module.get("versionInfo", ""),
                "source": top.get("downloadLocation", ""),
                "built": top.get("comment", "").splitlines()[0] if top.get("comment") else "",
                "tools": [f"{t['name']} {t.get('versionInfo', '')}".strip() for t in tools],
            })

    def __bool__(self):
        return bool(self.packages)

    def built_in(self, package_ids):
        """Third-party code compiled into the given Qt libraries and plugins,
        with the libraries each is in."""
        found = {}
        for root in package_ids:
            stack = [root]
            seen = set()
            while stack:
                for dep in self.depends.get(stack.pop(), []):
                    if dep in seen:
                        continue
                    seen.add(dep)
                    if "-qt-3rdparty-sources-" in dep or "-qt-bundled-3rdparty-module-" in dep:
                        found.setdefault(dep, set()).add(root)
                        stack.append(dep)
        return found


def common_licence_references(text):
    """The base system's licence texts a Debian copyright file refers to. A
    reference that ends a sentence does not take its full stop with it (as in
    libbz2-1.0's "... /usr/share/common-licenses/GPL-2."): no text there is
    named with one at the end."""
    return sorted({reference.rstrip(".") for reference in re.findall(
        r"/usr/share/common-licenses/[A-Za-z0-9.+-]+", text)})


def debian_package_of(name):
    result = subprocess.run(["dpkg", "-S", f"*/{name}"], capture_output=True, text=True)
    for line in result.stdout.splitlines():
        package, _, path = line.partition(": ")
        if Path(path.strip()).name == name:
            package = package.split(",")[0].split(":")[0].strip()
            version = subprocess.run(["dpkg-query", "-W", "-f=${Version}", package],
                                     capture_output=True, text=True).stdout.strip()
            copyright_file = Path("/usr/share/doc", package, "copyright")
            if copyright_file.is_file():
                text = copyright_file.read_text(encoding="utf-8")
                # Debian copyright files often refer to texts supplied by the
                # base system. An AppImage recipient may not have that system.
                for reference in common_licence_references(text):
                    referenced = Path(reference)
                    if not referenced.is_file():
                        raise ValueError(f"{package}: missing referenced licence {reference}")
                    text += f"\n\n{reference}\n" + referenced.read_text(encoding="utf-8")
                return package, version, text
            return package, version, None
    return None, None, None


def freetype_credit(components):
    years = []
    for c in components:
        for start, end in re.findall(r"(\d{4})(?:\s*-\s*(\d{4}))?", c["copyright"]):
            years.extend(int(y) for y in (start, end) if y)
    span = f"{min(years)}-{max(years)} " if years else ""
    return (f"Portions of this software are copyright © {span}The FreeType Project "
            "(www.freetype.org).  All rights reserved.")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", required=True, help="the package folder to examine")
    parser.add_argument("--qt-prefix", required=True, help="the Qt installation the package was built with")
    parser.add_argument("--output", required=True, help="THIRD-PARTY-LICENSES.txt to write")
    parser.add_argument("--inventory", help="also write the inventory as JSON here")
    parser.add_argument("--title", default="ERDFlow", help="first line of the file")
    parser.add_argument("--application", action="append", default=[],
                        help="file name of an ERDFlow program in the package")
    parser.add_argument("--declared", action="append", default=[], metavar="GLOB|NAME|LICENCE|NOTE",
                        help="libraries matching GLOB (several separated by commas) are NAME under LICENCE (an SPDX id with a "
                             "text in licenses/, or '-' when the terms are given elsewhere)")
    parser.add_argument("--component", action="append", default=[], metavar="NAME|LICENCE|NOTE",
                        help="a component that is part of the package but not a separate file")
    parser.add_argument("--debian-system-libraries", action="store_true",
                        help="account for other libraries through the Debian packages they came from")
    parser.add_argument("--allow-incomplete", action="store_true",
                        help="write the file even if something is unaccounted for (local tests only)")
    parser.add_argument("--release-blocker", action="append", default=[],
                        help="unresolved redistribution obligation recorded in notices and inventory")
    args = parser.parse_args()

    root = Path(args.root)
    problems = []
    sbom = QtSbom(args.qt_prefix)
    if not sbom:
        problems.append(f"No Qt SBOM in {args.qt_prefix}/sbom; the Qt components cannot be identified. "
                        "Build with the official Qt binaries.")

    declared = []
    for spec in args.declared:
        glob, name, licence, note = (spec.split("|") + ["", "", ""])[:4]
        declared.append({"glob": glob, "name": name, "licence": licence, "note": note, "files": []})
    extra = []
    for spec in args.component:
        name, licence, note = (spec.split("|") + ["", ""])[:3]
        extra.append({"name": name, "licence": licence, "note": note})

    qt_files = {}       # package id -> [paths]
    system = {}         # debian package -> record
    applications = []
    unknown = []
    binary_hashes = {}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for filename in sorted(filenames):
            path = Path(dirpath, filename)
            if path.is_symlink() or not is_binary(path):
                continue
            rel = path.relative_to(root).as_posix()
            binary_hashes[rel] = hashlib.sha256(path.read_bytes()).hexdigest()
            if filename in args.application:
                applications.append(rel)
                continue
            owner = sbom.owner_of.get(comparable_name(filename))
            if owner:
                qt_files.setdefault(owner, []).append(rel)
                continue
            match = next((d for d in declared
                          if any(fnmatch.fnmatch(filename.lower(), g.strip().lower()) for g in d["glob"].split(","))),
                         None)
            if match:
                match["files"].append(rel)
                continue
            if args.debian_system_libraries:
                package, version, text = debian_package_of(filename)
                if package and text:
                    record = system.setdefault(package, {"version": version, "text": text, "files": []})
                    record["files"].append(rel)
                    continue
                if package:
                    problems.append(f"{rel}: from Debian package {package}, which has no copyright file")
                    continue
            unknown.append(rel)
    for rel in unknown:
        problems.append(f"{rel}: not ERDFlow, not a Qt component in the SBOM, and not declared")
    if not applications:
        problems.append("The ERDFlow program itself was not found in the package")
    if not qt_files:
        problems.append("No shared Qt libraries or plugins were found in the package")

    def qt_component(package_id):
        p = sbom.packages[package_id]
        return {
            "name": p["name"],
            "version": p.get("versionInfo", ""),
            "licence": p.get("licenseConcluded", "NOASSERTION"),
            "elected": elect(p.get("licenseConcluded", ""), True),
            "copyright": p.get("copyrightText", ""),
            "files": qt_files[package_id],
        }

    qt_components = sorted((qt_component(i) for i in qt_files), key=lambda c: c["name"])
    built_in = []
    for package_id, holders in sbom.built_in(list(qt_files)).items():
        p = sbom.packages[package_id]
        licence = p.get("licenseConcluded", "NOASSERTION")
        built_in.append({
            "name": p["name"].replace("_Attribution_", ": "),
            "version": p.get("versionInfo", ""),
            "licence": licence,
            "elected": elect(licence, False),
            "copyright": p.get("copyrightText", ""),
            "source": p.get("downloadLocation", ""),
            "in": sorted(sbom.packages[h]["name"] for h in holders),
        })
    built_in.sort(key=lambda c: c["name"].lower())

    for c in qt_components:
        if c["elected"] != QT_ELECTED:
            problems.append(f"Qt component {c['name']} is not offered under {QT_ELECTED}: {c['licence']}")
    for c in built_in:
        if c["licence"] in ("NOASSERTION", "NONE", ""):
            problems.append(f"No licence recorded for {c['name']} (in {', '.join(c['in'])})")
        if c["copyright"] in ("NOASSERTION", ""):
            problems.append(f"No copyright attribution recorded for {c['name']}; review its source")

    # Every licence text the file refers to.
    needed = set()
    for c in qt_components + built_in:
        needed.update(licence_ids(c["elected"]))
    if qt_components:
        needed.add("GPL-3.0-only")  # LGPLv3 incorporates GPLv3 by reference.
    for d in declared + extra:
        if d["licence"] != "-":
            needed.update(licence_ids(d["licence"]))
    texts = {}
    for licence in sorted(needed):
        if licence in SEPARATE_TEXTS:
            if not Path(args.output).parent.joinpath(SEPARATE_TEXTS[licence]).is_file():
                problems.append(f"Missing accompanying licence text: {SEPARATE_TEXTS[licence]}")
            continue
        text_file = LICENSE_TEXTS / f"{licence}.txt"
        if text_file.is_file():
            texts[licence] = text_file.read_text(encoding="utf-8")
        else:
            problems.append(f"No text for the licence {licence} in {LICENSE_TEXTS}")

    lines = []
    out = lines.append
    out(f"{args.title} - third-party components and their licences")
    out("=" * 72)
    out("")
    out("TEST CANDIDATE - REDISTRIBUTION HAS NOT BEEN APPROVED.")
    out("A complete binary inventory does not establish licence compliance.")
    out("Source delivery, replacement instructions, and final package review")
    out("must satisfy docs/QT_LGPL_COMPLIANCE.md before distribution.")
    for blocker in args.release_blocker:
        out(f"  Release blocker: {blocker}")
    out("")
    if problems:
        out("INCOMPLETE - THIS PACKAGE MUST NOT BE DISTRIBUTED.")
        out("The following could not be accounted for:")
        for problem in problems:
            out(f"  * {problem}")
        out("")
    out("This file was generated when the package was built, from the programs")
    out("and libraries the package contains. ERDFlow's own licence is in LICENSE;")
    out("THIRD-PARTY-NOTICES.md and QT-LGPL-INFORMATION.md are beside this file.")
    out("")

    out("1. Qt")
    out("-" * 72)
    out("")
    out("ERDFlow uses the Qt libraries, copyright (C) The Qt Company Ltd. and")
    out("other contributors, under the GNU Lesser General Public License,")
    out("version 3 (LGPL-3.0-only). The texts of the LGPL version 3 and of the GNU")
    out("General Public License version 3 it refers to are in LGPL-3.0.txt and")
    out("GPL-3.0.txt. ERDFlow links to the Qt libraries dynamically; they are")
    out("separate files in this package that can be replaced, as described in")
    out("QT-LGPL-INFORMATION.md, which also says where to get their source code.")
    out("They are the official Qt binaries; their code is unmodified, and the")
    out("packaging tools only set where they look for one another and, on")
    out("macOS, sign them.")
    out("")
    for repo in sbom.repositories:
        out(f"  {repo['name']} {repo['version']}")
        out(f"    source:   {repo['source']}")
        if repo["built"]:
            out(f"    built:    {repo['built']}")
        if repo["tools"]:
            out(f"    tools:    {', '.join(repo['tools'])}")
    out("")
    out("Qt libraries and plugins in this package:")
    for c in qt_components:
        out(f"  {c['name']} {c['version']} - {c['elected']}")
        for f in c["files"]:
            out(f"    {f}")
    out("")

    out("2. Third-party code built into those Qt libraries")
    out("-" * 72)
    out("")
    out("As recorded in Qt's SBOM. Where a component is offered under a choice of")
    out("licences, 'used under' names the one that applies here.")
    out("")
    for c in built_in:
        out(f"{c['name']}" + (f" {c['version']}" if c["version"] and c["version"] != "unknown" else ""))
        out(f"  licence:  {c['licence']}")
        if c["elected"] != c["licence"]:
            out(f"  used under: {c['elected']}")
        out(f"  in:       {', '.join(c['in'])}")
        if c["source"] and c["source"] != "NOASSERTION":
            out(f"  source:   {c['source']}")
        for i, line in enumerate(c["copyright"].splitlines()):
            out(("  " + ("copyright: " if i == 0 else "           ")) + line)
        out("")
    freetype = [c for c in built_in if "FTL" in licence_ids(c["elected"])]
    if freetype:
        out(freetype_credit(freetype))
        out("")

    other = [d for d in declared if d["files"]] + extra
    if other or system:
        out("3. Other components in this package")
        out("-" * 72)
        out("")
        for d in other:
            out(d["name"])
            if d["licence"] != "-":
                out(f"  licence:  {d['licence']}")
            if d.get("note"):
                out(f"  {d['note']}")
            for f in d.get("files", []):
                out(f"    {f}")
            out("")
        for package in sorted(system):
            record = system[package]
            out(f"{package} {record['version']} (Ubuntu package; its copyright file follows)")
            for f in record["files"]:
                out(f"    {f}")
            out("")
            out(record["text"].rstrip())
            out("")

    out("4. Licence texts")
    out("-" * 72)
    out("")
    for licence in sorted(needed):
        if licence in SEPARATE_TEXTS:
            out(f"{licence}: see {SEPARATE_TEXTS[licence]}.")
    out("")
    for licence, text in texts.items():
        out("=" * 72)
        out(licence)
        out("=" * 72)
        out(text.rstrip())
        out("")

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text("\n".join(lines) + "\n", encoding="utf-8")

    if args.inventory:
        Path(args.inventory).parent.mkdir(parents=True, exist_ok=True)
        Path(args.inventory).write_text(json.dumps({
            "inventory_complete": not problems,
            "redistribution_approved": False,
            "release_blockers": args.release_blocker,
            "scope": "Native binaries identified by filename against the build Qt SBOM; not a final-package SPDX SBOM or proof of provenance. Hashes precede signing and final container assembly.",
            "binary_sha256": binary_hashes,
            "problems": problems,
            "applications": applications,
            "qt": {"repositories": sbom.repositories, "components": qt_components},
            "built_into_qt": built_in,
            "declared": [d for d in declared if d["files"]] + extra,
            "system_libraries": {k: {"version": v["version"], "files": v["files"]} for k, v in system.items()},
        }, indent=2) + "\n", encoding="utf-8")

    if problems:
        stream = sys.stderr
        print("package_notices: the package contains components whose licences are not established:",
              file=stream)
        for problem in problems:
            print(f"  * {problem}", file=stream)
        if not args.allow_incomplete:
            return 1
        print("package_notices: written anyway, marked INCOMPLETE (--allow-incomplete)", file=stream)
    return 0


if __name__ == "__main__":
    sys.exit(main())
