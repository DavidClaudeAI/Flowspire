// StreamDirector — FileStore concret adosse a OBS (plugin uniquement).
// Resout les chemins relatifs sous %config-plugin% (obs_module_config_path) et
// fait l'IO via l'API fichier d'OBS (os_*), gere les chemins UTF-8 sous Windows.
// Ecriture atomique + .bak deleguee a fs_atomic. NON inclus dans la boucle de
// test rapide (depend d'OBS) : les tests utilisent FakeFileStore.
#pragma once

#include <string>
#include <vector>

#include "obs/file_store.hpp"

namespace sd::obsbridge {

class ObsFileStore final : public FileStore {
public:
    bool read(const std::string& relPath, std::string& out) const override;
    bool exists(const std::string& relPath) const override;
    FsResult write(const std::string& relPath, const std::string& text) override;
    bool remove(const std::string& relPath) override;
    std::vector<std::string> list(const std::string& relDir, const std::string& ext) const override;
};

} // namespace sd::obsbridge
