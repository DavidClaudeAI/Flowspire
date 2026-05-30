// StreamDirector — modele de configuration (pur) + (de)serialisation JSON.
// Reflete la "table de reglage" specs/streamdirector-defaults.json.
#pragma once

#include <string>
#include <vector>

namespace sd::core {

// Une scene du pool d'un intervenant, avec son poids relatif (pas un %).
struct SceneWeight {
    std::string scene;   // nom de la scene OBS
    int weight = 1;      // poids relatif (>= 0) ; le % est calcule a l'affichage
};

// Un intervenant = un nom + une source audio OBS + un pool de scenes.
struct Speaker {
    std::string id;                     // identifiant stable
    std::string name;                   // nom affiche
    std::string audioSource;            // nom de la source audio OBS
    std::vector<SceneWeight> scenes;    // Contexte A : quand cette personne parle
};

// Reglages de detection audio.
struct AudioSettings {
    double voiceThresholdDb = -35.0;  // seuil de voix (sensibilite)
    double volumeFloorDb = -60.0;     // plancher d'affichage
    double vadThreshold = 0.75;       // voix vs bruit (reserve usage futur)
    int attackFrames = 2;             // frames au-dessus du seuil -> "parle"
    int releaseFrames = 8;            // frames sous le seuil -> "ne parle plus"
};

// Reglages de rythme (secondes).
struct TimingSettings {
    double minShotSeconds = 3.0;          // verrou anti-nervosite
    double maxShotSeconds = 12.0;         // rafraichissement du plan
    double pingPongWindowSeconds = 12.0;  // memoire des derniers locuteurs
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
    std::string wideShotScene;   // vide => pas de plan large
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

}  // namespace sd::core
