// StreamDirector — magasin de profils (gestion + IO via FileStore injecte).
//
// Modele de stockage (Run 8) — le MOTEUR LIT DIRECTEMENT LE PROFIL ACTIF :
//   %config-plugin%/
//     config.json            <- INERTE : ancien fichier, plus jamais relu/reecrit
//                               (laisse en place pour un downgrade eventuel) ; lu
//                               UNE fois a la migration initiale.
//     profiles/
//       index.json           <- catalogue {id, nom} + profil actif + nextId.
//                               POINT DE COMMIT d'une bascule (1 ecriture atomique).
//       <id>.json            <- contenu d'un profil = une Config brute. SOURCE
//                               UNIQUE DE VERITE lue par le moteur (via le profil
//                               actif). Sauvegarde `.bak` automatique a chaque ecrit.
//
// Plus de double ecriture config.json + index -> la desynchro "config vs actif" ne
// peut plus exister. Filets : recuperation `.bak` si un fichier devient illisible,
// reconstruction du catalogue par SCAN du dossier si l'index est perdu.
//
// La LOGIQUE (cette classe) ne touche pas OBS : elle parle a un FileStore injecte
// -> testable hors OBS avec un FakeFileStore. L'API libre en bas delegue a un
// ProfileStore par defaut adosse a OBS (ObsFileStore) pour les ecrans UI.
#pragma once

#include <string>

#include "core/config.hpp"
#include "core/profiles.hpp"
#include "obs/file_store.hpp"

namespace sd::profiles {

// Resultat generique d'une operation de magasin.
struct StoreResult {
    bool ok = false;
    std::string error;
    int id = 0;  // nouvel id pour create/duplicate (sinon non renseigne)
};

// Catalogue charge depuis le disque.
struct ListResult {
    sd::core::ProfileIndex index;
    bool ok = false;
    std::string error;
};

// Contenu d'un profil charge depuis le disque.
struct ConfigResult {
    sd::core::Config config;
    bool ok = false;
    std::string error;
};

// Config du profil ACTIF + son id : c'est CE que lit le moteur (dock/assistant).
struct ActiveConfigResult {
    sd::core::Config config;
    int activeId = 0;
    bool ok = false;
    std::string error;
};

// Gestion des profils sur un FileStore injecte. OBS-free -> testable.
class ProfileStore {
public:
    explicit ProfileStore(sd::obsbridge::FileStore& store) : store_(store) {}

    // Charge le catalogue. Recupere sans perte si l'index est illisible : .bak ->
    // scan du dossier -> sinon premiere utilisation (migration de l'ancien
    // config.json, ou des defauts, en profil n.1 actif). Heale le fichier au passage.
    ListResult loadList(const std::string& defaultName);

    // Charge le contenu (Config) du profil `id` (recuperation `.bak` si illisible).
    ConfigResult loadProfile(int id);

    // Config du profil ACTIF (ce que le moteur doit charger). Garantit l'existence
    // du catalogue (migration au premier usage). `defaultName` = nom du profil cree
    // a la migration (localise, fourni par l'UI).
    ActiveConfigResult loadActiveConfig(const std::string& defaultName);

    // Bascule le profil actif sur `id` : UNE ecriture atomique de index.json (le
    // moteur lira directement <id>.json). L'appelant recharge le moteur ensuite.
    StoreResult setActive(int id);

    // Enregistre `cfg` dans le profil ACTIF : ecrit <actif>.json (source de verite).
    StoreResult saveActive(const sd::core::Config& cfg);

    // Cree un profil `name` de contenu `cfg`. Si `makeActive`, le marque actif.
    StoreResult createProfile(const std::string& name, const sd::core::Config& cfg, bool makeActive);

    // Duplique `id` -> "<nom> <copySuffix>", meme contenu. Ne bascule pas.
    StoreResult duplicateProfile(int id, const std::string& copySuffix);

    // Renomme `id` (nom rendu unique).
    StoreResult renameProfile(int id, const std::string& name);

    // Supprime `id` (refuse l'actif, le dernier restant, ou un id absent).
    StoreResult removeProfile(int id);

private:
    // Index brut tel que lu (sans recuperation ni ecriture).
    struct RawIndex {
        sd::core::ProfileIndex index;
        bool found = false;  // le fichier index.json existe-t-il ?
        bool ok = false;     // a-t-il ete lu ET parse ?
        std::string error;
    };
    RawIndex loadIndexRaw() const;
    // Charge le catalogue pour une operation MUTANTE : si illisible, renseigne
    // `res.error` et renvoie false (l'appelant retourne `res` tel quel) ; sinon
    // remplit `idx` et renvoie true. Mutualise le prologue des 6 methodes d'ecriture.
    bool loadIndexForWrite(sd::core::ProfileIndex& idx, StoreResult& res) const;

    // Lit + parse une Config a `rel` (false si absent ou JSON invalide).
    bool readConfig(const std::string& rel, sd::core::Config& out) const;
    // Lit + parse un ProfileIndex a `rel` (false si absent ou JSON invalide).
    bool readIndex(const std::string& rel, sd::core::ProfileIndex& out) const;

    sd::obsbridge::FsResult writeIndex(const sd::core::ProfileIndex& idx);
    sd::obsbridge::FsResult writeProfile(int id, const sd::core::Config& cfg);

    // Reconstruit un catalogue en scannant les fichiers profiles/<n>.json (noms
    // synthetiques, actif = plus petit id). false si aucun fichier de profil.
    bool reconstructFromScan(const std::string& defaultName, sd::core::ProfileIndex& out) const;

    // Rehausse idx.nextId au-dessus de tout fichier profiles/<n>.json present sur
    // disque (evite de reattribuer l'id d'un orphelin apres recuperation d'un .bak).
    void reconcileNextIdWithFiles(sd::core::ProfileIndex& idx) const;

    // Premiere utilisation : profil n.1 depuis l'ancien config.json (s'il est
    // lisible) ou les defauts codes en dur, marque actif.
    ListResult migrate(const std::string& defaultName);

    sd::obsbridge::FileStore& store_;
};

// --- API libre (plugin) : delegue a un ProfileStore par defaut (ObsFileStore). ---
// Les ecrans UI appellent ces fonctions. AUCUNE n'ecrit plus config.json.
ListResult loadList(const std::string& defaultName);
ConfigResult loadProfile(int id);
ActiveConfigResult loadActiveConfig(const std::string& defaultName);
StoreResult setActive(int id);
StoreResult saveActive(const sd::core::Config& cfg);
StoreResult createProfile(const std::string& name, const sd::core::Config& cfg, bool makeActive);
StoreResult duplicateProfile(int id, const std::string& copySuffix);
StoreResult renameProfile(int id, const std::string& name);
StoreResult removeProfile(int id);

}  // namespace sd::profiles
