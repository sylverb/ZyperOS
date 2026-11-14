#!/usr/bin/env python3
"""Writes the latest.json a ZyperOS release carries for the installer's own
updates: Tauri's updater reads it from
https://github.com/tarikbc/ZyperOS/releases/latest/download/latest.json
(tauri.conf.json's plugins.updater) and finds the file for its system in it,
with that file's signature.

    latest-json.py --version 1.0.7 --dir dist --output dist/latest.json
    latest-json.py --example        the same for made-up signatures, on stdout

--dir holds the release's update files and their signatures, as
.github/workflows/release.yml names them:
    ZyperOS-Installer-<version>-macOS-update.tar.gz(.sig)  the macOS app,
        for Apple Silicon and Intel alike
    ZyperOS-Installer-<version>-Windows.exe(.sig)          the portable exe
Linux has no entry: its packages are the package manager's, and the
installer offers the releases page there instead (selfupdate.rs).

The installer's tests read --example's file with the updater's own parser, so
a change here that the updater can't read fails them.
"""
import argparse
import datetime
import json
import sys
from pathlib import Path

REPO = "tarikbc/ZyperOS"
# Each key the updater may look for, and the file it gets
FILES = {
    "darwin-aarch64": "ZyperOS-Installer-{v}-macOS-update.tar.gz",
    "darwin-x86_64": "ZyperOS-Installer-{v}-macOS-update.tar.gz",
    "windows-x86_64": "ZyperOS-Installer-{v}-Windows.exe",
}


def manifest(version, signatures, date, repo=REPO):
    """latest.json's content. `signatures` maps each file name to its .sig
    file's text."""
    base = f"https://github.com/{repo}/releases/download/v{version}/"
    platforms = {}
    for key, pattern in FILES.items():
        name = pattern.format(v=version)
        platforms[key] = {"signature": signatures[name].strip(), "url": base + name}
    return {
        "version": version,
        "notes": f"ZyperOS Installer {version}: https://github.com/{repo}/releases/tag/v{version}",
        "pub_date": date,
        "platforms": platforms,
    }


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--version")
    p.add_argument("--dir", type=Path)
    p.add_argument("--output", type=Path)
    p.add_argument("--example", action="store_true")
    a = p.parse_args()
    if a.example:
        version = "1.0.7"
        names = {pattern.format(v=version) for pattern in FILES.values()}
        signatures = {n: "made-up signature of " + n + "\n" for n in names}
        out = manifest(version, signatures, "2026-12-01T12:00:00Z")
        json.dump(out, sys.stdout, indent=2)
        print()
        return 0
    if not (a.version and a.dir and a.output):
        p.error("--version, --dir and --output, or --example")
    signatures = {}
    for pattern in sorted(set(FILES.values())):
        name = pattern.format(v=a.version)
        if not (a.dir / name).is_file() or (a.dir / name).stat().st_size == 0:
            sys.exit(f"latest-json: {a.dir / name} is missing or empty")
        sig = a.dir / (name + ".sig")
        if not sig.is_file() or not sig.read_text().strip():
            sys.exit(f"latest-json: {sig} is missing or empty")
        signatures[name] = sig.read_text()
    date = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    a.output.write_text(json.dumps(manifest(a.version, signatures, date), indent=2) + "\n")
    print(a.output.read_text(), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
