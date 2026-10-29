#!/usr/bin/env python3
"""Validate MacFlow mod metadata blocks.

Checks the subset of ramensoftware/windhawk-mods' own validation rules
(see docs/RESEARCH.md Section 1, sourced from their .github/pr_validation.py)
that can be checked without network access to the SPDX license list:
required fields, @id format and macflow- prefix, @id-matches-filename, and
@version format.
"""

import pathlib
import re
import sys

MODS_DIR = pathlib.Path(__file__).resolve().parent.parent / "mods"

ID_RE = re.compile(r"^([0-9a-z]+-)*[0-9a-z]+$")
VERSION_RE = re.compile(r"^([0-9]+\.)*[0-9]+$")
REQUIRED_FIELDS = ["id", "name", "description", "version", "author", "include"]


def parse_metadata(text):
    match = re.search(r"// ==WindhawkMod==(.*?)// ==/WindhawkMod==", text, re.S)
    if not match:
        return None

    fields = {}
    for line in match.group(1).splitlines():
        line = line.strip()
        if not line.startswith("// @"):
            continue
        body = line[4:].strip()
        parts = body.split(None, 1)
        if len(parts) != 2:
            continue
        key, value = parts
        fields.setdefault(key, []).append(value.strip())
    return fields


def validate_file(path):
    errors = []
    text = path.read_text(encoding="utf-8")
    fields = parse_metadata(text)

    if fields is None:
        return [f"{path.name}: missing ==WindhawkMod== metadata block"]

    for required in REQUIRED_FIELDS:
        if required not in fields:
            errors.append(f"{path.name}: missing required @{required}")

    if "id" in fields:
        mod_id = fields["id"][0]
        if not ID_RE.match(mod_id):
            errors.append(
                f"{path.name}: @id '{mod_id}' does not match "
                f"required pattern ([0-9a-z]+-)*[0-9a-z]+"
            )
        if not (6 <= len(mod_id) <= 48):
            errors.append(
                f"{path.name}: @id '{mod_id}' must be 6-48 characters long"
            )
        if not mod_id.startswith("macflow-"):
            errors.append(f"{path.name}: @id '{mod_id}' must start with 'macflow-'")
        expected_filename = f"{mod_id}.wh.cpp"
        if path.name != expected_filename:
            errors.append(
                f"{path.name}: filename must equal @id, expected "
                f"'{expected_filename}'"
            )

    if "version" in fields:
        version = fields["version"][0]
        if not VERSION_RE.match(version):
            errors.append(
                f"{path.name}: @version '{version}' does not match "
                f"required pattern ([0-9]+\\.)*[0-9]+"
            )

    return errors


def main():
    if not MODS_DIR.is_dir():
        print(f"mods directory not found: {MODS_DIR}", file=sys.stderr)
        return 1

    mod_files = sorted(MODS_DIR.glob("*.wh.cpp"))
    if not mod_files:
        print("no .wh.cpp files found under mods/", file=sys.stderr)
        return 1

    all_errors = []
    for path in mod_files:
        all_errors.extend(validate_file(path))

    if all_errors:
        for error in all_errors:
            print(f"ERROR: {error}")
        return 1

    print(f"OK: {len(mod_files)} mod file(s) passed metadata validation")
    return 0


if __name__ == "__main__":
    sys.exit(main())
