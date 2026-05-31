// StreamDirector — pont OBS : chargement de la configuration (implementation).
#include "obs/config_loader.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <exception>
#include <string>

#include "core/config.hpp"
#include "obs/fs_atomic.hpp"

namespace sd::obsbridge {

ConfigLoadResult loadConfig() {
    ConfigLoadResult result;

    // 1) Emplacement standard du fichier de config du plugin. OBS renvoie un
    //    chemin dans le dossier de config utilisateur (sous Windows :
    //    %APPDATA%\obs-studio\plugin_config\<plugin>\config.json). A liberer.
    char* path = obs_module_config_path("config.json");
    if (path) {
        result.path = path;
        bfree(path);
    }
    if (result.path.empty()) {
        result.error = "chemin de configuration introuvable";
        return result;
    }

    // 2) Le fichier existe-t-il ? Sinon on reste en mode auto (pas une erreur).
    if (!os_file_exists(result.path.c_str())) {
        result.fileFound = false;
        return result;
    }
    result.fileFound = true;

    // 3) Lecture via l'API OBS (gere les chemins UTF-8 sous Windows). A liberer.
    char* raw = os_quick_read_utf8_file(result.path.c_str());
    if (!raw) {
        result.error = "lecture du fichier impossible";
        return result;
    }
    std::string text(raw);
    bfree(raw);

    // 4) Parse delegue au coeur pur (tolerant aux cles absentes, leve si invalide).
    try {
        result.config = sd::core::fromJson(text);
        result.parsed = true;
    } catch (const std::exception& e) {
        result.error = e.what();
    }
    return result;
}

ConfigSaveResult saveConfig(const sd::core::Config& cfg) {
    ConfigSaveResult result;

    // 1) Meme emplacement que loadConfig (symetrie lecture/ecriture).
    char* path = obs_module_config_path("config.json");
    if (path) {
        result.path = path;
        bfree(path);
    }
    if (result.path.empty()) {
        result.error = "chemin de configuration introuvable";
        return result;
    }

    // 2) Serialisation (coeur pur) + ecriture ATOMIQUE mutualisee (fs_atomic) :
    //    config.json est la copie VIVANTE lue par le moteur. L'ecriture atomique
    //    (tmp + os_safe_replace) cree le dossier parent au besoin et garantit
    //    qu'une coupure ne laisse jamais de fichier tronque -> jamais de config
    //    perdue. La meme primitive sert aux fichiers de profils.
    const std::string text = sd::core::toJson(cfg);
    const FsResult written = writeUtf8Atomic(result.path, text);
    if (!written.ok) {
        result.error = written.error;
        return result;
    }
    result.saved = true;
    return result;
}

}  // namespace sd::obsbridge
