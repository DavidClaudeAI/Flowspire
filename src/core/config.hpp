// Flowspire — modele de configuration (pur) + (de)serialisation JSON.
// Reflete la "table de reglage" specs/flowspire-defaults.json.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace sd::core {

// Une scene du pool d'un intervenant, avec son poids relatif (pas un %).
struct SceneWeight {
    std::string scene; // nom de la scene OBS
    int weight = 1;    // poids relatif (>= 0) ; le % est calcule a l'affichage
};

// Un intervenant = un nom + une source audio OBS + un pool de scenes.
struct Speaker {
    std::string id;                  // identifiant stable
    std::string name;                // nom affiche
    std::string audioSource;         // nom de la source audio OBS
    std::vector<SceneWeight> scenes; // Contexte A : quand cette personne parle
    // Seuil de voix PROPRE a cet intervenant (dB). Absent => on utilise le seuil
    // global (audio.voiceThresholdDb). Regle au slider du dock et persiste dans le
    // profil -> survit a la fermeture d'OBS. Le Director s'en sert pour positionner
    // l'override au chargement (cf. setConfig).
    std::optional<double> thresholdDb;
};

// Reglages de detection audio.
struct AudioSettings {
    double voiceThresholdDb = -35.0; // seuil de voix (sensibilite)
    // Reserve : le plancher d'affichage actif est la constante kDbFloor
    // (audio_util.hpp). Ce champ existe pour le rendre configurable plus tard ;
    // il n'est pas encore lu par le coeur.
    double volumeFloorDb = -60.0;
    int attackFrames = 2;  // frames au-dessus du seuil -> "parle"
    int releaseFrames = 8; // frames sous le seuil -> "ne parle plus"
};

// Reglages de rythme (secondes).
struct TimingSettings {
    double minShotSeconds = 5.0;  // verrou anti-nervosite
    double maxShotSeconds = 12.0; // rafraichissement du plan
    // Anti ping-pong : DESACTIVE par defaut (0 = opt-in). Feature subtile, a valider
    // en live avant d'activer par defaut. Pour qu'elle agisse, la regler AU-DESSUS du
    // temps mini (spec : 12 s). Les profils existants gardent leur valeur enregistree.
    double pingPongWindowSeconds = 0.0;
    // "Delai avant reaction au silence" : une fois que PLUS PERSONNE ne parle, duree
    // pendant laquelle on GARDE le plan courant avant d'appliquer la decision de silence
    // (plan large / dernier locuteur). Un blanc court (respiration, temps entre deux
    // phrases) ne fait donc plus basculer ; si l'orateur reprend dans l'intervalle, on ne
    // l'a jamais quitte (reprise instantanee). 0 = reaction immediate (comportement
    // historique). N'affecte QUE le silence : un nouveau locuteur bascule normalement.
    // DISTINCT du "delai de silence" (audio.releaseFrames = a partir de quand une personne
    // est consideree silencieuse, detection par personne).
    double silenceReactionSeconds = 1.0;
};

// Contexte B : plusieurs parlent en meme temps (poids relatifs).
struct MultiWeights {
    int loudestSpeaker = 45;
    int currentSpeaker = 30;
    int wideShot = 25;
};

// Contexte C : personne ne parle (poids relatifs).
struct SilenceWeights {
    int lastSpeaker = 80;
    int wideShot = 20;
};

struct Config {
    int version = 1;
    std::vector<Speaker> speakers;
    std::string wideShotScene; // vide => pas de plan large
    AudioSettings audio;
    TimingSettings timing;
    MultiWeights whenMultiple;
    SilenceWeights whenSilence;
};

// Serialise la config en JSON (texte indente).
std::string toJson(const Config& cfg);

// Parse une config depuis du JSON. Tolerant : les cles absentes prennent la
// valeur par defaut. Leve std::exception en cas de JSON invalide.
Config fromJson(const std::string& text);

} // namespace sd::core
