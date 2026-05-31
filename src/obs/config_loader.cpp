// StreamDirector — pont OBS : chargement de la configuration (implementation).
#include "obs/config_loader.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <exception>
#include <string>

#include "core/config.hpp"

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

    // 2) S'assurer que le dossier parent existe (au tout premier enregistrement,
    //    %config-plugin%/<plugin>/ peut ne pas encore exister). os_mkdirs cree
    //    toute l'arborescence et renvoie MKDIR_EXISTS si elle est deja la.
    const size_t sep = result.path.find_last_of("/\\");
    if (sep != std::string::npos) {
        const std::string dir = result.path.substr(0, sep);
        if (os_mkdirs(dir.c_str()) == MKDIR_ERROR) {
            result.error = "creation du dossier de configuration impossible";
            return result;
        }
    }

    // 3) Serialisation (coeur pur). marker=false : pas de BOM (le parser n'en veut pas).
    const std::string text = sd::core::toJson(cfg);

    // 4) Ecriture ATOMIQUE : config.json est le SEUL etat persistant du plugin.
    //    Une coupure (crash, panne, disque plein) en plein milieu d'un ecrasement
    //    direct laisserait un fichier tronque que loadConfig ne saurait plus parser
    //    -> toute la config perdue. On ecrit donc d'abord un fichier temporaire, puis
    //    on le bascule sur la cible via os_safe_replace (rename atomique sur le meme
    //    volume, la meme primitive qu'OBS utilise pour ses collections de scenes).
    const std::string tmp = result.path + ".tmp";
    if (!os_quick_write_utf8_file(tmp.c_str(), text.c_str(), text.size(), false)) {
        result.error = "ecriture du fichier temporaire impossible";
        return result;
    }
    if (os_safe_replace(result.path.c_str(), tmp.c_str(), nullptr) != 0) {
        os_unlink(tmp.c_str());  // pas de .tmp orphelin si la bascule echoue
        result.error = "remplacement atomique du fichier impossible";
        return result;
    }
    result.saved = true;
    return result;
}

}  // namespace sd::obsbridge
