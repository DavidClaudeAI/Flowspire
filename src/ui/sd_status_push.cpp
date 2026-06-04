#include "ui/sd_status_push.hpp"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include <util/base.h>      // LOG_WARNING
#include "plugin-support.h" // obs_log

namespace sd::ui {

namespace {
// Nom (brut) de la variable personnalisee a creer cote Companion. Source UNIQUE : si on
// le change, le mettre a jour ICI et dans le guide utilisateur (section integration).
constexpr const char* kCustomVariableName = "flowspire_active";
} // namespace

void pushRegieStatus(QObject* ctx, const std::string& host, int port, bool active) {
    if (host.empty() || port <= 0 || port > 65535) {
        return; // cible invalide -> on n'envoie rien (best-effort, jamais bloquant).
    }

    // POST http://<host>:<port>/api/custom-variable/flowspire_active/value?value=<0|1>
    // On construit l'URL via QUrl/QUrlQuery : encodage correct de l'hote et de la valeur.
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(QString::fromStdString(host));
    url.setPort(port);
    url.setPath(QStringLiteral("/api/custom-variable/%1/value").arg(QString::fromUtf8(kCustomVariableName)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("value"), active ? QStringLiteral("1") : QStringLiteral("0"));
    url.setQuery(query);

    // `host` vient d'une saisie utilisateur (≠ URL constante de la verif MAJ) : un hote mal
    // forme (espace, URL collee en entier, IPv6 sans crochets) donnerait une URL invalide ou
    // une mauvaise cible. On verifie AVANT d'emettre -> diagnostic clair plutot qu'un envoi
    // silencieux dans le vide.
    if (!url.isValid()) {
        obs_log(LOG_WARNING, "Flowspire: cible de statut invalide (host='%s', port=%d) -> non envoye", host.c_str(),
                port);
        return;
    }

    // ctx (le dock) possede le manager -> il est detruit avec lui ; aucune requete ne
    // survit au dock. La valeur est dans le query param, le corps du POST reste vide.
    auto* nam = new QNetworkAccessManager(ctx);
    QNetworkRequest request{url};
    request.setRawHeader("User-Agent", "Flowspire");

    QNetworkReply* reply = nam->post(request, QByteArray());
    QObject::connect(reply, &QNetworkReply::finished, ctx, [reply, nam]() {
        if (reply->error() != QNetworkReply::NoError) {
            // Best-effort : pas de pop-up ni d'impact pilotage, on trace seulement pour
            // le diagnostic (cible eteinte, mauvais port, API non joignable...).
            obs_log(LOG_WARNING, "Flowspire: envoi du statut a l'appli externe echoue: %s",
                    reply->errorString().toUtf8().constData());
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

} // namespace sd::ui
