// StreamDirector — pont OBS : inventaire des ressources (implementation).
#include "obs/obs_inventory.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <unordered_set>

namespace sd::obsbridge {

std::vector<std::string> audioSourceNames() {
    // Memes criteres que AudioLevelMonitor::start() : on garde la coherence entre
    // ce que l'assistant propose et ce que le moteur sait reellement ecouter.
    struct EnumCtx {
        std::vector<std::string> names;
        std::unordered_set<std::string> seen;
    };
    EnumCtx ctx;
    obs_enum_sources(
        [](void* param, obs_source_t* source) -> bool {
            const uint32_t flags = obs_source_get_output_flags(source);
            if (flags & OBS_SOURCE_AUDIO) {
                const char* name = obs_source_get_name(source);
                if (name && *name) {
                    auto* c = static_cast<EnumCtx*>(param);
                    if (c->seen.insert(name).second) {
                        c->names.emplace_back(name);
                    }
                }
            }
            return true;  // continuer l'enumeration
        },
        &ctx);
    return ctx.names;
}

std::vector<std::string> sceneNames() {
    // obs_frontend_get_scenes : chaque source du tableau est +1 ref'ee ;
    // obs_frontend_source_list_free les relache toutes.
    struct obs_frontend_source_list scenes = {};
    obs_frontend_get_scenes(&scenes);

    std::vector<std::string> names;
    names.reserve(scenes.sources.num);
    for (size_t i = 0; i < scenes.sources.num; ++i) {
        obs_source_t* scene = scenes.sources.array[i];
        const char* n = scene ? obs_source_get_name(scene) : nullptr;
        if (n && *n) {
            names.emplace_back(n);
        }
    }
    obs_frontend_source_list_free(&scenes);
    return names;
}

}  // namespace sd::obsbridge
