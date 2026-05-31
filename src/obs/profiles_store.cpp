// StreamDirector — pont OBS : magasin de profils (implementation IO).
#include "obs/profiles_store.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <exception>
#include <string>

#include "core/config.hpp"
#include "core/profiles.hpp"
#include "obs/config_loader.hpp"
#include "obs/fs_atomic.hpp"

namespace sd::profiles {

namespace {

// Resout %config-plugin%/<rel> (chemin malloc'd par OBS -> a liberer).
std::string resolvePath(const std::string& rel) {
    std::string out;
    char* p = obs_module_config_path(rel.c_str());
    if (p) {
        out = p;
        bfree(p);
    }
    return out;
}

std::string indexPath() {
    return resolvePath("profiles/index.json");
}

std::string profilePath(int id) {
    return resolvePath("profiles/" + std::to_string(id) + ".json");
}

// Lit un fichier texte UTF-8. false si absent/illisible (out inchange).
bool readFile(const std::string& path, std::string& out) {
    if (path.empty() || !os_file_exists(path.c_str())) {
        return false;
    }
    char* raw = os_quick_read_utf8_file(path.c_str());
    if (!raw) {
        return false;
    }
    out = raw;
    bfree(raw);
    return true;
}

// Ecrit l'index sur disque (atomique).
sd::obsbridge::FsResult writeIndexFile(const sd::core::ProfileIndex& idx) {
    const std::string path = indexPath();
    if (path.empty()) {
        return {false, "chemin du catalogue de profils introuvable"};
    }
    return sd::obsbridge::writeUtf8Atomic(path, sd::core::profileIndexToJson(idx));
}

// Ecrit le fichier d'un profil (= une Config brute) sur disque (atomique).
sd::obsbridge::FsResult writeProfileFile(int id, const sd::core::Config& cfg) {
    const std::string path = profilePath(id);
    if (path.empty()) {
        return {false, "chemin de fichier de profil introuvable"};
    }
    return sd::obsbridge::writeUtf8Atomic(path, sd::core::toJson(cfg));
}

// Charge l'index. found=false si index.json absent (-> migration a faire).
// ok=false avec error si present mais illisible (-> on N'ECRASE PAS, on remonte
// l'erreur pour ne pas detruire un catalogue recuperable).
struct RawIndex {
    sd::core::ProfileIndex index;
    bool found = false;
    bool ok = false;
    std::string error;
};
RawIndex loadIndexRaw() {
    RawIndex r;
    const std::string path = indexPath();
    if (path.empty()) {
        r.error = "chemin du catalogue de profils introuvable";
        return r;
    }
    std::string text;
    if (!readFile(path, text)) {
        return r;  // absent : found=false, ok=false, pas d'erreur
    }
    r.found = true;
    try {
        r.index = sd::core::profileIndexFromJson(text);
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = std::string("catalogue de profils illisible : ") + e.what();
    }
    return r;
}

// Premiere utilisation : cree le dossier profiles/ avec un profil unique a partir
// du config.json existant (ou des valeurs par defaut), marque actif.
ListResult migrate(const std::string& defaultName) {
    ListResult res;

    // Config de depart = config.json existant s'il est lisible, sinon defauts.
    sd::core::Config base;
    const sd::obsbridge::ConfigLoadResult loaded = sd::obsbridge::loadConfig();
    if (loaded.parsed) {
        base = loaded.config;
    }

    // Catalogue a un seul profil, actif.
    sd::core::ProfileIndex idx;
    const int id = sd::core::addProfile(idx, defaultName);  // id = 1
    idx.activeId = id;

    const sd::obsbridge::FsResult wp = writeProfileFile(id, base);
    if (!wp.ok) {
        res.error = wp.error;
        return res;
    }
    const sd::obsbridge::FsResult wi = writeIndexFile(idx);
    if (!wi.ok) {
        res.error = wi.error;
        return res;
    }
    // S'assurer que config.json (copie vivante) existe. On ne le REECRIT JAMAIS
    // s'il existe deja (meme corrompu) -> aucun ecrasement d'un fichier
    // potentiellement recuperable ; il se resynchronisera au prochain
    // enregistrement.
    if (!loaded.fileFound) {
        const sd::obsbridge::ConfigSaveResult sc = sd::obsbridge::saveConfig(base);
        if (!sc.saved) {
            res.error = sc.error;
            return res;
        }
    }

    res.index = idx;
    res.ok = true;
    return res;
}

}  // namespace

ListResult loadList(const std::string& defaultName) {
    const RawIndex raw = loadIndexRaw();
    if (raw.ok) {
        ListResult res;
        res.index = raw.index;
        res.ok = true;
        return res;
    }
    if (raw.found) {  // present mais illisible : on remonte l'erreur (pas de migration destructrice)
        ListResult res;
        res.error = raw.error.empty() ? "catalogue de profils illisible" : raw.error;
        return res;
    }
    return migrate(defaultName);  // absent : premiere utilisation
}

ConfigResult loadProfile(int id) {
    ConfigResult res;
    const std::string path = profilePath(id);
    std::string text;
    if (!readFile(path, text)) {
        res.error = "fichier de profil introuvable";
        return res;
    }
    try {
        res.config = sd::core::fromJson(text);
        res.ok = true;
    } catch (const std::exception& e) {
        res.error = e.what();
    }
    return res;
}

StoreResult setActive(int id) {
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
    const ConfigResult pc = loadProfile(id);
    if (!pc.ok) {
        res.error = pc.error;
        return res;
    }
    // config.json (copie vivante) D'ABORD, index ensuite : si une coupure survient
    // entre les deux, le moteur tourne deja sur le bon contenu ; seule l'etiquette
    // "actif" serait en retard d'un cran (corrigee au prochain enregistrement).
    const sd::obsbridge::ConfigSaveResult sc = sd::obsbridge::saveConfig(pc.config);
    if (!sc.saved) {
        res.error = sc.error;
        return res;
    }
    sd::core::setActiveProfile(idx, id);
    const sd::obsbridge::FsResult wi = writeIndexFile(idx);
    if (!wi.ok) {
        res.error = wi.error;
        return res;
    }
    res.ok = true;
    res.id = id;
    return res;
}

StoreResult saveActive(const sd::core::Config& cfg) {
    StoreResult res;
    const RawIndex raw = loadIndexRaw();
    if (!raw.ok) {
        res.error = raw.error.empty() ? "catalogue de profils introuvable" : raw.error;
        return res;
    }
    // config.json (copie vivante) d'abord, fichier du profil actif ensuite.
    const sd::obsbridge::ConfigSaveResult sc = sd::obsbridge::saveConfig(cfg);
    if (!sc.saved) {
        res.error = sc.error;
        return res;
    }
    const sd::obsbridge::FsResult wp = writeProfileFile(raw.index.activeId, cfg);
    if (!wp.ok) {
        res.error = wp.error;
        return res;
    }
    res.ok = true;
    res.id = raw.index.activeId;
    return res;
}

StoreResult createProfile(const std::string& name, const sd::core::Config& cfg, bool makeActive) {
    StoreResult res;
    const RawIndex raw = loadIndexRaw();
    if (!raw.ok) {
        res.error = raw.error.empty() ? "catalogue de profils introuvable" : raw.error;
        return res;
    }
    sd::core::ProfileIndex idx = raw.index;
    const int id = sd::core::addProfile(idx, name);
    const sd::obsbridge::FsResult wp = writeProfileFile(id, cfg);
    if (!wp.ok) {
        res.error = wp.error;
        return res;
    }
    if (makeActive) {
        sd::core::setActiveProfile(idx, id);
        const sd::obsbridge::ConfigSaveResult sc = sd::obsbridge::saveConfig(cfg);
        if (!sc.saved) {
            res.error = sc.error;
            return res;
        }
    }
    const sd::obsbridge::FsResult wi = writeIndexFile(idx);
    if (!wi.ok) {
        res.error = wi.error;
        return res;
    }
    res.ok = true;
    res.id = id;
    return res;
}

StoreResult duplicateProfile(int id, const std::string& copySuffix) {
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
    const sd::obsbridge::FsResult wp = writeProfileFile(newId, pc.config);
    if (!wp.ok) {
        res.error = wp.error;
        return res;
    }
    const sd::obsbridge::FsResult wi = writeIndexFile(idx);
    if (!wi.ok) {
        res.error = wi.error;
        return res;
    }
    res.ok = true;
    res.id = newId;
    return res;
}

StoreResult renameProfile(int id, const std::string& name) {
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
    const sd::obsbridge::FsResult wi = writeIndexFile(idx);
    res.ok = wi.ok;
    res.error = wi.error;
    res.id = id;
    return res;
}

StoreResult removeProfile(int id) {
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
    const sd::obsbridge::FsResult wi = writeIndexFile(idx);
    if (!wi.ok) {
        res.error = wi.error;
        return res;
    }
    // Best-effort : retire le fichier orphelin (plus reference par l'index).
    const std::string path = profilePath(id);
    if (!path.empty()) {
        os_unlink(path.c_str());
    }
    res.ok = true;
    return res;
}

}  // namespace sd::profiles
