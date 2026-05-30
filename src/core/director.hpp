// StreamDirector — moteur de decision "quelle scene montrer ?".
// Pur (aucune dependance OBS). Implemente le modele unifie a 3 contextes,
// chacun resolu par un TIRAGE AU SORT PONDERE (jamais une regle rigide) :
//   A - une personne parle      : tirage parmi les scenes de cette personne
//   B - plusieurs parlent       : { le plus fort / rester / plan large }
//   C - personne ne parle       : { dernier locuteur / plan large }
//
// Garde-fous :
//   - "hold" : un plan de LOCUTEUR (ou un plan FORCE) doit etre maintenu un
//     minimum de temps (verrou temps-mini) pour eviter la nervosite. Un plan
//     large issu d'un simple silence n'est PAS un hold : une prise de parole
//     peut le remplacer immediatement.
//   - rafraichissement au temps-max (variete des plans).
//   - memoisation du tirage aleatoire par "situation" -> pas de scintillement.
//   - une bascule bloquee par le verrou est re-evaluee a chaque tick et
//     s'execute des que le verrou tombe.
//
// Anti-nervosite : assuree aujourd'hui par le verrou temps-mini. La memoire des
// derniers locuteurs (history_/recentSpeakers) est de la FONDATION exposee pour
// un raffinement futur facon Gabin (biaiser le choix des plans selon qui vient
// de parler) ; elle n'influence PAS encore la decision.
#pragma once

#include <deque>
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
    std::string scene;      // scene a afficher
    std::string owner;      // id de l'intervenant proprietaire ("" = plan large/aucun)
    bool switched = false;  // true si on a change de scene a ce tick
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

    // Avance d'un tick. `now` = secondes monotones. `levelsDb` = niveau courant
    // par id d'intervenant. Renvoie la decision (scene a afficher).
    Decision update(double now, const std::map<std::string, double>& levelsDb);

    // Force une scene precise (action manuelle : Stream Deck / clavier).
    // Le plan force est un "hold" : il tient le temps-mini avant d'etre remplace.
    Decision forceScene(double now, const std::string& scene, const std::string& owner = "");

    const std::string& currentScene() const { return currentScene_; }
    Context lastContext() const { return lastContext_; }

    // Intervenants qui "parlent" a ce tick, du plus fort au plus faible.
    std::vector<std::string> speakingIds() const { return speakingSorted_; }

    // Ids distincts ayant parle dans la fenetre anti ping-pong (du + recent au + ancien).
    std::vector<std::string> recentSpeakers(double now) const;

private:
    const Speaker* findSpeaker(const std::string& id) const;
    double rngValue();  // borne le RNG injecte dans [0, 1)
    std::string drawSceneFromPool(const std::vector<SceneWeight>& pool);
    // Resout une scene REELLEMENT jouable pour la cible demandee, avec fallback :
    //   1) la cible (owner -> une de ses scenes ; wide -> plan large)
    //   2) sinon, s'il y a des locuteurs : le plus fort -> une de ses scenes
    //   3) sinon : le plan large s'il existe
    // Garantit qu'on ne reste jamais sur du vide quand quelqu'un parle et qu'une
    // scene est disponible. Renvoie false si rien n'est jouable (aucun pool,
    // aucun plan large). Renseigne outScene/outOwner en cas de succes.
    bool resolvePlayable(const std::string& owner, bool wide, std::string& outScene,
                         std::string& outOwner);
    void commit(double now, const std::string& scene, const std::string& owner, bool hold,
                Decision& out);
    void recordSpeakerChange(double now, const std::string& id);
    void pruneHistory(double now);

    Config cfg_;
    Rng rng_;
    bool autoEnabled_ = true;

    std::map<std::string, SpeakerDetector> detectors_;
    std::vector<std::string> speakingSorted_;  // ids parlants, plus fort -> plus faible

    std::string currentScene_;
    std::string currentOwner_;  // "" = plan large / aucun proprietaire
    bool hold_ = false;         // le plan courant doit-il etre maintenu (verrou) ?
    std::string lastSpeaker_;   // dernier intervenant a avoir parle
    double lastSwitch_ = -1e9;  // instant du dernier changement de scene
    Context lastContext_ = Context::Silence;

    // Memoise la decision aleatoire par "situation" (evite le scintillement :
    // on ne retire pas un nouveau plan a chaque tick d'une meme situation).
    std::string decisionKey_;
    std::string cachedOwner_;
    bool cachedWide_ = false;

    std::deque<std::pair<double, std::string>> history_;  // (instant, id) des prises de parole
};

}  // namespace sd::core
