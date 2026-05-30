# StreamDirector

> **Nom de travail provisoire.**

Plugin **OBS Studio** open source qui agit comme un **réalisateur automatique** : il met en
grand l'intervenant **qui parle**, de façon **organique** (jamais mécanique), pour les lives
multicam à plusieurs (invités VDO.Ninja, micros…) — **sans aucun driver / câble audio virtuel**.

## Statut

🚧 **Fondation (Run 1).** Specs et maquettes validées. **Risque technique n°1 levé** : on peut
détecter qui parle via l'audio interne d'OBS, **sans driver / câble audio virtuel**.

Acquis :
- **Cœur** (`src/core`) — détection + décision pondérée à 3 contextes, **testé** (doctest/CTest, sans OBS).
- **Plugin natif** — compile (`streamdirector.dll`) sur l'obs-plugintemplate (multi-OS), charge dans
  OBS et enregistre un **dock Qt** (placeholder pour l'instant).

Suite : brancher la lecture audio (`obs_volmeter`) et afficher les vumètres par intervenant.

## Build (Windows)

```bat
scripts\dev-build.bat      :: cœur + tests (rapide, sans OBS)
scripts\build-plugin.bat   :: plugin complet (1er run : télécharge libobs + Qt6)
scripts\install-local.bat  :: installe le plugin dans OBS (OBS fermé)
```

## Principe

À chaque décision de bascule, le plugin fait un **tirage au sort pondéré** (jamais une règle
rigide) selon 3 contextes : *une personne parle* / *plusieurs parlent* / *personne ne parle*.
L'utilisateur crée ses scènes dans OBS ; le plugin les **pilote** selon la source audio active.

## Architecture cible

- **Plugin natif OBS** (C++ / CMake via `obs-plugintemplate`), **multi-OS** (Windows, macOS, Linux).
- **Licence : GPLv2+** (imposée par le lien à libobs). Gratuit + dons.
- Pilotage via **hotkeys OBS** → compatibles Stream Deck et clavier nativement.

## Licence

[GPL-2.0-or-later](LICENSE).
