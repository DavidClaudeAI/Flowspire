// Flowspire — styles de realisation (modele PUR, sans OBS ni Qt).
//
// Un "style de realisation" = un bundle NOMME de parametres de RYTHME, choisi d'un clic
// (le "temperament" du realisateur : pose, equilibre, vif). Choisir un style = copier ses
// valeurs dans la Config courante (cf. applyRhythmStyle).
//
// PERIMETRE VOLONTAIREMENT RESTREINT AU RYTHME (decision produit, veille §9/§14) :
//   - INCLUS  : temps mini de plan, temps maxi, delai avant reaction au silence, anti ping-pong.
//   - EXCLUS  : le seuil / la sensibilite (depend de la salle et des micros -> calibre a part),
//               la tendance plan large (poids whenMultiple/whenSilence -> reste manuel, preserve
//               le tuning de l'utilisateur), l'attaque/relache (famille "detection", releve de
//               l'assistant de calibration, pas du temperament).
// Ce decouplage garantit qu'un changement de style ne casse jamais la calibration audio.
#pragma once

#include <string>
#include <vector>

#include "core/config.hpp"

namespace sd::core {

// Un style = un nom + les 4 parametres de rythme. Les defauts repliquent ceux de
// TimingSettings (un RhythmStyle vide == config livree par defaut).
struct RhythmStyle {
    std::string name;                     // identifiant ET libelle affiche (nom propre, non traduit)
    double minShotSeconds = 3.0;          // verrou anti-nervosite
    double maxShotSeconds = 6.0;          // rafraichissement du plan
    double silenceReactionSeconds = 1.5;  // grace de silence
    double pingPongWindowSeconds = 0.0;   // 0 = anti ping-pong desactive
};

// Les 3 styles LIVRES, en lecture seule, du plus pose au plus vif :
//   Chill (plans longs), Cool (= defauts "Cyp Live"), Speed (coupe nette + anti ping-pong).
// L'utilisateur ne les modifie pas : il part de l'un d'eux, ajuste, puis enregistre sa
// variante dans la bibliotheque globale (etape ulterieure).
std::vector<RhythmStyle> builtinRhythmStyles();

// Applique un style a une config : copie UNIQUEMENT les 4 parametres de rythme dans
// cfg.timing et memorise le nom du style actif (cfg.styleName). Ne touche ni au seuil, ni
// aux poids plan large, ni a l'attaque/relache.
void applyRhythmStyle(Config& cfg, const RhythmStyle& style);

} // namespace sd::core
