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
    LayoutGrid,    // bandeau Stream Deck + recap "plan large"
    // --- Assistant de configuration (Run 5) ---
    X,           // fermer la fenetre
    Trash2,      // supprimer (intervenant / scene)
    Plus,        // ajouter (intervenant / scene)
    ArrowRight,  // bouton Suivant
    ChevronDown, // liste deroulante (source / scene)
    Play,        // bouton "Activer la realisation auto"
    Pencil,      // editer un bloc depuis le recap
    Users,       // recap intervenants
    Video,       // recap cameras
    Clock,       // recap / etape rythme
    Mic,         // sources audio (etape prerequis)
    Info,        // bandeau d'info (etape rythme)
    // --- Parametres avances + Soutenir (Run 6) ---
    Bookmark,    // sidebar : profils
    Heart,       // soutenir le projet
    EllipsisV,   // menu "..." (profils, Run 7)
    Github,      // lien Sponsors
    Coffee,      // lien Ko-fi
    CreditCard,  // lien PayPal
    RotateCcw,   // reinitialiser aux defauts
    SlidersHorizontal,  // sidebar : parametres generaux (reglages d'app)
};

// Pixmap de l'icone `which`, trait de couleur `colorHex` (ex "#3FB950"), carre de
// `sizePx` (taille logique). Rendu a la resolution physique de l'ecran (net HiDPI).
QPixmap icon(Icon which, const QString& colorHex, int sizePx);

}  // namespace sd::ui
