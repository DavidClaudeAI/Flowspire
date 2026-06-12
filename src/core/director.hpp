// Flowspire — moteur de decision "quelle scene montrer ?".
// Pur (aucune dependance OBS). Implemente le modele unifie a 3 contextes,
// chacun resolu par un TIRAGE AU SORT PONDERE (jamais une regle rigide) :
//   A - une personne parle      : tirage parmi les scenes de cette personne
//   B - plusieurs parlent       : { rester sur le plan courant / plan large } (le volume
//                                  n'est PAS un critere -> pas d'option "le plus fort")
//   C - personne ne parle       : { dernier locuteur / plan large }
//
// Garde-fous :
//   - "hold" : un plan affiche est maintenu un minimum de temps (verrou temps-mini,
//     minShotSeconds) avant de pouvoir etre remplace. Concerne les plans de LOCUTEUR,
//     les plans FORCES, et le PLAN LARGE des qu'il succede a un plan deja affiche (evite
//     le "flash" : on passe au plan large puis quelqu'un parle aussitot). Seule exception :
//     le plan large de DEPART (init, avant toute bascule) n'est pas un hold -> la premiere
//     prise de parole y est suivie sans delai.
//   - rafraichissement au temps-max (variete des plans).
//   - memoisation du tirage aleatoire par "situation" -> pas de scintillement.
//   - une bascule bloquee par le verrou est re-evaluee a chaque tick et
//     s'execute des que le verrou tombe.
//
// Anti-nervosite : deux garde-fous complementaires.
//   - verrou temps-mini : empeche tout re-cut avant minShotSeconds.
//   - anti ping-pong (ownerLeftAt_) = "retour au plan large sur echange rapide" : quand
//     deux personnes se relaient comme SEULS locuteurs (navette), si recouper reviendrait
//     sur un plan qu'on vient de quitter (< fenetre pingPongWindowSeconds), on se RECULE
//     sur le plan large le temps que ca respire, puis on repart. N'agit QUE si un plan
//     large existe (sinon le temps-mini gere). Fenetre a 0 => off.
//
// Variete forcee (repetition-max) : pour eviter de marteler un meme cadrage quand une personne
// monologue, on borne le nombre de fenetres temps-max consecutives passees sur le MEME plan
// (cfg.timing.maxPlanRepeats). Au-dela, on RESPIRE : re-tirage PONDERE dans le pool de l'intervenant
// prive du plan sur-repete (autre camera, plan de reaction, plan large s'il y figure ; repli sur le
// plan large global sinon) -> ce n'est PAS une regle rigide "va au plan large", les poids decident.
// Compte PAR SCENE affichee (modele A) : deux cameras d'une meme personne ont chacune leur compteur,
// remis a zero des qu'on change de cadrage -> leur alternance suffit deja a aerer. Opt-in (0 = off,
// retrocompat) et limite au contexte Single (mono-locuteur) ; multi/silence ont deja leur variete par
// re-tirage. Sans aucune alternative jouable (pool d'une seule scene, pas de plan large) : on reste.
#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/config.hpp"
#include "core/speaker_detector.hpp"

namespace sd::core {

enum class Context { Silence, Single, Multiple };

struct Decision {
    std::string scene;     // scene a afficher
    std::string owner;     // id de l'intervenant proprietaire ("" = plan large/aucun)
    bool switched = false; // true si on a change de scene a ce tick
    Context context = Context::Silence;
};

class Director {
public:
    // RNG : renvoie une valeur dans [0, 1). Injectable pour des tests deterministes.
    using Rng = std::function<double()>;

    explicit Director(Config cfg, Rng rng = {});

    void setConfig(const Config& cfg);
    const Config& config() const { return cfg_; }

    void setAutoEnabled(bool enabled) { autoEnabled_ = enabled; }
    bool autoEnabled() const { return autoEnabled_; }

    // Seuil de voix REGLE EN DIRECT pour un intervenant (slider du dock). Surpasse
    // le seuil global (cfg.audio.voiceThresholdDb) pour ce seul intervenant, sans
    // toucher la config. Reinitialise par setConfig (la config/JSON reste la
    // source de verite au rechargement). No-op si l'intervenant est inconnu.
    void setSpeakerThreshold(const std::string& speakerId, double db);

    // Seuil effectif d'un intervenant : son override s'il existe, sinon le seuil
    // global. Sert a positionner le slider a l'ouverture.
    double speakerThresholdDb(const std::string& speakerId) const;

    // Avance d'un tick. `now` = secondes monotones. `levelsDb` = niveau courant
    // par id d'intervenant. Renvoie la decision (scene a afficher).
    Decision update(double now, const std::map<std::string, double>& levelsDb);

    // Force une scene precise (action manuelle : Stream Deck / clavier).
    // Le plan force est un "hold" : il tient le temps-mini avant d'etre remplace.
    Decision forceScene(double now, const std::string& scene, const std::string& owner = "");

    // Force le plan d'un intervenant precis : tire une scene dans SON pool
    // (meme tirage pondere que le contexte A). C'est un "hold", comme forceScene.
    // No-op (Decision.switched == false, scene courante inchangee) si l'intervenant
    // est inconnu ou n'a aucune scene jouable.
    Decision forceSpeaker(double now, const std::string& speakerId);

    // Une scene fait-elle partie de la REGIE ? -> true si c'est le plan large OU une
    // scene du pool d'un intervenant. Renseigne alors `owner` : id de l'intervenant
    // proprietaire, ou "" pour le plan large. Sert au forcage par clic dans le dock
    // natif d'OBS : distinguer une scene DE la regie (toujours forcage temporaire)
    // d'une scene HORS regie (comportement configurable). Pur, sans effet de bord.
    bool sceneInProgram(const std::string& scene, std::string& owner) const;

    const std::string& currentScene() const { return currentScene_; }
    Context lastContext() const { return lastContext_; }

    // Intervenants qui "parlent" a ce tick, du plus fort au plus faible.
    std::vector<std::string> speakingIds() const { return speakingSorted_; }

private:
    const Speaker* findSpeaker(const std::string& id) const;
    double rngValue(); // borne le RNG injecte dans [0, 1)
    std::string drawSceneFromPool(const std::vector<SceneWeight>& pool);
    // Resout une scene REELLEMENT jouable pour la cible demandee, avec fallback :
    //   1) la cible (owner -> une de ses scenes ; wide -> plan large)
    //   2) sinon, s'il y a des locuteurs : le plus fort -> une de ses scenes
    //   3) sinon : le plan large s'il existe
    // Garantit qu'on ne reste jamais sur du vide quand quelqu'un parle et qu'une
    // scene est disponible. Renvoie false si rien n'est jouable (aucun pool,
    // aucun plan large). Renseigne outScene/outOwner en cas de succes.
    bool resolvePlayable(const std::string& owner, bool wide, std::string& outScene, std::string& outOwner);
    // `recordLeave` (defaut true) : memorise l'instant ou l'on quitte le proprietaire
    // courant (anti ping-pong). Les FORCAGES (Stream Deck/clavier) passent false : un
    // choix manuel ne doit pas faire croire a une navette sur le retour auto suivant.
    void commit(double now, const std::string& scene, const std::string& owner, bool hold, Decision& out,
                bool recordLeave = true);
    // Vrai si basculer vers `owner` serait une NAVETTE : on a quitte ce plan il y a
    // moins de pingPongWindowSeconds. Toujours faux si la fenetre vaut 0 (anti
    // ping-pong desactive) ou si owner est vide.
    bool isPingPongBounce(double now, const std::string& owner) const;
    // Repetition-max : a-t-on atteint le cap de repetition sur `candidate` (le plan qu'on rejouerait) ?
    // Vrai si la feature est active (maxPlanRepeats > 0), que `candidate` est le MEME plan que l'actuel
    // (et que ce n'est pas le plan large) et qu'on l'a deja tenu maxPlanRepeats fenetres temps-max. Pur ;
    // ne dit PAS s'il existe une respiration jouable -> c'est resolveBreather qui tranche.
    bool needsRepetitionBreather(const std::string& candidate) const;
    // Repetition-max : choisit un plan de RESPIRATION pour `owner` en EXCLUANT le plan sur-repete
    // `avoid`, par TIRAGE PONDERE dans le pool de l'intervenant prive de `avoid` (autre camera, plan de
    // reaction, plan large s'il y figure...). Repli sur le plan large GLOBAL si le pool n'offre rien
    // d'autre. Renvoie false si aucune respiration n'est jouable (l'appelant garde alors le plan).
    bool resolveBreather(const std::string& owner, const std::string& avoid, std::string& outScene,
                         std::string& outOwner);

    Config cfg_;
    Rng rng_;
    bool autoEnabled_ = true;

    std::map<std::string, SpeakerDetector> detectors_;
    std::map<std::string, double> thresholdOverride_; // seuil regle en direct par intervenant
    std::vector<std::string> speakingSorted_;         // ids parlants, plus fort -> plus faible

    std::string currentScene_;
    std::string currentOwner_; // "" = plan large / aucun proprietaire
    bool hold_ = false;        // le plan courant doit-il etre maintenu (verrou) ?
    std::string lastSpeaker_;  // dernier intervenant a avoir parle
    double lastSwitch_ = -1e9; // instant du dernier changement de scene
    Context lastContext_ = Context::Silence;
    // "Delai avant reaction au silence" : instant ou le silence (plus personne ne parle) a
    // commence. < 0 => on n'est pas en silence. Tant que (now - silenceSince_) < seuil, on
    // GARDE le plan courant au lieu d'appliquer la decision de silence.
    double silenceSince_ = -1.0;

    // Memoise la decision aleatoire par "situation" (evite le scintillement :
    // on ne retire pas un nouveau plan a chaque tick d'une meme situation).
    std::string decisionKey_;
    std::string cachedOwner_;
    bool cachedWide_ = false;

    // Anti ping-pong : instant ou l'on a QUITTE chaque proprietaire de plan. Sert a
    // detecter une bascule "retour" trop rapide (navette) -> on prefere alors rester.
    std::map<std::string, double> ownerLeftAt_;

    // Repetition-max ("variete forcee") : nombre de fenetres temps-max consecutives durant
    // lesquelles le MEME plan (currentScene_) est reste a l'antenne. Incremente quand un
    // rafraichissement rejoue la meme scene, remis a 1 des qu'on change de cadrage (modele A :
    // par scene, jamais par intervenant). Au-dela de cfg.timing.maxPlanRepeats, on respire sur le
    // plan large (cf. shouldBreatheForRepetition). 0 tant qu'aucun plan n'a ete affiche.
    int planRepeatCount_ = 0;
};

} // namespace sd::core
