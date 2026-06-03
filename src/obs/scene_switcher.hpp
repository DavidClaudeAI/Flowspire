// Flowspire — pont OBS : pilotage reel de la scene programme.
// Couche "obs" : seul endroit qui demande a OBS de changer la scene active.
// A n'appeler QUE depuis le thread UI (la frontend-api d'OBS n'est pas concue
// pour etre appelee depuis un thread arbitraire).
#pragma once

#include <string>

namespace sd::obsbridge {

class SceneSwitcher {
public:
    // Bascule la scene programme d'OBS vers `sceneName`.
    // - no-op si `sceneName` est vide, inexistant, ou deja la scene active.
    // Renvoie true uniquement si une bascule a reellement eu lieu.
    bool switchTo(const std::string& sceneName);

    // Nom de la scene programme actuellement active ("" si indisponible).
    std::string currentProgramScene() const;
};

} // namespace sd::obsbridge
