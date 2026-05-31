// StreamDirector — panneaux d'edition de configuration partages (couche UI, Run 6).
//
// SOURCE UNIQUE des 4 panneaux qui editent une sd::core::Config :
//   - Intervenants (nom + source audio + corbeille + "Ajouter")
//   - Scenes (onglets-dossier par intervenant -> scenes + poids % + corbeille)
//   - Plan large & priorites (scene plan large + 2 groupes de poids)
//   - Rythme & sensibilite (timings + audio avance)
//
// Utilise par l'ASSISTANT (sd_assistant, parcours guide) ET les PARAMETRES AVANCES
// (sd_settings, edition directe) -> meme rendu, zero duplication (lecon review Run 5).
//
// Modele de cycle de vie : chaque mountX(host) memorise le layout hote et le
// remplit ; les actions internes (ajout/suppression/onglet) appellent un rebuild
// interne sur CE host. Re-montable a volonte (l'appelant peut clearLayout son hote
// et re-mount). Pas de QObject (callbacks std::function ; connexions Qt rattachees
// aux widgets enfants -> liberees avec eux). Edite la Config par REFERENCE fournie
// par l'appelant : les modifications sont visibles immediatement cote appelant.
#pragma once

#include <string>
#include <vector>

#include "core/config.hpp"

// Forward-declarations AU NIVEAU GLOBAL (pas dans sd::ui) : sinon `class QSlider`
// ecrit dans le namespace declarerait un faux `sd::ui::QSlider` incomplet qui
// masquerait le vrai ::QSlider pour tout le code de sd::ui (erreur C2027).
class QLabel;
class QSlider;
class QVBoxLayout;
class QWidget;

namespace sd::ui {

// Nettoie une config avant ecriture : retire les intervenants sans nom OU sans
// source audio, et les scenes vides ou de poids nul. Partage par l'assistant ET
// les parametres avances (meme politique d'enregistrement, zero duplication).
// NOTE (dette tracee, review Run 5) : ce nettoyage est SILENCIEUX ; la validation
// remontee a l'UI (marquer les champs incomplets) reste a faire.
sd::core::Config sanitizedConfig(const sd::core::Config& in);

class ConfigPanels {
public:
    // `cfg` est edite PAR REFERENCE (doit survivre a ce ConfigPanels). `audioSources`
    // et `scenes` = inventaire OBS pour les listes deroulantes (capture a l'ouverture).
    ConfigPanels(sd::core::Config& cfg, std::vector<std::string> audioSources,
                 std::vector<std::string> scenes);

    // Monte un panneau dans `host` (vide d'abord le host). Re-appelable.
    void mountSpeakers(QVBoxLayout* host);
    void mountCameras(QVBoxLayout* host);
    void mountWide(QVBoxLayout* host);
    void mountRhythm(QVBoxLayout* host);

private:
    // Construit le contenu (sans vider) — appeles par mountX et les rebuilds.
    void rebuildSpeakers();
    void rebuildCameras();
    QWidget* buildSpeakerCard(std::size_t idx);
    QWidget* buildSceneRow(int spk, std::size_t s);
    void recomputeSceneWeights();

    // Helpers config.
    sd::core::Speaker* findSpeaker(const std::string& id);
    std::string uniqueId();
    void addBlankSpeaker();
    void eraseSpeaker(const std::string& id);

    sd::core::Config& cfg_;
    std::vector<std::string> audioSources_;
    std::vector<std::string> scenes_;
    int idCounter_ = 0;
    int selSpeaker_ = 0;  // onglet actif du panneau Scenes

    QVBoxLayout* speakersHost_ = nullptr;
    QVBoxLayout* camerasHost_ = nullptr;

    // Recompute des % du panneau Scenes (alignes : meme index slider <-> badge).
    std::vector<QSlider*> sceneWeightSliders_;
    std::vector<QLabel*> sceneWeightBadges_;
};

}  // namespace sd::ui
