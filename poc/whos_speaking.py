"""
StreamDirector — POC live : "qui parle ?" lu depuis l'audio interne d'OBS.

OBJECTIF : lever le risque n.1 du projet.
Demontre qu'on peut detecter en temps reel quelle source audio parle, a partir
des niveaux internes d'OBS (evenement obs-websocket InputVolumeMeters),
SANS AUCUN driver/cable audio virtuel.

Ces niveaux sont exactement ceux que lira le futur plugin natif (obs_volmeter_*).

PREREQUIS
  1. OBS Studio ouvert (v28+), avec des sources audio (VDO.Ninja, micro...).
  2. Serveur WebSocket active : Outils > Parametres du serveur WebSocket.
  3. pip install obsws-python   (deja fait)

LANCER
  python poc/whos_speaking.py
  (mot de passe : via --password, ou variable d'env SD_OBS_PASSWORD,
   sinon lecture auto de la config OBS locale)

Quitter : Ctrl+C
"""

from __future__ import annotations
import argparse
import json
import os
import sys
import time
from pathlib import Path

from detector import SpeakerDetector, active_speaker, levels_to_db, DB_FLOOR

try:
    import obsws_python as obs
except ImportError:
    print("obsws-python manquant. Installe-le :  pip install obsws-python")
    sys.exit(1)


def read_obs_config_password() -> tuple[str | None, int]:
    """Lit mot de passe + port depuis la config locale d'obs-websocket (Windows)."""
    appdata = os.environ.get("APPDATA", "")
    cfg = Path(appdata) / "obs-studio" / "plugin_config" / "obs-websocket" / "config.json"
    if cfg.is_file():
        try:
            data = json.loads(cfg.read_text(encoding="utf-8"))
            return data.get("server_password"), int(data.get("server_port", 4455))
        except Exception:
            pass
    return None, 4455


def db_bar(db: float, width: int = 28) -> str:
    """Petit vu-metre ASCII de -60 dB (vide) a 0 dB (plein)."""
    frac = (db - DB_FLOOR) / (0.0 - DB_FLOOR)
    frac = max(0.0, min(1.0, frac))
    filled = int(round(frac * width))
    return "#" * filled + "." * (width - filled)


def main() -> int:
    ap = argparse.ArgumentParser(description="POC StreamDirector : qui parle ?")
    ap.add_argument("--host", default="localhost")
    ap.add_argument("--port", type=int, default=0, help="defaut: config OBS ou 4455")
    ap.add_argument("--password", default=None)
    ap.add_argument("--threshold", type=float, default=-35.0, help="seuil de voix (dB)")
    args = ap.parse_args()

    cfg_pw, cfg_port = read_obs_config_password()
    password = args.password or os.environ.get("SD_OBS_PASSWORD") or cfg_pw
    port = args.port or cfg_port

    if not password:
        print("Aucun mot de passe WebSocket trouve.")
        print("Donne-le avec --password XXX ou la variable SD_OBS_PASSWORD.")
        return 2

    print(f"Connexion a OBS {args.host}:{port} ...")
    try:
        ev = obs.EventClient(host=args.host, port=port, password=password,
                             subs=obs.Subs.INPUTVOLUMEMETERS)
    except Exception as e:
        print()
        print("Connexion impossible. Verifie que :")
        print("  - OBS Studio est OUVERT")
        print("  - Outils > Parametres du serveur WebSocket > serveur ACTIVE")
        print(f"  - port = {port}")
        print(f"\nDetail technique : {e}")
        return 1

    detectors: dict[str, SpeakerDetector] = {}
    state = {"last_render": 0.0, "frames": 0}

    def on_input_volume_meters(data):
        inputs = getattr(data, "inputs", None) or []
        seen = set()
        for item in inputs:
            name = item.get("inputName") or item.get("input_name")
            levels = item.get("inputLevelsMul") or item.get("input_levels_mul")
            if name is None:
                continue
            seen.add(name)
            db = levels_to_db(levels)
            det = detectors.get(name)
            if det is None:
                det = SpeakerDetector(threshold_db=args.threshold)
                detectors[name] = det
            det.update(db)

        state["frames"] += 1
        now = time.time()
        if now - state["last_render"] < 0.1:   # throttle ~10 fps
            return
        state["last_render"] = now
        render(seen)

    def render(seen):
        speaker = active_speaker(detectors)
        lines = []
        lines.append("\x1b[2J\x1b[H")  # clear + home
        lines.append("  StreamDirector POC — qui parle ? (audio interne OBS, sans cable virtuel)")
        lines.append(f"  seuil={args.threshold:.0f} dB   frames={state['frames']}   Ctrl+C pour quitter")
        lines.append("  " + "-" * 70)
        if not detectors:
            lines.append("  (en attente de niveaux audio... fais du bruit dans une source OBS)")
        for name, det in detectors.items():
            flag = "PARLE  " if det.speaking else "silence"
            mark = ">" if name == speaker else " "
            db = det.last_db
            dbtxt = "-inf" if db <= DB_FLOOR else f"{db:5.1f}"
            short = (name[:22]).ljust(22)
            lines.append(f" {mark}{short} [{db_bar(db)}] {dbtxt} dB  {flag}")
        lines.append("  " + "-" * 70)
        lines.append(f"  A L'ANTENNE -> {speaker if speaker else '(personne)'}")
        sys.stdout.write("\n".join(lines) + "\n")
        sys.stdout.flush()

    ev.callback.register(on_input_volume_meters)
    print("Connecte. En ecoute des niveaux audio (Ctrl+C pour quitter)...\n")

    try:
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\nArret. POC termine.")
    finally:
        try:
            ev.disconnect()
        except Exception:
            pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
