// StreamDirector — icones (couche UI). Rend des icones lucide (https://lucide.dev,
// licence ISC) en SVG TEINTE a la couleur demandee, via QSvgRenderer. Pas de
// fichier externe : les chemins SVG sont embarques dans le .cpp -> l'icone est
// toujours disponible (rien a installer dans data/). Rendu en HiDPI (net).
#pragma once

#include <QPixmap>
#include <QString>

namespace sd::ui {

enum class Icon {
    Clapperboard,  // marque (header)
    User,          // avatar intervenant
    Settings,      // footer : parametres avances
    Sparkles,      // footer : assistant
    LayoutGrid,    // bandeau Stream Deck
};

// Pixmap de l'icone `which`, trait de couleur `colorHex` (ex "#3FB950"), carre de
// `sizePx` (taille logique). Rendu a la resolution physique de l'ecran (net HiDPI).
QPixmap icon(Icon which, const QString& colorHex, int sizePx);

}  // namespace sd::ui
