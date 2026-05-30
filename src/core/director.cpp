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
    return [engine, dist]() { return (*dist)(*engine); };
}
}  // namespace

Director::Director(Config cfg, Rng rng)
    : cfg_(std::move(cfg)), rng_(rng ? std::move(rng) : makeDefaultRng()) {
    setConfig(cfg_);
}

void Director::setConfig(const Config& cfg) {
    cfg_ = cfg;
    detectors_.clear();
    for (const auto& sp : cfg_.speakers) {
        detectors_.emplace(
            sp.id, SpeakerDetector(cfg_.audio.voiceThresholdDb, cfg_.audio.attackFrames,
                                   cfg_.audio.releaseFrames));
    }
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

void Director::recordSpeakerChange(double now, const std::string& id) {
    if (id.empty()) {
        return;
    }
    if (history_.empty() || history_.back().second != id) {
        history_.emplace_back(now, id);
    }
    pruneHistory(now);
}

void Director::pruneHistory(double now) {
    const double limit = now - cfg_.timing.pingPongWindowSeconds;
    while (!history_.empty() && history_.front().first < limit) {
        history_.pop_front();
    }
}

std::vector<std::string> Director::recentSpeakers(double now) const {
    const double limit = now - cfg_.timing.pingPongWindowSeconds;
    std::vector<std::string> result;
    for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
        if (it->first < limit) {
            break;
        }
        if (std::find(result.begin(), result.end(), it->second) == result.end()) {
            result.push_back(it->second);
        }
    }
    return result;
}

Decision Director::update(double now, const std::map<std::string, double>& levelsDb) {
    // 1) Alimenter les detecteurs et collecter les parlants (tries par dB desc).
    std::vector<std::pair<std::string, double>> speaking;
    for (auto& entry : detectors_) {
        const std::string& id = entry.first;
        SpeakerDetector& det = entry.second;
        const auto it = levelsDb.find(id);
        const double db = (it != levelsDb.end()) ? it->second : kDbFloor;
        det.setThresholdDb(cfg_.audio.voiceThresholdDb);
        if (det.update(db)) {
            speaking.emplace_back(id, det.lastDb());
        }
    }
    std::sort(speaking.begin(), speaking.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    speakingSorted_.clear();
    for (const auto& s : speaking) {
        speakingSorted_.push_back(s.first);
    }

    const std::string activeId = speaking.empty() ? std::string{} : speaking.front().first;
    if (!activeId.empty()) {
        lastSpeaker_ = activeId;
        recordSpeakerChange(now, activeId);
    }

    const Context ctx = speaking.empty()
                            ? Context::Silence
                            : (speaking.size() == 1 ? Context::Single : Context::Multiple);
    lastContext_ = ctx;

    Decision out;
    out.scene = currentScene_;
    out.owner = currentOwner_;
    out.context = ctx;

    if (!autoEnabled_) {
        return out;
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
        // Contexte B : tirage { le plus fort / rester / plan large }.
        const std::string& loudest = speakingSorted_.front();
        key = "M:" + loudest;
        if (key != decisionKey_) {
            std::vector<std::pair<std::string, int>> opts;
            opts.emplace_back("loudest", cfg_.whenMultiple.loudestSpeaker);
            if (!currentScene_.empty()) {
                opts.emplace_back("current", cfg_.whenMultiple.currentSpeaker);
            }
            if (!cfg_.wideShotScene.empty()) {
                opts.emplace_back("wide", cfg_.whenMultiple.wideShot);
            }
            const std::string* tag = weightedPick(opts, rngValue());
            const std::string choice = tag ? *tag : std::string("loudest");
            if (choice == "wide") {
                cachedOwner_.clear();
                cachedWide_ = true;
            } else if (choice == "current") {
                cachedOwner_ = currentOwner_;
                cachedWide_ = currentOwner_.empty() && !currentScene_.empty();
            } else {
                cachedOwner_ = loudest;
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

    // 3) Comparer la cible a l'etat courant et decider de basculer (ou non).
    const bool init = currentScene_.empty();

    const bool sameTarget =
        desiredWide ? (currentOwner_.empty() && currentScene_ == cfg_.wideShotScene)
                    : (!desiredOwner.empty() && !currentOwner_.empty() &&
                       currentOwner_ == desiredOwner);

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

    // Cible differente (ou initialisation) : on veut basculer. Le verrou ne
    // s'applique qu'a un plan "hold" (plan de locuteur ou plan force), pas a un
    // plan large issu d'un simple silence.
    const bool locked = hold_ && !init && (now - lastSwitch_) < cfg_.timing.minShotSeconds;
    if (locked) {
        return out;  // re-evalue au prochain tick : la bascule partira des que le verrou tombe.
    }

    // Un plan issu d'un contexte avec locuteurs (Single/Multiple) est un "hold".
    // Un plan large de silence ne l'est pas (une prise de parole peut le remplacer).
    std::string scene, owner;
    if (resolvePlayable(desiredOwner, desiredWide, scene, owner)) {
        const bool willHold = (ctx != Context::Silence);
        commit(now, scene, owner, willHold, out);
    } else {
        // Aucune scene jouable (pas de pool, pas de plan large) : ne pas figer la
        // decision memoisee -> re-tenter au prochain tick (cause racine : on ne
        // reste jamais bloque sur une cible morte).
        decisionKey_.clear();
    }
    return out;
}

bool Director::resolvePlayable(const std::string& owner, bool wide, std::string& outScene,
                              std::string& outOwner) {
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
        if (loud != owner) {  // deja tente ci-dessus
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

void Director::commit(double now, const std::string& scene, const std::string& owner, bool hold,
                      Decision& out) {
    out.switched = (scene != currentScene_);
    currentScene_ = scene;
    currentOwner_ = owner;
    hold_ = hold;
    lastSwitch_ = now;  // reinitialise verrou + temps-max (meme si meme scene)
    out.scene = scene;
    out.owner = owner;
}

Decision Director::forceScene(double now, const std::string& scene, const std::string& owner) {
    Decision out;
    out.context = lastContext_;
    commit(now, scene, owner, /*hold=*/true, out);
    decisionKey_.clear();  // un forçage reinitialise la situation memorisee
    return out;
}

Decision Director::forceSpeaker(double now, const std::string& speakerId) {
    Decision out;
    out.context = lastContext_;
    out.scene = currentScene_;
    out.owner = currentOwner_;

    const Speaker* sp = findSpeaker(speakerId);
    if (!sp) {
        return out;  // intervenant inconnu : on ne touche a rien.
    }
    const std::string scene = drawSceneFromPool(sp->scenes);
    if (scene.empty()) {
        return out;  // pas de scene jouable pour cet intervenant.
    }
    commit(now, scene, speakerId, /*hold=*/true, out);
    decisionKey_.clear();  // un forçage reinitialise la situation memorisee
    return out;
}

}  // namespace sd::core
