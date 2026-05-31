// StreamDirector — pont OBS : magasin de profils (IO disque).
//
// Modele de stockage (retrocompatible) :
//   %config-plugin%/
//     config.json            <- copie VIVANTE lue par le moteur (INCHANGEE)
//     profiles/
//       index.json           <- catalogue {id, nom}, profil actif, nextId
//       <id>.json            <- contenu d'un profil = une Config brute (meme
//                               format que config.json)
//
// Le moteur ne lit QUE config.json : les profils sont une couche au-dessus.
// "Charger" un profil recopie son contenu dans config.json (l'appelant recharge
// ensuite le moteur). "Enregistrer" le profil actif ecrit config.json ET son
// fichier de profil, en phase. Au premier usage, l'existant est migre en
// profil n.1 (zero perte). Toute la logique pure du catalogue vit dans
// core/profiles ; ici, uniquement l'IO.
#pragma once

#include <string>

#include "core/config.hpp"
#include "core/profiles.hpp"

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

// Charge le catalogue. Au premier usage (pas d'index.json), MIGRE le config.json
// existant (ou des valeurs par defaut) en un profil unique nomme `defaultName`,
// marque actif. `defaultName` est fourni par l'UI (localise).
ListResult loadList(const std::string& defaultName);

// Charge le contenu (Config) du profil `id`.
ConfigResult loadProfile(int id);

// Bascule le profil actif sur `id` : recopie son contenu dans config.json (copie
// vivante) et met a jour l'index. L'appelant doit recharger le moteur ensuite.
StoreResult setActive(int id);

// Enregistre `cfg` dans le profil ACTIF : ecrit config.json (copie vivante) ET le
// fichier du profil actif, en phase. C'est l'ecriture appelee par les ecrans
// d'edition (assistant / parametres avances) au lieu de saveConfig direct.
StoreResult saveActive(const sd::core::Config& cfg);

// Cree un nouveau profil `name` avec le contenu `cfg`. Si `makeActive`, il devient
// aussi le profil actif (config.json recopiee). Renvoie le nouvel id.
StoreResult createProfile(const std::string& name, const sd::core::Config& cfg, bool makeActive);

// Duplique le profil `id` -> nouveau profil "<nom> <copySuffix>" (ex: "X (copie)"),
// meme contenu. Ne bascule pas. Renvoie le nouvel id.
StoreResult duplicateProfile(int id, const std::string& copySuffix);

// Renomme le profil `id` (nom rendu unique automatiquement).
StoreResult renameProfile(int id, const std::string& name);

// Supprime le profil `id` (refuse l'actif et le dernier restant — cf. core).
StoreResult removeProfile(int id);

}  // namespace sd::profiles
