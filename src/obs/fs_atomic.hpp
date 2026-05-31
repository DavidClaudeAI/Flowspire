// StreamDirector — pont OBS : ecriture de fichiers ATOMIQUE (mutualisee).
// Extrait de config_loader : la meme primitive sert au config.json ET aux
// fichiers de profils. Ecrit un .tmp puis bascule via os_safe_replace (rename
// atomique sur le meme volume) -> jamais de fichier tronque si une coupure
// survient en cours d'ecriture. Couche "obs" : depend de l'API fichier d'OBS.
#pragma once

#include <string>

namespace sd::obsbridge {

// Resultat d'une operation fichier.
struct FsResult {
    bool ok = false;
    std::string error;
};

// Cree l'arborescence parente de `path` si besoin (no-op si elle existe).
// Renvoie false uniquement sur erreur dure de creation.
bool ensureParentDir(const std::string& path);

// Ecriture ATOMIQUE UTF-8 (sans BOM). Cree le dossier parent au besoin.
FsResult writeUtf8Atomic(const std::string& path, const std::string& text);

}  // namespace sd::obsbridge
