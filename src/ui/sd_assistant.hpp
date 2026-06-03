// Flowspire — assistant de configuration visuel (couche UI, Run 5 + Run 7).
//
// Fenetre modale (QDialog frameless) : parcours guide en 6 ecrans (maquette Pencil :
// 0 Prerequis -> 5 Resume).
//
// Run 7 : l'assistant est une CREATION GUIDEE d'un NOUVEAU profil nomme. On lui passe
// le nom voulu (`newProfileName`) : il part d'une config VIERGE et, A LA FIN
// seulement (validation), cree le profil + l'active (ecrit <id>.json + index.json).
// -> le profil ne devient JAMAIS actif tant que l'assistant n'est pas valide (sinon
// le dock affichait un profil vide "actif" — retour David). Si `newProfileName` est
// vide (compat), il edite et enregistre le profil ACTIF.
//
// THREADS : thread UI uniquement (ouvert depuis le dock via exec()). Pendant la
// modale, OBS est bloque -> les listes de scenes/sources, lues une fois a
// l'ouverture (obs_inventory), restent valides.
//
// i18n : toutes les chaines via i18n() (sd_i18n), JAMAIS tr() (collision
// QObject::tr). Style : jetons sd_theme + helper rgba() (sd_style).
#pragma once

#include <QDialog>
#include <QString>

#include <memory>

class QMouseEvent;
class QShowEvent;

namespace sd::ui {

class SdAssistant : public QDialog {
public:
    // `newProfileName` non vide -> creation guidee d'un nouveau profil (cree + active
    // A LA FIN). Vide -> edite/enregistre le profil actif (compat).
    explicit SdAssistant(QWidget* parent = nullptr, const QString& newProfileName = QString());
    ~SdAssistant() override;

    // Valide uniquement apres exec() == QDialog::Accepted : true si l'utilisateur
    // a termine par "Activer la realisation auto" (le dock activera alors le
    // pilotage). false si l'assistant a juste enregistre sans demander l'activation.
    bool activateAutoRequested() const;

protected:
    // Fenetre sans cadre natif : on gere le deplacement par la barre de titre.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    // Centre la fenetre sur OBS au premier affichage.
    void showEvent(QShowEvent* event) override;

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace sd::ui
