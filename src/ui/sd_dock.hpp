// Flowspire — dock du plugin (couche UI).
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
#include "core/threshold_calibration.hpp"
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
        QLabel* avatar = nullptr; // icone user teintee selon l'etat
        QLabel* nameLabel = nullptr;
        LevelMeter* meter = nullptr;         // vumetre custom (+ marqueur de seuil)
        QSlider* threshold = nullptr;        // slider de seuil par intervenant
        QPushButton* calibrateBtn = nullptr; // bouton "calibrer ce seuil" (icone = etat)
        QLabel* stateLabel = nullptr;
        QWidget* tally = nullptr; // temoin vertical "a l'antenne" (rouge), a gauche de la carte
        bool isWide = false;      // carte "Plan large" (pas un intervenant : ni audio ni vumetre)
        int shownDb = 1;          // hors plage [-60,0] -> force la 1ere maj
        int shownSpeaking = -1;   // -1 inconnu, 0 silence, 1 parle
        int shownOnAir = -1;      // -1 inconnu, 0 hors antenne, 1 a l'antenne (tally)
    };

    // Un intervenant tel qu'affiche/pilote : mode config = speaker du JSON ;
    // mode auto = source audio (id == name == audioSource).
    struct DisplaySpeaker {
        std::string id;
        std::string name;
        std::string audioSource;
    };

    void tick(); // timer : lit l'audio, nourrit le coeur, pilote OBS, rafraichit
    // Met a jour le Speaker.thresholdDb de activeConfig_ pour `speakerId` (mode profil
    // seulement) -> prepare la prochaine ecriture, sans toucher le disque.
    void rememberSpeakerThreshold(const std::string& speakerId, int db);
    // Ecrit le profil actif sur disque (saveActive) si on est en mode profil. Best-effort.
    void saveActiveProfileNow();
    void openAssistant(); // bouton Assistant : demande un nom puis cree un profil guide
    // Ouvre l'assistant en mode CREATION d'un profil nomme (cree + active a la fin).
    void openAssistantWith(const QString& newProfileName);
    // Ouvre les parametres avances (modale) sur l'onglet `initialTab` (cf. SdSettings::Tab,
    // passe en int pour eviter d'inclure sd_settings.hpp ici). Recharge a la fermeture.
    void openSettings(int initialTab = 0);
    void updateProfileBar();    // rafraichit le nom du profil actif (selecteur du dock)
    void showProfileMenu();     // menu du selecteur : liste des profils + "Gerer..."
    void switchProfile(int id); // bascule le profil actif (1 clic) puis recharge
    void updateStyleBar();      // rafraichit le nom du style actif + visibilite (selecteur dock)
    void showStyleMenu();       // menu groupe : styles fournis + separateur "Mes styles" + persos
    void applyDecision(const sd::core::Decision& decision, const std::string& currentOnAir);
    // Detecte et traite un changement de la scene programme NON initie par nous (clic
    // dans le dock natif d'OBS, hotkey OBS, transition externe). Appelee EN DEBUT de
    // tick, avant la decision auto : sinon le tick rebasculerait sur l'ancienne decision
    // et "ecraserait" le clic (clignotement). No-op si l'antenne est celle qu'on a
    // demandee (lastRequestedScene_), ou hors mode auto.
    void handleManualSceneChange(const std::string& onAir, double now);
    void updateStatusBadge();
    void updateModeLabel();
    void startUpdateCheck(); // verif MAJ async (Qt Network) -> affiche le bandeau si dispo
    // Remontee best-effort de l'etat regie (ON/OFF) vers une appli externe (Companion) si
    // l'option est activee. Lit le CACHE de prefs (rafraichi par reload) -> aucune I/O ici.
    // `force` (battement) outrepasse la dedup pour re-synchroniser une cible qui a redemarre ;
    // sans force, n'emet que si l'etat ou la cible a change. Async, sans impact sur le pilotage.
    void pushStatusIfEnabled(bool force = false);
    void styleSpeakerCard(Row& row, bool speaking);
    // --- Calibration auto du seuil (#3) ---
    void onCalibrateClicked(const std::string& speakerId);       // bouton carte : (re)demarre / annule
    void startCalibrationAll();                                  // bouton global : arme tout le monde
    void finishCalibration();                                    // "Terminer" : fige ce qui peut l'etre, ferme
    void updateCalibrateButton(Row& row);                        // icone selon l'etat (repos / en cours / cale)
    void updateCalibrationBanner();                              // libelle "N/M cales" + visibilite du bandeau
    void applyCalibratedThreshold(Row& row, double thresholdDb); // ecrit le seuil VIA le slider (reuse cablage)
    // Construit la carte d'accueil (etat sans config) dans rowsLayout_ : titre, rappel
    // des prerequis OBS, nombre d'entrees audio detectees, bouton "Lancer l'assistant".
    void addWelcomeCard(size_t audioInputCount);
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
    bool autoEnabled_ = false; // GARDE-FOU : pilotage auto OFF par defaut.

    // Derniere scene que NOUS avons demandee a l'antenne (auto ou forcage). Le tick
    // compare l'antenne reelle a ce repere : si elle differe, c'est un changement venu
    // du dock natif d'OBS (clic manuel) et non l'echo de notre propre bascule.
    std::string lastRequestedScene_;

    QVBoxLayout* rowsLayout_ = nullptr;
    std::vector<Row> rows_;
    QWidget* welcomeCard_ = nullptr;       // carte d'accueil (etat sans config), nullptr sinon
    QPushButton* profileButton_ = nullptr; // selecteur de profil (en-tete du dock)
    QLabel* profileNameLabel_ = nullptr;   // nom du profil actif (dans le selecteur)
    QWidget* styleBar_ = nullptr;          // barre "Realisation : [style v]" (masquee sans config)
    QPushButton* styleButton_ = nullptr;   // selecteur de style de realisation (en-tete du dock)
    QLabel* styleNameLabel_ = nullptr;     // nom du style actif (dans le selecteur)
    QLabel* scenesSectionLabel_ = nullptr; // en-tete "SCENES" (masque en etat d'accueil)
    QLabel* statusDot_ = nullptr;
    QLabel* statusText_ = nullptr;
    QWidget* statusBadge_ = nullptr;
    QLabel* modeLabel_ = nullptr;
    QWidget* updateBanner_ = nullptr; // bandeau "mise a jour dispo" (masque par defaut)
    QLabel* updateBannerLabel_ = nullptr;
    QPushButton* updateBannerButton_ = nullptr;

    // --- Calibration auto du seuil (#3) ---
    // Un calibrateur par intervenant en cours OU cale dans la session courante (presence
    // dans la map = pas au repos). Vide = repos. La calibration ne fait qu'OBSERVER les
    // niveaux deja lus par le tick ; a "cale" -> on ecrit le seuil via le slider (reuse).
    std::map<std::string, sd::core::ThresholdCalibrator> calibrators_;
    bool calibrationBannerActive_ = false;           // session GLOBALE en cours -> bandeau affiche
    QPushButton* calibrateAllButton_ = nullptr;      // "Calibrer tous les seuils" (en-tete SCENES)
    QWidget* calibrationBanner_ = nullptr;           // bandeau de progression (masque par defaut)
    QLabel* calibrationBannerLabel_ = nullptr;       // "Calibration... N/M cales"
    QPushButton* calibrationFinishButton_ = nullptr; // "Terminer"
    QTimer* timer_ = nullptr;
    // Ecriture differee (debounce) du seuil regle au slider : un glissement souris ou
    // une rafale clavier/molette ne declenche qu'UNE ecriture disque, apres stabilisation.
    QTimer* thresholdSaveTimer_ = nullptr;
    // Battement de re-synchro du statut vers l'appli externe (Companion). Toujours actif ;
    // pushStatusIfEnabled ne fait rien (et ne lit AUCUN fichier) tant que l'option est OFF.
    QTimer* statusPushTimer_ = nullptr;
    // Cache des prefs de remontee de statut, rafraichi dans reload() (seul moment ou elles
    // peuvent changer : la fenetre Parametres rejoue reload a sa fermeture). Evite de lire
    // prefs.json a chaque battement / bascule.
    bool statusPushEnabled_ = false;
    std::string statusPushHost_;
    int statusPushPort_ = 0;
    // Dernier statut effectivement envoye (dedup) : -1 = jamais. Bloque les POST redondants
    // quand reload() est rejoue sans changement d'etat ni de cible.
    int lastPushedActive_ = -1;
    std::string lastPushedHost_;
    int lastPushedPort_ = 0;
    int shownStatus_ = -1;
};

} // namespace sd::ui
