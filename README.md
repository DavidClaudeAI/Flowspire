# StreamDirector

> **Nom de travail provisoire.**

Plugin **OBS Studio** open source qui agit comme un **réalisateur automatique** : il met en
grand l'intervenant **qui parle**, de façon **organique** (jamais mécanique), pour les lives
multicam à plusieurs (invités VDO.Ninja, micros…) — **sans aucun driver / câble audio virtuel**.

## Statut

✅ **Pilotage automatique fonctionnel (Run 3).** Le plugin **bascule réellement la scène OBS**
selon qui parle — validé en conditions réelles. **Risque technique n°1 levé** : détection de qui
parle via l'audio interne d'OBS, **sans driver / câble audio virtuel**.

Acquis :
- **Cœur** (`src/core`) — détection + décision pondérée à 3 contextes, **testé** (doctest/CTest, sans OBS).
- **Plugin natif** — compile (`streamdirector.dll`) sur l'obs-plugintemplate (multi-OS), charge dans OBS.
- **Lecture audio native** (`src/obs`) — un `obs_volmeter` par source audio, niveaux internes d'OBS
  (zéro driver/câble virtuel), avec retombée temporelle et accès thread-safe.
- **Pilotage réel des scènes** (`src/obs/scene_switcher`) — bascule la scène programme OBS selon le
  locuteur ; reprend la main si la scène est changée manuellement.
- **Profils multi-config** (`src/obs/profiles_store`) — chaque profil = un fichier
  `profiles/<id>.json` (intervenant → source audio → scène(s) pondérées + plan large) ; le **profil
  actif** est la **source de vérité** lue par le moteur. Écriture atomique + `.bak`, récupération sans
  perte (`.bak`, puis scan). Sans intervenant configuré : mode lecture seule.
- **Raccourcis OBS** — activer/désactiver l'auto, forcer le plan large, forcer un intervenant
  (mappables Stream Deck et clavier).
- **Dock Qt** (`src/ui`) — interface alignée sur la maquette (thème sombre, icônes), un vumètre par
  intervenant **avec marqueur de seuil** (slider de sensibilité réglable en direct), scène réellement
  à l'antenne, badge d'état cliquable + boutons plan large / forcer / auto. Garde-fou : auto **OFF**
  par défaut. Auto-rafraîchi au lancement, défile si réduit en hauteur.
- **Multilingue** — i18n native OBS (`obs_module_text` + `data/locale/*.ini`) : le dock suit la
  langue d'OBS. Langues fournies : anglais (source) + français. Ajouter une langue = un fichier `.ini`.

Suite : assistant de configuration visuel (éditer intervenants/scènes/poids sans toucher au JSON).

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

## Configuration

Le plugin gère des **profils** (multi-config). Chaque profil est un fichier JSON (intervenants →
source audio → scène(s) pondérées + plan large) ; le **profil actif** est la **source de vérité**
lue par le moteur. Les profils vivent à :

```
Windows : %APPDATA%\obs-studio\plugin_config\streamdirector\profiles\
macOS   : ~/Library/Application Support/obs-studio/plugin_config/streamdirector/profiles/
Linux   : ~/.config/obs-studio/plugin_config/streamdirector/profiles/
```

`index.json` = catalogue + profil actif ; `<id>.json` = contenu d'un profil. Toute écriture est
**atomique** avec sauvegarde `.bak` ; un fichier illisible est récupéré automatiquement (`.bak`,
puis reconstruction par scan du dossier). L'ancien `config.json` mono-config est **migré** en profil
n°1 au premier lancement, puis laissé inerte (jamais relu).

La configuration s'édite **sans toucher au JSON** : assistant guidé (création de profil) et
paramètres avancés (édition fine, sélecteur de profil dans le dock). Sans aucun intervenant
configuré, le plugin reste en **lecture seule** et ne touche pas aux scènes.

## Architecture cible

- **Plugin natif OBS** (C++ / CMake via `obs-plugintemplate`), **multi-OS** (Windows, macOS, Linux).
- **Licence : GPLv2+** (imposée par le lien à libobs). Gratuit + dons.
- Pilotage via **hotkeys OBS** → compatibles Stream Deck et clavier nativement.

## Licence

[GPL-2.0-or-later](LICENSE).
