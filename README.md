# StreamDirector

> **Nom de travail provisoire.**

Plugin **OBS Studio** open source qui agit comme un **réalisateur automatique** : il met en
grand l'intervenant **qui parle**, de façon **organique** (jamais mécanique), pour les lives
multicam à plusieurs (invités VDO.Ninja, micros…) — **sans aucun driver / câble audio virtuel**.

## Statut

🚧 **Phase de fondation.** Les specs et les maquettes sont validées ; on lève actuellement le
risque technique n°1 via un POC (voir [`poc/`](poc/)).

## Principe

À chaque décision de bascule, le plugin fait un **tirage au sort pondéré** (jamais une règle
rigide) selon 3 contextes : *une personne parle* / *plusieurs parlent* / *personne ne parle*.
L'utilisateur crée ses scènes dans OBS ; le plugin les **pilote** selon la source audio active.

## Architecture cible

- **Plugin natif OBS** (C++ / CMake via `obs-plugintemplate`), **multi-OS** (Windows, macOS, Linux).
- **Licence : GPLv2+** (imposée par le lien à libobs). Gratuit + dons.
- Pilotage via **hotkeys OBS** → compatibles Stream Deck et clavier nativement.

## POC

Le dossier [`poc/`](poc/) contient un prototype **Python jetable** qui prouve la détection
« qui parle » à partir de l'audio interne d'OBS (obs-websocket), sans câble virtuel.
Voir [`poc/README.md`](poc/README.md).

## Licence

[GPL-2.0-or-later](LICENSE).
