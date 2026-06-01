// StreamDirector — API libre du magasin de profils (plugin uniquement).
// Delegue a un ProfileStore par defaut adosse a un ObsFileStore (IO reelle OBS).
// Ce TU depend d'OBS (ObsFileStore) -> hors boucle de test rapide. La LOGIQUE
// testee vit dans profiles_store.cpp (OBS-free), exercee via FakeFileStore.
#include "obs/obs_file_store.hpp"
#include "obs/profiles_store.hpp"

namespace sd::profiles {

namespace {
// Magasin par defaut : ObsFileStore + ProfileStore en statiques locales (init
// thread-safe C++11 ; l'UI tourne sur le thread Qt). Le ProfileStore garde une
// reference sur le FileStore -> ce dernier est declare en PREMIER.
ProfileStore& defaultStore() {
    static sd::obsbridge::ObsFileStore fileStore;
    static ProfileStore store(fileStore);
    return store;
}
}  // namespace

ListResult loadList(const std::string& defaultName) {
    return defaultStore().loadList(defaultName);
}
ConfigResult loadProfile(int id) {
    return defaultStore().loadProfile(id);
}
ActiveConfigResult loadActiveConfig(const std::string& defaultName) {
    return defaultStore().loadActiveConfig(defaultName);
}
StoreResult setActive(int id) {
    return defaultStore().setActive(id);
}
StoreResult saveActive(const sd::core::Config& cfg) {
    return defaultStore().saveActive(cfg);
}
StoreResult createProfile(const std::string& name, const sd::core::Config& cfg, bool makeActive) {
    return defaultStore().createProfile(name, cfg, makeActive);
}
StoreResult duplicateProfile(int id, const std::string& copySuffix) {
    return defaultStore().duplicateProfile(id, copySuffix);
}
StoreResult renameProfile(int id, const std::string& name) {
    return defaultStore().renameProfile(id, name);
}
StoreResult removeProfile(int id) {
    return defaultStore().removeProfile(id);
}

}  // namespace sd::profiles
