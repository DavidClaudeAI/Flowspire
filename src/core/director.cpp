#include "core/director.hpp"

#include <algorithm>
#include <memory>
#include <random>
#include <utility>

#include "core/weighted_pick.hpp"

namespace sd::core {

namespace {
// RNG par defaut (production) : Mersenne Twister, graine non deterministe.
Director::Rng makeDefaultRng() {
    auto engine = std::make_shared<std::mt19937_64>(std::random_device{}());
    auto dist = std::make_shared<std::uniform_real_distribution<double>>(0.0, 1.0);
    return [engine, dist]() {
        return (*dist)(*engine);
    };
}
} // namespace

Director::Director(Config cfg, Rng rng) : cfg_(std::move(cfg)), rng_(rng ? std::move(rng) : makeDefaultRng()) {
    setConfig(cfg_);
}

void Director::setConfig(const Config& cfg) {
    cfg_ = cfg;
    detectors_.clear();
    ownerLeftAt_.clear(); // memoire anti ping-pong : repart de zero a chaque (re)config
    silenceSince_ = -1.0; // grace de silence : on repart "pas en silence" a chaque (re)config
    // On repart de zero puis on SEME les overrides depuis la config : le seuil
    // par intervenant (Speaker.thresholdDb) est desormais persiste dans le profil,
    // donc la config/JSON reste la source de verite au chargement. Un intervenant
    // sans seuil propre retombe sur le seuil global (pas d'entree d'override).
    thresholdOverride_.clear();
    for (const auto& sp : cfg_.speakers) {
        detectors_.emplace(sp.id, SpeakerDetector(cfg_.audio.voiceThresholdDb, cfg_.audio.attackFrames,
                                                  cfg_.audio.releaseFrames));
        if (sp.thresholdDb.has_value()) {
            thresholdOverride_[sp.id] = *sp.thresholdDb;
        }
    }
}

void Director::setSpeakerThreshold(const std::string& speakerId, double db) {
    if (detectors_.find(speakerId) == detectors_.end()) {
        return; // intervenant inconnu : on n'enregistre pas d'override orphelin.
    }
    thresholdOverride_[speakerId] = db;
}

double Director::speakerThresholdDb(const std::string& speakerId) const {
    const auto it = thresholdOverride_.find(speakerId);
    return (it != thresholdOverride_.end()) ? it->second : cfg_.audio.voiceThresholdDb;
}

const Speaker* Director::findSpeaker(const std::string& id) const {
    for (const auto& sp : cfg_.speakers) {
        if (sp.id == id) {
            return &sp;
        }
    }
    return nullptr;
}

double Director::rngValue() {
    const double r = rng_();
    if (r < 0.0) {
        return 0.0;
    }
    if (r >= 1.0) {
        return 0.999999;
    }
    return r;
}

std::string Director::drawSceneFromPool(const std::vector<SceneWeight>& pool) {
    std::vector<std::pair<std::string, int>> options;
    options.reserve(pool.size());
    for (const auto& sw : pool) {
        options.emplace_back(sw.scene, sw.weight);
    }
    const std::string* picked = weightedPick(options, rngValue());
    return picked ? *picked : std::string{};
}

bool Director::isPingPongBounce(double now, const std::string& owner) const {
    if (owner.empty() || cfg_.timing.pingPongWindowSeconds <= 0.0) {
        return false; // fenetre a 0 (ou pas de cible) -> anti ping-pong desactive
    }
    const auto it = ownerLeftAt_.find(owner);
    if (it == ownerLeftAt_.end()) {
        return false; // on n'a jamais quitte ce plan -> pas une navette
    }
    return (now - it->second) < cfg_.timing.pingPongWindowSeconds;
}

Decision Director::update(double now, const std::map<std::string, double>& levelsDb) {
    // 1) Alimenter les detecteurs et collecter les parlants (tries par dB desc).
    std::vector<std::pair<std::string, double>> speaking;
    for (auto& entry : detectors_) {
        const std::string& id = entry.first;
        SpeakerDetector& det = entry.second;
        const auto it = levelsDb.find(id);
        const double db = (it != levelsDb.end()) ? it->second : kDbFloor;
        // Seuil par intervenant : override en direct s'il existe, sinon global.
        const auto ov = thresholdOverride_.find(id);
        det.setThresholdDb(ov != thresholdOverride_.end() ? ov->second : cfg_.audio.voiceThresholdDb);
        if (det.update(db)) {
            speaking.emplace_back(id, det.lastDb());
        }
    }
    std::sort(speaking.begin(), speaking.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    speakingSorted_.clear();
    for (const auto& s : speaking) {
        speakingSorted_.push_back(s.first);
    }

    const std::string activeId = speaking.empty() ? std::string{} : speaking.front().first;
    if (!activeId.empty()) {
        lastSpeaker_ = activeId;
    }

    const Context ctx = speaking.empty() ? Context::Silence
                                         : (speaking.size() == 1 ? Context::Single : Context::Multiple);
    lastContext_ = ctx;

    Decision out;
    out.scene = currentScene_;
    out.owner = currentOwner_;
    out.context = ctx;

    if (!autoEnabled_) {
        silenceSince_ = -1.0; // auto coupe : la grace de silence ne court pas (repart propre a la reactivation).
        return out;
    }

    // 0) GRACE DE SILENCE ("delai avant reaction au silence"). Quand plus personne ne
    //    parle, on GARDE le plan courant tant que le silence n'a pas dure
    //    silenceReactionSeconds, puis on laisse la decision de silence s'appliquer plus
    //    bas. Un blanc court (respiration, temps entre deux phrases) ne bascule donc plus
    //    au plan large ; et si l'orateur reprend dans l'intervalle, on ne l'a jamais quitte
    //    (reprise instantanee, sans payer le temps-mini). N'agit QUE sur le silence : un
    //    nouveau locuteur (ctx != Silence) reinitialise le minuteur et bascule normalement.
    //    DISTINCT de la detection par personne (releaseFrames) qui, elle, definit a partir
    //    de quand quelqu'un est "silencieux".
    if (ctx == Context::Silence) {
        if (silenceSince_ < 0.0) {
            silenceSince_ = now; // le silence vient de commencer.
        }
        // On ne "tient" que s'il y a un plan a tenir (pas au demarrage, currentScene_ vide).
        if (!currentScene_.empty() && (now - silenceSince_) < cfg_.timing.silenceReactionSeconds) {
            return out; // hold du plan courant ; re-evalue au prochain tick.
        }
    } else {
        silenceSince_ = -1.0; // quelqu'un parle -> la grace de silence est annulee.
    }

    // 1bis) VARIETE AU TEMPS-MAX : si on tient le plan courant depuis plus de
    //   maxShotSeconds, on invalide la situation memoisee pour FORCER un nouveau
    //   tirage pondere ci-dessous. Sans cela, tant que le contexte ne change pas
    //   (silence prolonge, ou meme locuteur le plus fort), la decision restait
    //   figee et AUCUNE variete n'apparaissait : en silence on ne repassait jamais
    //   au plan large, meme apres 30 s. Le re-tirage rejoue alors whenSilence /
    //   whenMultiple selon leurs poids. (En contexte Single c'est sans effet : la
    //   cible y est deterministe ; la variete du plan vient deja du re-tirage du
    //   pool fait plus bas au temps-max.)
    if (!currentScene_.empty() && (now - lastSwitch_) >= cfg_.timing.maxShotSeconds) {
        decisionKey_.clear();
    }

    // 2) Determiner la cible desiree (owner ou plan large), avec memoisation par
    //    "situation" pour ne pas re-tirer un plan a chaque tick (anti-scintillement).
    std::string desiredOwner;
    bool desiredWide = false;
    std::string key;

    if (ctx == Context::Single) {
        // Contexte A : cible = celui qui parle (deterministe, pas de tirage).
        desiredOwner = activeId;
        desiredWide = false;
        key = "S:" + activeId;
    } else if (ctx == Context::Multiple) {
        // Contexte B : tirage { rester sur le plan courant / plan large }. Le VOLUME n'est
        // pas un critere de bascule -> pas d'option "le plus fort". Mettre en avant une
        // personne se fait naturellement quand elle "gagne" la parole (-> contexte Single).
        // "Rester" n'a de sens que si le locuteur courant PARLE ENCORE : sinon (il s'est tu
        // mais 2+ parlent toujours) on le filmerait muet jusqu'au temps-max. La cle inclut donc
        // currentOwner_ tant qu'il parle ; des qu'il se tait, la cle bascule -> re-tirage. Ce
        // re-tirage ne saute JAMAIS "au plus fort" (option supprimee) : au pire le plan large.
        const bool ownerSpeaking =
            !currentOwner_.empty() &&
            std::find(speakingSorted_.begin(), speakingSorted_.end(), currentOwner_) != speakingSorted_.end();
        key = ownerSpeaking ? ("M:" + currentOwner_) : "M";
        if (key != decisionKey_) {
            std::vector<std::pair<std::string, int>> opts;
            if (!currentScene_.empty()) {
                opts.emplace_back("current", cfg_.whenMultiple.currentSpeaker);
            }
            if (!cfg_.wideShotScene.empty()) {
                opts.emplace_back("wide", cfg_.whenMultiple.wideShot);
            }
            const std::string* tag = weightedPick(opts, rngValue());
            const std::string choice = tag ? *tag : std::string("current");
            if (choice == "wide" || !ownerSpeaking) {
                // Soit le tirage a donne le plan large, soit on ne peut pas "rester" sur un
                // locuteur qui parle (locuteur courant muet, ou plan courant large/force sans
                // owner) -> plan large s'il existe. Jamais de saut "au plus fort" ; sans plan
                // large, resolvePlayable evitera l'ecran noir.
                cachedOwner_.clear();
                cachedWide_ = !cfg_.wideShotScene.empty();
            } else {
                cachedOwner_ = currentOwner_; // rester sur le locuteur courant (il parle encore)
                cachedWide_ = false;
            }
        }
        desiredOwner = cachedOwner_;
        desiredWide = cachedWide_;
    } else {
        // Contexte C : tirage { dernier locuteur / plan large }.
        key = "Z";
        if (key != decisionKey_) {
            std::vector<std::pair<std::string, int>> opts;
            if (!lastSpeaker_.empty()) {
                opts.emplace_back("last", cfg_.whenSilence.lastSpeaker);
            }
            if (!cfg_.wideShotScene.empty()) {
                opts.emplace_back("wide", cfg_.whenSilence.wideShot);
            }
            const std::string* tag = weightedPick(opts, rngValue());
            const std::string choice = tag ? *tag : std::string{};
            if (choice == "wide") {
                cachedOwner_.clear();
                cachedWide_ = true;
            } else if (choice == "last") {
                cachedOwner_ = lastSpeaker_;
                cachedWide_ = false;
            } else {
                cachedOwner_.clear();
                cachedWide_ = false;
            }
        }
        desiredOwner = cachedOwner_;
        desiredWide = cachedWide_;
    }
    decisionKey_ = key;

    // 2bis) ANTI PING-PONG = "retour au plan large sur echange rapide". Re-evalue a CHAQUE
    //   tick (deterministe, hors memoisation -> la fenetre est une VRAIE borne de temps, pas
    //   couplee au temps-max). Quand on s'apprete a (re)couper sur un locuteur qu'on vient de
    //   QUITTER il y a moins de pingPongWindowSeconds (navette : deux personnes qui se relaient
    //   comme seuls locuteurs), on se RECULE sur le plan large le temps que ca respire, puis on
    //   repart (la fenetre ecoulee, la cible n'est plus "recemment quittee"). N'agit QUE si un
    //   plan large existe : sinon il n'y a nulle part ou se reculer -> on laisse la navette, le
    //   temps-mini gere (l'amortir sans plan large reviendrait juste a allonger le temps-mini).
    //   En multi normal la cible est currentOwner_ (on "reste", il parle encore) ou le plan
    //   large -> desiredOwner != currentOwner_ n'y est pas vraie : pas de navette en multi.
    if (!desiredWide && !desiredOwner.empty() && desiredOwner != currentOwner_ &&
        !cfg_.wideShotScene.empty() && isPingPongBounce(now, desiredOwner)) {
        desiredOwner.clear();
        desiredWide = true;
    }

    // 3) Comparer la cible a l'etat courant et decider de basculer (ou non).
    const bool init = currentScene_.empty();

    const bool sameTarget = desiredWide
                                ? (currentOwner_.empty() && currentScene_ == cfg_.wideShotScene)
                                : (!desiredOwner.empty() && !currentOwner_.empty() && currentOwner_ == desiredOwner);

    if (!init && sameTarget) {
        // Rafraichissement (variete) une fois le temps-max ecoule : on re-tire
        // une scene dans le meme pool (meme cible), sans casser le "hold".
        if ((now - lastSwitch_) >= cfg_.timing.maxShotSeconds) {
            std::string scene, owner;
            if (resolvePlayable(desiredOwner, desiredWide, scene, owner)) {
                commit(now, scene, owner, hold_, out);
            }
        }
        return out;
    }

    // Cible differente (ou initialisation) : on veut basculer. Le verrou temps-mini
    // s'applique a tout plan en "hold" (cf. willHold ci-dessous) : il tient alors au
    // moins minShotSeconds avant qu'on puisse en sortir.
    const bool locked = hold_ && !init && (now - lastSwitch_) < cfg_.timing.minShotSeconds;
    if (locked) {
        return out; // re-evalue au prochain tick : la bascule partira des que le verrou tombe.
    }

    // "hold" = le plan tient le temps-mini avant de pouvoir etre remplace.
    //  - plan de LOCUTEUR : toujours un hold (anti-nervosite).
    //  - PLAN LARGE : hold des qu'il SUCCEDE a un plan deja affiche (!init) -> evite le
    //    "flash" (on passe au plan large, puis quelqu'un parle aussitot). PAS un hold s'il
    //    est l'ecran de DEPART (init == currentScene_ vide, avant toute bascule) -> la 1ere
    //    parole y est alors suivie sans delai. Critere `!init` (et non "on quitte un
    //    locuteur") : couvre aussi le plan large succedant a une scene FORCEE sans
    //    proprietaire (forceScene owner="") -> pas de flash reintroduit.
    std::string scene, owner;
    if (resolvePlayable(desiredOwner, desiredWide, scene, owner)) {
        const bool willHold = !owner.empty() || !init;
        commit(now, scene, owner, willHold, out);
    } else {
        // Aucune scene jouable (pas de pool, pas de plan large) : ne pas figer la
        // decision memoisee -> re-tenter au prochain tick (cause racine : on ne
        // reste jamais bloque sur une cible morte).
        decisionKey_.clear();
    }
    return out;
}

bool Director::resolvePlayable(const std::string& owner, bool wide, std::string& outScene, std::string& outOwner) {
    // 1) Cible demandee.
    if (wide) {
        if (!cfg_.wideShotScene.empty()) {
            outScene = cfg_.wideShotScene;
            outOwner.clear();
            return true;
        }
    } else if (!owner.empty()) {
        if (const Speaker* sp = findSpeaker(owner)) {
            const std::string scene = drawSceneFromPool(sp->scenes);
            if (!scene.empty()) {
                outScene = scene;
                outOwner = owner;
                return true;
            }
        }
    }

    // 2) Fallback : si quelqu'un parle, montrer le plus fort plutot que du vide.
    if (!speakingSorted_.empty()) {
        const std::string& loud = speakingSorted_.front();
        if (loud != owner) { // deja tente ci-dessus
            if (const Speaker* sp = findSpeaker(loud)) {
                const std::string scene = drawSceneFromPool(sp->scenes);
                if (!scene.empty()) {
                    outScene = scene;
                    outOwner = loud;
                    return true;
                }
            }
        }
    }

    // 3) Dernier recours : le plan large s'il existe.
    if (!cfg_.wideShotScene.empty()) {
        outScene = cfg_.wideShotScene;
        outOwner.clear();
        return true;
    }

    return false;
}

void Director::commit(double now, const std::string& scene, const std::string& owner, bool hold, Decision& out,
                      bool recordLeave) {
    // Anti ping-pong : on note l'instant ou l'on QUITTE le proprietaire courant (pour
    // detecter ensuite un retour trop rapide vers lui = navette). On n'arme QUE si l'on part
    // vers UN AUTRE LOCUTEUR (owner non vide) : c'est le signal d'un echange A<->B. Quitter
    // un locuteur pour le PLAN LARGE (owner vide : sa propre pause -> silence -> plan large)
    // n'est PAS une navette -> ne pas armer, sinon sa reprise resterait bloquee sur le plan
    // large (revue : faux declenchement sur un orateur seul qui fait des pauses). PAS non plus
    // sur un forcage (recordLeave=false) : un choix manuel ne doit pas bloquer le retour auto.
    if (recordLeave && !currentOwner_.empty() && !owner.empty() && currentOwner_ != owner) {
        ownerLeftAt_[currentOwner_] = now;
    }
    out.switched = (scene != currentScene_);
    currentScene_ = scene;
    currentOwner_ = owner;
    hold_ = hold;
    lastSwitch_ = now; // reinitialise verrou + temps-max (meme si meme scene)
    out.scene = scene;
    out.owner = owner;
}

Decision Director::forceScene(double now, const std::string& scene, const std::string& owner) {
    Decision out;
    out.context = lastContext_;
    commit(now, scene, owner, /*hold=*/true, out, /*recordLeave=*/false);
    decisionKey_.clear(); // un forçage reinitialise la situation memorisee
    return out;
}

Decision Director::forceSpeaker(double now, const std::string& speakerId) {
    Decision out;
    out.context = lastContext_;
    out.scene = currentScene_;
    out.owner = currentOwner_;

    const Speaker* sp = findSpeaker(speakerId);
    if (!sp) {
        return out; // intervenant inconnu : on ne touche a rien.
    }
    const std::string scene = drawSceneFromPool(sp->scenes);
    if (scene.empty()) {
        return out; // pas de scene jouable pour cet intervenant.
    }
    commit(now, scene, speakerId, /*hold=*/true, out, /*recordLeave=*/false);
    decisionKey_.clear(); // un forçage reinitialise la situation memorisee
    return out;
}

bool Director::sceneInProgram(const std::string& scene, std::string& owner) const {
    if (scene.empty()) {
        return false;
    }
    // Le plan large est un concept de premier ordre -> teste EN PREMIER : une scene a
    // la fois plan large ET presente dans un pool reste consideree comme le plan large
    // (owner vide), pas comme la scene d'un intervenant.
    if (!cfg_.wideShotScene.empty() && scene == cfg_.wideShotScene) {
        owner.clear();
        return true;
    }
    for (const auto& sp : cfg_.speakers) {
        if (sp.id.empty()) {
            continue; // invariant : owner vide == plan large. Un intervenant a id vide
                      // (config corrompue) ne doit jamais etre confondu avec le plan large.
        }
        for (const auto& sw : sp.scenes) {
            if (sw.scene == scene) {
                owner = sp.id;
                return true;
            }
        }
    }
    return false;
}

} // namespace sd::core
