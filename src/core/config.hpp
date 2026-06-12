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
    int attackFrames = 3;  // frames au-dessus du seuil -> "parle" (0.15 s ; defaut affine 2026-06-07)
    int releaseFrames = 8; // frames sous le seuil -> "ne parle plus"
};

// Reglages de rythme (secondes).
struct TimingSettings {
    double minShotSeconds = 3.0; // verrou anti-nervosite
    double maxShotSeconds = 10.0; // rafraichissement du plan (= style Cool ; recale sur corpus 2026-06-12)
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
    // est consideree silencieuse, detection par personne). Defaut recale a 1.0 sur corpus (2026-06-12) :
    // la grace de silence s'est revelee idiosyncratique (sans lien au rythme) -> une valeur commune.
    double silenceReactionSeconds = 1.0;
    // Repetition max d'un MEME plan (cadrage) avant respiration (re-tirage PONDERE dans le pool prive
    // de ce plan : autre camera / plan de reaction / plan large s'il y figure). 0 =
    // DESACTIVE (opt-in, comme l'anti ping-pong) : un profil neuf, un profil ou un preset perso
    // anterieur a la feature garde un comportement INCHANGE (aucune respiration imposee). Compte
    // PAR SCENE affichee, jamais par intervenant : deux cameras d'une meme personne ont chacune
    // leur compteur, qui repart de zero des qu'on change de cadrage (modele A). Les styles livres
    // portent leur valeur (Very Fast 1 .. Very Chill 7). Effet en secondes ~ maxShotSeconds * maxPlanRepeats.
    int maxPlanRepeats = 0;
};

// Contexte B : plusieurs parlent en meme temps (poids relatifs : rester sur le plan courant
// / plan large). Le VOLUME n'est PAS un critere de bascule -> pas d'option "le plus fort"
// (le fait de parler fort ne doit pas decider qu'on vous montre). Mettre en avant une
// personne se fait naturellement quand elle "gagne" la parole (contexte Single). Defauts
// tunes en reel (profil "Cyp Live") : forte preference pour le plan large des que 2+ parlent.
struct MultiWeights {
    int currentSpeaker = 10;
    int wideShot = 94;
};

// Contexte C : personne ne parle (poids relatifs). Defauts tunes en reel ("Cyp Live") :
// on revient quasi systematiquement au plan large quand le silence s'installe.
struct SilenceWeights {
    int lastSpeaker = 10;
    int wideShot = 94;
};

struct Config {
    int version = 1;
    std::vector<Speaker> speakers;
    std::string wideShotScene; // vide => pas de plan large
    AudioSettings audio;
    TimingSettings timing;
    MultiWeights whenMultiple;
    SilenceWeights whenSilence;
    // Nom du "style de realisation" actif (cf. rhythm_style.hpp) : un style est un bundle
    // nomme de parametres de RYTHME (mini/maxi/grace silence/anti ping-pong). Vide => "Perso"
    // (reglage manuel). Purement INFORMATIF : ce sont les valeurs de `timing` qui font foi ; ce
    // champ ne sert qu'a reafficher le bon style selectionne a la reouverture. Pose par
    // applyRhythmStyle ; remis a vide par l'UI des qu'un curseur de rythme est touche.
    std::string styleName;
};

// Serialise la config en JSON (texte indente).
std::string toJson(const Config& cfg);

// Parse une config depuis du JSON. Tolerant : les cles absentes prennent la
// valeur par defaut. Leve std::exception en cas de JSON invalide.
Config fromJson(const std::string& text);

} // namespace sd::core
