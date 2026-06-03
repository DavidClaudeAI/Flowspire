#!/usr/bin/env python3
"""Verifie que chaque symbole Qt importe par flowspire.dll est bien exporte par
le Qt embarque par OBS (6.6). Un symbole present dans le Qt du build (plus recent)
mais absent du Qt d'OBS empechait le chargement du plugin (lecon recurrente).

ASCII-only dans les print (console Windows). Aucune dependance externe.
"""
import glob
import re
import subprocess
import sys
from pathlib import Path

# Chemins decouverts automatiquement (pas de version MSVC/edition VS codee en dur).
_REPO = Path(__file__).resolve().parent.parent
DLL = str(_REPO / "build_x64" / "rundir" / "RelWithDebInfo" / "flowspire.dll")

_OBS_QT_CANDIDATES = [
    r"C:\Program Files\obs-studio\bin\64bit",
    r"C:\Program Files (x86)\obs-studio\bin\64bit",
]


def _find_dumpbin():
    pats = [
        r"C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe",
        r"C:\Program Files (x86)\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe",
    ]
    hits = []
    for p in pats:
        hits += glob.glob(p)
    if not hits:
        print("[ERREUR] dumpbin.exe introuvable (Visual Studio non detecte)")
        sys.exit(2)
    return sorted(hits)[-1]  # version la plus recente


def _find_obs_qt_dir():
    for d in _OBS_QT_CANDIDATES:
        if Path(d).is_dir():
            return d
    print("[ERREUR] dossier Qt d'OBS introuvable (OBS installe ?)")
    sys.exit(2)


DUMPBIN = _find_dumpbin()
OBS_QT_DIR = _find_obs_qt_dir()


def run(args):
    return subprocess.run([DUMPBIN] + args, capture_output=True, text=True, errors="replace").stdout


def parse_imports(dll):
    """-> {dllname_lower: set(symbols)} pour les DLL Qt6*."""
    out = run(["/imports", dll])
    imports = {}
    cur = None
    for line in out.splitlines():
        s = line.strip()
        m = re.match(r"^(Qt6\w+\.dll)$", s, re.IGNORECASE)
        if m:
            cur = m.group(1).lower()
            imports.setdefault(cur, set())
            continue
        if cur is None:
            continue
        # lignes de symboles : "<hint hex> <name>"  ou "Ordinal <N>"
        mm = re.match(r"^[0-9A-Fa-f]+\s+(\S.*)$", s)
        if mm:
            name = mm.group(1).strip()
            if name and not name.startswith("Import ") and "time date stamp" not in name \
               and "Index of first" not in name:
                imports[cur].add(name)
        # une ligne vide referme la section courante
        if s == "":
            cur = None if cur and imports.get(cur) else cur
    return {k: v for k, v in imports.items() if v}


def parse_exports(dll):
    out = run(["/exports", dll])
    syms = set()
    for line in out.splitlines():
        # ordinal hint RVA name
        m = re.match(r"^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)", line)
        if m:
            syms.add(m.group(1))
    return syms


def main():
    if not Path(DLL).exists():
        print("[ERREUR] DLL introuvable:", DLL)
        return 2
    imports = parse_imports(DLL)
    if not imports:
        print("[ERREUR] aucun import Qt detecte (parse ?)")
        return 2
    total = 0
    missing = []
    for qtdll, syms in sorted(imports.items()):
        obs_dll = Path(OBS_QT_DIR) / qtdll
        if not obs_dll.exists():
            # capitalisation reelle du fichier
            cand = [p for p in Path(OBS_QT_DIR).glob("Qt6*.dll") if p.name.lower() == qtdll]
            if not cand:
                print("[ERREUR] OBS ne fournit pas", qtdll)
                missing.append((qtdll, "<DLL absente d'OBS>"))
                continue
            obs_dll = cand[0]
        exports = parse_exports(str(obs_dll))
        for sym in syms:
            total += 1
            if sym not in exports:
                missing.append((qtdll, sym))
    print("Symboles Qt importes verifies :", total)
    print("Manquants dans le Qt d'OBS     :", len(missing))
    for qtdll, sym in missing:
        print("  MANQUANT  [%s]  %s" % (qtdll, sym))
    print("RESULT:", "OK (0 manquant)" if not missing else "ECHEC")
    return 0 if not missing else 1


if __name__ == "__main__":
    sys.exit(main())
