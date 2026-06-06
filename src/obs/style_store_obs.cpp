// Flowspire — API libre du magasin de styles (plugin uniquement).
// Delegue a un StyleStore par defaut adosse a un ObsFileStore (IO reelle OBS). Ce TU depend
// d'OBS -> hors boucle de test rapide. La LOGIQUE testee vit dans style_store.cpp (OBS-free).
#include "obs/obs_file_store.hpp"
#include "obs/style_store.hpp"

namespace sd::styles {

namespace {
// Magasin par defaut : ObsFileStore + StyleStore en statiques locales (init thread-safe
// C++11 ; l'UI tourne sur le thread Qt). Le StyleStore garde une reference sur le FileStore
// -> ce dernier est declare en PREMIER.
StyleStore& defaultStore() {
    static sd::obsbridge::ObsFileStore fileStore;
    static StyleStore store(fileStore);
    return store;
}
} // namespace

LibraryResult loadLibrary() {
    return defaultStore().load();
}
SaveResult saveLibrary(const std::vector<sd::core::RhythmStyle>& styles) {
    return defaultStore().save(styles);
}

} // namespace sd::styles
