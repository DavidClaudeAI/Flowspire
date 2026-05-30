// StreamDirector — pont OBS : pilotage reel de la scene programme (implementation).
#include "obs/scene_switcher.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

namespace sd::obsbridge {

std::string SceneSwitcher::currentProgramScene() const {
    // obs_frontend_get_current_scene renvoie une reference forte (+1) a liberer.
    obs_source_t* current = obs_frontend_get_current_scene();
    std::string name;
    if (current) {
        const char* n = obs_source_get_name(current);
        if (n) {
            name = n;
        }
        obs_source_release(current);
    }
    return name;
}

bool SceneSwitcher::switchTo(const std::string& sceneName) {
    if (sceneName.empty()) {
        return false;
    }
    // Deja la scene active : rien a faire (evite de "lutter" / de spammer OBS).
    if (currentProgramScene() == sceneName) {
        return false;
    }

    // Cherche la scene par nom dans la liste OFFICIELLE des scenes utilisateur
    // (obs_frontend_get_scenes : exactement les scenes de la liste OBS, pas les
    //  groupes ni les scenes privees). Chaque source du tableau est +1 ref'ee ;
    //  obs_frontend_source_list_free les relache toutes.
    struct obs_frontend_source_list scenes = {};
    obs_frontend_get_scenes(&scenes);

    bool switched = false;
    for (size_t i = 0; i < scenes.sources.num; ++i) {
        obs_source_t* scene = scenes.sources.array[i];
        const char* n = scene ? obs_source_get_name(scene) : nullptr;
        if (n && sceneName == n) {
            obs_frontend_set_current_scene(scene);
            switched = true;
            break;
        }
    }

    obs_frontend_source_list_free(&scenes);
    return switched;
}

}  // namespace sd::obsbridge
