# POC — « Qui parle ? » (lever le risque n°1)

Ce POC **jetable** prouve qu'on peut détecter en temps réel **quelle source audio parle**
à partir des **niveaux audio internes d'OBS**, **sans aucun driver / câble audio virtuel**.

Il lit l'événement `InputVolumeMeters` d'obs-websocket — ce sont **exactement** les niveaux
que lira le futur **plugin natif** (`obs_volmeter_*`). Si le signal est exploitable ici,
il le sera (encore mieux) en natif. Le POC valide donc **l'hypothèse**, pas l'architecture finale.

> ⚠️ Code Python jetable. Le produit final reste un **plugin natif C++** (voir les specs).

## Prérequis

1. **OBS Studio** ouvert (v28+) avec au moins une source audio (micro, VDO.Ninja…).
2. **Serveur WebSocket activé** : `Outils → Paramètres du serveur WebSocket` → cocher *Activer*.
   *(Déjà activé automatiquement dans la config OBS locale lors de la mise en place.)*
3. Dépendance : `pip install obsws-python` *(déjà installé)*.

## Lancer

```bash
# 1) la logique de détection, sans OBS (auto-test) :
python poc/test_detector.py

# 2) le test live, OBS ouvert :
python poc/whos_speaking.py
```

Le mot de passe WebSocket est lu **automatiquement** depuis la config OBS locale.
On peut le forcer avec `--password XXX` ou la variable d'env `SD_OBS_PASSWORD`.
Régler la sensibilité : `--threshold -40` (plus bas = plus sensible).

## Ce qu'on doit observer

Un tableau temps réel : une ligne par source audio, avec un vu-mètre, le niveau en dB,
et `PARLE` / `silence`. La ligne `À L'ANTENNE ->` indique la source la plus forte qui parle
= ce sur quoi le plugin basculerait. Parle dans un micro → la bonne ligne passe à `PARLE`.

## Fichiers

- `detector.py` — logique **pure** et testable (seuil + hystérésis attaque/relâchement, façon Gabin).
- `test_detector.py` — auto-test hors-ligne (aucun OBS requis).
- `whos_speaking.py` — outil live connecté à OBS via websocket.
