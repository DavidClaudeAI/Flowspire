// Flowspire — styles de realisation (modele PUR, sans OBS ni Qt).
//
// Un "style de realisation" = un bundle NOMME de parametres de RYTHME, choisi d'un clic
// (le "temperament" du realisateur : pose, equilibre, vif). Choisir un style = copier ses
// valeurs dans la Config courante (cf. applyRhythmStyle).
//
// PERIMETRE = la POLITIQUE DE REALISATION complete (decision produit, veille §9/§14 +
// recentrage 2026-06-06, apres test reel) :
//   - INCLUS  : le tempo (temps mini/maxi, reaction au silence, anti ping-pong) ET la
//               tendance plan large (poids whenMultiple/whenSilence). Le temperament couvre
//               les 3 situations (1 parle / 2+ parlent / silence), pas seulement le tempo :
//               c'est ce qui faisait que "Speed" restait lent en multi/silence avant.
//   - EXCLUS  : le seuil / la sensibilite ET l'attaque/relache (famille "detection" : depend
//               de la salle et des micros, releve de l'assistant de calibration, pas du
//               temperament). Un changement de style ne casse donc jamais la calibration audio.
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
    double pingPongWindowSeconds = 0.0;   // 0 = anti ping-pong (retour au plan large) desactive
    MultiWeights whenMultiple;            // contexte B (2+ parlent) : { rester / plan large }
    SilenceWeights whenSilence;           // contexte C (silence)    : { dernier locuteur / plan large }
};

// Les 3 styles LIVRES, en lecture seule, du plus pose au plus vif :
//   Chill (plans longs), Cool (= defauts "Cyp Live"), Speed (coupe nette + anti ping-pong).
// L'utilisateur ne les modifie pas : il part de l'un d'eux, ajuste, puis enregistre sa
// variante dans la bibliotheque globale (etape ulterieure).
std::vector<RhythmStyle> builtinRhythmStyles();

// Applique un style a une config : copie le TEMPO (cfg.timing : 4 params) ET la tendance
// plan large (cfg.whenMultiple + cfg.whenSilence), puis memorise le nom du style actif
// (cfg.styleName). Ne touche NI au seuil/sensibilite (cfg.audio) NI a la scene plan large
// (cfg.wideShotScene) NI aux intervenants -> un changement de style ne casse pas la calibration.
void applyRhythmStyle(Config& cfg, const RhythmStyle& style);

} // namespace sd::core
