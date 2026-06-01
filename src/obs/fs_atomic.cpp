// StreamDirector — pont OBS : ecriture de fichiers atomique (implementation).
#include "obs/fs_atomic.hpp"

#include <util/platform.h>

#include <string>

namespace sd::obsbridge {

bool ensureParentDir(const std::string& path) {
    const size_t sep = path.find_last_of("/\\");
    if (sep == std::string::npos) {
        return true;  // pas de composante dossier -> rien a creer
    }
    const std::string dir = path.substr(0, sep);
    if (dir.empty()) {
        return true;
    }
    // os_mkdirs cree toute l'arborescence et renvoie MKDIR_EXISTS si deja la.
    return os_mkdirs(dir.c_str()) != MKDIR_ERROR;
}

FsResult writeUtf8Atomic(const std::string& path, const std::string& text) {
    FsResult result;
    if (!ensureParentDir(path)) {
        result.error = "creation du dossier impossible";
        return result;
    }
    // tmp + bascule atomique (meme primitive qu'OBS pour ses collections de scenes).
    const std::string tmp = path + ".tmp";
    if (!os_quick_write_utf8_file(tmp.c_str(), text.c_str(), text.size(), false)) {
        result.error = "ecriture du fichier temporaire impossible";
        return result;
    }
    // 3e argument = chemin de SAUVEGARDE : os_safe_replace deplace l'ancien
    // contenu de `path` vers `path.bak` AVANT de le remplacer (ReplaceFileW sous
    // Windows, rename sous nix). `.bak` porte donc toujours l'avant-derniere
    // version valide -> filet de recuperation si le fichier courant devient
    // illisible. Au tout premier ecrit (fichier absent), pas de .bak (rien a
    // sauvegarder) : os_safe_replace bascule simplement le .tmp en place.
    const std::string bak = backupPath(path);
    if (os_safe_replace(path.c_str(), tmp.c_str(), bak.c_str()) != 0) {
        os_unlink(tmp.c_str());  // pas de .tmp orphelin si la bascule echoue
        result.error = "remplacement atomique du fichier impossible";
        return result;
    }
    result.ok = true;
    return result;
}

}  // namespace sd::obsbridge
