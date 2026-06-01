// StreamDirector — magasin de profils (implementation : gestion + IO via FileStore).
// OBS-free : n'inclut aucun entete OBS, parle uniquement au FileStore injecte.
#include "obs/profiles_store.hpp"

#include <algorithm>
#include <exception>
#include <limits>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/profiles.hpp"

namespace sd::profiles {

namespace {
// Source unique des composants de chemin : tout le reste en derive (pas de litteral
// "profiles/" / ".json" disperse qui pourrait diverger).
const std::string kProfilesDir = "profiles";
const std::string kJsonExt = ".json";
const std::string kIndexRel = kProfilesDir + "/index.json";
const std::string kConfigRel = "config.json";  // ancien fichier (migration uniquement)

std::string profileRel(int id) {
    return kProfilesDir + "/" + std::to_string(id) + kJsonExt;
}

// Id d'un fichier "profiles/<n>.json" si <n> est un entier CANONIQUE (chiffres purs,
// sans zero de tete, dans la plage int), sinon -1. La verification du round-trip
// (to_string(id) == stem) ecarte "007.json" (-> 7) dont le fichier reel serait
// introuvable via profileRel(7), et la garde de taille evite un substr qui
// deborderait sur un nom plus court que l'extension.
int parseProfileFileId(const std::string& filename) {
    if (filename.size() <= kJsonExt.size()) {
        return -1;
    }
    const std::string stem = filename.substr(0, filename.size() - kJsonExt.size());
    if (stem.empty() ||
        !std::all_of(stem.begin(), stem.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        return -1;
    }
    int id = 0;
    try {
        id = std::stoi(stem);
    } catch (const std::exception&) {
        return -1;  // hors plage int
    }
    if (std::to_string(id) != stem) {
        return -1;  // nom non canonique (zeros de tete) -> profileRel ne le retrouverait pas
    }
    return id;
}

// Lit `rel` via `store` et le parse avec `parse`. Renvoie false si le fichier est
// absent/illisible OU si le JSON est invalide (l'appelant a un repli). Mutualise le
// schema lecture + try/parse pour la Config et le ProfileIndex.
template <class T, class Parse>
bool readParsed(const sd::obsbridge::FileStore& store, const std::string& rel, Parse parse,
                T& out) {
    std::string text;
    if (!store.read(rel, text)) {
        return false;
    }
    try {
        out = parse(text);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
}  // namespace

bool ProfileStore::readConfig(const std::string& rel, sd::core::Config& out) const {
    return readParsed(store_, rel, sd::core::fromJson, out);
}

bool ProfileStore::readIndex(const std::string& rel, sd::core::ProfileIndex& out) const {
    return readParsed(store_, rel, sd::core::profileIndexFromJson, out);
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

bool ProfileStore::loadIndexForWrite(sd::core::ProfileIndex& idx, StoreResult& res) const {
    const RawIndex raw = loadIndexRaw();
    if (!raw.ok) {
        res.error = raw.error.empty() ? "catalogue de profils introuvable" : raw.error;
        return false;
    }
    idx = raw.index;
    return true;
}

bool ProfileStore::reconstructFromScan(const std::string& defaultName,
                                       sd::core::ProfileIndex& out) const {
    std::vector<int> ids;
    for (const auto& f : store_.list(kProfilesDir, kJsonExt)) {
        const int id = parseProfileFileId(f);  // ecarte index.json + noms non canoniques
        if (id >= 0) {
            ids.push_back(id);
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
    const int maxId = ids.back();
    out.nextId = (maxId < std::numeric_limits<int>::max()) ? maxId + 1 : maxId;
    return true;
}

void ProfileStore::reconcileNextIdWithFiles(sd::core::ProfileIndex& idx) const {
    // Rehausse nextId au-dessus de TOUT fichier profiles/<n>.json present sur disque
    // (pas seulement ceux references par l'index). Indispensable apres une recuperation
    // depuis un index .bak PERIME : ce .bak peut ignorer un profil cree plus tard dont
    // le fichier <id>.json existe encore -> sans ce rehaussement, un prochain
    // createProfile reattribuerait cet id et ECRASERAIT le fichier orphelin.
    for (const auto& f : store_.list(kProfilesDir, kJsonExt)) {
        const int id = parseProfileFileId(f);
        if (id >= 0 && id >= idx.nextId && id < std::numeric_limits<int>::max()) {
            idx.nextId = id + 1;
        }
    }
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
    if (readIndex(sd::obsbridge::backupPath(kIndexRel), recovered)) {
        // Le .bak peut etre anterieur a une creation de profil : on rehausse nextId
        // au-dessus des fichiers reellement presents pour ne pas reattribuer (donc
        // ecraser) l'id d'un profil orphelin encore sur disque.
        reconcileNextIdWithFiles(recovered);
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
    // Fichier absent ou illisible : recuperation depuis le .bak. LECTURE SEULE : on
    // ne reecrit PAS le fichier courant. Pourquoi : l'ecriture atomique ferait basculer
    // le contenu corrompu courant dans le .bak (os_safe_replace), detruisant le seul bon
    // filet. On preserve donc le .bak intact ; le fichier sera regenere au prochain
    // saveActive normal. Un "load" ne doit pas non plus provoquer d'ecriture surprise.
    if (readConfig(sd::obsbridge::backupPath(rel), res.config)) {
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
    sd::core::ProfileIndex idx;
    if (!loadIndexForWrite(idx, res)) {
        return res;
    }
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
    sd::core::ProfileIndex idx;
    if (!loadIndexForWrite(idx, res)) {
        return res;
    }
    // Ecrit le SEUL fichier du profil actif (source de verite lue par le moteur).
    const sd::obsbridge::FsResult wp = writeProfile(idx.activeId, cfg);
    if (!wp.ok) {
        res.error = wp.error;
        return res;
    }
    res.ok = true;
    res.id = idx.activeId;
    return res;
}

StoreResult ProfileStore::createProfile(const std::string& name, const sd::core::Config& cfg,
                                        bool makeActive) {
    StoreResult res;
    sd::core::ProfileIndex idx;
    if (!loadIndexForWrite(idx, res)) {
        return res;
    }
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
    sd::core::ProfileIndex idx;
    if (!loadIndexForWrite(idx, res)) {
        return res;
    }
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
    sd::core::ProfileIndex idx;
    if (!loadIndexForWrite(idx, res)) {
        return res;
    }
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
    sd::core::ProfileIndex idx;
    if (!loadIndexForWrite(idx, res)) {
        return res;
    }
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
    store_.remove(sd::obsbridge::backupPath(rel));
    res.ok = true;
    return res;
}

}  // namespace sd::profiles
