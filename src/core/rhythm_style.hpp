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

// Un style = un nom + les 5 parametres de rythme. Les defauts repliquent EXACTEMENT ceux de
// TimingSettings (un RhythmStyle vide == config livree par defaut) -> filet coherent quand une cle
// manque dans un preset perso (cf. rhythmStyleLibraryFromJson).
struct RhythmStyle {
    std::string name;                    // identifiant ET libelle affiche (nom propre, non traduit)
    double minShotSeconds = 3.0;         // verrou anti-nervosite
    double maxShotSeconds = 10.0;        // rafraichissement du plan (= defaut TimingSettings)
    double silenceReactionSeconds = 1.0; // grace de silence (= defaut TimingSettings)
    double pingPongWindowSeconds = 0.0;  // 0 = anti ping-pong (retour au plan large) desactive
    int maxPlanRepeats = 0;              // 0 = desactive (opt-in) ; sinon N repetitions max d'un meme plan
    MultiWeights whenMultiple;           // contexte B (2+ parlent) : { rester / plan large }
    SilenceWeights whenSilence;          // contexte C (silence)    : { dernier locuteur / plan large }
};

// Les 5 styles LIVRES, en lecture seule, du plus pose au plus vif : Very Chill, Chill, Cool,
// Fast, Very Fast. Grille validee au banc Flowspire-Bench (simulation du vrai moteur, corpus
// 57 episodes, 2026-06-12) ; chacun porte aussi sa repetition-max (hautes sur les rapides :
// temps tenu sur une personne ~ maxi x repetition). Cool == les defauts d'usine
// (cf. config.hpp) SAUF la repetition-max (opt-in : Cool porte 5, le defaut d'usine reste 0).
// L'utilisateur ne les modifie pas : il part de l'un d'eux, ajuste, puis enregistre sa
// variante dans la bibliotheque globale.
std::vector<RhythmStyle> builtinRhythmStyles();

// Applique un style a une config : copie le TEMPO (cfg.timing : 4 params) ET la tendance
// plan large (cfg.whenMultiple + cfg.whenSilence), puis memorise le nom du style actif
// (cfg.styleName). Ne touche NI au seuil/sensibilite (cfg.audio) NI a la scene plan large
// (cfg.wideShotScene) NI aux intervenants -> un changement de style ne casse pas la calibration.
void applyRhythmStyle(Config& cfg, const RhythmStyle& style);

// Construit un RhythmStyle a partir des valeurs ACTUELLES d'une config (tempo + tendance plan
// large), avec le nom donne. Sert a "Enregistrer sous..." : on capture le reglage courant.
RhythmStyle styleFromConfig(const Config& cfg, const std::string& name);

// --- Bibliotheque GLOBALE de styles utilisateur ("Enregistrer sous...") ---------------------
// Liste de styles NOMMES par l'utilisateur (RhythmStyle complets), INDEPENDANTE des profils :
// un style enregistre est reutilisable sur n'importe quel show. Les built-ins (Chill/Cool/
// Speed) n'y figurent PAS (fournis a part, en lecture seule). Stockee dans un unique fichier
// global (cf. couche obs : styles.json), contenu inline (pas d'index ni de fichiers separes).

// Serialise la bibliotheque (texte indente, pas de BOM).
std::string rhythmStyleLibraryToJson(const std::vector<RhythmStyle>& styles);

// Parse une bibliotheque. Tolerant : cles absentes -> defauts du RhythmStyle ; ignore les
// entrees sans nom. Leve std::exception si le JSON lui-meme est invalide.
std::vector<RhythmStyle> rhythmStyleLibraryFromJson(const std::string& text);

// Un nom est-il deja pris dans `styles` (comparaison exacte) ?
bool styleNameExists(const std::vector<RhythmStyle>& styles, const std::string& name);

// Rend `desired` unique en suffixant " (2)", " (3)"... au besoin. Evite AUSSI les noms des
// built-ins (un style perso ne doit pas s'appeler "Cool" et preter a confusion).
std::string makeUniqueStyleName(const std::vector<RhythmStyle>& styles, const std::string& desired);

} // namespace sd::core
