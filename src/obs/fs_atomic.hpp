// Flowspire — pont OBS : ecriture de fichiers ATOMIQUE (mutualisee).
// Primitive partagee par tous les fichiers du plugin (index + profils). Ecrit un
// .tmp puis bascule via os_safe_replace (rename atomique sur le meme volume) ->
// jamais de fichier tronque si une coupure survient en cours d'ecriture, et
// conserve l'ancien contenu en `.bak`. Couche "obs" : depend de l'API fichier OBS.
#pragma once

#include <string>

namespace sd::obsbridge {

// Resultat d'une operation fichier.
struct FsResult {
    bool ok = false;
    std::string error;
};

// Chemin du fichier de sauvegarde cree par writeUtf8Atomic : os_safe_replace y
// deplace l'ancien contenu avant de le remplacer. Centralise ici (et non en
// litteral disperse) pour que les sites de RECUPERATION (profiles_store) et la
// simulation de test referencent la MEME convention -> pas de drift silencieux.
inline std::string backupPath(const std::string& path) {
    return path + ".bak";
}

// Cree l'arborescence parente de `path` si besoin (no-op si elle existe).
// Renvoie false uniquement sur erreur dure de creation.
bool ensureParentDir(const std::string& path);

// Ecriture ATOMIQUE UTF-8 (sans BOM). Cree le dossier parent au besoin. Conserve
// l'ancien contenu de `path` dans `path.bak` (filet de recuperation : cf. .cpp).
FsResult writeUtf8Atomic(const std::string& path, const std::string& text);

} // namespace sd::obsbridge
