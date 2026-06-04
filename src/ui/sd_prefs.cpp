// Flowspire — preferences GLOBALES de l'application (implementation).
// Voir sd_prefs.hpp. Persiste dans prefs.json (dossier de config du plugin) via
// ObsFileStore (ecriture atomique + .bak). Lecture tolerante (defauts en cas de
// fichier absent/corrompu). Aucune exception ne remonte a l'appelant.
#include "ui/sd_prefs.hpp"

#include <string>

#include <nlohmann/json.hpp>

#include <util/base.h> // LOG_WARNING

#include "obs/obs_file_store.hpp"
#include "plugin-support.h" // obs_log

namespace sd::ui {

namespace {
constexpr const char* kPrefsFile = "prefs.json";

// Cles STABLES et lisibles pour offSceneBehavior (plus robustes qu'un entier : un
// re-ordonnancement de l'enum ne change pas la valeur persistee sur disque).
constexpr const char* kForceAsShot = "forceAsShot";
constexpr const char* kPauseAuto = "pauseAuto";

const char* toKey(OffSceneBehavior b) {
    return b == OffSceneBehavior::ForceAsShot ? kForceAsShot : kPauseAuto;
}

OffSceneBehavior fromKey(const std::string& s) {
    // Valeur inconnue -> defaut (pause), coherent avec GlobalPrefs.
    return s == kForceAsShot ? OffSceneBehavior::ForceAsShot : OffSceneBehavior::PauseAuto;
}
} // namespace

GlobalPrefs loadPrefs() {
    GlobalPrefs prefs; // valeurs par defaut
    sd::obsbridge::ObsFileStore store;
    std::string text;
    if (!store.read(kPrefsFile, text)) {
        return prefs; // absent / illisible -> defauts
    }
    try {
        const auto json = nlohmann::json::parse(text);
        prefs.checkUpdatesOnStartup = json.value("checkUpdatesOnStartup", prefs.checkUpdatesOnStartup);
        prefs.offSceneBehavior = fromKey(json.value("offSceneBehavior", std::string(toKey(prefs.offSceneBehavior))));
        prefs.statusPushEnabled = json.value("statusPushEnabled", prefs.statusPushEnabled);
        prefs.statusPushHost = json.value("statusPushHost", prefs.statusPushHost);
        prefs.statusPushPort = json.value("statusPushPort", prefs.statusPushPort);
    } catch (...) {
        return GlobalPrefs{}; // contenu invalide -> defauts (jamais d'echec)
    }
    return prefs;
}

void savePrefs(const GlobalPrefs& prefs) {
    sd::obsbridge::ObsFileStore store;
    nlohmann::json json;
    json["checkUpdatesOnStartup"] = prefs.checkUpdatesOnStartup;
    json["offSceneBehavior"] = toKey(prefs.offSceneBehavior);
    json["statusPushEnabled"] = prefs.statusPushEnabled;
    json["statusPushHost"] = prefs.statusPushHost;
    json["statusPushPort"] = prefs.statusPushPort;
    const auto res = store.write(kPrefsFile, json.dump(2));
    // Reglage applique immediatement : si l'ecriture rate, le choix ne persistera pas
    // -> on TRACE (sans pop-up : non bloquant). Le pilotage en cours n'est pas affecte.
    if (!res.ok) {
        obs_log(LOG_WARNING, "echec d'ecriture de %s : %s", kPrefsFile, res.error.c_str());
    }
}

} // namespace sd::ui
