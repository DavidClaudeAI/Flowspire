// Flowspire — styles de realisation (implementation pure). Voir rhythm_style.hpp.
#include "core/rhythm_style.hpp"

namespace sd::core {

std::vector<RhythmStyle> builtinRhythmStyles() {
    // {nom, mini, maxi, grace silence, anti ping-pong, whenMultiple{rester,large}, whenSilence{dernier,large}}
    // Valeurs de depart (a affiner en reel). La tendance plan large fait partie du temperament :
    // Speed reste serre quand 2+ parlent (rester >> large) ; Chill/Cool privilegient le groupe.
    return {
        {"Chill", 5.0, 10.0, 2.5, 0.0, {10, 90}, {10, 90}}, // pose : plans longs, plan large genereux
        {"Cool", 3.0, 6.0, 1.5, 0.0, {5, 100}, {5, 100}},   // equilibre = reglage reel "Cyp Live"
        {"Speed", 2.0, 4.0, 1.0, 5.0, {65, 10}, {30, 70}},  // vif : serre en multi, large surtout au silence ; anti ping-pong arme
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

} // namespace sd::core
