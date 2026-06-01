#include "core/config.hpp"

#include <cmath>

#include <nlohmann/json.hpp>

namespace sd::core {

using nlohmann::json;

std::string toJson(const Config& cfg) {
    json speakers = json::array();
    for (const auto& sp : cfg.speakers) {
        json scenes = json::array();
        for (const auto& sw : sp.scenes) {
            scenes.push_back({{"scene", sw.scene}, {"weight", sw.weight}});
        }
        json jsp = {
            {"id", sp.id},
            {"name", sp.name},
            {"audioSource", sp.audioSource},
            {"scenes", scenes},
        };
        // Seuil propre a l'intervenant : ecrit UNIQUEMENT s'il a ete regle. Un profil
        // sans reglage explicite n'embarque pas la cle (retrocompatible, et "absent"
        // signifie bien "utilise le seuil global").
        if (sp.thresholdDb.has_value()) {
            jsp["thresholdDb"] = *sp.thresholdDb;
        }
        speakers.push_back(std::move(jsp));
    }

    json j = {
        {"version", cfg.version},
        {"speakers", speakers},
        {"wideShotScene", cfg.wideShotScene},
        {"audio",
         {{"voiceThresholdDb", cfg.audio.voiceThresholdDb},
          {"volumeFloorDb", cfg.audio.volumeFloorDb},
          {"vadThreshold", cfg.audio.vadThreshold},
          {"attackFrames", cfg.audio.attackFrames},
          {"releaseFrames", cfg.audio.releaseFrames}}},
        {"timing",
         {{"minShotSeconds", cfg.timing.minShotSeconds},
          {"maxShotSeconds", cfg.timing.maxShotSeconds},
          {"pingPongWindowSeconds", cfg.timing.pingPongWindowSeconds}}},
        {"whenMultiple",
         {{"loudestSpeaker", cfg.whenMultiple.loudestSpeaker},
          {"currentSpeaker", cfg.whenMultiple.currentSpeaker},
          {"wideShot", cfg.whenMultiple.wideShot}}},
        {"whenSilence",
         {{"lastSpeaker", cfg.whenSilence.lastSpeaker},
          {"wideShot", cfg.whenSilence.wideShot}}},
    };
    return j.dump(2);
}

Config fromJson(const std::string& text) {
    const json j = json::parse(text);
    Config cfg;

    cfg.version = j.value("version", cfg.version);
    cfg.wideShotScene = j.value("wideShotScene", cfg.wideShotScene);

    if (j.contains("speakers") && j.at("speakers").is_array()) {
        for (const auto& js : j.at("speakers")) {
            Speaker sp;
            sp.id = js.value("id", std::string{});
            sp.name = js.value("name", std::string{});
            sp.audioSource = js.value("audioSource", std::string{});
            if (js.contains("scenes") && js.at("scenes").is_array()) {
                for (const auto& jsc : js.at("scenes")) {
                    SceneWeight sw;
                    sw.scene = jsc.value("scene", std::string{});
                    sw.weight = jsc.value("weight", 1);
                    sp.scenes.push_back(sw);
                }
            }
            // Seuil propre a l'intervenant : present uniquement s'il a ete regle. On
            // ecarte une valeur non finie (inf/NaN issue d'un JSON edite a la main) ->
            // un seuil non fini fausserait toutes les comparaisons de niveau.
            if (js.contains("thresholdDb") && js.at("thresholdDb").is_number()) {
                const double t = js.at("thresholdDb").get<double>();
                if (std::isfinite(t)) {
                    sp.thresholdDb = t;
                }
            }
            cfg.speakers.push_back(sp);
        }
    }

    if (j.contains("audio")) {
        const auto& a = j.at("audio");
        cfg.audio.voiceThresholdDb = a.value("voiceThresholdDb", cfg.audio.voiceThresholdDb);
        cfg.audio.volumeFloorDb = a.value("volumeFloorDb", cfg.audio.volumeFloorDb);
        cfg.audio.vadThreshold = a.value("vadThreshold", cfg.audio.vadThreshold);
        cfg.audio.attackFrames = a.value("attackFrames", cfg.audio.attackFrames);
        cfg.audio.releaseFrames = a.value("releaseFrames", cfg.audio.releaseFrames);
    }

    if (j.contains("timing")) {
        const auto& t = j.at("timing");
        cfg.timing.minShotSeconds = t.value("minShotSeconds", cfg.timing.minShotSeconds);
        cfg.timing.maxShotSeconds = t.value("maxShotSeconds", cfg.timing.maxShotSeconds);
        cfg.timing.pingPongWindowSeconds =
            t.value("pingPongWindowSeconds", cfg.timing.pingPongWindowSeconds);
    }

    if (j.contains("whenMultiple")) {
        const auto& m = j.at("whenMultiple");
        cfg.whenMultiple.loudestSpeaker = m.value("loudestSpeaker", cfg.whenMultiple.loudestSpeaker);
        cfg.whenMultiple.currentSpeaker = m.value("currentSpeaker", cfg.whenMultiple.currentSpeaker);
        cfg.whenMultiple.wideShot = m.value("wideShot", cfg.whenMultiple.wideShot);
    }

    if (j.contains("whenSilence")) {
        const auto& s = j.at("whenSilence");
        cfg.whenSilence.lastSpeaker = s.value("lastSpeaker", cfg.whenSilence.lastSpeaker);
        cfg.whenSilence.wideShot = s.value("wideShot", cfg.whenSilence.wideShot);
    }

    // Normalisation des invariants AU CHARGEMENT (source unique de verite), quelle
    // que soit l'origine du JSON (UI, edition manuelle, version anterieure) :
    //   - attack/release : au moins 1 frame (sinon l'hysteresis ne se declenche jamais)
    //   - temps maxi d'un plan >= temps mini (sinon le rafraichissement au temps-max
    //     court-circuiterait en permanence le verrou anti-nervosite -> re-cuts en boucle)
    // L'UI s'appuie sur cet invariant ; on ne le reduplique donc pas dans chaque ecran.
    if (cfg.audio.attackFrames < 1) {
        cfg.audio.attackFrames = 1;
    }
    if (cfg.audio.releaseFrames < 1) {
        cfg.audio.releaseFrames = 1;
    }
    if (cfg.timing.maxShotSeconds < cfg.timing.minShotSeconds) {
        cfg.timing.maxShotSeconds = cfg.timing.minShotSeconds;
    }

    return cfg;
}

}  // namespace sd::core
