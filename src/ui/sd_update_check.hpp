// StreamDirector — verification de mise a jour (couche UI, Qt Network).
//
// Demande a l'API GitHub Releases la derniere version publiee, la compare a la version
// locale (PLUGIN_VERSION) via le coeur pur (core/version), et renvoie le verdict.
// ASYNC NATIF : QNetworkAccessManager fait le GET sans bloquer ; le callback est
// invoque sur le thread UI (pas de thread manuel ni de marshalling). Tout echec
// (hors-ligne, 404 si aucune release, JSON inattendu) -> "pas de mise a jour" en
// silence (jamais de fausse alerte, jamais de crash).
#pragma once

#include <functional>
#include <string>

class QObject;

namespace sd::ui {

struct UpdateInfo {
    bool updateAvailable = false;
    std::string latestVersion;  // ex. "0.2.0" (sans prefixe)
    std::string releaseUrl;     // page a ouvrir pour telecharger
};

// Lance la verification. `ctx` = parent Qt (gestion de vie : passer le dock). `onResult`
// est appele une fois, sur le thread UI. No-op cote appelant si le reseau echoue
// (onResult est alors appele avec updateAvailable == false).
//
// L'opt-out "verifier au demarrage" vit desormais dans les preferences globales
// (sd_prefs : GlobalPrefs::checkUpdatesOnStartup) ; l'appelant garde cette requete
// sous condition.
void checkForUpdate(QObject* ctx, const std::string& currentVersion,
                    std::function<void(const UpdateInfo&)> onResult);

}  // namespace sd::ui
