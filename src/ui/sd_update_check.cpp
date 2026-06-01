#include "ui/sd_update_check.hpp"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QUrl>

#include <nlohmann/json.hpp>

#include "core/version.hpp"
#include "obs/obs_file_store.hpp"

namespace sd::ui {

namespace {

// Source UNIQUE de l'identite du depot. Si le depot est renomme (nom definitif de
// l'app), changer ces deux lignes ici (et nulle part ailleurs). NB : GitHub redirige
// automatiquement les anciennes URLs apres un renommage, donc rien ne casse dans
// l'intervalle.
constexpr const char* kReleasesApiUrl =
    "https://api.github.com/repos/DavidClaudeAI/StreamDirector/releases/latest";
constexpr const char* kReleasesPageUrl =
    "https://github.com/DavidClaudeAI/StreamDirector/releases/latest";

// Reglage "verifier au demarrage" : fichier dans le dossier de config du plugin.
constexpr const char* kPrefsFile = "update.json";

}  // namespace

void checkForUpdate(QObject* ctx, const std::string& currentVersion,
                    std::function<void(const UpdateInfo&)> onResult) {
    auto* nam = new QNetworkAccessManager(ctx);

    QNetworkRequest request{QUrl(QString::fromUtf8(kReleasesApiUrl))};
    // GitHub exige un User-Agent, et on fige la version de l'API consommee.
    request.setRawHeader("User-Agent", "StreamDirector");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

    QNetworkReply* reply = nam->get(request);
    QObject::connect(reply, &QNetworkReply::finished, ctx,
                     [reply, nam, currentVersion, onResult = std::move(onResult)]() {
                         UpdateInfo info;
                         if (reply->error() == QNetworkReply::NoError) {
                             const std::string body = reply->readAll().toStdString();
                             try {
                                 const auto json = nlohmann::json::parse(body);
                                 const std::string tag = json.value("tag_name", std::string{});
                                 if (sd::core::isNewerVersion(tag, currentVersion)) {
                                     info.updateAvailable = true;
                                     info.latestVersion = tag;
                                     info.releaseUrl =
                                         json.value("html_url", std::string{kReleasesPageUrl});
                                 }
                             } catch (...) {
                                 // JSON inattendu -> on ne notifie pas (silencieux).
                             }
                         }
                         reply->deleteLater();
                         nam->deleteLater();
                         onResult(info);
                     });
}

bool updateCheckEnabled() {
    sd::obsbridge::ObsFileStore store;
    std::string text;
    if (!store.read(kPrefsFile, text)) return true;  // absent -> actif par defaut
    try {
        return nlohmann::json::parse(text).value("checkOnStartup", true);
    } catch (...) {
        return true;  // illisible -> on garde le defaut (actif)
    }
}

void setUpdateCheckEnabled(bool enabled) {
    sd::obsbridge::ObsFileStore store;
    nlohmann::json json;
    json["checkOnStartup"] = enabled;
    store.write(kPrefsFile, json.dump(2));  // best-effort : un echec disque est sans gravite
}

}  // namespace sd::ui
