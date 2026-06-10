#include "ui/sd_update_check.hpp"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QUrl>

#include <nlohmann/json.hpp>

#include "core/version.hpp"

namespace sd::ui {

namespace {

// Source UNIQUE de l'identite du depot. Si le depot est renomme (nom definitif de
// l'app), changer ces deux lignes ici (et nulle part ailleurs). NB : GitHub redirige
// automatiquement les anciennes URLs apres un renommage, donc rien ne casse dans
// l'intervalle.
constexpr const char* kReleasesApiUrl = "https://api.github.com/repos/DavidClaudeAI/Flowspire/releases/latest";
constexpr const char* kReleasesPageUrl = "https://github.com/DavidClaudeAI/Flowspire/releases/latest";

// Plafond de transfert : une connexion qui s'enlise (proxy casse, pare-feu DROP) ne doit pas
// laisser la requete pendre jusqu'a la destruction du dock. Sans impact fonctionnel (async).
constexpr int kNetworkTimeoutMs = 15000;

// L'URL ouverte au clic du bandeau "Mettre a jour" provient du JSON de l'API GitHub (champ
// html_url) : donnee NON maitrisee. Defense en profondeur derriere TLS. On ne garde l'URL que si
// TOUT est verifie, et on renvoie sa forme CANONIQUE (pas la chaine brute) :
//  - StrictMode + isValid : pas de nettoyage silencieux qui ferait diverger la forme validee de
//    la forme re-parsee a l'ouverture (faille de divergence de parseur) ;
//  - host + CHEMIN des releases du depot (pas tout github.com) : bloque un open-redirect ;
//  - reference (host + prefixe) derivee de kReleasesPageUrl -> un seul endroit a maintenir au
//    renommage du depot (cf. constantes ci-dessus).
// Sinon -> repli sur la page de releases (constante sure). Empeche d'ouvrir un schema dangereux
// (file://, javascript:...) ou un chemin github.com arbitraire si la reponse JSON est alteree.
std::string sanitizeReleaseUrl(const std::string& raw) {
    const QUrl ref(QString::fromUtf8(kReleasesPageUrl));
    const QString safePathPrefix = ref.path().section('/', 0, 2) + QStringLiteral("/releases");
    const QUrl url(QString::fromStdString(raw), QUrl::StrictMode);
    if (url.isValid() && url.scheme() == QStringLiteral("https") && url.host() == ref.host() &&
        url.path().startsWith(safePathPrefix)) {
        return url.toString(QUrl::FullyEncoded).toStdString();
    }
    return kReleasesPageUrl;
}

} // namespace

void checkForUpdate(QObject* ctx, const std::string& currentVersion, std::function<void(const UpdateInfo&)> onResult) {
    auto* nam = new QNetworkAccessManager(ctx);
    nam->setTransferTimeout(kNetworkTimeoutMs); // ne pas laisser une connexion bloquee pendre

    QNetworkRequest request{QUrl(QString::fromUtf8(kReleasesApiUrl))};
    // GitHub exige un User-Agent, et on fige la version de l'API consommee.
    request.setRawHeader("User-Agent", "Flowspire");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    // Suivre les redirections 301 : indispensable apres un renommage du depot (l'API
    // GitHub redirige l'ancienne URL). C'est le defaut en Qt 6, explicite ici pour
    // verrouiller l'intention "robuste au renommage" et survivre a un changement de defaut.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam->get(request);
    QObject::connect(
        reply, &QNetworkReply::finished, ctx, [reply, nam, currentVersion, onResult = std::move(onResult)]() {
            UpdateInfo info;
            if (reply->error() == QNetworkReply::NoError) {
                const std::string body = reply->readAll().toStdString();
                try {
                    const auto json = nlohmann::json::parse(body);
                    const std::string tag = json.value("tag_name", std::string{});
                    const auto parsed = sd::core::parseSemVer(tag);
                    if (parsed && sd::core::isNewerVersion(tag, currentVersion)) {
                        info.updateAvailable = true;
                        // Version normalisee "M.m.p" (sans prefixe 'v' eventuel) ->
                        // affichage coherent avec le label "Flowspire v%1".
                        info.latestVersion = std::to_string(parsed->major) + "." + std::to_string(parsed->minor) + "." +
                                             std::to_string(parsed->patch);
                        info.releaseUrl = sanitizeReleaseUrl(json.value("html_url", std::string{}));
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

} // namespace sd::ui
