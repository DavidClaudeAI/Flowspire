// Flowspire — magasin de la bibliotheque de styles (logique + IO via FileStore injecte).
// OBS-free : n'inclut aucun entete OBS, parle uniquement au FileStore -> testable.
#include "obs/style_store.hpp"

#include <exception>

namespace sd::styles {

namespace {
// Fichier unique de la bibliotheque, a la racine config (a cote du dossier profiles/).
const std::string kStylesRel = "styles.json";

// Tente de lire+parser la bibliotheque a `rel`. false si absent/illisible OU JSON invalide.
bool tryRead(const sd::obsbridge::FileStore& store, const std::string& rel,
             std::vector<sd::core::RhythmStyle>& out) {
    std::string text;
    if (!store.read(rel, text)) {
        return false;
    }
    try {
        out = sd::core::rhythmStyleLibraryFromJson(text);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
} // namespace

LibraryResult StyleStore::load() const {
    LibraryResult r;
    if (!store_.exists(kStylesRel)) {
        r.ok = true; // premiere utilisation : bibliotheque vide (pas une erreur)
        return r;
    }
    if (tryRead(store_, kStylesRel, r.styles)) {
        r.ok = true;
        return r;
    }
    // Fichier present mais illisible/corrompu -> recuperation depuis le `.bak`.
    if (tryRead(store_, sd::obsbridge::backupPath(kStylesRel), r.styles)) {
        r.ok = true;
        return r;
    }
    r.error = "bibliotheque de styles illisible";
    return r;
}

SaveResult StyleStore::save(const std::vector<sd::core::RhythmStyle>& styles) {
    SaveResult r;
    const sd::obsbridge::FsResult w = store_.write(kStylesRel, sd::core::rhythmStyleLibraryToJson(styles));
    r.ok = w.ok;
    r.error = w.error;
    return r;
}

} // namespace sd::styles
