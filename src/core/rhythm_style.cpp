// Flowspire — styles de realisation (implementation pure). Voir rhythm_style.hpp.
#include "core/rhythm_style.hpp"

#include <string>

#include <nlohmann/json.hpp>

namespace sd::core {

using nlohmann::json;

std::vector<RhythmStyle> builtinRhythmStyles() {
    // {nom, mini, maxi, grace silence, anti ping-pong, repetition max, whenMultiple{rester,large},
    //  whenSilence{dernier,large}}. Mini/maxi/repetition derives du corpus Flowspire-Lab (57 episodes,
    //  2026-06-12) ; grace COMMUNE a 1 s (mesuree idiosyncratique, sans lien au rythme). Le "temps tenu
    //  sur une personne" vient du DECOUPLAGE maxi x repetition (plans courts + retours), pas d'un plan fige.
    //  Anti ping-pong : arme seulement sur les 2 rapides (valeur de depart, a figer au test live).
    //  ⚠️ POIDS PLAN LARGE = baseline heritee ("Cyp Live"), volontairement NON tunee pour les 5 crans :
    //     la data %large suggere l'INVERSE (poses peu de large, rapides beaucoup) -> a departager EN LIVE,
    //     separement de cette PR (on ne flippe pas la politique large en meme temps que le rythme).
    //  Cool == les defauts d'usine (cf. config.hpp) SAUF la repetition-max (opt-in : defaut d'usine 0).
    return {
        {"Very Chill", 5.0, 15.0, 1.0, 0.0, 7, {10, 94}, {10, 94}}, // tres pose : plans longs, retours rares
        {"Chill", 3.5, 13.0, 1.0, 0.0, 5, {10, 94}, {10, 94}},      // pose
        {"Cool", 3.0, 10.0, 1.0, 0.0, 4, {10, 94}, {10, 94}},       // equilibre = defauts d'usine (hors repetition)
        {"Fast", 2.0, 7.0, 1.0, 4.0, 2, {25, 75}, {20, 80}},        // vif : retours frequents ; anti ping-pong arme
        {"Very Fast", 1.5, 4.0, 1.0, 3.0, 1, {40, 60}, {25, 75}},   // nerveux : 1 plan par prise ; reste sur l'orateur
    };
}

void applyRhythmStyle(Config& cfg, const RhythmStyle& style) {
    cfg.timing.minShotSeconds = style.minShotSeconds;
    cfg.timing.maxShotSeconds = style.maxShotSeconds;
    cfg.timing.silenceReactionSeconds = style.silenceReactionSeconds;
    cfg.timing.pingPongWindowSeconds = style.pingPongWindowSeconds;
    cfg.timing.maxPlanRepeats = style.maxPlanRepeats;
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
    s.maxPlanRepeats = cfg.timing.maxPlanRepeats;
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
            {"maxPlanRepeats", s.maxPlanRepeats},
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
            // Cle absente (preset perso d'avant la feature) -> defaut 0 = desactive : le preset
            // garde EXACTEMENT son comportement, aucune respiration imposee dans le dos.
            s.maxPlanRepeats = js.value("maxPlanRepeats", s.maxPlanRepeats);
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
