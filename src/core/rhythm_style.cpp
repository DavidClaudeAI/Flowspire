// Flowspire — styles de realisation (implementation pure). Voir rhythm_style.hpp.
#include "core/rhythm_style.hpp"

namespace sd::core {

std::vector<RhythmStyle> builtinRhythmStyles() {
    // {nom, mini, maxi, grace silence, anti ping-pong} — valeurs de depart (a affiner en reel).
    return {
        {"Chill", 5.0, 10.0, 2.5, 0.0}, // pose : plans longs, on respire
        {"Cool", 3.0, 6.0, 1.5, 0.0},   // equilibre = defauts livres ("Cyp Live")
        {"Speed", 2.0, 4.0, 1.0, 5.0},  // vif : coupe nette ; SEUL a armer l'anti ping-pong (evite l'A-B-A-B)
    };
}

void applyRhythmStyle(Config& cfg, const RhythmStyle& style) {
    cfg.timing.minShotSeconds = style.minShotSeconds;
    cfg.timing.maxShotSeconds = style.maxShotSeconds;
    cfg.timing.silenceReactionSeconds = style.silenceReactionSeconds;
    cfg.timing.pingPongWindowSeconds = style.pingPongWindowSeconds;
    cfg.styleName = style.name;
}

} // namespace sd::core
