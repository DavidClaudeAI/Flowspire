// StreamDirector — dock du plugin (couche UI).
// Run 4 (design + i18n) : pilotage reel du Run 3 habille selon la maquette (theme
// sombre OBS, cf. sd_theme.hpp), icones lucide (sd_icons), vumetre custom a
// marqueur de seuil (sd_level_meter), et toutes les chaines via l'i18n native OBS
// (sd_i18n -> i18n(), PAS tr() qui entrerait en collision avec QObject::tr).
//
// Fonctionnel (Run 3) : un vumetre par intervenant, etat parle/silence, scene
// REELLEMENT a l'antenne, et — config chargee + auto actif — bascule effective
// des scenes OBS selon qui parle. GARDE-FOU : auto OFF par defaut.
//
// THREADS : tout sur le thread UI (timer Qt). Hotkeys OBS marshalees via
// runOnUiThread (postEvent).
#pragma once

#include <QWidget>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/director.hpp"
#include "obs/audio_level_monitor.hpp"
#include "obs/scene_switcher.hpp"

class QEvent;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;
class QVBoxLayout;

namespace sd::ui {

class LevelMeter;

class SdDock : public QWidget {
public:
    explicit SdDock(QWidget* parent = nullptr);
    ~SdDock() override;

    // Actions publiques (boutons du dock ET hotkeys OBS marshalees). No-op sans config.
    void setAuto(bool on);
    void toggleAuto();
    void forceWide();
    void forceSpeakerByIndex(int index);

    // Execute `fn` sur le thread UI du dock (thread-safe : postEvent + QEvent custom).
    void runOnUiThread(std::function<void()> fn);

    // Recharge config + sources + UI. Public car appele par le callback d'evenements
    // frontend OBS (auto-refresh quand OBS a fini de charger / change de collection).
    void reload();

protected:
    void customEvent(QEvent* event) override;
    // Rend le badge d'etat cliquable (toggle du pilotage auto) sans en faire un bouton.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // Une ligne d'intervenant dans l'UI (carte stylee de la maquette).
    struct Row {
        std::string id;           // id d'intervenant (cle cote Director)
        std::string audioSource;  // nom de la source audio OBS (cle cote monitor)
        QWidget* card = nullptr;  // carte (sa destruction detruit les enfants)
        QLabel* avatar = nullptr;     // icone user teintee selon l'etat
        QLabel* nameLabel = nullptr;
        LevelMeter* meter = nullptr;  // vumetre custom (+ marqueur de seuil)
        QSlider* threshold = nullptr; // slider de seuil par intervenant
        QLabel* stateLabel = nullptr;
        int shownDb = 1;          // hors plage [-60,0] -> force la 1ere maj
        int shownSpeaking = -1;   // -1 inconnu, 0 silence, 1 parle
    };

    // Un intervenant tel qu'affiche/pilote : mode config = speaker du JSON ;
    // mode auto = source audio (id == name == audioSource).
    struct DisplaySpeaker {
        std::string id;
        std::string name;
        std::string audioSource;
    };

    void tick();     // timer : lit l'audio, nourrit le coeur, pilote OBS, rafraichit
    // Met a jour le Speaker.thresholdDb de activeConfig_ pour `speakerId` (mode profil
    // seulement) -> prepare la prochaine ecriture, sans toucher le disque.
    void rememberSpeakerThreshold(const std::string& speakerId, int db);
    // Ecrit le profil actif sur disque (saveActive) si on est en mode profil. Best-effort.
    void saveActiveProfileNow();
    void openAssistant();  // bouton Assistant : demande un nom puis cree un profil guide
    // Ouvre l'assistant en mode CREATION d'un profil nomme (cree + active a la fin).
    void openAssistantWith(const QString& newProfileName);
    // Ouvre les parametres avances (modale) sur l'onglet `initialTab` (cf. SdSettings::Tab,
    // passe en int pour eviter d'inclure sd_settings.hpp ici). Recharge a la fermeture.
    void openSettings(int initialTab = 0);
    void updateProfileBar();  // rafraichit le nom du profil actif (selecteur du dock)
    void showProfileMenu();   // menu du selecteur : liste des profils + "Gerer..."
    void switchProfile(int id);  // bascule le profil actif (1 clic) puis recharge
    void applyDecision(const sd::core::Decision& decision, const std::string& currentOnAir);
    void rebuildDirectingGrid();
    void updateStatusBadge();
    void updateModeLabel();
    void updateAutoButtons();
    void startUpdateCheck();  // verif MAJ async (Qt Network) -> affiche le bandeau si dispo
    void styleSpeakerCard(Row& row, bool speaking);
    static double nowSeconds();

    sd::obsbridge::AudioLevelMonitor monitor_;
    sd::obsbridge::SceneSwitcher sceneSwitcher_;
    std::unique_ptr<sd::core::Director> director_;

    std::vector<DisplaySpeaker> displaySpeakers_;
    // Config du profil ACTIF telle que chargee (source de verite lue par le moteur).
    // Sert a persister le seuil par intervenant : on y met a jour Speaker.thresholdDb
    // puis on ecrit via saveActive. Rafraichie a chaque reload().
    sd::core::Config activeConfig_;
    bool configMode_ = false;
    bool autoEnabled_ = false;    // GARDE-FOU : pilotage auto OFF par defaut.

    QVBoxLayout* rowsLayout_ = nullptr;
    QVBoxLayout* directingLayout_ = nullptr;
    std::vector<Row> rows_;
    QPushButton* profileButton_ = nullptr;     // selecteur de profil (en-tete du dock)
    QLabel* profileNameLabel_ = nullptr;       // nom du profil actif (dans le selecteur)
    QPushButton* wideButton_ = nullptr;
    QPushButton* autoOnButton_ = nullptr;
    QPushButton* autoOffButton_ = nullptr;
    QLabel* statusDot_ = nullptr;
    QLabel* statusText_ = nullptr;
    QWidget* statusBadge_ = nullptr;
    QLabel* modeLabel_ = nullptr;
    QLabel* onAirLabel_ = nullptr;
    QLabel* emptyLabel_ = nullptr;
    QWidget* updateBanner_ = nullptr;          // bandeau "mise a jour dispo" (masque par defaut)
    QLabel* updateBannerLabel_ = nullptr;
    QPushButton* updateBannerButton_ = nullptr;
    QTimer* timer_ = nullptr;
    // Ecriture differee (debounce) du seuil regle au slider : un glissement souris ou
    // une rafale clavier/molette ne declenche qu'UNE ecriture disque, apres stabilisation.
    QTimer* thresholdSaveTimer_ = nullptr;
    int shownStatus_ = -1;
    std::string shownOnAir_ = "\x01";
};

}  // namespace sd::ui
