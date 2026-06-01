// StreamDirector — magasin de profils (implementation : gestion + IO via FileStore).
// OBS-free : n'inclut aucun entete OBS, parle uniquement au FileStore injecte.
#include "obs/profiles_store.hpp"

#include <algorithm>
#include <exception>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/profiles.hpp"

namespace sd::profiles {

namespace {
const std::string kIndexRel = "profiles/index.json";
const std::string kConfigRel = "config.json";
const std::string kProfilesDir = "profiles";
const std::string kJsonExt = ".json";

std::string profileRel(int id) {
    return "profiles/" + std::to_string(id) + ".json";
}
}  // namespace

bool ProfileStore::readConfig(const std::string& rel, sd::core::Config& out) const {
    std::string text;
    if (!store_.read(rel, text)) {
        return false;
    }
    try {
        out = sd::core::fromJson(text);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ProfileStore::readIndex(const std::string& rel, sd::core::ProfileIndex& out) const {
    std::string text;
    if (!store_.read(rel, text)) {
        return false;
    }
    try {
        out = sd::core::profileIndexFromJson(text);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

sd::obsbridge::FsResult ProfileStore::writeIndex(const sd::core::ProfileIndex& idx) {
    return store_.write(kIndexRel, sd::core::profileIndexToJson(idx));
}

sd::obsbridge::FsResult ProfileStore::writeProfile(int id, const sd::core::Config& cfg) {
    return store_.write(profileRel(id), sd::core::toJson(cfg));
}

ProfileStore::RawIndex ProfileStore::loadIndexRaw() const {
    RawIndex r;
    if (!store_.exists(kIndexRel)) {
        return r;  // absent : found=false, ok=false (-> recuperation/migration)
    }
    r.found = true;
    if (readIndex(kIndexRel, r.index)) {
        r.ok = true;
    } else {
        r.error = "catalogue de profils illisible";
    }
    return r;
}

bool ProfileStore::reconstructFromScan(const std::string& defaultName,
                                       sd::core::ProfileIndex& out) const {
    const std::vector<std::string> files = store_.list(kProfilesDir, kJsonExt);
    std::vector<int> ids;
    for (const auto& f : files) {
        // f = "<n>.json" -> on retire ".json" et on garde si c'est un entier pur
        // (ecarte "index.json" et tout fichier non numerique).
        const std::string stem = f.substr(0, f.size() - kJsonExt.size());
        if (stem.empty() ||
            !std::all_of(stem.begin(), stem.end(), [](char c) { return c >= '0' && c <= '9'; })) {
            continue;
        }
        try {
            ids.push_back(std::stoi(stem));
        } catch (const std::exception&) {
            // numero hors plage int : on ignore ce fichier.
        }
    }
    if (ids.empty()) {
        return false;
    }
    std::sort(ids.begin(), ids.end());
    out = sd::core::ProfileIndex{};
    for (const int id : ids) {
        // Noms perdus avec l'index : on regenere des noms synthetiques (le contenu
        // des profils, lui, est intact). L'utilisateur pourra renommer ensuite.
        out.profiles.push_back({id, defaultName + " " + std::to_string(id)});
    }
    out.activeId = ids.front();
    out.nextId = ids.back() + 1;
    return true;
}

ListResult ProfileStore::migrate(const std::string& defaultName) {
    ListResult res;

    // Config de depart = ancien config.json s'il est lisible, sinon defauts codes.
    sd::core::Config base;
    readConfig(kConfigRel, base);  // ignore l'echec : base garde les defauts

    sd::core::ProfileIndex idx;
    const int id = sd::core::addProfile(idx, defaultName);  // id = 1
    idx.activeId = id;

    const sd::obsbridge::FsResult wp = writeProfile(id, base);
    if (!wp.ok) {
        res.error = wp.error;
        return res;
    }
    const sd::obsbridge::FsResult wi = writeIndex(idx);
    if (!wi.ok) {
        res.error = wi.error;
        return res;
    }
    res.index = idx;
    res.ok = true;
    return res;
}

ListResult ProfileStore::loadList(const std::string& defaultName) {
    const RawIndex raw = loadIndexRaw();
    if (raw.ok) {
        ListResult res;
        res.index = raw.index;
        res.ok = true;
        return res;
    }

    // Index illisible (absent OU corrompu). On recupere SANS perte, dans l'ordre :
    //   1) .bak de l'index (avant-derniere version valide) -> on heale le fichier.
    sd::core::ProfileIndex recovered;
    if (readIndex(kIndexRel + ".bak", recovered)) {
        writeIndex(recovered);  // best-effort : remet un index.json sain
        ListResult res;
        res.index = recovered;
        res.ok = true;
        return res;
    }
    //   2) scan des fichiers profiles/<n>.json (les contenus existent toujours).
    if (reconstructFromScan(defaultName, recovered)) {
        writeIndex(recovered);
        ListResult res;
        res.index = recovered;
        res.ok = true;
        return res;
    }
    //   3) rien sur disque -> premiere utilisation : migration.
    return migrate(defaultName);
}

ConfigResult ProfileStore::loadProfile(int id) {
    ConfigResult res;
    const std::string rel = profileRel(id);
    if (readConfig(rel, res.config)) {
        res.ok = true;
        return res;
    }
    // Fichier absent ou illisible : tentative de recuperation depuis le .bak.
    if (readConfig(rel + ".bak", res.config)) {
        store_.write(rel, sd::core::toJson(res.config));  // heale le fichier courant
        res.ok = true;
        return res;
    }
    res.error = "fichier de profil introuvable ou illisible";
    return res;
}

ActiveConfigResult ProfileStore::loadActiveConfig(const std::string& defaultName) {
    ActiveConfigResult res;
    const ListResult list = loadList(defaultName);
    if (!list.ok) {
        res.error = list.error.empty() ? "catalogue de profils illisible" : list.error;
        return res;
    }
    const ConfigResult pc = loadProfile(list.index.activeId);
    if (!pc.ok) {
        res.error = pc.error;
        return res;
    }
    res.config = pc.config;
    res.activeId = list.index.activeId;
    res.ok = true;
    return res;
}

StoreResult ProfileStore::setActive(int id) {
    StoreResult res;
    const RawIndex raw = loadIndexRaw();
    if (!raw.ok) {
        res.error = raw.error.empty() ? "catalogue de profils introuvable" : raw.error;
        return res;
    }
    sd::core::ProfileIndex idx = raw.index;
    if (sd::core::findProfile(idx, id) == nullptr) {
        res.error = "profil introuvable";
        return res;
    }
    // UNE seule ecriture : l'etiquette "actif" dans l'index. Le moteur lira ensuite
    // <id>.json directement -> aucune copie de contenu, aucune desync possible.
    sd::core::setActiveProfile(idx, id);
    const sd::obsbridge::FsResult wi = writeIndex(idx);
    if (!wi.ok) {
        res.error = wi.error;
        return res;
    }
    res.ok = true;
    res.id = id;
    return res;
}

StoreResult ProfileStore::saveActive(const sd::core::Config& cfg) {
    StoreResult res;
    const RawIndex raw = loadIndexRaw();
    if (!raw.ok) {
        res.error = raw.error.empty() ? "catalogue de profils introuvable" : raw.error;
        return res;
    }
    // Ecrit le SEUL fichier du profil actif (source de verite lue par le moteur).
    const sd::obsbridge::FsResult wp = writeProfile(raw.index.activeId, cfg);
    if (!wp.ok) {
        res.error = wp.error;
        return res;
    }
    res.ok = true;
    res.id = raw.index.activeId;
    return res;
}

StoreResult ProfileStore::createProfile(const std::string& name, const sd::core::Config& cfg,
                                        bool makeActive) {
    StoreResult res;
    const RawIndex raw = loadIndexRaw();
    if (!raw.ok) {
        res.error = raw.error.empty() ? "catalogue de profils introuvable" : raw.error;
        return res;
    }
    sd::core::ProfileIndex idx = raw.index;
    const int id = sd::core::addProfile(idx, name);
    // Contenu D'ABORD, puis l'index (commit). Une coupure entre les deux laisse un
    // fichier orphelin inoffensif (non reference) plutot qu'une entree sans contenu.
    const sd::obsbridge::FsResult wp = writeProfile(id, cfg);
    if (!wp.ok) {
        res.error = wp.error;
        return res;
    }
    if (makeActive) {
        sd::core::setActiveProfile(idx, id);
    }
    const sd::obsbridge::FsResult wi = writeIndex(idx);
    if (!wi.ok) {
        res.error = wi.error;
        return res;
    }
    res.ok = true;
    res.id = id;
    return res;
}

StoreResult ProfileStore::duplicateProfile(int id, const std::string& copySuffix) {
    StoreResult res;
    const RawIndex raw = loadIndexRaw();
    if (!raw.ok) {
        res.error = raw.error.empty() ? "catalogue de profils introuvable" : raw.error;
        return res;
    }
    sd::core::ProfileIndex idx = raw.index;
    const sd::core::ProfileEntry* src = sd::core::findProfile(idx, id);
    if (src == nullptr) {
        res.error = "profil introuvable";
        return res;
    }
    const std::string desired = src->name + " " + copySuffix;
    const ConfigResult pc = loadProfile(id);
    if (!pc.ok) {
        res.error = pc.error;
        return res;
    }
    const int newId = sd::core::addProfile(idx, desired);  // unicite geree par core
    const sd::obsbridge::FsResult wp = writeProfile(newId, pc.config);
    if (!wp.ok) {
        res.error = wp.error;
        return res;
    }
    const sd::obsbridge::FsResult wi = writeIndex(idx);
    if (!wi.ok) {
        res.error = wi.error;
        return res;
    }
    res.ok = true;
    res.id = newId;
    return res;
}

StoreResult ProfileStore::renameProfile(int id, const std::string& name) {
    StoreResult res;
    const RawIndex raw = loadIndexRaw();
    if (!raw.ok) {
        res.error = raw.error.empty() ? "catalogue de profils introuvable" : raw.error;
        return res;
    }
    sd::core::ProfileIndex idx = raw.index;
    if (!sd::core::renameProfile(idx, id, name)) {
        res.error = "profil introuvable";
        return res;
    }
    const sd::obsbridge::FsResult wi = writeIndex(idx);
    res.ok = wi.ok;
    res.error = wi.error;
    res.id = id;
    return res;
}

StoreResult ProfileStore::removeProfile(int id) {
    StoreResult res;
    const RawIndex raw = loadIndexRaw();
    if (!raw.ok) {
        res.error = raw.error.empty() ? "catalogue de profils introuvable" : raw.error;
        return res;
    }
    sd::core::ProfileIndex idx = raw.index;
    if (!sd::core::removeProfile(idx, id)) {
        res.error = "suppression refusee (profil actif, dernier restant, ou introuvable)";
        return res;
    }
    const sd::obsbridge::FsResult wi = writeIndex(idx);
    if (!wi.ok) {
        res.error = wi.error;
        return res;
    }
    // Best-effort : retire le fichier orphelin ET son .bak (plus references).
    const std::string rel = profileRel(id);
    store_.remove(rel);
    store_.remove(rel + ".bak");
    res.ok = true;
    return res;
}

}  // namespace sd::profiles
