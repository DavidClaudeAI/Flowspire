// Flowspire — remontee du statut de la regie vers une application externe (couche UI,
// Qt Network).
//
// Pose une variable personnalisee dans Bitfocus Companion (ou toute appli exposant la
// meme API HTTP locale) via :
//   POST http://<host>:<port>/api/custom-variable/flowspire_active/value?value=<0|1>
// ASYNC NATIF : QNetworkAccessManager fait le POST sans bloquer ; aucun retour attendu.
// Best-effort : tout echec (cible injoignable, API absente) -> log seulement, jamais de
// crash ni d'impact sur le pilotage. HTTP en clair sur le reseau local -> AUCUN backend
// TLS requis (contrairement a la verification de mise a jour).
#pragma once

#include <string>

class QObject;

namespace sd::ui {

// Envoie l'etat de la regie (`active`) a l'appli cible. `ctx` = parent Qt (gestion de
// vie : passer le dock). No-op si `host` est vide ou `port` hors plage. N'attend pas de
// reponse (best-effort).
void pushRegieStatus(QObject* ctx, const std::string& host, int port, bool active);

} // namespace sd::ui
