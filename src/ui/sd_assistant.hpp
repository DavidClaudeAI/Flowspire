// StreamDirector — assistant de configuration visuel (couche UI, Run 5).
//
// Fenetre modale (QDialog frameless) qui remplace l'edition manuelle du
// config.json par un parcours guide en 6 ecrans (maquette Pencil : 0 Prerequis
// -> 5 Resume). A la fin, l'assistant ECRIT le config.json (sd::obsbridge::
// saveConfig) puis l'appelant (le dock) recharge et — si demande — active le
// pilotage auto.
//
// THREADS : thread UI uniquement (ouvert depuis le dock via exec()). Pendant la
// modale, OBS est bloque -> les listes de scenes/sources, lues une fois a
// l'ouverture (obs_inventory), restent valides.
//
// i18n : toutes les chaines via i18n() (sd_i18n), JAMAIS tr() (collision
// QObject::tr). Style : jetons sd_theme + helper rgba() (sd_style).
#pragma once

#include <QDialog>

#include <memory>

class QMouseEvent;
class QShowEvent;

namespace sd::ui {

class SdAssistant : public QDialog {
public:
    explicit SdAssistant(QWidget* parent = nullptr);
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

}  // namespace sd::ui
