"""
Auto-test HORS-LIGNE de la logique de detection (aucun OBS requis).

But : prouver que le code de detection est correct avant le test live.
Lancer :  python poc/test_detector.py
"""

from __future__ import annotations
import math
from detector import (
    mul_to_db, levels_to_db, SpeakerDetector, active_speaker, DB_FLOOR,
)

_passed = 0
_failed = 0


def check(label: str, cond: bool):
    global _passed, _failed
    if cond:
        _passed += 1
        print(f"  ok   {label}")
    else:
        _failed += 1
        print(f"  FAIL {label}")


print("== Conversion mul -> dB ==")
check("0.0 -> plancher", mul_to_db(0.0) == DB_FLOOR)
check("1.0 -> 0 dB", abs(mul_to_db(1.0) - 0.0) < 1e-9)
check("0.5 ~ -6 dB", abs(mul_to_db(0.5) - (-6.0206)) < 0.01)
check("clamp plancher", mul_to_db(0.0001) == DB_FLOOR)

print("== Extraction depuis inputLevelsMul (format OBS) ==")
# 2 canaux [magnitude, peak, inputPeak] ; on doit prendre le peak le plus fort
frame = [[0.2, 0.25, 0.3], [0.4, 0.5, 0.55]]
check("prend le peak max (0.5 ~ -6dB)", abs(levels_to_db(frame) - (-6.0206)) < 0.01)
check("liste vide -> plancher", levels_to_db([]) == DB_FLOOR)

print("== Detecteur : attaque (faut 2 frames au-dessus) ==")
d = SpeakerDetector(threshold_db=-35, attack_frames=2, release_frames=8)
loud = mul_to_db(0.5)   # ~ -6 dB, largement au-dessus du seuil
check("1 frame au-dessus : pas encore 'parle'", d.update(loud) is False)
check("2 frames au-dessus : 'parle'", d.update(loud) is True)

print("== Detecteur : relachement (faut 8 frames en-dessous) ==")
silence = DB_FLOOR
states = [d.update(silence) for _ in range(7)]
check("7 frames de silence : tient encore (anti-respiration)", all(states) is True)
check("8e frame de silence : 'ne parle plus'", d.update(silence) is False)

print("== Detecteur : un pic isole ne declenche pas ==")
d2 = SpeakerDetector(threshold_db=-35, attack_frames=2, release_frames=8)
check("pic unique ignore", d2.update(loud) is False and d2.update(silence) is False)
check("etat final = silencieux", d2.speaking is False)

print("== Locuteur actif : le plus fort gagne ==")
a = SpeakerDetector(threshold_db=-35, attack_frames=1)
b = SpeakerDetector(threshold_db=-35, attack_frames=1)
a.update(mul_to_db(0.3))   # ~ -10.5 dB
b.update(mul_to_db(0.6))   # ~ -4.4 dB (plus fort)
check("B plus fort que A -> actif = B", active_speaker({"A": a, "B": b}) == "B")
silent = SpeakerDetector(threshold_db=-35)
check("personne ne parle -> None", active_speaker({"X": silent}) is None)

print()
print(f"RESULTAT : {_passed} ok, {_failed} echec(s)")
raise SystemExit(1 if _failed else 0)
