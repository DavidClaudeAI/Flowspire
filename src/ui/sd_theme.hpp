// Flowspire — jetons de design (couche UI), source unique de verite visuelle.
// Traduit la maquette Pencil ("Design System" + "Dock Live") en constantes C++.
// Theme sombre aligne sur OBS. Police Inter. Voir design-tokens.md a la racine.
//
// Qt ne lit pas Pencil : on reproduit ici les hex/tailles pour les feuilles de
// style (QSS) et le code. Ne PAS disperser de couleurs en dur ailleurs.
#pragma once

namespace sd::ui::theme {

// --- Couleurs (hex string, pretes pour QSS) ---
inline constexpr const char* kBg = "#141417";         // fond app (sous le dock) / onglet inactif
inline constexpr const char* kSurface1 = "#1B1B1F";   // fond du dock / de l'assistant
inline constexpr const char* kSurface2 = "#26262B";   // cartes
inline constexpr const char* kSurface3 = "#2F2F37";   // boutons, pistes
inline constexpr const char* kSurfaceBar = "#1F1F24"; // barres en-tete / pied (assistant)
inline constexpr const char* kBorder = "#3A3A42";     // bordures, separateurs
inline constexpr const char* kOnAccent = "#0B1220";   // texte sur fond accent/success (boutons pleins)
inline constexpr const char* kAccent = "#4A9EFF";     // bleu : plan large, marque
inline constexpr const char* kSuccess = "#3FB950";    // vert : parle / Auto ON
inline constexpr const char* kDanger = "#E85550";     // rouge : Auto OFF
inline constexpr const char* kWarning = "#E0A23B";    // jaune : bandeau Stream Deck
inline constexpr const char* kTextPrimary = "#ECECEE";
inline constexpr const char* kTextSecondary = "#9C9CA3";
inline constexpr const char* kTextTertiary = "#6E6E76";

// Variantes translucides (fond/bordure teintes) — memes valeurs que la maquette.
inline constexpr const char* kAccentFill = "#4A9EFF26";
inline constexpr const char* kAccentBorder = "#4A9EFF80";
inline constexpr const char* kSuccessFillSoft = "#3FB9502B"; // badge actif
inline constexpr const char* kSuccessFill = "#3FB95024";     // bouton Auto ON
inline constexpr const char* kSuccessBorder = "#3FB95080";
inline constexpr const char* kSuccessBorderStrong = "#3FB95099"; // carte active
inline constexpr const char* kSuccessAvatarFill = "#3FB9502E";   // avatar actif
inline constexpr const char* kDangerFill = "#E8555024";
inline constexpr const char* kDangerBorder = "#E8555080";
inline constexpr const char* kWarningFill = "#E0A23B14";

// --- Typographie (px) ---
inline constexpr int kFontTitle = 15;   // titre du dock (header)
inline constexpr int kFontBody = 13;    // nom d'intervenant
inline constexpr int kFontButton = 12;  // libelles boutons
inline constexpr int kFontLabel = 11;   // labels
inline constexpr int kFontSection = 10; // entetes de section (interlettrage)
inline constexpr int kFontSmall = 9;    // seuil, raccourcis

// --- Geometrie ---
inline constexpr int kRadiusCard = 8;
inline constexpr int kRadiusDock = 10;
inline constexpr int kRadiusButton = 6;
inline constexpr int kMeterHeight = 6; // hauteur piste vumetre / slider
inline constexpr int kAvatarSize = 36;

} // namespace sd::ui::theme
