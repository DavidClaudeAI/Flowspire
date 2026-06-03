#!/usr/bin/env python3
"""Bump de version de Flowspire.

La version vit a UN SEUL endroit : buildspec.json (cle racine "version").
CMake la lit (cmake/common/bootstrap.cmake) et l'expose au code via PLUGIN_VERSION ;
le CI nomme les artefacts / la Release d'apres elle. Ce script ne touche QUE cette cle
(les versions de dependances, plus indentees, ne sont jamais modifiees).

Convention SemVer (MAJEUR.MINEUR.CORRECTIF) :
  - CORRECTIF (patch) : correction de bug, sans changement de comportement visible.
  - MINEUR (minor)    : nouvelle fonctionnalite retrocompatible.
  - MAJEUR (major)    : changement cassant (config, comportement).

Usage :
  python scripts/bump-version.py 0.2.0     # version explicite (X.Y.Z)
  python scripts/bump-version.py patch     # 0.1.0 -> 0.1.1
  python scripts/bump-version.py minor     # 0.1.0 -> 0.2.0
  python scripts/bump-version.py major     # 0.1.0 -> 1.0.0

Apres le bump, pour publier (le CI fabrique alors les installeurs des 3 OS) :
  git add buildspec.json
  git commit          # format du hook commit-msg, ex : "doc release : version X.Y.Z"
  git tag X.Y.Z       # tag = X.Y.Z SANS prefixe 'v' (le CI release se base dessus)
  git push origin main --tags
"""
import json
import re
import sys
from pathlib import Path

SEMVER = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")
BUILDSPEC = Path(__file__).resolve().parent.parent / "buildspec.json"
# Cible UNIQUEMENT la cle racine, indentee de 4 espaces (les deps sont a 12 espaces).
ROOT_VERSION = re.compile(r'^(    "version": ")[^"]*(")', re.M)


def fail(msg):
    print("ERREUR : " + msg)
    sys.exit(1)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    arg = sys.argv[1].strip()

    if not BUILDSPEC.is_file():
        fail("buildspec.json introuvable (attendu : %s)" % BUILDSPEC)

    text = BUILDSPEC.read_text(encoding="utf-8")
    try:
        current = str(json.loads(text)["version"])
    except Exception as exc:  # noqa: BLE001
        fail("lecture de la version courante impossible : %s" % exc)

    m = SEMVER.match(current)
    if not m:
        fail("version courante non SemVer (X.Y.Z) : '%s'" % current)
    major, minor, patch = (int(x) for x in m.groups())

    if arg == "major":
        major, minor, patch = major + 1, 0, 0
        new = "%d.%d.%d" % (major, minor, patch)
    elif arg == "minor":
        minor, patch = minor + 1, 0
        new = "%d.%d.%d" % (major, minor, patch)
    elif arg == "patch":
        patch += 1
        new = "%d.%d.%d" % (major, minor, patch)
    elif SEMVER.match(arg):
        new = arg
    else:
        fail("argument invalide : '%s' (attendu X.Y.Z, ou major/minor/patch)" % arg)

    if new == current:
        fail("la nouvelle version est identique a l'actuelle (%s)" % current)

    matches = ROOT_VERSION.findall(text)
    if len(matches) != 1:
        fail("attendu 1 cle 'version' racine, trouve %d -> abandon (securite)" % len(matches))

    updated = ROOT_VERSION.sub(r"\g<1>" + new + r"\g<2>", text, count=1)
    BUILDSPEC.write_text(updated, encoding="utf-8")

    print("Version : %s -> %s" % (current, new))
    print("buildspec.json mis a jour.")
    print("")
    print("Pour publier (le CI fabriquera les installeurs) :")
    print("  git add buildspec.json")
    print('  git commit         # ex : "doc release : version %s"' % new)
    print("  git tag %s" % new)
    print("  git push origin main --tags")


if __name__ == "__main__":
    main()
