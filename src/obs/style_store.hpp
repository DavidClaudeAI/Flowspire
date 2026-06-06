// Flowspire — magasin de la BIBLIOTHEQUE GLOBALE de styles de realisation.
//
// Un seul fichier global a la racine config du plugin : `styles.json` (contenu inline,
// pas d'index ni de fichiers separes -- bien plus simple que les profils, car un style est
// petit). La bibliotheque est INDEPENDANTE des profils : un style enregistre est reutilisable
// sur n'importe quel show.
//
// Comme le ProfileStore, la LOGIQUE (cette classe) ne touche pas OBS : elle parle a un
// FileStore injecte -> testable hors OBS (FakeFileStore). L'API libre en bas delegue a un
// StyleStore par defaut adosse a OBS (ObsFileStore) pour les ecrans UI.
#pragma once

#include <string>
#include <vector>

#include "core/rhythm_style.hpp"
#include "obs/file_store.hpp"

namespace sd::styles {

// Bibliotheque chargee depuis le disque. Fichier ABSENT => liste vide + ok=true (premiere
// utilisation, ce n'est pas une erreur). Illisible/corrompu => tentative `.bak`, sinon
// ok=false + error (l'appelant repli sur une liste vide).
struct LibraryResult {
    std::vector<sd::core::RhythmStyle> styles;
    bool ok = false;
    std::string error;
};

struct SaveResult {
    bool ok = false;
    std::string error;
};

class StyleStore {
public:
    explicit StyleStore(sd::obsbridge::FileStore& store) : store_(store) {}

    // Charge la bibliotheque (recuperation `.bak` si le fichier courant est illisible).
    LibraryResult load() const;

    // Ecrit la bibliotheque (atomique + `.bak` automatique via FileStore).
    SaveResult save(const std::vector<sd::core::RhythmStyle>& styles);

private:
    sd::obsbridge::FileStore& store_;
};

// --- API libre (plugin) : delegue a un StyleStore par defaut (ObsFileStore). ---
LibraryResult loadLibrary();
SaveResult saveLibrary(const std::vector<sd::core::RhythmStyle>& styles);

} // namespace sd::styles
