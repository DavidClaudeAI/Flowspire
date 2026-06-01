// StreamDirector — ObsFileStore (implementation OBS).
#include "obs/obs_file_store.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <string>
#include <vector>

#include "obs/fs_atomic.hpp"

namespace sd::obsbridge {

namespace {
// Resout %config-plugin%/<rel> (chemin malloc'd par OBS -> a liberer). Vide si
// la resolution echoue.
std::string resolve(const std::string& rel) {
    std::string out;
    char* p = obs_module_config_path(rel.c_str());
    if (p) {
        out = p;
        bfree(p);
    }
    return out;
}
}  // namespace

bool ObsFileStore::read(const std::string& relPath, std::string& out) const {
    const std::string path = resolve(relPath);
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

bool ObsFileStore::exists(const std::string& relPath) const {
    const std::string path = resolve(relPath);
    return !path.empty() && os_file_exists(path.c_str());
}

FsResult ObsFileStore::write(const std::string& relPath, const std::string& text) {
    const std::string path = resolve(relPath);
    if (path.empty()) {
        return {false, "chemin introuvable : " + relPath};
    }
    return writeUtf8Atomic(path, text);
}

bool ObsFileStore::remove(const std::string& relPath) {
    const std::string path = resolve(relPath);
    if (path.empty()) {
        return false;
    }
    os_unlink(path.c_str());
    return !os_file_exists(path.c_str());
}

std::vector<std::string> ObsFileStore::list(const std::string& relDir,
                                            const std::string& ext) const {
    std::vector<std::string> names;
    const std::string dir = resolve(relDir);
    if (dir.empty()) {
        return names;
    }
    os_dir_t* d = os_opendir(dir.c_str());
    if (!d) {
        return names;  // dossier absent : liste vide (pas une erreur)
    }
    for (struct os_dirent* ent = os_readdir(d); ent != nullptr; ent = os_readdir(d)) {
        if (ent->directory) {
            continue;
        }
        const std::string name = ent->d_name;
        if (name.size() >= ext.size() &&
            name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
            names.push_back(name);
        }
    }
    os_closedir(d);
    return names;
}

}  // namespace sd::obsbridge
