# StreamDirector

> **Le réalisateur automatique pour vos lives multicam dans OBS.**
> Il met en grand la personne **qui parle**, de façon **organique** — sans aucun driver ni câble audio virtuel.

*Nom de travail provisoire.*

---

## À quoi ça sert

Quand vous animez une émission à plusieurs (un talk, un podcast filmé, une table ronde, des invités à distance…), garder une vidéo **vivante** demande normalement un·e
réalisateur·rice qui bascule la caméra sur celui qui parle. Sans ça, on se rabat sur une
**grille fixe** où tout le monde est affiché en permanence : c'est plat et statique.

**StreamDirector fait ce travail de réalisation à votre place, automatiquement.** Il
écoute le son de chaque participant *à l'intérieur d'OBS* et **bascule sur la bonne scène**
dès que quelqu'un prend la parole, en douceur, jamais de façon mécanique ou nerveuse.

Le résultat : un live qui ressemble à une vraie production TV, sans que vous touchiez à
quoi que ce soit pendant l'émission.

### Fonctionne avec **toutes vos sources** — et toutes vos scènes

StreamDirector raisonne uniquement en **sources audio** et en **scènes OBS**. La règle est
simple : **dès qu'un vu-mètre bouge dans le mélangeur d'OBS, StreamDirector peut écouter
cette source** et réaliser dessus. Côté audio, une source peut être :

- votre **micro physique** local,
- un invité connecté via **VDO.Ninja**,
- une capture **NDI**, un navigateur, une carte d'acquisition,
- la sortie d'une autre application…
- bref, **n'importe quelle entrée du mélangeur audio d'OBS**.

Côté image, il pilote **n'importe quelle scène que vous avez créée** (gros plan, plan
d'écoute, plan large, avec vos overlays et votre design). Il se contente de les **afficher au
bon moment** : il ne crée, ne modifie et ne supprime jamais rien dans votre projet OBS.

### Sans aucun driver ni câble audio virtuel

C'est un parti pris fondateur : StreamDirector lit le son **directement à l'intérieur d'OBS**
(les niveaux audio natifs). **Aucun driver, aucun câble audio virtuel, aucun routage
externe** — rien qui puisse alourdir ou déstabiliser votre machine. Vous installez le
plugin, et c'est tout.

> **En résumé : si ça produit du son dans OBS et que vous avez des scènes, StreamDirector
> sait réaliser avec — sans rien brancher de plus.**

---

## Ce que ça fait

- 🎙️ **Détecte qui parle** en lisant les niveaux audio internes d'OBS (`obs_volmeter`),
  **sans aucun driver ni câble audio virtuel**.
- 🎬 **Bascule la scène automatiquement** sur la personne active, de façon organique.
- 🔗 **Mapping illimité** : associez autant de couples *source audio → scène(s)* que vous
  voulez.
- 🎲 **Variété des plans** : donnez plusieurs scènes à une même personne (gros plan, plan d'écoute…) et le plugin **alterne** pour éviter la monotonie.
- 🖼️ **Plan large** de repli quand plusieurs personnes parlent ou que personne ne parle.
- 🎛️ **Dock OBS** en temps réel : qui parle, scène à l'antenne, état du pilotage, vu-mètres et curseurs de sensibilité réglables **en direct**.
- ⌨️ **Raccourcis OBS natifs** (clavier **et Stream Deck**) : marche/arrêt, plan large,
  forcer une personne.
- 🗂️ **Profils** : une configuration par type d'émission, bascule en un clic.
- 🧙 **Assistant** de configuration pas-à-pas — tout se règle **sans toucher au moindre fichier**.
- 🌍 **Multilingue** (français + anglais aujourd'hui ; ajouter une langue = un fichier).
- 🛡️ **Stable** : conçu pour **ne jamais déstabiliser ni faire crasher OBS** (priorité n°1).

---

## Comment ça marche : « organique, jamais mécanique »

StreamDirector ne suit **jamais une règle rigide** du type « X parle → on montre X, point ».
À chaque décision, il fait un **tirage au sort pondéré** parmi plusieurs choix possibles.
C'est ce qui donne le côté naturel : deux situations identiques ne produisent pas forcément le même plan, exactement comme le ferait un humain derrière la régie.

Il distingue **trois situations**, et chacune a son propre tirage :

```
            ┌─────────────────────────────────────────────────────────────┐
            │                    QUI PARLE EN CE MOMENT ?                   │
            └─────────────────────────────────────────────────────────────┘
                 │                       │                        │
        ┌────────▼────────┐    ┌─────────▼─────────┐    ┌─────────▼─────────┐
        │  UNE personne   │    │ PLUSIEURS parlent │    │  PERSONNE ne parle│
        │                 │    │                   │    │                   │
        │ tirage parmi    │    │ tirage parmi :    │    │ tirage parmi :    │
        │ SES scènes      │    │ • le plus fort    │    │ • dernier orateur │
        │ (gros plan,     │    │ • rester sur      │    │ • plan large      │
        │  plan d'écoute, │    │   l'actuel        │    │                   │
        │  plan large…)   │    │ • plan large      │    │                   │
        └─────────────────┘    └───────────────────┘    └───────────────────┘
```

Par-dessus ces tirages, des **garde-fous** évitent la nervosité :

- **Temps mini de plan** : un plan reste affiché un minimum de temps avant qu'un changement soit autorisé (pas de coupe sur un éclat de rire).
- **Délai de silence** : on attend un court silence avant de considérer que quelqu'un a fini de parler (pas de coupe sur une respiration).
- **Temps maxi de plan** : au-delà d'une certaine durée sur le même plan, on rafraîchit la vue pour varier.
- **Anti ping-pong** : évite de revenir trop vite sur quelqu'un qu'on vient juste de
  quitter, quand deux personnes s'interrompent.

> **Les poids sont des proportions, pas des pourcentages.** Si vous mettez *gros plan 90* et *plan large 10*, vous verrez le gros plan ~90 % du temps. Pas besoin que la somme fasse 100 : le plugin calcule le % tout seul et l'affiche à côté.

---

## Installation

1. Récupérez `streamdirector.dll` (Windows) / `.so` (Linux) / `.plugin` (macOS) depuis une release, ou compilez-le (voir [Build & développement](#build--développement)).
2. Copiez-le dans le dossier des plugins d'OBS, puis **(re)lancez OBS**.
3. Dans OBS : menu **Docks → StreamDirector** pour afficher le panneau.

> OBS **28 ou supérieur** est requis.

---

## Démarrage rapide (l'assistant)

Avant de lancer l'assistant, **préparez vos scènes et vos sources audio dans OBS** :
StreamDirector ne les crée pas, il les pilote.

Cliquez sur **Assistant** dans le dock. Il vous guide en 6 écrans :

| Écran                          | Ce que vous y faites                                                                                                   |
| ------------------------------ | ---------------------------------------------------------------------------------------------------------------------- |
| **0 · Prérequis**              | Rappel : créez d'abord vos scènes et vos sources audio dans OBS.                                                       |
| **1 · Intervenants**           | Donnez un nom à chaque personne et reliez-la à sa **source audio** OBS.                                                |
| **2 · Scènes**                 | Pour chaque personne, choisissez **quelles scènes** montrer quand elle parle, avec un **poids** (chance d'apparition). |
| **3 · Plan large & priorités** | Désignez la scène de **plan large** et réglez quand y basculer.                                                        |
| **4 · Rythme & sensibilité**   | Ajustez le tempo et la détection. Les défauts marchent bien : ne touchez que si besoin.                                |
| **5 · Résumé**                 | Tout est récapitulé et **reste modifiable** ; un bouton **active la réalisation auto**.                                |

> ⚠️ Par sécurité, le pilotage automatique démarre **désactivé**. Tant qu'aucun intervenant n'est configuré, le plugin reste en **lecture seule** et ne touche pas à vos scènes.

---

## Le dock en direct

```
┌────────────────────────────────────────┐
│ StreamDirector              ● Actif     │  ← état : Actif / En pause / Lecture seule
│ Profil : Mon émission         ▾         │  ← sélecteur de profil
├────────────────────────────────────────┤
│ INTERVENANTS                            │
│  👤 Marie     ▓▓▓▓▓▓▒▒░░  PARLE         │  ← icône (vert = parle), vu-mètre…
│     Seuil  ───────●─────────            │  ← …et curseur de seuil (réglable en direct)
│  👤 Léo       ▓▓▒░░░░░░░  silent        │
│     Seuil  ──────●──────────            │
├────────────────────────────────────────┤
│ RÉALISATION                             │
│  À l'antenne : Marie – gros plan        │  ← scène réellement diffusée
│  [ Plan large ]   [ Auto ON / OFF ]     │  ← boutons de pilotage
└────────────────────────────────────────┘
```

- **Badge d'état** : *Actif* (le plugin réalise), *En pause* (figé), *Lecture seule*
  (aucune config).
- **Vu-mètre + curseur de seuil** par intervenant : le niveau bouge sous le curseur, vous calez le seuil visuellement. Ce réglage est **pris en compte immédiatement** et **mémorisé
  dans le profil** (il survit à la fermeture d'OBS).
- **À l'antenne** : la scène réellement diffusée à cet instant.
- **Boutons de pilotage** : plan large, marche/arrêt de l'auto. Doublés par les raccourcis.

---

## Tous les réglages en détail

Tout se règle depuis l'**Assistant** (création guidée) ou les **Paramètres avancés**
(édition fine, accessible via le dock). Voici chaque réglage et son effet.

### Intervenants

| Réglage          | Effet                                                                                     |
| ---------------- | ----------------------------------------------------------------------------------------- |
| **Nom**          | Le nom affiché de la personne (juste pour vous y retrouver).                              |
| **Source audio** | La source OBS qu'on écoute pour savoir si cette personne parle. C'est le cœur du système. |

### Caméras & poids

| Réglage   | Effet                                                                                                                                           |
| --------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| **Scène** | Une scène à montrer quand cette personne parle.                                                                                                 |
| **Poids** | Plus il est élevé, plus cette scène apparaît souvent **par rapport aux autres scènes de la même personne**. Le % est recalculé automatiquement. |

Ajoutez **plusieurs** scènes à une personne pour obtenir de la variété (ex. *gros plan 90*

+ *plan d'écoute 10*).

### Plan large & priorités

| Réglage                     | Effet                                                                                                                                        |
| --------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| **Scène de plan large**     | La scène de groupe, montrée quand plusieurs personnes parlent ou qu'un plan d'ensemble s'impose. **Laissez vide pour ne jamais l'utiliser.** |
| **Quand plusieurs parlent** | Poids du tirage entre *le plus fort* / *rester sur l'actuel* / *plan large*. *(défauts : 45 / 30 / 25)*                                      |
| **Quand personne ne parle** | Poids du tirage entre *dernier orateur* / *plan large*. *(défauts : 80 / 20)*                                                                |

### Rythme & sensibilité

| Réglage                      | Effet                                                                                                                      | Défaut        |
| ---------------------------- | -------------------------------------------------------------------------------------------------------------------------- | ------------- |
| **Temps mini de plan**       | Durée minimale d'affichage d'un plan avant qu'un changement soit permis (évite les coupes nerveuses).                      | 5 s           |
| **Temps maxi de plan**       | Au-delà de cette durée sur le même plan, on rafraîchit la vue pour varier.                                                 | 12 s          |
| **Anti ping-pong (mémoire)** | Évite de recouper trop vite vers quelqu'un qui vient de parler. **0 = désactivé** ; n'agit qu'**au-dessus du temps mini**. | 0 (désactivé) |
| **Seuil de voix**            | Niveau sonore au-dessus duquel une personne est considérée comme « parle ». Plus bas = plus sensible.                      | −35 dB        |
| **Délai de silence**         | Durée de silence avant de considérer qu'une personne a fini de parler.                                                     | court         |

> **Tout est pris en compte à chaud.** Dès que vous enregistrez (ou que vous bougez un curseur de seuil dans le dock), le moteur applique le changement **immédiatement** — pas besoin de redémarrer le pilotage ni OBS.

### Seuil par intervenant (réglage en direct)

Le curseur de seuil sous chaque intervenant, dans le dock, **surpasse le seuil global pour cette seule personne**. Pratique quand un micro est plus fort qu'un autre. Ce réglage est **sauvegardé dans le profil actif** et restauré au lancement suivant.

---

## Profils

Un **profil** = une configuration complète (intervenants, scènes, plan large, rythme).
Créez-en un par type d'émission (« 4 invités + moi », « Duo », « Table ronde »…) et
**basculez de l'un à l'autre en un clic** depuis le dock.

- Création via l'**assistant** (guidée) ou **profil vierge** (à remplir dans les paramètres).
- Actions par profil : **charger**, **renommer**, **dupliquer**, **éditer**, **supprimer**.
- Le **profil actif** est la **source de vérité** lue par le moteur.

---

## Raccourcis (clavier & Stream Deck)

StreamDirector expose ses actions comme des **raccourcis OBS natifs**. Ils n'ont **pas de
touche par défaut** : ouvrez **OBS → Paramètres → Raccourcis clavier** et assignez la touche
de votre choix (par exemple **F13–F18**, idéales pour un Stream Deck).

| Action (libellé dans OBS)                                | Effet                                                                 |
| -------------------------------------------------------- | --------------------------------------------------------------------- |
| **StreamDirector : activer/désactiver le pilotage auto** | Marche/arrêt de la réalisation automatique.                           |
| **StreamDirector : forcer le plan large**                | Bascule immédiatement sur le plan large.                              |
| **StreamDirector : forcer l'intervenant 1 … 8**          | Bascule immédiatement sur la personne choisie (jusqu'à 8 raccourcis). |

> Sur un **Stream Deck**, ajoutez un bouton *Touche* / *Hotkey* qui envoie la même touche que celle assignée dans OBS : le bouton pilote alors StreamDirector. Au-delà de 8 personnes, utilisez les boutons du dock.

---

## Reprendre la main pendant le live

Vous gardez le contrôle à tout moment, sans casser l'automatisme :

- **Forcer un plan** (une personne ou le plan large) : bascule immédiate, puis les règles
  normales reprennent (le plugin continue de réaliser ensuite). Un simple « coup de pouce »
  de réalisateur.
- **Mettre en pause** (Auto OFF) : le plugin **fige** la scène actuelle et arrête de
  switcher.
- **Reprendre** (Auto ON) : il repart de qui parle à cet instant.
- Changer une scène **manuellement** dans OBS rend aussi la main au plugin proprement.

---

## Où sont stockés les réglages

Les profils vivent dans le dossier de config d'OBS :

```
Windows : %APPDATA%\obs-studio\plugin_config\streamdirector\profiles\
macOS   : ~/Library/Application Support/obs-studio/plugin_config/streamdirector/profiles\
Linux   : ~/.config/obs-studio/plugin_config/streamdirector/profiles\
```

- `index.json` = catalogue des profils + profil actif.
- `<id>.json` = le contenu d'un profil.

Toute écriture est **atomique** avec sauvegarde `.bak` ; un fichier illisible est récupéré
automatiquement (depuis le `.bak`, puis par reconstruction en scannant le dossier). Vous
n'avez **jamais besoin d'éditer ces fichiers à la main** — l'interface fait tout.

---

## Build & développement

Plugin natif **C++ / CMake** bâti sur l'`obs-plugintemplate` officiel, **multi-OS**
(Windows · macOS · Linux). Licence **GPLv2+** (imposée par le lien à libobs).

```bat
scripts\dev-build.bat      :: cœur + tests (rapide, sans OBS)
scripts\build-plugin.bat   :: plugin complet (1er run : télécharge libobs + Qt6)
scripts\install-local.bat  :: installe le plugin dans OBS (OBS doit être fermé)
```

Le **cœur de décision** (`src/core`) est **pur** (aucune dépendance OBS) et **testé**
unitairement (doctest/CTest) — on peut faire évoluer la logique de réalisation sans OBS.

---

## Versions & mises à jour

StreamDirector suit le **versionnage sémantique** (`MAJEUR.MINEUR.CORRECTIF`) :

- **CORRECTIF** — correction de bug, sans changement visible (`0.1.0` → `0.1.1`).
- **MINEUR** — nouvelle fonctionnalité rétrocompatible (`0.1.0` → `0.2.0`).
- **MAJEUR** — changement cassant (`0.9.0` → `1.0.0`).

La version vit à **un seul endroit** (`buildspec.json`) et s'affiche en bas du dock. Pour la
faire évoluer, utilisez le script fourni :

```bat
python scripts\bump-version.py patch     :: 0.1.0 -> 0.1.1
python scripts\bump-version.py minor     :: 0.1.0 -> 0.2.0
python scripts\bump-version.py 1.4.2     :: version explicite
```

Pour **publier** une version (le CI fabrique alors les installeurs des 3 OS et crée la
**Release** GitHub) : `git tag X.Y.Z` puis `git push origin main --tags`.

---

## Crédits & inspiration

StreamDirector s'inspire de **[Gabin](https://github.com/one-click-studio/gabin)**, le
réalisateur automatique open source de **[One Click Studio](https://oneclickstudio.fr)**
(licence MIT). Gabin a posé l'idée d'une
réalisation **organique** pilotée par le son ; StreamDirector en reprend l'esprit pour
l'amener **nativement dans OBS, sans driver ni câble audio virtuel**.

## Licence

[GPL-2.0-or-later](LICENSE). Gratuit et open source.

## Soutenir

StreamDirector est **gratuit et open source — aucune fonction n'est jamais bloquée**. S'il
vous fait gagner du temps sur vos lives, un petit coup de pouce aide à le maintenir :
le bouton **♥ Soutenir** se trouve dans les paramètres du plugin.
