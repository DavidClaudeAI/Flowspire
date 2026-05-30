// StreamDirector — dock du plugin (couche UI).
// Run 3 (pilotage reel) : un vumetre par intervenant, l'etat parle/silence, la
// scene REELLEMENT a l'antenne, et — quand une config est chargee et que le
// pilotage auto est actif — la bascule effective des scenes OBS selon qui parle.
// Reutilise le coeur teste (sd::core::Director) alimente par les niveaux audio
// internes d'OBS (sd::obsbridge::AudioLevelMonitor) et pilote OBS via
// sd::obsbridge::SceneSwitcher.
//
// GARDE-FOU : le pilotage auto est OFF par defaut. Sans config.json, le dock
// reste en lecture seule (il ne touche jamais aux scenes).
//
// THREADS : tout se passe sur le thread UI (timer Qt). Les actions declenchees
// par les hotkeys OBS sont marshalees sur ce thread (cf. plugin-main.cpp), donc
// toggleAuto/forceWide/forceSpeakerByIndex s'executent toujours ici.
#pragma once

#include <QWidget>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/director.hpp"
#include "obs/audio_level_monitor.hpp"
#include "obs/scene_switcher.hpp"

class QCheckBox;
class QEvent;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;
class QVBoxLayout;

namespace sd::ui {

class SdDock : public QWidget {
public:
    explicit SdDock(QWidget* parent = nullptr);
    ~SdDock() override;

    // Actions publiques. Appelees depuis les boutons du dock ET depuis les
    // hotkeys OBS (marshalees sur le thread UI). Sans danger sans config : no-op.
    void setAuto(bool on);                  // active/desactive le pilotage auto
    void toggleAuto();                      // bascule l'etat du pilotage auto
    void forceWide();                       // force le plan large
    void forceSpeakerByIndex(int index);    // force le plan de l'intervenant #index

    // Execute `fn` sur le thread UI du dock. THREAD-SAFE : appelable depuis
    // n'importe quel thread (les callbacks de hotkey OBS). Implemente via
    // QCoreApplication::postEvent (API Qt stable, presente dans toutes les
    // versions 6.x) plutot que QMetaObject::invokeMethod(obj, lambda) qui exige
    // un symbole Qt 6.7+ ABSENT du Qt 6.6 embarque dans OBS 31.x -> sinon la DLL
    // ne se charge pas. Qt supprime les evenements postes a un objet detruit,
    // donc aucun risque d'execution sur un dock mort.
    void runOnUiThread(std::function<void()> fn);

protected:
    void customEvent(QEvent* event) override;  // execute les actions postees

private:
    struct Row {
        std::string id;           // id d'intervenant (cle cote Director)
        std::string audioSource;  // nom de la source audio OBS (cle cote monitor)
        QWidget* line = nullptr;  // conteneur de la ligne (detruit ses enfants)
        QProgressBar* bar = nullptr;
        QLabel* dbLabel = nullptr;
        QLabel* stateLabel = nullptr;
        // Dernier etat AFFICHE : evite de reecrire texte/style a chaque tick
        // (20 Hz) quand rien ne change -> pas de re-parse CSS / relayout inutile.
        int shownDb = 1;          // hors plage [-60,0] -> force la 1ere maj
        int shownSpeaking = -1;   // -1 = inconnu, 0 = silence, 1 = parle
    };

    // Un intervenant tel qu'affiche/pilote : en mode config = un speaker du
    // JSON ; en mode auto = une source audio (id == name == audioSource).
    struct DisplaySpeaker {
        std::string id;
        std::string name;
        std::string audioSource;
    };

    void reload();   // recharge la config + re-scanne les sources + reconstruit l'UI
    void tick();     // timer : lit l'audio, nourrit le coeur, pilote OBS, rafraichit
    void applyDecision(const sd::core::Decision& decision);  // bascule OBS si besoin
    void updateModeLabel();
    static double nowSeconds();

    sd::obsbridge::AudioLevelMonitor monitor_;
    sd::obsbridge::SceneSwitcher sceneSwitcher_;
    std::unique_ptr<sd::core::Director> director_;

    std::vector<DisplaySpeaker> displaySpeakers_;  // aligne avec rows_ (meme ordre)
    bool configMode_ = false;     // une config valide est-elle chargee ?
    bool autoEnabled_ = false;    // GARDE-FOU : pilotage auto OFF par defaut.

    QVBoxLayout* rowsLayout_ = nullptr;
    std::vector<Row> rows_;
    QCheckBox* autoCheck_ = nullptr;
    QPushButton* wideButton_ = nullptr;
    QLabel* modeLabel_ = nullptr;
    QLabel* onAirLabel_ = nullptr;    // scene REELLEMENT a l'antenne (OBS)
    QLabel* configLabel_ = nullptr;   // chemin + etat du config.json
    QLabel* emptyLabel_ = nullptr;
    QTimer* timer_ = nullptr;
    std::string shownOnAir_ = "\x01";  // sentinelle != tout nom -> 1ere maj forcee
};

}  // namespace sd::ui
