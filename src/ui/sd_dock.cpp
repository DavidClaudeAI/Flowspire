#include "ui/sd_dock.hpp"

#include <obs-frontend-api.h>

#include <QAction>
#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPoint>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <unordered_set>
#include <utility>

#include "plugin-support.h"       // PLUGIN_VERSION (derive de buildspec.json par CMake)
#include "core/audio_util.hpp"    // source de verite du plancher (kDbFloor)
#include "core/profiles.hpp"      // catalogue de profils (modele pur)
#include "core/rhythm_style.hpp"  // styles de realisation (built-ins, applyRhythmStyle)
#include "obs/profiles_store.hpp" // magasin de profils (profil actif = source de verite)
#include "obs/style_store.hpp"    // bibliotheque globale de styles (sd::styles::loadLibrary)
#include "ui/sd_assistant.hpp"    // assistant de configuration (modale)
#include "ui/sd_prefs.hpp"        // preferences globales (verif MAJ, scene hors regie)
#include "ui/sd_settings.hpp"     // parametres avances (modale)
#include "ui/sd_status_push.hpp"  // remontee du statut regie vers une appli externe (Companion)
#include "ui/sd_update_check.hpp" // verification de mise a jour (Qt Network)
#include "ui/sd_i18n.hpp"         // i18n native OBS (i18n)
#include "ui/sd_icons.hpp"        // icones lucide teintees
#include "ui/sd_level_meter.hpp"  // vumetre custom + marqueur de seuil
#include "ui/sd_runtime.hpp"      // kTickMs (cadence partagee dock <-> assistant)
#include "ui/sd_style.hpp"        // rgba() (helper QSS partage)
#include "ui/sd_theme.hpp"        // jetons de design (couleurs/typo)
#include "ui/sd_widgets.hpp"      // briques d'UI partagees (makeCard, primaryBtnQss)

namespace sd::ui {

namespace th = sd::ui::theme;

namespace {
constexpr int kFloorDb = static_cast<int>(sd::core::kDbFloor);
// Delai d'ecriture differee du seuil regle au slider (debounce) : couvre une rafale
// clavier/molette. Le relachement souris, lui, ecrit immediatement.
constexpr int kThresholdSaveDebounceMs = 500;

// Cadence du rappel periodique de statut vers l'appli externe (Companion). Le statut
// est aussi pousse a chaque changement d'etat et au (re)chargement ; ce battement sert
// uniquement de re-synchro douce si la cible a redemarre/raté un envoi (best-effort).
constexpr int kStatusHeartbeatMs = 15000;

int dbToInt(double db) {
    int v = static_cast<int>(std::lround(db));
    if (v < kFloorDb) {
        v = kFloorDb;
    } else if (v > 0) {
        v = 0;
    }
    return v;
}

enum Status { StatusReadOnly = 0, StatusPaused = 1, StatusActive = 2 };

// rgba() (conversion hex #RRGGBBAA -> rgba() pour QSS) est desormais partage avec
// l'assistant -> voir ui/sd_style.hpp.

const QEvent::Type kActionEventType = static_cast<QEvent::Type>(QEvent::registerEventType());

class ActionEvent : public QEvent {
public:
    explicit ActionEvent(std::function<void()> fn) : QEvent(kActionEventType), fn_(std::move(fn)) {}
    void run() {
        if (fn_) {
            fn_();
        }
    }

private:
    std::function<void()> fn_;
};

// Callback d'evenements frontend OBS. `data` = le dock. On rafraichit AUTOMATIQUEMENT
// aux moments ou la liste des scenes/sources devient valide :
// - FINISHED_LOADING : OBS a fini de charger au demarrage (le dock, lui, est cree plus
//   tot en post_load -> sans ca, la liste serait vide jusqu'a un rechargement).
// - SCENE_COLLECTION_CHANGED : on bascule sur une autre collection (tout change).
// On ne rafraichit PAS sur chaque ajout/suppression de source : un reload recree le
// Director (reset des reglages live) -> on evite de perturber une regie en cours.
//
// Le FORCAGE par clic dans le dock natif d'OBS n'est PAS gere ici (via un evenement
// SCENE_CHANGED) : ce serait un 2e chemin concurrent avec le tick (50 ms) -> race et
// clignotement. Le tick lui-meme detecte le changement manuel (handleManualSceneChange),
// AVANT sa decision -> source de verite unique, pas de combat de bascule.
void frontendEventCb(enum obs_frontend_event event, void* data) {
    if (event != OBS_FRONTEND_EVENT_FINISHED_LOADING && event != OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
        return;
    }
    auto* dock = static_cast<SdDock*>(data);
    if (dock) {
        dock->runOnUiThread([dock]() { dock->reload(); });
    }
}

// Label de section (INTERVENANTS / PILOTAGE).
QString sectionLabelQss() {
    return QString("color:%1; font-weight:700; letter-spacing:1px; font-size:%2px;")
        .arg(th::kTextTertiary)
        .arg(th::kFontSection);
}

// Feuille de style globale du dock. Seuls les selecteurs cibles -> pas de cascade
// de bordures sur les widgets enfants (#FlowspireDock matche le seul fond).
QString dockQss() {
    return QString("QWidget#FlowspireDock { background:%1; }"
                   "QToolTip { color:%2; background:%3; border:1px solid %4; padding:4px; }")
        .arg(th::kSurface1)
        .arg(th::kTextPrimary)
        .arg(th::kSurface2)
        .arg(th::kBorder);
}

// Slider de seuil. handle clair ; bordure verte quand l'intervenant parle.
QString sliderQss(bool active) {
    return QString("QSlider::groove:horizontal { height:%1px; background:%2; border-radius:3px; }"
                   "QSlider::sub-page:horizontal { height:%1px; background:%2; border-radius:3px; }"
                   "QSlider::add-page:horizontal { height:%1px; background:%2; border-radius:3px; }"
                   "QSlider::handle:horizontal { width:12px; height:12px; margin:-4px 0;"
                   " border-radius:6px; background:%3; border:2px solid %4; }")
        .arg(th::kMeterHeight)
        .arg(th::kSurface3)
        .arg(th::kTextPrimary)
        .arg(active ? QString::fromUtf8(th::kSuccess) : rgba(th::kBorder, 1.0));
}

// Temoin "tally" : petite pilule verticale a gauche de la carte. Rouge = cette scene
// est A L'ANTENNE (convention broadcast) ; transparente sinon (la pilule reste dans le
// layout -> pas de saut de mise en page quand elle s'allume). Plus courte que la carte
// et separee de la bordure (espace de chaque cote) pour ne pas chevaucher le contour.
QString tallyQss(bool onAir) {
    return QString("#tally { background:%1; border-radius:2px; }")
        .arg(onAir ? QString::fromUtf8(th::kDanger) : QStringLiteral("transparent"));
}

// Cree la pilule "tally" (temoin a l'antenne) et l'ajoute a gauche de `lay`, centree.
// SOURCE UNIQUE de sa geometrie : partagee par la carte intervenant ET la carte plan
// large -> pas de divergence visuelle silencieuse si on retouche la pilule.
QWidget* makeTally(QHBoxLayout* lay) {
    auto* tally = new QWidget();
    tally->setObjectName(QStringLiteral("tally"));
    tally->setFixedSize(4, 26);
    tally->setStyleSheet(tallyQss(false));
    lay->addWidget(tally, 0, Qt::AlignVCenter);
    return tally;
}

// Rend une carte cliquable pour le forcage : filtre d'evenements (le clic non consomme par un
// enfant interactif remonte jusqu'a la carte), curseur "main", infobulle. Le forcage lui-meme
// est resolu dans SdDock::eventFilter (qui retrouve la carte dans rows_). `filter` = le SdDock.
void makeCardClickable(QWidget* card, QObject* filter, const QString& tooltip) {
    card->installEventFilter(filter);
    card->setCursor(Qt::PointingHandCursor);
    card->setToolTip(tooltip);
}
} // namespace

double SdDock::nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

SdDock::SdDock(QWidget* parent) : QWidget(parent) {
    namespace w = sd::ui::widgets; // briques d'UI partagees (QSS de boutons, etc.)
    setStyleSheet(dockQss());

    // Zone scrollable : quand le dock est reduit en hauteur, on fait DEFILER le
    // contenu au lieu de l'ecraser (sinon le bas de chaque carte est coupe).
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background:transparent; border:none; }"));
    auto* content = new QWidget();
    content->setObjectName(QStringLiteral("FlowspireDock")); // cible du fond (dockQss)
    scroll->setWidget(content);
    outer->addWidget(scroll);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(14);

    // --- Header : icone clapperboard + marque + badge d'etat ---
    auto* header = new QHBoxLayout();
    header->setSpacing(8);
    // Marque : logo "Flowspire" (wordmark SVG) a la place de l'ancien clap + texte.
    // Le SVG est cadre tres serre -> petite marge a gauche pour aerer (demande David).
    auto* brandLogo = new QLabel();
    brandLogo->setPixmap(logoFlowspire(20));
    brandLogo->setContentsMargins(6, 0, 0, 0);
    header->addWidget(brandLogo);
    header->addStretch();

    statusBadge_ = new QWidget();
    statusBadge_->setObjectName(QStringLiteral("statusBadge"));
    auto* badgeLay = new QHBoxLayout(statusBadge_);
    // Badge agrandi (surtout en hauteur) : c'est desormais LE controle du dock
    // (activer/couper la regie), la section "Realisation" ayant ete retiree.
    badgeLay->setContentsMargins(14, 8, 14, 8);
    badgeLay->setSpacing(7);
    statusDot_ = new QLabel(QStringLiteral("●"));
    statusText_ = new QLabel();
    badgeLay->addWidget(statusDot_);
    badgeLay->addWidget(statusText_);
    // Badge cliquable : c'est le toggle du pilotage auto (ON/OFF). Les memes actions
    // restent accessibles par les hotkeys OBS (clavier / Stream Deck / Companion).
    // Le curseur et l'infobulle sont poses par updateStatusBadge selon l'etat : en
    // lecture seule (aucune config) le badge n'est PAS un interrupteur.
    statusBadge_->installEventFilter(this);
    header->addWidget(statusBadge_);
    root->addLayout(header);

    // --- Bandeau "mise a jour disponible" (masque ; revele par startUpdateCheck) ---
    updateBanner_ = new QWidget();
    updateBanner_->setObjectName(QStringLiteral("updateBanner"));
    // rgba() (helper QSS partage) : Qt lit un hex 8 chiffres comme #AARRGGBB -> il NE faut
    // PAS passer les jetons translucides (#RRGGBBAA) tels quels (cf. statusBadge_).
    // Selecteur #updateBanner : sinon la bordure CASCADE sur le label/bouton enfants
    // (doubles lignes parasites) -> meme piege que #FlowspireDock / #spkCard.
    updateBanner_->setStyleSheet(QString("#updateBanner { background:%1; border:1px solid %2; border-radius:%3px; }")
                                     .arg(rgba(th::kAccent, 0.15))
                                     .arg(rgba(th::kAccent, 0.5))
                                     .arg(th::kRadiusButton));
    auto* updateLay = new QHBoxLayout(updateBanner_);
    updateLay->setContentsMargins(10, 6, 10, 6);
    updateLay->setSpacing(8);
    updateBannerLabel_ = new QLabel();
    updateBannerLabel_->setWordWrap(true);
    updateBannerLabel_->setStyleSheet(
        QString("color:%1; font-size:%2px; background:transparent;").arg(th::kTextPrimary).arg(th::kFontLabel));
    updateBannerButton_ = new QPushButton(i18n("Update.Get"));
    updateBannerButton_->setCursor(Qt::PointingHandCursor);
    updateBannerButton_->setStyleSheet(w::accentOutlineBtnQss(QStringLiteral("5px 10px")));
    updateLay->addWidget(updateBannerLabel_, 1);
    updateLay->addWidget(updateBannerButton_);
    updateBanner_->setVisible(false);
    root->addWidget(updateBanner_);

    // --- Etat de la realisation (AU-DESSUS des profils, demande David) : statut auto on/off,
    // fin et colore (vert actif / orange en pause), a la taille du titre "Realisation" (kFontLabel). ---
    modeLabel_ = new QLabel();
    modeLabel_->setWordWrap(true);
    root->addWidget(modeLabel_);

    // --- Selecteur de profil (Run 7) : "Profil : [ nom v ] [icone]" ---
    // Bascule du profil actif en 1 clic (le select) + icone qui ouvre les
    // parametres avances directement sur l'onglet Profils (demande David).
    auto* profileBar = new QHBoxLayout();
    profileBar->setSpacing(8);
    auto* profileLabel = new QLabel(i18n("Dock.Profile"));
    profileLabel->setStyleSheet(QString("color:%1; font-size:%2px;").arg(th::kTextTertiary).arg(th::kFontLabel));
    // Bouton-select : nom a GAUCHE + chevron a DROITE (layout interne ; labels
    // transparents a la souris -> le clic remonte au bouton). text-align ne suffit pas
    // a coller le chevron a droite, d'ou ce layout (retour David : chevron a droite).
    profileButton_ = new QPushButton();
    profileButton_->setCursor(Qt::PointingHandCursor);
    profileButton_->setStyleSheet(QString("QPushButton { background:%1; border:1px solid %2; border-radius:%3px; }"
                                          "QPushButton:hover { border-color:%4; }")
                                      .arg(th::kSurface3)
                                      .arg(th::kBorder)
                                      .arg(th::kRadiusButton)
                                      .arg(rgba(th::kAccent, 0.6)));
    auto* pbLay = new QHBoxLayout(profileButton_);
    pbLay->setContentsMargins(10, 7, 8, 7);
    pbLay->setSpacing(8);
    profileNameLabel_ = new QLabel();
    profileNameLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    profileNameLabel_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:600; background:transparent;").arg(th::kTextPrimary));
    auto* pbChevron = new QLabel();
    pbChevron->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    pbChevron->setPixmap(icon(Icon::ChevronDown, th::kTextSecondary, 14));
    pbLay->addWidget(profileNameLabel_);
    pbLay->addStretch();
    pbLay->addWidget(pbChevron);
    connect(profileButton_, &QPushButton::clicked, this, [this]() { showProfileMenu(); });
    auto* profileEdit = new QPushButton();
    profileEdit->setIcon(icon(Icon::Settings, th::kAccent, 14));
    profileEdit->setCursor(Qt::PointingHandCursor);
    profileEdit->setToolTip(i18n("Settings.Nav.Profiles"));
    profileEdit->setStyleSheet(QString("QPushButton { background:%1; border:1px solid %2; border-radius:%3px;"
                                       " padding:7px 9px; }"
                                       "QPushButton:hover { border-color:%4; }")
                                   .arg(th::kSurface3)
                                   .arg(rgba(th::kAccent, 0.4))
                                   .arg(th::kRadiusButton)
                                   .arg(th::kAccent));
    connect(profileEdit, &QPushButton::clicked, this, [this]() { openSettings(SdSettings::TabProfiles); });
    profileBar->addWidget(profileLabel);
    profileBar->addWidget(profileButton_, 1);
    profileBar->addWidget(profileEdit);
    root->addLayout(profileBar);

    // --- Selecteur de STYLE de realisation (Etape 4) : "Realisation : [ style v ]" ---
    // Change le style EN DIRECT (applique + sauve le profil actif + recharge le moteur).
    // Menu groupe : styles FOURNIS (Chill/Cool/Speed) + separateur "Mes styles" + persos.
    // Masque en etat d'accueil (pas de style a regler sans config). Calque le selecteur de profil.
    styleBar_ = new QWidget();
    auto* styleBarLay = new QHBoxLayout(styleBar_);
    styleBarLay->setContentsMargins(0, 0, 0, 0);
    styleBarLay->setSpacing(8);
    auto* styleLabel = new QLabel(i18n("Dock.Style"));
    styleLabel->setStyleSheet(QString("color:%1; font-size:%2px;").arg(th::kTextTertiary).arg(th::kFontLabel));
    styleButton_ = new QPushButton();
    styleButton_->setCursor(Qt::PointingHandCursor);
    styleButton_->setStyleSheet(QString("QPushButton { background:%1; border:1px solid %2; border-radius:%3px; }"
                                        "QPushButton:hover { border-color:%4; }")
                                    .arg(th::kSurface3)
                                    .arg(th::kBorder)
                                    .arg(th::kRadiusButton)
                                    .arg(rgba(th::kAccent, 0.6)));
    auto* sbLay = new QHBoxLayout(styleButton_);
    sbLay->setContentsMargins(10, 7, 8, 7);
    sbLay->setSpacing(8);
    styleNameLabel_ = new QLabel();
    styleNameLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    styleNameLabel_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:600; background:transparent;").arg(th::kTextPrimary));
    auto* sbChevron = new QLabel();
    sbChevron->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    sbChevron->setPixmap(icon(Icon::ChevronDown, th::kTextSecondary, 14));
    sbLay->addWidget(styleNameLabel_);
    sbLay->addStretch();
    sbLay->addWidget(sbChevron);
    connect(styleButton_, &QPushButton::clicked, this, [this]() { showStyleMenu(); });
    // Icone d'acces direct a la gestion (comme le profil) : ouvre les reglages sur l'onglet
    // "Realisation" (gerer les styles, ajuster les curseurs).
    auto* styleEdit = new QPushButton();
    styleEdit->setIcon(icon(Icon::Settings, th::kAccent, 14));
    styleEdit->setCursor(Qt::PointingHandCursor);
    styleEdit->setToolTip(i18n("Settings.Nav.Rhythm"));
    styleEdit->setStyleSheet(QString("QPushButton { background:%1; border:1px solid %2; border-radius:%3px;"
                                     " padding:7px 9px; }"
                                     "QPushButton:hover { border-color:%4; }")
                                 .arg(th::kSurface3)
                                 .arg(rgba(th::kAccent, 0.4))
                                 .arg(th::kRadiusButton)
                                 .arg(th::kAccent));
    connect(styleEdit, &QPushButton::clicked, this, [this]() { openSettings(SdSettings::TabRhythm); });
    styleBarLay->addWidget(styleLabel);
    styleBarLay->addWidget(styleButton_, 1);
    styleBarLay->addWidget(styleEdit);
    root->addWidget(styleBar_);

    // --- Section scenes (intervenants + plan large) ---
    // En-tete "SCENES" a gauche + bouton "Calibrer tous les seuils" (#3) a droite.
    auto* scenesHeader = new QHBoxLayout();
    scenesHeader->setSpacing(8);
    scenesSectionLabel_ = new QLabel(i18n("Section.Scenes"));
    scenesSectionLabel_->setStyleSheet(sectionLabelQss());
    calibrateAllButton_ = new QPushButton(i18n("Calibrate.All"));
    calibrateAllButton_->setIcon(icon(Icon::Wand, th::kAccent, 13));
    calibrateAllButton_->setCursor(Qt::PointingHandCursor);
    calibrateAllButton_->setStyleSheet(w::accentOutlineBtnQss(QStringLiteral("4px 9px")));
    connect(calibrateAllButton_, &QPushButton::clicked, this, [this]() { startCalibrationAll(); });
    scenesHeader->addWidget(scenesSectionLabel_);
    scenesHeader->addStretch();
    scenesHeader->addWidget(calibrateAllButton_);
    root->addLayout(scenesHeader);

    // Bandeau de progression de la calibration globale (masque ; revele par startCalibrationAll).
    // Reutilise le langage visuel du bandeau "MAJ dispo" (#updateBanner).
    calibrationBanner_ = new QWidget();
    calibrationBanner_->setObjectName(QStringLiteral("calibrationBanner"));
    calibrationBanner_->setStyleSheet(
        QString("#calibrationBanner { background:%1; border:1px solid %2; border-radius:%3px; }")
            .arg(rgba(th::kAccent, 0.15))
            .arg(rgba(th::kAccent, 0.5))
            .arg(th::kRadiusButton));
    auto* calBanLay = new QHBoxLayout(calibrationBanner_);
    calBanLay->setContentsMargins(10, 6, 10, 6);
    calBanLay->setSpacing(8);
    calibrationBannerLabel_ = new QLabel();
    calibrationBannerLabel_->setWordWrap(true);
    calibrationBannerLabel_->setStyleSheet(
        QString("color:%1; font-size:%2px; background:transparent;").arg(th::kTextPrimary).arg(th::kFontLabel));
    calibrationFinishButton_ = new QPushButton(i18n("Calibrate.Finish"));
    calibrationFinishButton_->setCursor(Qt::PointingHandCursor);
    calibrationFinishButton_->setStyleSheet(w::accentOutlineBtnQss(QStringLiteral("5px 10px")));
    connect(calibrationFinishButton_, &QPushButton::clicked, this, [this]() { finishCalibration(); });
    calBanLay->addWidget(calibrationBannerLabel_, 1);
    calBanLay->addWidget(calibrationFinishButton_);
    calibrationBanner_->setVisible(false);
    root->addWidget(calibrationBanner_);

    rowsLayout_ = new QVBoxLayout();
    rowsLayout_->setSpacing(8);
    root->addLayout(rowsLayout_);

    // La scene a l'antenne n'est plus affichee en TEXTE ici (doublon avec le
    // gestionnaire de scenes natif d'OBS qui la surligne deja). A la place, un temoin
    // vertical ("tally") sur la carte de l'intervenant a l'antenne (cf. reload + tick).
    //
    // La section "Realisation" (boutons Forcer / Plan large / Auto ON-OFF) a ete
    // retiree : on pilote desormais via le gestionnaire de scenes natif d'OBS et les
    // hotkeys (clavier / Stream Deck / Companion). L'activation/coupure de la regie
    // se fait par le badge d'etat (en haut), qui sert de toggle.

    // (Run 7 polish) Le libelle "config chargee + chemin" et le bouton "Rafraichir"
    // ont ete retires : sans interet pour l'utilisateur final, et le rechargement
    // se fait automatiquement (assistant/parametres + evenements frontend OBS).

    // --- Separateur + footer (acces futurs : grises "a venir") ---
    auto* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color:%1;").arg(th::kBorder));
    root->addWidget(sep);

    // Footer : deux acces ACTIFS (Run 5/6), accent pour inviter au clic. Meme style.
    auto* footer = new QHBoxLayout();
    footer->setSpacing(8);
    const QString footerBtnQss = w::accentOutlineBtnQss(QStringLiteral("8px 10px"));

    // Parametres avances (Run 6) : ouvre la fenetre de reglages fins.
    auto* settingsBtn = new QPushButton(i18n("Footer.AdvancedSettings"));
    settingsBtn->setIcon(icon(Icon::Settings, th::kAccent, 14));
    settingsBtn->setCursor(Qt::PointingHandCursor);
    settingsBtn->setStyleSheet(footerBtnQss);
    connect(settingsBtn, &QPushButton::clicked, this, [this]() { openSettings(); });
    footer->addWidget(settingsBtn, 1);

    // Assistant de configuration (Run 5) : ouvre le parcours guide.
    auto* assistantBtn = new QPushButton(i18n("Footer.Assistant"));
    assistantBtn->setIcon(icon(Icon::Sparkles, th::kAccent, 14));
    assistantBtn->setCursor(Qt::PointingHandCursor);
    assistantBtn->setStyleSheet(footerBtnQss);
    connect(assistantBtn, &QPushButton::clicked, this, [this]() { openAssistant(); });
    footer->addWidget(assistantBtn, 1);

    root->addLayout(footer);

    root->addStretch();

    // Bas du dock : version a droite. La case "verifier les MAJ au demarrage" a ete
    // DEPLACEE dans l'onglet "Parametres generaux" des parametres avances (avec le
    // reglage de scene hors regie) -> le dock reste epure.
    auto* bottomBar = new QHBoxLayout();
    bottomBar->setSpacing(8);
    auto* versionLabel = new QLabel(QString("Flowspire v%1").arg(PLUGIN_VERSION));
    versionLabel->setStyleSheet(QString("color:%1; font-size:%2px;").arg(th::kTextTertiary).arg(th::kFontSmall));
    bottomBar->addStretch();
    bottomBar->addWidget(versionLabel);
    root->addLayout(bottomBar);

    reload();

    timer_ = new QTimer(this);
    timer_->setInterval(kTickMs);
    connect(timer_, &QTimer::timeout, this, [this]() { tick(); });
    timer_->start();

    startUpdateCheck(); // verif MAJ au demarrage (async, silencieuse si hors-ligne)

    // Ecriture differee du seuil par intervenant : (re)demarree a chaque pas du slider,
    // elle n'ecrit le profil qu'une fois le reglage stabilise (~debounce). Le relachement
    // souris fige immediatement (saveActiveProfileNow), donc ceci couvre clavier/molette.
    thresholdSaveTimer_ = new QTimer(this);
    thresholdSaveTimer_->setSingleShot(true);
    thresholdSaveTimer_->setInterval(kThresholdSaveDebounceMs);
    connect(thresholdSaveTimer_, &QTimer::timeout, this, [this]() { saveActiveProfileNow(); });

    // Rappel periodique du statut vers l'appli externe (re-synchro douce si la cible a
    // redemarre ou rate un envoi). No-op tant que l'option n'est pas activee (loadPrefs).
    statusPushTimer_ = new QTimer(this);
    statusPushTimer_->setInterval(kStatusHeartbeatMs);
    connect(statusPushTimer_, &QTimer::timeout, this, [this]() { pushStatusIfEnabled(/*force=*/true); });
    statusPushTimer_->start();

    // Auto-refresh : OBS termine de charger ses scenes/sources APRES la creation
    // du dock -> on se reabonne pour recharger tout seul (plus besoin de cliquer
    // Rafraichir au lancement). Callback retire dans le destructeur.
    obs_frontend_add_event_callback(frontendEventCb, this);
}

SdDock::~SdDock() {
    // L'hote (plugin-main) oublie ce dock AVANT toute destruction : il remet son pointeur global
    // a nullptr -> une hotkey pressee pendant la fermeture ne peut plus router vers un objet mort.
    if (onDestroyed_) {
        onDestroyed_();
    }
    // Retrait du callback AVANT toute destruction : plus aucun evenement frontend
    // ne sera route vers ce dock apres ce point.
    obs_frontend_remove_event_callback(frontendEventCb, this);
    if (timer_) {
        timer_->stop();
    }
    if (statusPushTimer_) {
        statusPushTimer_->stop(); // plus aucun envoi de statut lance pendant la destruction
    }
    monitor_.stop();
}

void SdDock::setOnDestroyed(std::function<void()> onDestroyed) {
    onDestroyed_ = std::move(onDestroyed);
}

bool SdDock::eventFilter(QObject* watched, QEvent* event) {
    // Clic sur le badge d'etat -> bascule du pilotage auto (en plus des boutons).
    if (watched == statusBadge_ && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            toggleAuto();
            return true;
        }
    }
    // Clic sur une carte -> forcage : l'intervenant correspondant (forcage TEMPORAIRE, l'auto
    // reprend apres le temps mini), ou le plan large pour la carte "Plan large". Les enfants
    // interactifs (slider de seuil, bouton de calibration) ont deja consomme le clic : on ne
    // recoit ici que les clics tombes sur le corps de la carte.
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            for (size_t i = 0; i < rows_.size(); ++i) {
                if (rows_[i].card == watched) {
                    if (rows_[i].isWide) {
                        forceWide();
                    } else {
                        forceSpeakerByIndex(static_cast<int>(i));
                    }
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SdDock::styleSpeakerCard(Row& row, bool speaking) {
    // Carte : bordure verte si actif, grise sinon. Selecteur #spkCard -> la
    // bordure ne touche QUE la carte (pas de traits parasites sur les enfants).
    // Survol : la carte etant cliquable (forcage), on la teinte legerement en bleu (accent) +
    // bordure accentuee (#spkCard:hover). IMPORTANT : surtout PAS kSurface3 en fond de survol —
    // c'est la couleur des PISTES (vumetre + glissiere de seuil), qui se fondraient dedans et
    // "disparaitraient". Un bleu translucide ne peut pas entrer en collision avec ces pistes
    // grises. L'indice principal de clicabilite reste le curseur "main" pose sur la carte.
    row.card->setStyleSheet(QString("#spkCard { background:%1; border:1px solid %2;"
                                    " border-radius:%3px; }"
                                    "#spkCard:hover { background:%4; border-color:%5; }")
                                .arg(th::kSurface2)
                                .arg(speaking ? QString::fromUtf8(th::kSuccess) : rgba(th::kBorder, 1.0))
                                .arg(th::kRadiusCard)
                                .arg(rgba(th::kAccent, 0.12))
                                .arg(rgba(th::kAccent, 0.6)));
    // Avatar : icone user teintee (vert si actif), fond rond translucide.
    row.avatar->setPixmap(icon(Icon::User, speaking ? th::kSuccess : th::kTextTertiary, 18));
    row.avatar->setStyleSheet(QString("background:%1; border:2px solid %2; border-radius:%3px;")
                                  .arg(speaking ? rgba(th::kSuccess, 0.18) : QString::fromUtf8(th::kSurface3))
                                  .arg(speaking ? QString::fromUtf8(th::kSuccess) : rgba(th::kBorder, 1.0))
                                  .arg(th::kAvatarSize / 2));
    row.nameLabel->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:600;")
                                     .arg(speaking ? th::kTextPrimary : th::kTextSecondary)
                                     .arg(th::kFontBody));
    if (row.meter) {
        row.meter->setSpeaking(speaking);
    }
    if (row.threshold) {
        row.threshold->setStyleSheet(sliderQss(speaking));
    }
}

// --- Calibration auto du seuil (#3) ----------------------------------------------------

void SdDock::onCalibrateClicked(const std::string& speakerId) {
    auto it = calibrators_.find(speakerId);
    if (it != calibrators_.end() && !it->second.done()) {
        calibrators_.erase(it); // en cours -> un clic annule.
    } else {
        calibrators_[speakerId] = sd::core::ThresholdCalibrator{}; // repos ou deja cale -> (re)demarre.
    }
    // refreshCalibrationUi (et PAS juste l'icone de cette carte) : annuler la derniere carte
    // "en cours" d'une session globale doit pouvoir la conclure (fin auto reevaluee).
    refreshCalibrationUi();
}

void SdDock::startCalibrationAll() {
    if (!configMode_) {
        return;
    }
    calibrators_.clear();
    for (const auto& ds : displaySpeakers_) {
        calibrators_[ds.id] = sd::core::ThresholdCalibrator{};
    }
    calibrationBannerActive_ = !calibrators_.empty();
    refreshCalibrationUi();
}

void SdDock::finishCalibration() {
    // Bouton "Terminer" : fige tout ce qui peut l'etre (assez de donnees + ecart franc),
    // laisse les autres seuils inchanges.
    for (auto& row : rows_) {
        auto it = calibrators_.find(row.id);
        if (it == calibrators_.end() || it->second.done()) {
            continue;
        }
        if (it->second.finalizeNow()) {
            applyCalibratedThreshold(row, it->second.thresholdDb());
        }
    }
    calibrationBannerActive_ = false;
    refreshCalibrationUi();
}

void SdDock::applyCalibratedThreshold(Row& row, double thresholdDb) {
    if (!row.threshold) {
        return;
    }
    int t = static_cast<int>(std::lround(thresholdDb));
    if (t < kFloorDb + 1) {
        t = kFloorDb + 1;
    }
    if (t > 0) {
        t = 0;
    }
    // setValue declenche valueChanged -> director.setSpeakerThreshold + marqueur vumetre +
    // memorisation (cablage EXISTANT du slider). valueChanged arme aussi le timer d'ecriture
    // differee : on l'annule et on ecrit UNE fois tout de suite (pas de double ecriture).
    row.threshold->setValue(t);
    if (thresholdSaveTimer_) {
        thresholdSaveTimer_->stop();
    }
    saveActiveProfileNow();
}

void SdDock::updateCalibrateButton(Row& row) {
    if (!row.calibrateBtn) {
        return; // carte "Plan large" : pas de seuil, pas de bouton.
    }
    const auto it = calibrators_.find(row.id);
    if (it == calibrators_.end()) {
        row.calibrateBtn->setIcon(icon(Icon::Wand, th::kTextTertiary, 15));
        row.calibrateBtn->setToolTip(i18n("Calibrate.Tooltip"));
    } else if (!it->second.done()) {
        row.calibrateBtn->setIcon(icon(Icon::Wand, th::kAccent, 15));
        row.calibrateBtn->setToolTip(i18n("Calibrate.InProgress"));
    } else {
        row.calibrateBtn->setIcon(icon(Icon::Check, th::kSuccess, 15));
        row.calibrateBtn->setToolTip(i18n("Calibrate.Done"));
    }
}

void SdDock::updateCalibrationBanner() {
    if (calibrationBanner_) {
        calibrationBanner_->setVisible(calibrationBannerActive_);
    }
    if (calibrateAllButton_) {
        // Cache pendant une session globale (le bandeau prend le relais) et hors config.
        calibrateAllButton_->setVisible(configMode_ && !calibrationBannerActive_);
    }
    if (!calibrationBannerActive_ || !calibrationBannerLabel_) {
        return;
    }
    int done = 0;
    for (const auto& kv : calibrators_) {
        if (kv.second.done()) {
            ++done;
        }
    }
    calibrationBannerLabel_->setText(i18n("Calibrate.Progress").arg(done).arg(static_cast<int>(calibrators_.size())));
}

void SdDock::refreshCalibrationUi() {
    // MAJ de toutes les icones de carte + du bandeau. La fin AUTO de la session globale est
    // reevaluee ICI (a chaque changement d'etat), pas seulement sur une transition "cale" du
    // tick : ainsi annuler la derniere carte "en cours" (ou tout autre changement) conclut
    // bien la session au lieu de laisser le bandeau fige.
    for (auto& row : rows_) {
        updateCalibrateButton(row);
    }
    if (calibrationBannerActive_ && !calibrators_.empty()) {
        bool allDone = true;
        for (const auto& kv : calibrators_) {
            if (!kv.second.done()) {
                allDone = false;
                break;
            }
        }
        if (allDone) {
            calibrationBannerActive_ = false;
        }
    }
    updateCalibrationBanner();
}

void SdDock::rememberSpeakerThreshold(const std::string& speakerId, int db) {
    // Mode profil uniquement : en mode auto (sans config), il n'y a pas de profil
    // ou ecrire. Les ids affiches y sont des noms de sources, absents du profil.
    if (!configMode_) {
        return;
    }
    for (auto& sp : activeConfig_.speakers) {
        if (sp.id == speakerId) {
            sp.thresholdDb = static_cast<double>(db);
            return;
        }
    }
    // intervenant hors profil actif : rien a memoriser.
}

void SdDock::saveActiveProfileNow() {
    if (!configMode_) {
        return;
    }
    // Ecrit le profil actif (source de verite). Best-effort : un echec disque ne doit
    // pas casser l'UI ni le pilotage en cours ; il sera re-tente au prochain reglage.
    sd::profiles::saveActive(activeConfig_);
}

void SdDock::startUpdateCheck() {
    // opt-out (onglet Parametres generaux) : aucune requete reseau si l'utilisateur a decoche.
    if (!loadPrefs().checkUpdatesOnStartup)
        return;
    // Verifie en arriere-plan s'il existe une release plus recente que la version locale.
    // Aucune release / hors-ligne / erreur -> rien ne s'affiche (updateAvailable == false).
    checkForUpdate(this, PLUGIN_VERSION, [this](const UpdateInfo& info) {
        if (!info.updateAvailable)
            return;
        updateBannerLabel_->setText(i18n("Update.Available").arg(QString::fromStdString(info.latestVersion)));
        const QString url = QString::fromStdString(info.releaseUrl);
        connect(updateBannerButton_, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
        updateBanner_->setVisible(true);
    });
}

void SdDock::reload() {
    monitor_.start();
    const std::vector<std::string> names = monitor_.sourceNames();

    // Le moteur lit DIRECTEMENT le profil actif (source de verite). loadActiveConfig
    // garantit l'existence du catalogue (migration au premier usage) et recupere
    // sans perte un fichier illisible (.bak / scan).
    const sd::profiles::ActiveConfigResult loaded =
        sd::profiles::loadActiveConfig(i18n("Profiles.DefaultName").toStdString());
    activeConfig_ = loaded.config; // memorise pour persister le seuil par intervenant

    sd::core::Config cfg;
    displaySpeakers_.clear();
    if (loaded.ok && !loaded.config.speakers.empty()) {
        configMode_ = true;
        cfg = loaded.config;
        for (const auto& sp : cfg.speakers) {
            displaySpeakers_.push_back({sp.id, sp.name, sp.audioSource});
        }
    } else {
        // Etat d'accueil : on NE liste PLUS toutes les entrees audio (ca laissait croire
        // a une regie deja configuree). displaySpeakers_ et cfg restent vides -> aucune
        // carte d'intervenant ; une carte d'accueil (addWelcomeCard) invitera a lancer
        // l'assistant, en rappelant d'abord les prerequis OBS (scenes + entrees audio).
        configMode_ = false;
    }

    director_ = std::make_unique<sd::core::Director>(cfg);
    if (!configMode_) {
        autoEnabled_ = false; // GARDE-FOU : pas de pilotage sans config.
    }
    director_->setAutoEnabled(autoEnabled_);
    // Rafraichit le CACHE des prefs de remontee de statut (modifiables via les Parametres,
    // qui rejouent un reload a leur fermeture) puis (re)synchronise l'appli externe. La dedup
    // dans pushStatusIfEnabled evite les POST redondants quand reload est rejoue (events
    // frontend) sans changement d'etat ni de cible.
    const GlobalPrefs statusPrefs = loadPrefs();
    statusPushEnabled_ = statusPrefs.statusPushEnabled;
    statusPushHost_ = statusPrefs.statusPushHost;
    statusPushPort_ = statusPrefs.statusPushPort;
    pushStatusIfEnabled();
    // Pas de re-application d'overrides en session : le seuil par intervenant est
    // PERSISTE dans le profil (Speaker.thresholdDb) et le Director vient d'etre seme
    // depuis activeConfig_ par son constructeur (setConfig). La valeur a l'ecran est
    // donc deja la bonne, sans etat parallele a maintenir (qui fuyait entre profils).

    // Reconstruit les cartes intervenants (suppression SYNCHRONE).
    for (auto& row : rows_) {
        delete row.card;
    }
    rows_.clear();
    // Une reconfiguration annule toute session de calibration en cours (les intervenants
    // peuvent changer) : on repart au repos. Les seuils deja ecrits restent dans le profil.
    calibrators_.clear();
    calibrationBannerActive_ = false;
    // Carte d'accueil : suppression DIFFEREE (hide + deleteLater). Son bouton "Lancer
    // l'assistant" declenche reload() (donc CE bloc) alors que son propre clic est encore
    // en cours de traitement -> un delete immediat serait un use-after-free (meme piege
    // qu'au Run 5). deleteLater repousse la destruction a la fin de l'evenement courant.
    if (welcomeCard_) {
        welcomeCard_->hide();
        welcomeCard_->deleteLater();
        welcomeCard_ = nullptr;
    }

    for (size_t i = 0; i < displaySpeakers_.size(); ++i) {
        const DisplaySpeaker& ds = displaySpeakers_[i];

        auto* card = new QWidget();
        card->setObjectName(QStringLiteral("spkCard"));
        // Hauteur mini : la carte ne se laisse pas ecraser quand le dock est reduit
        // (le contenu ne sera pas coupe ; le defilement prend le relais).
        card->setMinimumHeight(th::kAvatarSize + 20);
        // Carte CLIQUABLE : un clic sur le corps force cet intervenant a l'antenne (forcage
        // temporaire -> l'auto reprend apres le temps mini). Les enfants interactifs (slider de
        // seuil) consomment leurs propres clics -> naturellement exemptes (l'eventFilter ne voit
        // que les clics qui remontent jusqu'a la carte).
        makeCardClickable(card, this, i18n("Speaker.ForceHint"));
        auto* lay = new QHBoxLayout(card);
        lay->setContentsMargins(8, 10, 10, 10); // marge gauche reduite : espace du tally
        lay->setSpacing(10);

        // Temoin "tally" : pilule verticale (rouge quand cette scene est a l'antenne).
        // Centree, plus courte que la carte, avec de l'espace de chaque cote (pas sur la
        // bordure). Toujours presente (transparente) -> pas de saut de layout.
        auto* tally = makeTally(lay);

        // Avatar : icone user dans une pastille ronde.
        auto* avatar = new QLabel();
        avatar->setFixedSize(th::kAvatarSize, th::kAvatarSize);
        avatar->setAlignment(Qt::AlignCenter);

        // Colonne droite : ligne du HAUT (nom + etat + label "Seuil") au-dessus de la ligne du
        // BAS (vumetre + slider de seuil). Le vumetre et le slider sont dans le MEME HBox ->
        // centres verticalement ensemble = leurs deux lignes sont alignees PAR CONSTRUCTION.
        // (Avant : vumetre et slider vivaient dans deux colonnes de hauteurs differentes -> leger
        // decalage. C'est le fix du polish.)
        constexpr int kThreshWidth = 96; // largeur commune au label "Seuil" et au slider
        constexpr int kCalBtnSize = 28;  // bouton calibration (carre), dans la rangee du bas
        auto* rightCol = new QWidget();
        auto* rightColLay = new QVBoxLayout(rightCol);
        rightColLay->setContentsMargins(0, 0, 0, 0);
        rightColLay->setSpacing(6);

        auto* topRow = new QWidget();
        auto* topRowLay = new QHBoxLayout(topRow);
        topRowLay->setContentsMargins(0, 0, 0, 0);
        topRowLay->setSpacing(6);
        auto* nameLabel = new QLabel(QString::fromStdString(ds.name));
        auto* stateLabel = new QLabel(i18n("Speaker.State.Silent"));
        stateLabel->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700;").arg(th::kTextTertiary).arg(th::kFontSmall));
        auto* threshLabel = new QLabel(i18n("Speaker.Threshold"));
        threshLabel->setStyleSheet(QString("color:%1; font-size:%2px;").arg(th::kTextTertiary).arg(th::kFontSmall));
        threshLabel->setFixedWidth(kThreshWidth);
        threshLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter); // pile au-dessus du slider
        topRowLay->addWidget(nameLabel);
        topRowLay->addStretch();
        topRowLay->addWidget(stateLabel);
        topRowLay->addWidget(threshLabel);
        // Reserve la colonne du bouton calibration (qui vit dans la rangee du BAS) pour que le
        // label "Seuil" reste pile au-dessus du slider et pas decale par le bouton. (+6 = espace
        // entre slider et bouton dans la rangee du bas.)
        topRowLay->addSpacing(kCalBtnSize + 6);

        auto* bottomRow = new QWidget();
        auto* bottomRowLay = new QHBoxLayout(bottomRow);
        bottomRowLay->setContentsMargins(0, 0, 0, 0);
        bottomRowLay->setSpacing(6);
        auto* meter = new LevelMeter();

        // Slider de seuil : regle EN DIRECT le seuil de cet intervenant ET deplace le marqueur
        // sur le vumetre. Largeur fixe = celle du label "Seuil" pose juste au-dessus (topRow).
        auto* threshold = new QSlider(Qt::Horizontal);
        threshold->setFixedWidth(kThreshWidth);
        // Borne basse = kFloorDb + 1 (et non kFloorDb) : tout en bas, le seuil
        // resterait AU plancher de silence -> avec le comparateur "niveau >= seuil"
        // une source muette (niveau == plancher) passerait pour "parle" en
        // permanence. On garde donc le minimum un cran au-dessus du silence absolu.
        threshold->setRange(kFloorDb + 1, 0); // dB : [-59, 0]
        int initThresh = director_ ? static_cast<int>(std::lround(director_->speakerThresholdDb(ds.id))) : -35;
        if (initThresh < kFloorDb + 1) {
            initThresh = kFloorDb + 1; // clamp si une config descend au plancher
        }
        threshold->setValue(initThresh);
        threshold->setStyleSheet(sliderQss(false));
        meter->setThresholdDb(initThresh); // marqueur initial sur le vumetre
        const std::string sid = ds.id;
        connect(threshold, &QSlider::valueChanged, this, [this, sid, meter](int v) {
            if (director_) {
                director_->setSpeakerThreshold(sid, static_cast<double>(v)); // moteur en direct
            }
            meter->setThresholdDb(v);         // le marqueur suit le slider en direct
            rememberSpeakerThreshold(sid, v); // prepare l'ecriture (pas encore sur disque)
            // Ecriture DIFFEREE : un glissement souris ou une rafale clavier/molette ne
            // declenche qu'UNE ecriture, ~apres stabilisation. Evite le martelement disque
            // et la rotation .bak a chaque pas.
            if (thresholdSaveTimer_) {
                thresholdSaveTimer_->start();
            }
        });
        // Relachement souris : on fige tout de suite (pas d'attente de la temporisation).
        connect(threshold, &QSlider::sliderReleased, this, [this]() {
            if (thresholdSaveTimer_) {
                thresholdSaveTimer_->stop();
            }
            saveActiveProfileNow();
        });
        // Vumetre + slider DANS LE MEME HBox : centres verticalement ensemble -> leurs lignes
        // (barre du vumetre / glissiere du seuil) sont alignees, quelle que soit la hauteur du
        // texte au-dessus.
        bottomRowLay->addWidget(meter, 1);
        bottomRowLay->addWidget(threshold);

        rightColLay->addWidget(topRow);
        rightColLay->addWidget(bottomRow);

        lay->addWidget(avatar);
        lay->addWidget(rightCol, 1);

        // Bouton "calibrer ce seuil" (#3) : place dans la rangee du BAS, a droite du slider ->
        // centre verticalement AVEC le vumetre et le slider = aligne sur leur ligne (sa colonne
        // est reservee dans la rangee du haut via topRow). L'icone PORTE l'etat (repos / en
        // cours / cale). QPushButton -> consomme son clic = exempt du clic-carte (forcage).
        auto* calBtn = new QPushButton();
        calBtn->setCursor(Qt::PointingHandCursor);
        calBtn->setFixedSize(kCalBtnSize, kCalBtnSize);
        calBtn->setStyleSheet(QString("QPushButton { background:transparent; border:none; border-radius:%1px; }"
                                      "QPushButton:hover { background:%2; }")
                                  .arg(th::kRadiusButton)
                                  .arg(QString::fromUtf8(th::kSurface3)));
        connect(calBtn, &QPushButton::clicked, this, [this, sid]() { onCalibrateClicked(sid); });
        bottomRowLay->addWidget(calBtn, 0, Qt::AlignVCenter);

        // Clic-carte : on ne pose PAS WA_TransparentForMouseEvents sur les enfants -> il
        // desactiverait AUSSI leurs propres enfants (doc Qt : "the widget and its children"),
        // ce qui casserait le slider de seuil et le bouton de calibration. Inutile de toute
        // facon : les elements passifs (labels, avatar, vumetre, conteneurs) ignorent les
        // clics, qui remontent donc naturellement jusqu'a la carte (meme mecanique que le
        // badge d'etat) ; le slider et le bouton, eux, consomment leurs propres clics.

        rowsLayout_->addWidget(card);
        // Init membre-a-membre (comme la carte plan large) : robuste a un reordonnancement
        // futur des champs de Row, contrairement a une init positionnelle.
        Row row;
        row.id = ds.id;
        row.audioSource = ds.audioSource;
        row.card = card;
        row.avatar = avatar;
        row.nameLabel = nameLabel;
        row.meter = meter;
        row.threshold = threshold;
        row.calibrateBtn = calBtn;
        row.stateLabel = stateLabel;
        row.tally = tally;
        styleSpeakerCard(row, /*speaking=*/false);
        updateCalibrateButton(row); // icone initiale (repos)
        rows_.push_back(row);
    }

    // Carte "Plan large" (si configuree) : pas un intervenant -> pas de vumetre ni de
    // seuil. Icone "groupe" (2 personnes) + tally qui s'allume quand le plan large est
    // a l'antenne. Permet de VOIR dans le dock qu'on est passe sur le plan large (sinon
    // angle mort : il fallait regarder le gestionnaire de scenes d'OBS).
    if (configMode_ && director_ && !director_->config().wideShotScene.empty()) {
        auto* card = new QWidget();
        card->setObjectName(QStringLiteral("spkCard"));
        card->setMinimumHeight(th::kAvatarSize + 20);
        // Meme principe que les cartes d'intervenants : clic = forcage du plan large.
        makeCardClickable(card, this, i18n("Dock.WideShot.ForceHint"));
        auto* lay = new QHBoxLayout(card);
        lay->setContentsMargins(8, 10, 10, 10);
        lay->setSpacing(10);

        auto* tally = makeTally(lay);

        auto* avatar = new QLabel();
        avatar->setFixedSize(th::kAvatarSize, th::kAvatarSize);
        avatar->setAlignment(Qt::AlignCenter);

        auto* nameLabel = new QLabel(i18n("Dock.WideShot"));
        lay->addWidget(avatar);
        lay->addWidget(nameLabel, 1);

        rowsLayout_->addWidget(card);
        Row wideRow;
        wideRow.card = card;
        wideRow.avatar = avatar;
        wideRow.nameLabel = nameLabel;
        wideRow.tally = tally;
        wideRow.isWide = true;
        // Style "au repos" reutilise (carte grise, nom secondaire), puis on remplace
        // l'icone par "groupe" (2 personnes) : meter/threshold sont nuls -> styleSpeakerCard
        // saute les parties vumetre/seuil (gardes internes).
        styleSpeakerCard(wideRow, /*speaking=*/false);
        wideRow.avatar->setPixmap(icon(Icon::Users, th::kTextTertiary, 18));
        rows_.push_back(wideRow);
    }

    // Etat d'accueil (aucune config) : carte d'accueil a la place de la liste d'entrees.
    // On masque le seul en-tete "SCENES" (sans objet ici) ; le selecteur de profil reste
    // VISIBLE -> on peut toujours rebasculer sur un profil configure (pas de piege quand
    // un profil vierge est actif). Tout reapparait des qu'une config existe.
    if (!configMode_) {
        addWelcomeCard(names.size());
    }
    scenesSectionLabel_->setVisible(configMode_);

    updateModeLabel();
    shownStatus_ = -1;
    updateStatusBadge();
    updateProfileBar();
    updateStyleBar();
    updateCalibrationBanner(); // bandeau masque + visibilite du bouton "Calibrer tous"

    // Apres un rechargement, la scene actuellement a l'antenne est l'etat de REFERENCE :
    // on ne la traitera pas comme un forcage manuel au prochain tick (le moteur vient
    // d'etre recree, sa decision repart de zero).
    lastRequestedScene_ = sceneSwitcher_.currentProgramScene();
}

void SdDock::addWelcomeCard(size_t audioInputCount) {
    namespace w = sd::ui::widgets;
    // Carte affichee quand aucune regie n'est configuree. Remplace l'ancienne liste de
    // TOUTES les entrees audio (trompeuse : elle laissait croire a une regie en place).
    // On invite a lancer l'assistant en rappelant d'abord les prerequis OBS.
    auto* card = w::makeCard(QStringLiteral("welcomeCard"));
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(16, 18, 16, 18);
    lay->setSpacing(12);

    // Icone d'accueil (memes etincelles que le bouton Assistant -> lien visuel).
    auto* iconLabel = new QLabel();
    iconLabel->setPixmap(icon(Icon::Sparkles, th::kAccent, 28));
    iconLabel->setAlignment(Qt::AlignCenter);
    lay->addWidget(iconLabel);

    auto* title = new QLabel(i18n("Welcome.Title"));
    title->setWordWrap(true);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:700;").arg(th::kTextPrimary).arg(th::kFontTitle));
    lay->addWidget(title);

    // Rappel des prerequis AVANT de lancer l'assistant (sinon l'utilisateur le lance,
    // decouvre qu'il n'a rien prepare dans OBS, et perd du temps). makeSub = texte
    // secondaire standard (kTextSecondary + 13px + word-wrap) -> pas de style duplique.
    lay->addWidget(w::makeSub(i18n("Welcome.Prereq")));

    // Ligne d'etat : 2 cas distincts (demande David). Entrees audio presentes -> ligne
    // rassurante (vert) ; aucune entree -> on guide d'abord vers leur creation dans OBS.
    auto* status = new QLabel();
    status->setWordWrap(true);
    if (audioInputCount == 0) {
        status->setText(i18n("Welcome.NoSources"));
        status->setStyleSheet(QString("color:%1; font-size:%2px;").arg(th::kWarning).arg(th::kFontBody));
    } else {
        const QString line = (audioInputCount == 1)
                                 ? i18n("Welcome.Sources.One")
                                 : i18n("Welcome.Sources.Many").arg(static_cast<int>(audioInputCount));
        status->setText(line);
        status->setStyleSheet(QString("color:%1; font-size:%2px;").arg(th::kSuccess).arg(th::kFontBody));
    }
    lay->addWidget(status);

    // Appel a l'action principal : bouton plein accent, pleine largeur.
    auto* startBtn = new QPushButton(i18n("Welcome.Start"));
    startBtn->setCursor(Qt::PointingHandCursor);
    startBtn->setStyleSheet(w::primaryBtnQss());
    connect(startBtn, &QPushButton::clicked, this, [this]() { openAssistant(); });
    lay->addWidget(startBtn);

    rowsLayout_->addWidget(card);
    welcomeCard_ = card;
}

void SdDock::updateStatusBadge() {
    const int status = !configMode_ ? StatusReadOnly : (autoEnabled_ ? StatusActive : StatusPaused);
    if (status == shownStatus_) {
        return;
    }
    shownStatus_ = status;

    const char* color = th::kTextTertiary;
    double fillA = 0.0;
    const char* borderC = th::kBorder;
    QString text = i18n("Dock.Status.ReadOnly");
    if (status == StatusActive) {
        color = th::kSuccess;
        fillA = 0.17;
        borderC = th::kSuccess;
        text = i18n("Dock.Status.Active");
    } else if (status == StatusPaused) {
        color = th::kWarning;
        fillA = 0.12;
        borderC = th::kWarning;
        text = i18n("Dock.Status.Paused");
    } else {
        fillA = 1.0; // lecture seule : fond surface3 plein
    }
    const QString fill = (status == StatusReadOnly) ? QString::fromUtf8(th::kSurface3) : rgba(color, fillA);
    statusBadge_->setStyleSheet(QString("#statusBadge { background:%1; border:1px solid %2;"
                                        " border-radius:6px; }")
                                    .arg(fill)
                                    .arg(rgba(borderC, status == StatusReadOnly ? 1.0 : 0.5)));
    statusDot_->setStyleSheet(QString("color:%1; font-size:%2px;").arg(color).arg(th::kFontBody));
    statusText_->setText(text);
    statusText_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;").arg(color).arg(th::kFontBody));

    // En lecture seule (aucune config), le badge n'est PAS un interrupteur (rien a
    // piloter) : curseur neutre + infobulle d'aide -> on n'invite pas a un clic sans
    // effet. Configure -> il redevient le toggle ON/OFF (curseur main + infobulle).
    const bool interactive = (status != StatusReadOnly);
    statusBadge_->setCursor(interactive ? Qt::PointingHandCursor : Qt::ArrowCursor);
    statusBadge_->setToolTip(interactive ? i18n("Control.ToggleAuto") : i18n("Control.ReadOnlyHint"));
}

void SdDock::updateModeLabel() {
    if (!modeLabel_) {
        return;
    }
    if (!configMode_) {
        // Etat d'accueil : le message est porte par la carte d'accueil -> on masque ce
        // libelle (sinon doublon avec l'invitation a lancer l'assistant).
        modeLabel_->setVisible(false);
        return;
    }
    modeLabel_->setVisible(true);
    if (autoEnabled_) {
        modeLabel_->setText(i18n("Mode.Auto.On"));
        modeLabel_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:400;").arg(th::kSuccess).arg(th::kFontLabel));
    } else {
        modeLabel_->setText(i18n("Mode.Auto.Off"));
        modeLabel_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:400;").arg(th::kWarning).arg(th::kFontLabel));
    }
}

void SdDock::pushStatusIfEnabled(bool force) {
    // Best-effort, asynchrone, sans TLS (HTTP local) -> aucun impact sur le pilotage. Lit le
    // CACHE (rafraichi par reload), pas le disque : un battement quand l'option est OFF ne
    // coute qu'un test booleen.
    if (!statusPushEnabled_) {
        return;
    }
    const int active = autoEnabled_ ? 1 : 0;
    // Dedup : on n'emet que si l'etat OU la cible a change depuis le dernier envoi. Le
    // battement (force) outrepasse pour re-synchroniser une cible qui aurait redemarre.
    if (!force && active == lastPushedActive_ && statusPushHost_ == lastPushedHost_ &&
        statusPushPort_ == lastPushedPort_) {
        return;
    }
    lastPushedActive_ = active;
    lastPushedHost_ = statusPushHost_;
    lastPushedPort_ = statusPushPort_;
    pushRegieStatus(this, statusPushHost_, statusPushPort_, autoEnabled_);
}

void SdDock::setAuto(bool on) {
    if (on && !configMode_) {
        return; // GARDE-FOU : pas d'activation sans config (couvre la hotkey).
    }
    autoEnabled_ = on;
    if (director_) {
        director_->setAutoEnabled(on);
    }
    if (on) {
        // On (re)prend la main DEPUIS la scene actuellement a l'antenne : on l'adopte
        // comme reference pour ne pas la confondre avec un changement manuel au tick
        // suivant. Sinon, reactiver l'auto depuis une scene hors regie la repauserait
        // aussitot (cas "mettre la regie en pause"). Le moteur rebasculera ensuite
        // normalement selon qui parle.
        lastRequestedScene_ = sceneSwitcher_.currentProgramScene();
    }
    updateModeLabel();
    updateStatusBadge();
    pushStatusIfEnabled(); // reflete immediatement le nouvel etat ON/OFF cote appli externe
}

void SdDock::toggleAuto() {
    setAuto(!autoEnabled_);
}

void SdDock::openAssistant() {
    // Bouton "Assistant" : l'assistant cree TOUJOURS un nouveau profil nomme (creation
    // guidee). On demande donc le nom d'abord, comme "+ Nouveau > Avec l'assistant"
    // (coherence demandee par David). Annuler le nom -> on n'ouvre pas l'assistant.
    bool ok = false;
    const QString name = QInputDialog::getText(this, i18n("Profiles.NamePrompt.Title"),
                                               i18n("Profiles.NamePrompt.Body"), QLineEdit::Normal,
                                               i18n("Profiles.NewBlankName"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    openAssistantWith(name.trimmed());
}

void SdDock::openAssistantWith(const QString& newProfileName) {
    // Mode CREATION : le profil n'est cree + active qu'A LA FIN de l'assistant. Si
    // l'utilisateur annule, RIEN n'a ete cree (pas d'orphelin), rien a nettoyer.
    auto* mainWin = static_cast<QWidget*>(obs_frontend_get_main_window());
    SdAssistant dlg(mainWin ? mainWin : this, newProfileName);
    if (dlg.exec() == QDialog::Accepted) {
        // L'assistant a cree + active le profil (<id>.json + index.json) -> on recharge
        // (le moteur lira le nouveau profil actif). S'il a termine par "Activer", on
        // lance le pilotage.
        reload();
        if (dlg.activateAutoRequested()) {
            setAuto(true);
        }
    }
}

void SdDock::openSettings(int initialTab) {
    // Parametres avances : edition directe + gestion des profils. Recharge si la
    // config a change, que la fenetre soit fermee par "Enregistrer et fermer" OU par
    // la croix (une action de profil — charger/nouveau — ecrit IMMEDIATEMENT). Si
    // l'utilisateur a choisi "Nouveau > Avec l'assistant", on enchaine sur l'assistant
    // (mode creation avec le nom choisi ; le profil sera cree + active a la fin).
    auto* mainWin = static_cast<QWidget*>(obs_frontend_get_main_window());
    SdSettings dlg(mainWin ? mainWin : this, static_cast<SdSettings::Tab>(initialTab));
    dlg.exec();
    if (dlg.savedConfig()) {
        reload();
    }
    const QString pending = dlg.pendingAssistantName();
    if (!pending.isEmpty()) {
        openAssistantWith(pending);
    }
}

void SdDock::updateProfileBar() {
    if (!profileNameLabel_) {
        return;
    }
    const sd::profiles::ListResult list = sd::profiles::loadList(i18n("Profiles.DefaultName").toStdString());
    QString name;
    if (list.ok) {
        if (const sd::core::ProfileEntry* e = sd::core::findProfile(list.index, list.index.activeId)) {
            name = QString::fromStdString(e->name);
        }
    }
    if (name.isEmpty()) {
        name = i18n("Profiles.DefaultName");
    }
    profileNameLabel_->setText(name); // le chevron est un label fixe a droite du bouton
}

void SdDock::showProfileMenu() {
    const sd::profiles::ListResult list = sd::profiles::loadList(i18n("Profiles.DefaultName").toStdString());
    if (!list.ok) {
        openSettings(SdSettings::TabProfiles); // catalogue illisible : on ouvre la gestion
        return;
    }
    QMenu menu;
    menu.setStyleSheet(QString("QMenu { background:%1; border:1px solid %2; border-radius:6px; padding:4px; }"
                               "QMenu::item { color:%3; padding:6px 22px 6px 12px; border-radius:4px; }"
                               "QMenu::item:selected { background:%4; }")
                           .arg(th::kSurface2)
                           .arg(th::kBorder)
                           .arg(th::kTextPrimary)
                           .arg(rgba(th::kAccent, 0.25)));
    // Menu aussi large que le bouton (retour David : le sous-menu prend toute la largeur).
    menu.setMinimumWidth(profileButton_->width());
    for (const auto& e : list.index.profiles) {
        QAction* a = menu.addAction(QString::fromStdString(e.name));
        a->setCheckable(true);
        a->setChecked(e.id == list.index.activeId);
        a->setData(e.id);
    }
    menu.addSeparator();
    QAction* manage = menu.addAction(i18n("Dock.Profile.Manage"));
    manage->setData(-1);

    QAction* chosen = menu.exec(profileButton_->mapToGlobal(QPoint(0, profileButton_->height() + 2)));
    if (chosen == nullptr) {
        return;
    }
    const int id = chosen->data().toInt();
    if (id == -1) {
        openSettings(SdSettings::TabProfiles);
    } else if (id != list.index.activeId) {
        switchProfile(id);
    }
}

void SdDock::switchProfile(int id) {
    // Bascule du profil actif : une seule ecriture (index.json), puis on recharge
    // tout le dock. reload() relit DIRECTEMENT le nouveau profil actif (source de
    // verite) -> intervenants, scenes, reglages et selecteur se mettent a jour.
    const sd::profiles::StoreResult res = sd::profiles::setActive(id);
    if (!res.ok) {
        return; // best-effort : un echec laisse le profil courant inchange
    }
    reload();
}

void SdDock::updateStyleBar() {
    if (!styleButton_ || !styleNameLabel_) {
        return;
    }
    if (styleBar_) {
        styleBar_->setVisible(configMode_); // pas de style a regler sans config (etat d'accueil)
    }
    // Libelle = nom du style actif (fourni OU perso), ou "Perso" si reglage manuel (styleName
    // vide). On NE relit PAS la bibliotheque ici : (1) evite une I/O disque a chaque reload ;
    // (2) si styles.json etait illisible, on n'afficherait pas "Perso" a tort (le nom reste
    // affiche). Un style supprime ailleurs montre encore son nom -> ses VALEURS sont de toute
    // facon conservees dans le profil, donc c'est plus juste que "Perso".
    const std::string& sn = activeConfig_.styleName;
    styleNameLabel_->setText(sn.empty() ? i18n("Rhythm.StyleCustom") : QString::fromStdString(sn));
}

void SdDock::showStyleMenu() {
    QMenu menu;
    menu.setStyleSheet(QString("QMenu { background:%1; border:1px solid %2; border-radius:6px; padding:4px; }"
                               "QMenu::item { color:%3; padding:6px 22px 6px 12px; border-radius:4px; }"
                               "QMenu::item:selected { background:%4; }")
                           .arg(th::kSurface2)
                           .arg(th::kBorder)
                           .arg(th::kTextPrimary)
                           .arg(rgba(th::kAccent, 0.25)));
    menu.setMinimumWidth(styleButton_->width());
    const std::string active = activeConfig_.styleName;
    // Styles FOURNIS (Chill/Cool/Speed) en haut.
    for (const auto& b : sd::core::builtinRhythmStyles()) {
        QAction* a = menu.addAction(QString::fromStdString(b.name));
        a->setCheckable(true);
        a->setChecked(b.name == active);
        a->setData(QString::fromStdString(b.name));
    }
    // Styles PERSO : separateur + section "Mes styles", puis la liste (seulement s'il y en a).
    const std::vector<sd::core::RhythmStyle> lib = sd::styles::loadLibrary().styles;
    if (!lib.empty()) {
        menu.addSeparator();
        menu.addSection(i18n("Rhythm.StyleMineSection")); // "MES STYLES"
        for (const auto& s : lib) {
            QAction* a = menu.addAction(QString::fromStdString(s.name));
            a->setCheckable(true);
            a->setChecked(s.name == active);
            a->setData(QString::fromStdString(s.name));
        }
    }

    QAction* chosen = menu.exec(styleButton_->mapToGlobal(QPoint(0, styleButton_->height() + 2)));
    if (chosen == nullptr) {
        return;
    }
    const std::string name = chosen->data().toString().toStdString();
    if (name.empty() || name == active) {
        return; // section (sans data) ou style deja actif : rien a faire
    }
    // Retrouve le style choisi (fourni ou perso) et l'applique EN DIRECT.
    sd::core::RhythmStyle picked;
    bool found = false;
    for (const auto& b : sd::core::builtinRhythmStyles()) {
        if (b.name == name) {
            picked = b;
            found = true;
            break;
        }
    }
    if (!found) {
        for (const auto& s : lib) {
            if (s.name == name) {
                picked = s;
                found = true;
                break;
            }
        }
    }
    if (!found) {
        return;
    }
    sd::core::applyRhythmStyle(activeConfig_, picked);
    saveActiveProfileNow();
    reload(); // recree le moteur (nouveau style applique) + rafraichit le selecteur du dock
}

void SdDock::runOnUiThread(std::function<void()> fn) {
    QCoreApplication::postEvent(this, new ActionEvent(std::move(fn)));
}

void SdDock::customEvent(QEvent* event) {
    if (event && event->type() == kActionEventType) {
        static_cast<ActionEvent*>(event)->run();
        return;
    }
    QWidget::customEvent(event);
}

void SdDock::applyDecision(const sd::core::Decision& decision, const std::string& currentOnAir) {
    if (!autoEnabled_ || decision.scene.empty()) {
        return;
    }
    // Memorise la scene VOULUE par l'auto : tout passage du programme vers une AUTRE
    // scene sera reconnu comme un changement MANUEL (handleManualSceneChange), pas comme
    // l'echo de notre propre bascule. On la pose meme sans switchTo (deja a l'antenne)
    // pour rester aligne avec l'etat reel.
    lastRequestedScene_ = decision.scene;
    if (decision.scene != currentOnAir) {
        sceneSwitcher_.switchTo(decision.scene);
    }
}

void SdDock::forceWide() {
    if (!director_) {
        return;
    }
    const std::string wide = director_->config().wideShotScene;
    if (wide.empty()) {
        return;
    }
    const sd::core::Decision decision = director_->forceScene(nowSeconds(), wide, "");
    const std::string scene = decision.scene.empty() ? wide : decision.scene;
    lastRequestedScene_ = scene; // bascule initiee par nous (cf. handleManualSceneChange)
    sceneSwitcher_.switchTo(scene);
}

void SdDock::forceSpeakerByIndex(int index) {
    if (!director_ || index < 0 || index >= static_cast<int>(displaySpeakers_.size())) {
        return;
    }
    const sd::core::Decision decision =
        director_->forceSpeaker(nowSeconds(), displaySpeakers_[static_cast<size_t>(index)].id);
    if (!decision.scene.empty()) {
        lastRequestedScene_ = decision.scene; // bascule initiee par nous (cf. handleManualSceneChange)
        sceneSwitcher_.switchTo(decision.scene);
    }
}

void SdDock::handleManualSceneChange(const std::string& onAir, double now) {
    // On ne s'interesse au changement que si la realisation auto pilote reellement
    // (mode config + auto ON). `onAir == lastRequestedScene_` => c'est NOTRE propre
    // bascule (ou l'etat de reference) -> rien a faire. Vide => antenne indisponible.
    if (!autoEnabled_ || !configMode_ || onAir.empty() || onAir == lastRequestedScene_) {
        return;
    }
    // Changement initie par l'UTILISATEUR (clic dans le dock natif d'OBS, hotkey OBS...).
    std::string owner;
    if (director_->sceneInProgram(onAir, owner)) {
        // Scene DE la regie -> toujours un forcage TEMPORAIRE (hold) : l'auto reprend
        // apres le temps mini. La scene est deja a l'antenne -> pas de switchTo ; au
        // reste du tick, decision == antenne -> aucun combat de bascule.
        director_->forceScene(now, onAir, owner);
        lastRequestedScene_ = onAir;
    } else if (loadPrefs().offSceneBehavior == OffSceneBehavior::ForceAsShot) {
        // Scene HORS regie, reglage "la compter comme un plan" : forcage temporaire.
        director_->forceScene(now, onAir, "");
        lastRequestedScene_ = onAir;
    } else {
        // Scene HORS regie, reglage "mettre la regie en pause" (defaut) : on coupe l'auto
        // et on reste sur la scene choisie jusqu'a reactivation manuelle.
        setAuto(false);
    }
}

void SdDock::tick() {
    if (!director_) {
        return;
    }

    const double now = nowSeconds();

    // 1) Detecter un changement de scene NON initie par nous (clic dans le dock natif
    //    d'OBS) AVANT la decision auto. Sinon, le tick verrait l'antenne != sa decision
    //    et rebasculerait dessus -> clignotement + clic "perdu". currentProgramScene()
    //    est la verite terrain de l'antenne.
    const std::string onAir = sceneSwitcher_.currentProgramScene();
    handleManualSceneChange(onAir, now);

    // 2) Audio -> decision auto -> bascule.
    const std::map<std::string, double> levelsBySource = monitor_.snapshot(now);
    std::map<std::string, double> levelsById;
    for (const auto& ds : displaySpeakers_) {
        const auto it = levelsBySource.find(ds.audioSource);
        levelsById[ds.id] = (it != levelsBySource.end()) ? it->second : static_cast<double>(kFloorDb);
    }

    const sd::core::Decision decision = director_->update(now, levelsById);
    applyDecision(decision, onAir);

    const std::vector<std::string> speaking = director_->speakingIds();
    const std::unordered_set<std::string> speakingSet(speaking.begin(), speaking.end());

    // Scene a l'antenne (relue APRES la decision, qui a pu basculer ce tick) : on
    // identifie son proprietaire pour le tally. sceneInProgram renvoie true + owner vide
    // UNIQUEMENT pour le plan large -> wideOnAir. owner non vide -> carte d'un intervenant.
    std::string onAirOwner;
    const bool inProgram = director_->sceneInProgram(sceneSwitcher_.currentProgramScene(), onAirOwner);
    const bool wideOnAir = inProgram && onAirOwner.empty();

    for (auto& row : rows_) {
        // Bloc audio (vumetre + etat parle) : uniquement pour les cartes intervenant
        // (la carte "Plan large" n'a ni source audio ni vumetre -> meter nul).
        if (row.meter) {
            const auto it = levelsBySource.find(row.audioSource);
            const double db = (it != levelsBySource.end()) ? it->second : static_cast<double>(kFloorDb);

            // Calibration en cours pour cet intervenant : on alimente le calibrateur avec le
            // niveau du tick ; des qu'il a "cale" -> on ecrit le seuil (via le slider, qui
            // recable director + marqueur + memorisation) et on fige.
            const auto calIt = calibrators_.find(row.id);
            if (calIt != calibrators_.end() && !calIt->second.done()) {
                calIt->second.addSample(db);
                if (calIt->second.done()) {
                    applyCalibratedThreshold(row, calIt->second.thresholdDb());
                    refreshCalibrationUi(); // MAJ icones + bandeau N/M + fin auto de session
                }
            }

            const int dbInt = dbToInt(db);
            if (dbInt != row.shownDb) {
                row.shownDb = dbInt;
                row.meter->setLevelDb(db);
            }

            const int speakingNow = speakingSet.count(row.id) ? 1 : 0;
            if (speakingNow != row.shownSpeaking) {
                row.shownSpeaking = speakingNow;
                row.stateLabel->setText(speakingNow ? i18n("Speaker.State.Speaking") : i18n("Speaker.State.Silent"));
                row.stateLabel->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;")
                                                  .arg(speakingNow ? th::kSuccess : th::kTextTertiary)
                                                  .arg(th::kFontSmall));
                styleSpeakerCard(row, speakingNow != 0);
            }
        }

        // Tally : allume (rouge) si la scene a l'antenne est celle de cette carte
        // (le plan large pour la carte "Plan large", sinon le pool d'un intervenant).
        const int onAirNow = (row.isWide ? wideOnAir : (!onAirOwner.empty() && row.id == onAirOwner)) ? 1 : 0;
        if (onAirNow != row.shownOnAir) {
            row.shownOnAir = onAirNow;
            if (row.tally) {
                row.tally->setStyleSheet(tallyQss(onAirNow != 0));
            }
        }
    }
}

} // namespace sd::ui
