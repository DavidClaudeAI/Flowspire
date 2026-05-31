// StreamDirector — fenetre Parametres avances (couche UI, Run 6).
//
// Fenetre modale (QDialog frameless) a SIDEBAR gauche (maquette Pencil `QbX5t`).
// Edite une config EXISTANTE en direct, panneau par panneau (Intervenants /
// Cameras & poids / Plan large / Rythme), la ou l'assistant (sd_assistant) cree
// une config de zero en parcours guide. Reutilise les memes briques d'UI
// partagees (sd_widgets : ClickButton, ComboField, SliderRow, helpers, QSS) ->
// fidelite visuelle a l'assistant, zero duplication.
//
// Entrees laterales "Profils" et "Soutenir" : Soutenir ouvre la pop-up de don
// (Run 6). Profils = fenetre dediee, livree au Run 7 (entree presente, branchee
// plus tard). A la fermeture (Termine), ecrit le config.json (saveConfig atomique)
// et l'appelant (le dock) recharge.
//
// THREADS : thread UI uniquement (modale ouverte depuis le dock). i18n via i18n()
// (jamais tr()). Style : jetons sd_theme + helpers sd_widgets.
#pragma once

#include <QDialog>

#include <memory>

class QMouseEvent;
class QShowEvent;

namespace sd::ui {

class SdSettings : public QDialog {
public:
    explicit SdSettings(QWidget* parent = nullptr);
    ~SdSettings() override;

    // Valide apres exec() == QDialog::Accepted : true si la config a ete
    // enregistree (bouton "Termine"). Le dock recharge alors sa config.
    bool savedConfig() const;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

}  // namespace sd::ui
