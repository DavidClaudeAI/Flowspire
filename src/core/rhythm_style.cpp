// Flowspire — styles de realisation (implementation pure). Voir rhythm_style.hpp.
#include "core/rhythm_style.hpp"

#include <string>

#include <nlohmann/json.hpp>

namespace sd::core {

using nlohmann::json;

std::vector<RhythmStyle> builtinRhythmStyles() {
    // {nom, mini, maxi, grace silence, anti ping-pong, whenMultiple{rester,large}, whenSilence{dernier,large}}
    // Valeurs affinees en reel (2026-06-07). La tendance plan large fait partie du temperament :
    // Chill/Cool privilegient FORTEMENT le groupe (10/94) ; Speed reste plus serre sur l'orateur
    // (40/60) pour suivre l'action. Cool == les defauts d'usine (cf. config.hpp, garde le couplage).
    return {
        {"Chill", 5.0, 12.0, 2.0, 0.0, {10, 94}, {10, 94}}, // pose : plans longs, plan large tres genereux
        {"Cool", 3.0, 8.0, 1.5, 0.0, {10, 94}, {10, 94}},   // equilibre = defauts d'usine
        {"Speed", 2.0, 4.0, 1.0, 3.0, {40, 60}, {25, 75}},  // vif : reste plus sur l'orateur ; anti ping-pong arme
    };
}

void applyRhythmStyle(Config& cfg, const RhythmStyle& style) {
    cfg.timing.minShotSeconds = style.minShotSeconds;
    cfg.timing.maxShotSeconds = style.maxShotSeconds;
    cfg.timing.silenceReactionSeconds = style.silenceReactionSeconds;
    cfg.timing.pingPongWindowSeconds = style.pingPongWindowSeconds;
    cfg.whenMultiple = style.whenMultiple;
    cfg.whenSilence = style.whenSilence;
    cfg.styleName = style.name;
}

RhythmStyle styleFromConfig(const Config& cfg, const std::string& name) {
    RhythmStyle s;
    s.name = name;
    s.minShotSeconds = cfg.timing.minShotSeconds;
    s.maxShotSeconds = cfg.timing.maxShotSeconds;
    s.silenceReactionSeconds = cfg.timing.silenceReactionSeconds;
    s.pingPongWindowSeconds = cfg.timing.pingPongWindowSeconds;
    s.whenMultiple = cfg.whenMultiple;
    s.whenSilence = cfg.whenSilence;
    return s;
}

std::string rhythmStyleLibraryToJson(const std::vector<RhythmStyle>& styles) {
    json arr = json::array();
    for (const auto& s : styles) {
        arr.push_back({
            {"name", s.name},
            {"minShotSeconds", s.minShotSeconds},
            {"maxShotSeconds", s.maxShotSeconds},
            {"silenceReactionSeconds", s.silenceReactionSeconds},
            {"pingPongWindowSeconds", s.pingPongWindowSeconds},
            {"whenMultiple",
             {{"currentSpeaker", s.whenMultiple.currentSpeaker}, {"wideShot", s.whenMultiple.wideShot}}},
            {"whenSilence", {{"lastSpeaker", s.whenSilence.lastSpeaker}, {"wideShot", s.whenSilence.wideShot}}},
        });
    }
    json j = {{"schemaVersion", 1}, {"styles", arr}};
    return j.dump(2);
}

std::vector<RhythmStyle> rhythmStyleLibraryFromJson(const std::string& text) {
    const json j = json::parse(text);
    std::vector<RhythmStyle> out;
    if (j.contains("styles") && j.at("styles").is_array()) {
        for (const auto& js : j.at("styles")) {
            RhythmStyle s;
            s.name = js.value("name", std::string{});
            if (s.name.empty()) {
                continue; // entree sans nom : ignoree (un style anonyme n'a pas de sens)
            }
            s.minShotSeconds = js.value("minShotSeconds", s.minShotSeconds);
            s.maxShotSeconds = js.value("maxShotSeconds", s.maxShotSeconds);
            s.silenceReactionSeconds = js.value("silenceReactionSeconds", s.silenceReactionSeconds);
            s.pingPongWindowSeconds = js.value("pingPongWindowSeconds", s.pingPongWindowSeconds);
            if (js.contains("whenMultiple")) {
                const auto& m = js.at("whenMultiple");
                s.whenMultiple.currentSpeaker = m.value("currentSpeaker", s.whenMultiple.currentSpeaker);
                s.whenMultiple.wideShot = m.value("wideShot", s.whenMultiple.wideShot);
            }
            if (js.contains("whenSilence")) {
                const auto& sl = js.at("whenSilence");
                s.whenSilence.lastSpeaker = sl.value("lastSpeaker", s.whenSilence.lastSpeaker);
                s.whenSilence.wideShot = sl.value("wideShot", s.whenSilence.wideShot);
            }
            out.push_back(s);
        }
    }
    return out;
}

bool styleNameExists(const std::vector<RhythmStyle>& styles, const std::string& name) {
    for (const auto& s : styles) {
        if (s.name == name) {
            return true;
        }
    }
    return false;
}

std::string makeUniqueStyleName(const std::vector<RhythmStyle>& styles, const std::string& desired) {
    const auto taken = [&](const std::string& n) {
        if (styleNameExists(styles, n)) {
            return true;
        }
        for (const auto& b : builtinRhythmStyles()) { // un perso ne doit pas masquer un nom livre
            if (b.name == n) {
                return true;
            }
        }
        return false;
    };
    if (!taken(desired)) {
        return desired;
    }
    for (int n = 2;; ++n) {
        const std::string candidate = desired + " (" + std::to_string(n) + ")";
        if (!taken(candidate)) {
            return candidate;
        }
    }
}

} // namespace sd::core
