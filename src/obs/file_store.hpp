// StreamDirector — abstraction de stockage fichier (couche obs, OBS-free).
//
// But : decoupler la LOGIQUE de gestion des profils (ProfileStore) de l'IO disque
// reelle, pour la rendre TESTABLE hors OBS. ProfileStore ne parle qu'a cette
// interface ; en production on lui injecte un ObsFileStore (os_* + chemins OBS),
// en test un FakeFileStore en memoire (avec simulateur de coupure).
//
// Les chemins sont RELATIFS a la racine de config du plugin (resolue par
// l'implementation). Ex : "config.json", "profiles/index.json", "profiles/3.json".
//
// Ce header n'inclut AUCUN entete OBS -> compilable dans la boucle de test rapide.
#pragma once

#include <string>
#include <vector>

#include "obs/fs_atomic.hpp" // FsResult (struct pur, header OBS-free)

namespace sd::obsbridge {

class FileStore {
public:
    virtual ~FileStore() = default;

    // Lit le contenu texte de `relPath`. Renvoie true et renseigne `out` si lu ;
    // false si absent OU illisible (out inchange). L'appelant a toujours une
    // strategie de repli (defauts, .bak) -> on ne distingue pas les deux echecs.
    virtual bool read(const std::string& relPath, std::string& out) const = 0;

    // Le fichier existe-t-il ?
    virtual bool exists(const std::string& relPath) const = 0;

    // Ecriture ATOMIQUE (tmp + bascule) avec sauvegarde `.bak` de l'ancien contenu.
    // Cree l'arborescence parente au besoin.
    virtual FsResult write(const std::string& relPath, const std::string& text) = 0;

    // Supprime `relPath` (best-effort). Renvoie true si le fichier n'existe plus.
    virtual bool remove(const std::string& relPath) = 0;

    // Noms de fichiers (basenames, pas les chemins) du dossier `relDir` se terminant
    // par `ext` (ex : ".json"). Vide si le dossier est absent. Sert au scan de
    // reconstruction du catalogue quand l'index est perdu.
    virtual std::vector<std::string> list(const std::string& relDir, const std::string& ext) const = 0;
};

} // namespace sd::obsbridge
