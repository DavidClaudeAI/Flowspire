// Flowspire — icones (couche UI). Rend des icones lucide (https://lucide.dev,
// licence ISC) en SVG TEINTE a la couleur demandee, via QSvgRenderer. Pas de
// fichier externe : les chemins SVG sont embarques dans le .cpp -> l'icone est
// toujours disponible (rien a installer dans data/). Rendu en HiDPI (net).
#pragma once

#include <QPixmap>
#include <QString>

namespace sd::ui {

enum class Icon {
    Clapperboard, // marque (header)
    User,         // avatar intervenant
    Settings,     // footer : parametres avances
    Sparkles,     // footer : assistant
    LayoutGrid,   // bandeau Stream Deck + recap "plan large"
    // --- Assistant de configuration (Run 5) ---
    X,            // fermer la fenetre
    Trash2,       // supprimer (intervenant / scene)
    Plus,         // ajouter (intervenant / scene)
    ArrowRight,   // bouton Suivant
    ChevronDown,  // liste deroulante (source / scene) + section repliable ouverte
    ChevronRight, // section repliable fermee (tiroir "parametres avances")
    Play,         // bouton "Activer la realisation auto"
    Pencil,       // editer un bloc depuis le recap
    Users,        // recap intervenants
    Video,        // recap cameras
    Clock,        // recap / etape rythme
    Mic,          // sources audio (etape prerequis)
    Info,         // bandeau d'info (etape rythme)
    // --- Parametres avances + Soutenir (Run 6) ---
    Bookmark,          // sidebar : profils
    Heart,             // soutenir le projet
    EllipsisV,         // menu "..." (profils, Run 7)
    Github,            // lien Sponsors
    Coffee,            // lien Ko-fi
    CreditCard,        // lien PayPal
    RotateCcw,         // reinitialiser aux defauts
    SlidersHorizontal, // sidebar : parametres generaux (reglages d'app)
    // --- Calibration auto du seuil (#3) ---
    Wand,  // bouton "calibrer" (carte + global) = reglage automatique du seuil
    Check, // etat "cale" (seuil calibre avec succes)
};

// Pixmap de l'icone `which`, trait de couleur `colorHex` (ex "#3FB950"), carre de
// `sizePx` (taille logique). Rendu a la resolution physique de l'ecran (net HiDPI).
QPixmap icon(Icon which, const QString& colorHex, int sizePx);

// Logo "Flowspire" (wordmark complet). Rendu a la HAUTEUR demandee (taille logique) ;
// la largeur derive du ratio d'origine (646x169) -> aucune deformation. Net HiDPI.
// Couleur figee = l'artwork (blanc), lisible sur le fond sombre du dock.
QPixmap logoFlowspire(int heightPx);

} // namespace sd::ui
