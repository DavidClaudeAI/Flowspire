// StreamDirector — pont OBS : chargement du fichier de configuration.
// Couche "obs" : resout l'emplacement standard du config.json du plugin
// (via obs_module_config_path) et le lit avec l'API fichier d'OBS (gere les
// chemins UTF-8 sous Windows). Le PARSE lui-meme reste dans le coeur pur
// (sd::core::fromJson) : ici on ne fait que localiser + lire + deleguer.
#pragma once

#include <string>

#include "core/config.hpp"

namespace sd::obsbridge {

// Resultat d'une tentative de chargement de config.
struct ConfigLoadResult {
    std::string path;         // chemin resolu du config.json (toujours renseigne si resolvable)
    bool fileFound = false;   // le fichier existe-t-il a cet emplacement ?
    bool parsed = false;      // le JSON a-t-il ete parse avec succes ?
    std::string error;        // message d'erreur (lecture / parse) si echec
    sd::core::Config config;  // valide uniquement si parsed == true
};

// Resout %config-plugin%/config.json, le lit s'il existe et le parse.
// N'ecrit rien sur le disque. Ne leve pas : toute erreur est portee par le
// resultat (champs fileFound/parsed/error).
ConfigLoadResult loadConfig();

// Resultat d'une tentative d'ecriture de config.
struct ConfigSaveResult {
    std::string path;     // chemin resolu du config.json (toujours renseigne si resolvable)
    bool saved = false;   // l'ecriture a-t-elle reussi ?
    std::string error;    // message d'erreur (chemin / dossier / ecriture) si echec
};

// Serialise `cfg` (via sd::core::toJson) et l'ecrit dans %config-plugin%/config.json,
// en creant le dossier parent au besoin. Ecriture ATOMIQUE : ecrit un fichier .tmp
// puis le bascule via os_safe_replace (rename atomique) -> jamais de config.json
// tronque si une coupure survient en cours d'ecriture. Ne leve pas : toute erreur
// est portee par le resultat.
ConfigSaveResult saveConfig(const sd::core::Config& cfg);

}  // namespace sd::obsbridge
