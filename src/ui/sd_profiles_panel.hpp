// StreamDirector — panneau "Profils" (couche UI, Run 7).
//
// Liste les profils (configurations nommees) avec, par ligne : icone, nom, resume
// auto (n sources · plan large), badge "Actif" ou bouton "Charger", et un menu "⋮"
// (Renommer / Dupliquer / Modifier / Supprimer). En haut a droite : bouton
// "+ Nouveau" (Avec assistant / Sans assistant). Maquette Pencil `oYypH`.
//
// Vit dans la fenetre Parametres avances (sd_settings) comme les autres panneaux
// (plus de pop-up : decision David, Run 7). Les actions de STRUCTURE du catalogue
// (renommer / dupliquer / supprimer) sont faites ICI (magasin de profils + rebuild).
// Les actions qui touchent la config en EDITION ou la fenetre (charger / modifier /
// nouveau) sont DELEGUEES a la fenetre via des callbacks (elle gere la garde
// anti-perte, le rechargement de l'editeur et le changement d'onglet).
//
// Pas de QObject : callbacks std::function ; widgets enfants rattaches au host.
// i18n via i18n() (jamais tr()).
#pragma once

#include <QPointer>
#include <QString>

#include <functional>

class QVBoxLayout;
class QWidget;

namespace sd::ui {

class ProfilesPanel {
public:
    // Callbacks remontants vers la fenetre (toutes peuvent etre nulles).
    struct Callbacks {
        std::function<void(int id)> onLoad;        // basculer le profil actif
        std::function<void(int id)> onEdit;        // charger + aller a l'onglet edition
        std::function<void()> onNewWithAssistant;  // creer un profil via l'assistant
        std::function<void()> onNewBlank;          // creer un profil vierge
    };

    ProfilesPanel(QWidget* dialogParent, Callbacks callbacks);

    // Monte le panneau dans `host` (le vide d'abord). Re-appelable.
    void mount(QVBoxLayout* host);

    // Reconstruit le contenu depuis le magasin de profils (apres une action).
    void rebuild();

private:
    QWidget* buildHeaderRow();
    QWidget* buildCard(int id, const QString& name, bool active, const QString& summary);
    void showRowMenu(int id, bool active, QWidget* anchor);

    // Actions internes (magasin + rebuild) — la garde/structure vit dans le coeur.
    void doRename(int id);
    void doDuplicate(int id);
    void doDelete(int id, const QString& name);

    QWidget* dialogParent_ = nullptr;
    Callbacks cb_;
    QPointer<QVBoxLayout> host_;
};

}  // namespace sd::ui
