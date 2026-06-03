# Flowspire — Design tokens (extraits de la maquette Pencil)

> Source de vérité visuelle : `F:\Max\design\Flowspire` (Pencil, frame "Design System" + "Dock Live").
> Thème sombre aligné sur OBS Studio. Police **Inter**. Coins arrondis 6–10 px.
> Ces tokens guident le code Qt du dock (Run 4).

## Couleurs

| Token | Hex | Usage |
|-------|-----|-------|
| surface1 | `#1B1B1F` | fond du dock |
| surface2 | `#26262B` | cartes (intervenant, à l'antenne) |
| surface3 | `#2F2F37` | boutons, pistes de vumètre/slider |
| bg app | `#141417` | fond hors dock (design system) |
| border | `#3A3A42` | bordures, séparateurs (1 px) |
| accent (bleu) | `#4A9EFF` | plan large, marque, bouton primaire |
| success (vert) | `#3FB950` | actif/parle, Auto ON |
| danger (rouge) | `#E85550` | Auto OFF, destructif |
| warning (jaune) | `#E0A23B` | bandeau Stream Deck |
| texte primaire | `#ECECEE` | texte principal |
| texte secondaire | `#9C9CA3` | texte atténué |
| texte tertiaire | `#6E6E76` | labels SECTION, raccourcis, inactif |

Variantes alpha utilisées : accent `#4A9EFF26` (fond), `#4A9EFF80` (bordure) ;
success `#3FB9502B`/`#3FB95024` (fond), `#3FB95080`/`#3FB95099` (bordure) ;
danger `#E8555024` (fond), `#E8555080` (bordure) ; warning `#E0A23B14` (fond).

## Typographie (Inter)

| Rôle | Taille / graisse |
|------|------------------|
| Titre | 22 / 700 |
| Sous-titre | 15 / 600 |
| Corps | 13 / 500–600 |
| Label | 11 / 600 |
| SECTION | 10–11 / 700 · interlettrage 1 |
| Petit (raccourci/seuil) | 9 / normal |

## Dock Live — structure (haut → bas)

- Largeur maquette 380, padding 16, gap 14, radius 10, fond surface1, bordure border.
- **Header** : icône `clapperboard` (accent) + "Flowspire" (15/700) | badge état "● Actif" (success, fond `#3FB9502B`, bordure `#3FB95080`, radius 5, pad [5,14]).
- **Section "INTERVENANTS"** (label tertiaire 10/700).
- **Carte intervenant** (radius 8, fond surface2, pad 10, gap 10) :
  - Avatar rond 36 px : icône `user`. Actif = vert (fill `#3FB9502E`, bordure success 2px) ; inactif = gris (fill surface3, bordure border 2px).
  - Centre : nom (13/600, primaire si actif sinon secondaire) + **vumètre** (piste surface3 h6 radius3, remplissage success h6).
  - Droite (largeur 84) : label "Seuil" (9, tertiaire) + **slider de seuil** (piste surface3 h6, remplissage `#9C9CA3`, poignée ellipse 12 px `#ECECEE`) — le vumètre et le slider sont alignés verticalement (vumètre au-dessus, slider seuil juste sous — cf. point UX "vumètre sous le slider").
  - Carte active : bordure success `#3FB95099` ; inactive : bordure border.
- **Séparateur** 1 px.
- **Carte "À l'antenne"** (surface2, radius 8, pad [12,10]) : label "À l'antenne" (11, tertiaire) + valeur (13/600) en `space_between`.
- **Section "PILOTAGE"**.
- **Grille boutons rangée 1** (gap 8) : un bouton par intervenant — nom (12/600) + raccourci (9, tertiaire), fond surface3, radius 6, pad [6,8].
- **Grille boutons rangée 2** : "Plan large" (accent), "Auto OFF" (danger), "Auto ON" (success) — chacun fond teinté + bordure teintée.
- **Bandeau Stream Deck** (warning) : icône `layout-grid` + "Stream Deck : mappez ces raccourcis (F13–F18)." (10/500), fond `#E0A23B14`, radius 6.
- **Séparateur** 1 px.
- **Footer** (gap 8) : "Paramètres avancés" (icône `settings`) + "Assistant" (icône `sparkles`), boutons secondaires surface3.

## Notes d'implémentation Qt (Run 4)
- Qt ne lit pas Pencil : on traduit ces tokens en QSS/QPalette. Centraliser les hex dans un en-tête de style (ex. `src/ui/sd_theme.hpp`) pour une seule source de vérité.
- Les raccourcis affichés (F13–F18) sont **indicatifs** : OBS laisse l'utilisateur mapper lui-même dans Réglages ▸ Raccourcis. Ne pas coder F13… en dur comme binding — afficher un libellé neutre ou l'état réel.
- Le badge "Actif/Pause" reflète l'état réel du pilotage auto (success/idle).
