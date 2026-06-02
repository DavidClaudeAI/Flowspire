// StreamDirector — fenetre Parametres avances (couche UI, Run 6 + Run 7).
//
// Fenetre modale (QDialog frameless) a SIDEBAR gauche (maquette Pencil `QbX5t`).
// Edite une config EXISTANTE en direct, panneau par panneau (Intervenants /
// Cameras & poids / Plan large / Rythme), la ou l'assistant (sd_assistant) cree
// une config de zero en parcours guide. Reutilise les memes briques d'UI
// partagees (sd_widgets) -> fidelite visuelle a l'assistant, zero duplication.
//
// Run 7 : "plus de pop-up sauf l'assistant" (decision David). Les entrees laterales
// "Profils" et "Soutenir" sont desormais de vrais PANNEAUX (sd_profiles_panel /
// sd_support), plus des pop-ups. Le dock peut ouvrir la fenetre directement sur un
// onglet precis (ex. Profils). Le modele d'enregistrement "tout-sur-Enregistrer"
// reste : seuls "Enregistrer et fermer" persiste la config d'edition (vers le
// profil ACTIF) ; la croix annule. Les actions de profils (charger/renommer/...)
// sont, elles, immediates.
//
// THREADS : thread UI uniquement. i18n via i18n() (jamais tr()).
#pragma once

#include <QDialog>
#include <QString>

#include <memory>

class QMouseEvent;
class QShowEvent;

namespace sd::ui {

class SdSettings : public QDialog {
public:
    // Onglet d'ouverture (le dock ouvre p.ex. directement sur Profils).
    // TabGeneral ajoute en fin -> aucune valeur existante ne change (le dock passe
    // ces constantes en int via openSettings).
    enum Tab { TabSpeakers, TabCameras, TabWide, TabRhythm, TabProfiles, TabSupport, TabGeneral };

    explicit SdSettings(QWidget* parent = nullptr, Tab initialTab = TabSpeakers);
    ~SdSettings() override;

    // true si le profil actif a change pendant la session (enregistrement OU action
    // de profil immediate : charger/nouveau) -> le dock doit recharger, meme si la
    // fenetre s'est fermee par la croix.
    bool savedConfig() const;

    // Non vide si l'utilisateur a choisi "Nouveau profil > Avec l'assistant" : le dock
    // doit alors ouvrir l'assistant en mode CREATION avec ce nom (le profil sera cree
    // + active a la fin de l'assistant, pas avant). Vide sinon.
    QString pendingAssistantName() const;

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
