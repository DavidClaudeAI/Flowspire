"""
StreamDirector — POC : logique de detection "qui parle".

Ce module est PUR (aucune dependance a OBS) pour pouvoir etre teste hors-ligne.
Il reproduit l'esprit de la logique organique de Gabin (MIT) :
seuil de voix + hysteresis (attaque / relachement) pour eviter les faux
declenchements et les coupures sur une respiration.

Le produit final sera un plugin natif C++ lisant obs_volmeter_*.
Ici on lit les memes niveaux via obs-websocket (evenement InputVolumeMeters),
ce qui suffit a LEVER LE RISQUE : peut-on distinguer qui parle a partir de
l'audio interne d'OBS, SANS aucun driver/cable audio virtuel ?
"""

from __future__ import annotations
import math
from dataclasses import dataclass, field

# Plancher d'affichage (silence total)
DB_FLOOR = -60.0


def mul_to_db(mul: float) -> float:
    """Convertit un niveau lineaire OBS (0..1) en decibels (<=0)."""
    if mul is None or mul <= 0.0:
        return DB_FLOOR
    db = 20.0 * math.log10(mul)
    if db < DB_FLOOR:
        return DB_FLOOR
    if db > 0.0:
        return 0.0
    return db


def levels_to_db(input_levels_mul) -> float:
    """
    Extrait un niveau dB a partir du champ `inputLevelsMul` d'OBS.

    Format OBS : liste de canaux ; chaque canal = [magnitude, peak, inputPeak]
    (valeurs lineaires). On prend le PEAK le plus fort parmi les canaux
    (plus reactif que la magnitude pour detecter une prise de parole).
    """
    if not input_levels_mul:
        return DB_FLOOR
    best = 0.0
    for channel in input_levels_mul:
        if not channel:
            continue
        # index 1 = peak ; fallback index 0 = magnitude si peak absent
        peak = channel[1] if len(channel) > 1 else channel[0]
        if peak > best:
            best = peak
    return mul_to_db(best)


@dataclass
class SpeakerDetector:
    """
    Detecteur d'activite pour UNE source audio.

    - threshold_db : seuil de voix (= sensibilite). Au-dessus = candidat "parle".
    - attack_frames : nb de frames consecutives au-dessus du seuil avant de
      declarer "parle" (anti faux-positif sur un clic/pic bref).
    - release_frames : nb de frames consecutives sous le seuil avant de declarer
      "ne parle plus" (= delai de silence ; evite de couper sur une respiration).

    Les flux InputVolumeMeters arrivent ~20x/s (toutes les ~50 ms), donc
    attack=2 ~= 100 ms et release=8 ~= 400 ms (coherent avec nos defauts).
    """
    threshold_db: float = -35.0
    attack_frames: int = 2
    release_frames: int = 8

    speaking: bool = False
    last_db: float = DB_FLOOR
    _above: int = field(default=0, repr=False)
    _below: int = field(default=0, repr=False)

    def update(self, db: float) -> bool:
        """Injecte un niveau dB, met a jour l'etat, renvoie True si "parle"."""
        self.last_db = db
        if db >= self.threshold_db:
            self._above += 1
            self._below = 0
            if not self.speaking and self._above >= self.attack_frames:
                self.speaking = True
        else:
            self._below += 1
            self._above = 0
            if self.speaking and self._below >= self.release_frames:
                self.speaking = False
        return self.speaking


def active_speaker(detectors: dict[str, SpeakerDetector]) -> str | None:
    """
    Parmi les sources qui "parlent", renvoie le nom de la plus forte
    (logique du Contexte B : "le plus fort"). None si personne ne parle.
    """
    speaking = [(name, d.last_db) for name, d in detectors.items() if d.speaking]
    if not speaking:
        return None
    speaking.sort(key=lambda x: x[1], reverse=True)
    return speaking[0][0]
