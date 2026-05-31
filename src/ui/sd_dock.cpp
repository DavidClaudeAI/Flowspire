#include "ui/sd_dock.hpp"

#include <obs-frontend-api.h>

#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <unordered_set>
#include <utility>

#include "core/audio_util.hpp"     // source de verite du plancher (kDbFloor)
#include "obs/config_loader.hpp"   // chargement du config.json
#include "ui/sd_assistant.hpp"     // assistant de configuration (modale)
#include "ui/sd_i18n.hpp"          // i18n native OBS (i18n)
#include "ui/sd_icons.hpp"         // icones lucide teintees
#include "ui/sd_level_meter.hpp"   // vumetre custom + marqueur de seuil
#include "ui/sd_runtime.hpp"       // kTickMs (cadence partagee dock <-> assistant)
#include "ui/sd_style.hpp"         // rgba() (helper QSS partage)
#include "ui/sd_theme.hpp"         // jetons de design (couleurs/typo)

namespace sd::ui {

namespace th = sd::ui::theme;

namespace {
constexpr int kFloorDb = static_cast<int>(sd::core::kDbFloor);

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
    explicit ActionEvent(std::function<void()> fn)
        : QEvent(kActionEventType), fn_(std::move(fn)) {}
    void run() {
        if (fn_) {
            fn_();
        }
    }

private:
    std::function<void()> fn_;
};

// Callback d'evenements frontend OBS. `data` = le dock. On rafraichit
// AUTOMATIQUEMENT aux moments ou la liste des scenes/sources devient valide :
// - FINISHED_LOADING : OBS a fini de charger au demarrage (le dock, lui, est cree
//   plus tot en post_load -> sans ca, il faut cliquer Rafraichir a la main).
// - SCENE_COLLECTION_CHANGED : on bascule sur une autre collection (tout change).
// On ne rafraichit PAS sur chaque ajout/suppression de source : un reload recree
// le Director (reset des reglages live) -> on evite de perturber une regie en
// cours ; le bouton Rafraichir reste pour ces cas ponctuels.
void frontendEventCb(enum obs_frontend_event event, void* data) {
    if (event != OBS_FRONTEND_EVENT_FINISHED_LOADING &&
        event != OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
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
// de bordures sur les widgets enfants (#StreamDirectorDock matche le seul fond).
QString dockQss() {
    return QString("QWidget#StreamDirectorDock { background:%1; }"
                   "QToolTip { color:%2; background:%3; border:1px solid %4; padding:4px; }")
        .arg(th::kSurface1)
        .arg(th::kTextPrimary)
        .arg(th::kSurface2)
        .arg(th::kBorder);
}

// Bouton "pilule" : fond + bordure + texte. Selecteur QPushButton (feuille, sans
// enfants) -> pas de cascade. fillAlpha/borderAlpha en rgba pour les teintes.
QString pillBtnQss(const char* color, double fillAlpha, double borderAlpha, bool bold) {
    return QString("QPushButton { background:%1; border:1px solid %2; border-radius:%3px;"
                   " color:%4; font-size:12px; font-weight:%5; padding:8px 8px; }"
                   "QPushButton:hover:enabled { border-color:%4; }"
                   "QPushButton:disabled { color:%6; }")
        .arg(rgba(color, fillAlpha))
        .arg(rgba(color, borderAlpha))
        .arg(th::kRadiusButton)
        .arg(QString::fromUtf8(color))
        .arg(bold ? 700 : 600)
        .arg(rgba(th::kTextTertiary, 0.6));
}

// Bouton neutre (surface3, texte secondaire) : invites, footer, rafraichir.
QString neutralBtnQss() {
    return QString("QPushButton { background:%1; border:1px solid %2; border-radius:%3px;"
                   " color:%4; font-size:12px; font-weight:600; padding:8px 10px; text-align:center; }"
                   "QPushButton:hover:enabled { border-color:%5; }"
                   "QPushButton:disabled { color:%6; }")
        .arg(th::kSurface3)
        .arg(th::kBorder)
        .arg(th::kRadiusButton)
        .arg(th::kTextPrimary)
        .arg(th::kTextSecondary)
        .arg(rgba(th::kTextTertiary, 0.5));
}

// Slider de seuil. handle clair ; bordure verte quand l'intervenant parle.
QString sliderQss(bool active) {
    return QString(
               "QSlider::groove:horizontal { height:%1px; background:%2; border-radius:3px; }"
               "QSlider::sub-page:horizontal { height:%1px; background:%2; border-radius:3px; }"
               "QSlider::add-page:horizontal { height:%1px; background:%2; border-radius:3px; }"
               "QSlider::handle:horizontal { width:12px; height:12px; margin:-4px 0;"
               " border-radius:6px; background:%3; border:2px solid %4; }")
        .arg(th::kMeterHeight)
        .arg(th::kSurface3)
        .arg(th::kTextPrimary)
        .arg(active ? QString::fromUtf8(th::kSuccess) : rgba(th::kBorder, 1.0));
}
}  // namespace

double SdDock::nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

SdDock::SdDock(QWidget* parent) : QWidget(parent) {
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
    content->setObjectName(QStringLiteral("StreamDirectorDock"));  // cible du fond (dockQss)
    scroll->setWidget(content);
    outer->addWidget(scroll);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(14);

    // --- Header : icone clapperboard + marque + badge d'etat ---
    auto* header = new QHBoxLayout();
    header->setSpacing(8);
    auto* brandIcon = new QLabel();
    brandIcon->setPixmap(icon(Icon::Clapperboard, th::kAccent, 18));
    auto* brand = new QLabel(i18n("Dock.Title"));
    brand->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;")
                             .arg(th::kTextPrimary)
                             .arg(th::kFontTitle));
    header->addWidget(brandIcon);
    header->addWidget(brand);
    header->addStretch();

    statusBadge_ = new QWidget();
    statusBadge_->setObjectName(QStringLiteral("statusBadge"));
    auto* badgeLay = new QHBoxLayout(statusBadge_);
    badgeLay->setContentsMargins(12, 4, 12, 4);
    badgeLay->setSpacing(6);
    statusDot_ = new QLabel(QStringLiteral("●"));
    statusText_ = new QLabel();
    badgeLay->addWidget(statusDot_);
    badgeLay->addWidget(statusText_);
    // Badge cliquable : raccourci pour activer/couper le pilotage auto (ergonomie
    // demandee par David). On garde aussi les boutons Auto ON/OFF en bas.
    statusBadge_->setCursor(Qt::PointingHandCursor);
    statusBadge_->setToolTip(i18n("Control.ToggleAuto"));
    statusBadge_->installEventFilter(this);
    header->addWidget(statusBadge_);
    root->addLayout(header);

    modeLabel_ = new QLabel();
    modeLabel_->setWordWrap(true);
    root->addWidget(modeLabel_);

    // --- Section intervenants ---
    auto* speakersSection = new QLabel(i18n("Section.Speakers"));
    speakersSection->setStyleSheet(sectionLabelQss());
    root->addWidget(speakersSection);

    rowsLayout_ = new QVBoxLayout();
    rowsLayout_->setSpacing(8);
    root->addLayout(rowsLayout_);

    emptyLabel_ = new QLabel(i18n("Empty.NoSources"));
    emptyLabel_->setWordWrap(true);
    emptyLabel_->setStyleSheet(QString("color:%1;").arg(th::kTextSecondary));
    rowsLayout_->addWidget(emptyLabel_);

    // --- Carte "a l'antenne" ---
    auto* onAirCard = new QWidget();
    onAirCard->setObjectName(QStringLiteral("onAirCard"));
    onAirCard->setStyleSheet(QString("#onAirCard { background:%1; border:1px solid %2;"
                                     " border-radius:%3px; }")
                                 .arg(th::kSurface2)
                                 .arg(th::kBorder)
                                 .arg(th::kRadiusCard));
    auto* onAirLay = new QHBoxLayout(onAirCard);
    onAirLay->setContentsMargins(10, 12, 10, 12);
    auto* onAirCaption = new QLabel(i18n("OnAir.Label"));
    onAirCaption->setStyleSheet(
        QString("color:%1; font-size:%2px;").arg(th::kTextTertiary).arg(th::kFontLabel));
    onAirLabel_ = new QLabel();
    onAirLabel_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:600;")
                                   .arg(th::kTextPrimary)
                                   .arg(th::kFontBody));
    onAirLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    onAirLay->addWidget(onAirCaption);
    onAirLay->addStretch();
    onAirLay->addWidget(onAirLabel_);
    root->addWidget(onAirCard);

    // --- Section pilotage ---
    auto* directingSection = new QLabel(i18n("Section.Directing"));
    directingSection->setStyleSheet(sectionLabelQss());
    root->addWidget(directingSection);

    directingLayout_ = new QVBoxLayout();
    directingLayout_->setSpacing(8);
    root->addLayout(directingLayout_);

    // --- Bandeau Stream Deck (icone grid + texte, fond jaune tres subtil) ---
    auto* deckCard = new QWidget();
    deckCard->setObjectName(QStringLiteral("deckCard"));
    deckCard->setStyleSheet(QString("#deckCard { background:%1; border-radius:%2px; }")
                                .arg(rgba(th::kWarning, 0.08))
                                .arg(th::kRadiusButton));
    auto* deckLay = new QHBoxLayout(deckCard);
    deckLay->setContentsMargins(8, 8, 8, 8);
    deckLay->setSpacing(8);
    auto* deckIcon = new QLabel();
    deckIcon->setPixmap(icon(Icon::LayoutGrid, th::kWarning, 14));
    deckIcon->setAlignment(Qt::AlignTop);
    auto* deckHint = new QLabel(i18n("StreamDeck.Hint"));
    deckHint->setWordWrap(true);
    deckHint->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:500;")
                                .arg(th::kWarning)
                                .arg(th::kFontSection));
    deckLay->addWidget(deckIcon);
    deckLay->addWidget(deckHint, 1);
    root->addWidget(deckCard);

    // --- Etat de la config (chemin + statut), selectionnable ---
    configLabel_ = new QLabel();
    configLabel_->setWordWrap(true);
    configLabel_->setStyleSheet(QString("color:%1; font-size:11px;").arg(th::kTextTertiary));
    configLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(configLabel_);

    auto* refresh = new QPushButton(i18n("Config.Refresh"));
    refresh->setCursor(Qt::PointingHandCursor);
    refresh->setStyleSheet(neutralBtnQss());
    connect(refresh, &QPushButton::clicked, this, [this]() { reload(); });
    root->addWidget(refresh);

    // --- Separateur + footer (acces futurs : grises "a venir") ---
    auto* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color:%1;").arg(th::kBorder));
    root->addWidget(sep);

    // Footer : surfaces a venir (Run 5/6), desactivees mais lisibles (texte
    // secondaire comme la maquette, pas le gris-eteint par defaut de Qt).
    const QString footerQss =
        QString("QPushButton { background:%1; border:1px solid %2; border-radius:%3px;"
                " color:%4; font-size:12px; font-weight:600; padding:8px 10px; }"
                "QPushButton:disabled { color:%4; }")
            .arg(th::kSurface3)
            .arg(th::kBorder)
            .arg(th::kRadiusButton)
            .arg(th::kTextSecondary);
    auto* footer = new QHBoxLayout();
    footer->setSpacing(8);

    // Parametres avances : encore a venir (Run 6) -> grise + tooltip.
    auto* settingsBtn = new QPushButton(i18n("Footer.AdvancedSettings"));
    settingsBtn->setIcon(icon(Icon::Settings, th::kTextSecondary, 14));
    settingsBtn->setStyleSheet(footerQss);
    settingsBtn->setEnabled(false);
    settingsBtn->setToolTip(i18n("Footer.ComingSoon"));
    footer->addWidget(settingsBtn, 1);

    // Assistant de configuration (Run 5) : ACTIF. Accent pour inviter au clic.
    auto* assistantBtn = new QPushButton(i18n("Footer.Assistant"));
    assistantBtn->setIcon(icon(Icon::Sparkles, th::kAccent, 14));
    assistantBtn->setCursor(Qt::PointingHandCursor);
    assistantBtn->setStyleSheet(
        QString("QPushButton { background:%1; border:1px solid %2; border-radius:%3px;"
                " color:%4; font-size:12px; font-weight:600; padding:8px 10px; }"
                "QPushButton:hover { border-color:%4; }")
            .arg(th::kSurface3)
            .arg(rgba(th::kAccent, 0.4))
            .arg(th::kRadiusButton)
            .arg(th::kAccent));
    connect(assistantBtn, &QPushButton::clicked, this, [this]() { openAssistant(); });
    footer->addWidget(assistantBtn, 1);

    root->addLayout(footer);

    root->addStretch();

    reload();

    timer_ = new QTimer(this);
    timer_->setInterval(kTickMs);
    connect(timer_, &QTimer::timeout, this, [this]() { tick(); });
    timer_->start();

    // Auto-refresh : OBS termine de charger ses scenes/sources APRES la creation
    // du dock -> on se reabonne pour recharger tout seul (plus besoin de cliquer
    // Rafraichir au lancement). Callback retire dans le destructeur.
    obs_frontend_add_event_callback(frontendEventCb, this);
}

SdDock::~SdDock() {
    // Retrait du callback AVANT toute destruction : plus aucun evenement frontend
    // ne sera route vers ce dock apres ce point.
    obs_frontend_remove_event_callback(frontendEventCb, this);
    if (timer_) {
        timer_->stop();
    }
    monitor_.stop();
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
    return QWidget::eventFilter(watched, event);
}

void SdDock::styleSpeakerCard(Row& row, bool speaking) {
    // Carte : bordure verte si actif, grise sinon. Selecteur #spkCard -> la
    // bordure ne touche QUE la carte (pas de traits parasites sur les enfants).
    row.card->setStyleSheet(QString("#spkCard { background:%1; border:1px solid %2;"
                                    " border-radius:%3px; }")
                                .arg(th::kSurface2)
                                .arg(speaking ? QString::fromUtf8(th::kSuccess) : rgba(th::kBorder, 1.0))
                                .arg(th::kRadiusCard));
    // Avatar : icone user teintee (vert si actif), fond rond translucide.
    row.avatar->setPixmap(icon(Icon::User, speaking ? th::kSuccess : th::kTextTertiary, 18));
    row.avatar->setStyleSheet(
        QString("background:%1; border:2px solid %2; border-radius:%3px;")
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

void SdDock::rebuildDirectingGrid() {
    while (QLayoutItem* item = directingLayout_->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            delete w;
        }
        delete item;
    }
    wideButton_ = nullptr;
    autoOnButton_ = nullptr;
    autoOffButton_ = nullptr;

    // Rangee 1 : un bouton "forcer" par intervenant (mode config seulement).
    if (configMode_ && !displaySpeakers_.empty()) {
        auto* rowFrame = new QWidget();
        auto* rowLay = new QHBoxLayout(rowFrame);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->setSpacing(8);
        for (size_t i = 0; i < displaySpeakers_.size(); ++i) {
            auto* btn = new QPushButton(QString::fromStdString(displaySpeakers_[i].name));
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(neutralBtnQss());
            const int index = static_cast<int>(i);
            connect(btn, &QPushButton::clicked, this,
                    [this, index]() { forceSpeakerByIndex(index); });
            rowLay->addWidget(btn, 1);
        }
        directingLayout_->addWidget(rowFrame);
    }

    // Rangee 2 : Plan large (bleu) / Auto OFF (rouge) / Auto ON (vert).
    auto* row2 = new QWidget();
    auto* row2Lay = new QHBoxLayout(row2);
    row2Lay->setContentsMargins(0, 0, 0, 0);
    row2Lay->setSpacing(8);

    wideButton_ = new QPushButton(i18n("Control.WideShot"));
    wideButton_->setCursor(Qt::PointingHandCursor);
    wideButton_->setStyleSheet(pillBtnQss(th::kAccent, 0.15, 0.5, true));
    connect(wideButton_, &QPushButton::clicked, this, [this]() { forceWide(); });

    autoOffButton_ = new QPushButton(i18n("Control.AutoOff"));
    autoOffButton_->setCursor(Qt::PointingHandCursor);
    connect(autoOffButton_, &QPushButton::clicked, this, [this]() { setAuto(false); });

    autoOnButton_ = new QPushButton(i18n("Control.AutoOn"));
    autoOnButton_->setCursor(Qt::PointingHandCursor);
    connect(autoOnButton_, &QPushButton::clicked, this, [this]() { setAuto(true); });

    row2Lay->addWidget(wideButton_, 1);
    row2Lay->addWidget(autoOffButton_, 1);
    row2Lay->addWidget(autoOnButton_, 1);
    directingLayout_->addWidget(row2);

    const bool hasWide = director_ && !director_->config().wideShotScene.empty();
    wideButton_->setEnabled(configMode_ && hasWide);

    updateAutoButtons();
}

void SdDock::updateAutoButtons() {
    if (!autoOnButton_ || !autoOffButton_) {
        return;
    }
    // En mode config, les DEUX boutons restent cliquables (cliquer celui de l'etat
    // courant = no-op inoffensif) -> evite le texte grise illisible d'un bouton
    // desactive sur fond colore. En lecture seule : desactives (pas de pilotage).
    autoOnButton_->setEnabled(configMode_);
    autoOffButton_->setEnabled(configMode_);

    if (!configMode_) {
        autoOnButton_->setStyleSheet(pillBtnQss(th::kSuccess, 0.08, 0.4, false));
        autoOffButton_->setStyleSheet(pillBtnQss(th::kDanger, 0.08, 0.4, false));
        return;
    }
    // L'etat courant est "plein" (fond marque + bordure nette + gras) ; l'autre
    // reste en teinte douce mais lisible (texte couleur pleine).
    autoOnButton_->setStyleSheet(
        pillBtnQss(th::kSuccess, autoEnabled_ ? 0.22 : 0.10, autoEnabled_ ? 0.7 : 0.45, autoEnabled_));
    autoOffButton_->setStyleSheet(
        pillBtnQss(th::kDanger, !autoEnabled_ ? 0.20 : 0.10, !autoEnabled_ ? 0.7 : 0.45, !autoEnabled_));
}

void SdDock::reload() {
    monitor_.start();
    const std::vector<std::string> names = monitor_.sourceNames();

    const sd::obsbridge::ConfigLoadResult loaded = sd::obsbridge::loadConfig();

    sd::core::Config cfg;
    displaySpeakers_.clear();
    if (loaded.parsed && !loaded.config.speakers.empty()) {
        configMode_ = true;
        cfg = loaded.config;
        for (const auto& sp : cfg.speakers) {
            displaySpeakers_.push_back({sp.id, sp.name, sp.audioSource});
        }
    } else {
        configMode_ = false;
        for (const auto& name : names) {
            sd::core::Speaker sp;
            sp.id = name;
            sp.name = name;
            sp.audioSource = name;
            cfg.speakers.push_back(sp);
            displaySpeakers_.push_back({name, name, name});
        }
    }

    director_ = std::make_unique<sd::core::Director>(cfg);
    if (!configMode_) {
        autoEnabled_ = false;  // GARDE-FOU : pas de pilotage sans config.
    }
    director_->setAutoEnabled(autoEnabled_);

    // Reapplique les seuils regles au slider : le Director vient d'etre recree
    // (donc remis aux defauts), mais les reglages live de l'utilisateur sont
    // memorises dans thresholdOverrides_ -> on les restaure pour qu'un reload
    // (manuel ou auto-refresh OBS) ne les perde pas. Les ids absents de la config
    // courante sont ignores par setSpeakerThreshold (no-op).
    for (const auto& kv : thresholdOverrides_) {
        director_->setSpeakerThreshold(kv.first, kv.second);
    }

    // Libelle de config (chemin exact + etat) — traduit + selectionnable.
    const QString path = QString::fromStdString(loaded.path);
    if (loaded.parsed) {
        configLabel_->setText(
            i18n("Config.Loaded").arg(static_cast<int>(loaded.config.speakers.size())) +
            QStringLiteral("\n") + path);
    } else if (loaded.fileFound) {
        configLabel_->setText(i18n("Config.Invalid").arg(QString::fromStdString(loaded.error)) +
                              QStringLiteral("\n") + path);
    } else {
        configLabel_->setText(i18n("Config.Missing") + QStringLiteral("\n") + path);
    }

    // Reconstruit les cartes intervenants (suppression SYNCHRONE).
    for (auto& row : rows_) {
        delete row.card;
    }
    rows_.clear();
    shownOnAir_ = "\x01";

    emptyLabel_->setVisible(displaySpeakers_.empty());

    for (size_t i = 0; i < displaySpeakers_.size(); ++i) {
        const DisplaySpeaker& ds = displaySpeakers_[i];

        auto* card = new QWidget();
        card->setObjectName(QStringLiteral("spkCard"));
        // Hauteur mini : la carte ne se laisse pas ecraser quand le dock est reduit
        // (le contenu ne sera pas coupe ; le defilement prend le relais).
        card->setMinimumHeight(th::kAvatarSize + 20);
        auto* lay = new QHBoxLayout(card);
        lay->setContentsMargins(10, 10, 10, 10);
        lay->setSpacing(10);

        // Avatar : icone user dans une pastille ronde.
        auto* avatar = new QLabel();
        avatar->setFixedSize(th::kAvatarSize, th::kAvatarSize);
        avatar->setAlignment(Qt::AlignCenter);

        // Centre : nom + etat (ligne) puis vumetre (custom + marqueur de seuil).
        auto* center = new QWidget();
        auto* centerLay = new QVBoxLayout(center);
        centerLay->setContentsMargins(0, 0, 0, 0);
        centerLay->setSpacing(6);
        auto* nameRow = new QWidget();
        auto* nameRowLay = new QHBoxLayout(nameRow);
        nameRowLay->setContentsMargins(0, 0, 0, 0);
        nameRowLay->setSpacing(6);
        auto* nameLabel = new QLabel(QString::fromStdString(ds.name));
        auto* stateLabel = new QLabel(i18n("Speaker.State.Silent"));
        stateLabel->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;")
                                      .arg(th::kTextTertiary)
                                      .arg(th::kFontSmall));
        nameRowLay->addWidget(nameLabel);
        nameRowLay->addStretch();
        nameRowLay->addWidget(stateLabel);
        auto* meter = new LevelMeter();
        centerLay->addWidget(nameRow);
        centerLay->addWidget(meter);

        // Droite : bloc "Seuil" (label + slider). Le slider regle EN DIRECT le
        // seuil de cet intervenant ET deplace le marqueur sur le vumetre.
        auto* threshBlock = new QWidget();
        threshBlock->setFixedWidth(96);
        auto* threshLay = new QVBoxLayout(threshBlock);
        threshLay->setContentsMargins(0, 0, 0, 0);
        threshLay->setSpacing(4);
        auto* threshLabel = new QLabel(i18n("Speaker.Threshold"));
        threshLabel->setStyleSheet(
            QString("color:%1; font-size:%2px;").arg(th::kTextTertiary).arg(th::kFontSmall));
        auto* threshold = new QSlider(Qt::Horizontal);
        // Borne basse = kFloorDb + 1 (et non kFloorDb) : tout en bas, le seuil
        // resterait AU plancher de silence -> avec le comparateur "niveau >= seuil"
        // une source muette (niveau == plancher) passerait pour "parle" en
        // permanence. On garde donc le minimum un cran au-dessus du silence absolu.
        threshold->setRange(kFloorDb + 1, 0);  // dB : [-59, 0]
        int initThresh =
            director_ ? static_cast<int>(std::lround(director_->speakerThresholdDb(ds.id))) : -35;
        if (initThresh < kFloorDb + 1) {
            initThresh = kFloorDb + 1;  // clamp si une config descend au plancher
        }
        threshold->setValue(initThresh);
        threshold->setStyleSheet(sliderQss(false));
        meter->setThresholdDb(initThresh);  // marqueur initial sur le vumetre
        const std::string sid = ds.id;
        connect(threshold, &QSlider::valueChanged, this, [this, sid, meter](int v) {
            thresholdOverrides_[sid] = static_cast<double>(v);  // memorise (survit aux reloads)
            if (director_) {
                director_->setSpeakerThreshold(sid, static_cast<double>(v));
            }
            meter->setThresholdDb(v);  // le marqueur suit le slider en direct
        });
        threshLay->addWidget(threshLabel);
        threshLay->addWidget(threshold);

        lay->addWidget(avatar);
        lay->addWidget(center, 1);
        lay->addWidget(threshBlock);

        rowsLayout_->addWidget(card);
        Row row{ds.id, ds.audioSource, card, avatar, nameLabel, meter, threshold, stateLabel};
        styleSpeakerCard(row, /*speaking=*/false);
        rows_.push_back(row);
    }

    rebuildDirectingGrid();
    updateModeLabel();
    shownStatus_ = -1;
    updateStatusBadge();
}

void SdDock::updateStatusBadge() {
    const int status =
        !configMode_ ? StatusReadOnly : (autoEnabled_ ? StatusActive : StatusPaused);
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
        fillA = 1.0;  // lecture seule : fond surface3 plein
    }
    const QString fill = (status == StatusReadOnly) ? QString::fromUtf8(th::kSurface3)
                                                    : rgba(color, fillA);
    statusBadge_->setStyleSheet(QString("#statusBadge { background:%1; border:1px solid %2;"
                                        " border-radius:6px; }")
                                    .arg(fill)
                                    .arg(rgba(borderC, status == StatusReadOnly ? 1.0 : 0.5)));
    statusDot_->setStyleSheet(QString("color:%1; font-size:%2px;").arg(color).arg(th::kFontSmall));
    statusText_->setText(text);
    statusText_->setStyleSheet(
        QString("color:%1; font-size:%2px; font-weight:700;").arg(color).arg(th::kFontLabel));
}

void SdDock::updateModeLabel() {
    if (!modeLabel_) {
        return;
    }
    if (!configMode_) {
        modeLabel_->setText(i18n("Mode.ReadOnly"));
        modeLabel_->setStyleSheet(QString("color:%1;").arg(th::kTextSecondary));
        return;
    }
    if (autoEnabled_) {
        modeLabel_->setText(i18n("Mode.Auto.On"));
        modeLabel_->setStyleSheet(QString("color:%1; font-weight:600;").arg(th::kSuccess));
    } else {
        modeLabel_->setText(i18n("Mode.Auto.Off"));
        modeLabel_->setStyleSheet(QString("color:%1; font-weight:600;").arg(th::kWarning));
    }
}

void SdDock::setAuto(bool on) {
    if (on && !configMode_) {
        return;  // GARDE-FOU : pas d'activation sans config (couvre la hotkey).
    }
    autoEnabled_ = on;
    if (director_) {
        director_->setAutoEnabled(on);
    }
    updateAutoButtons();
    updateModeLabel();
    updateStatusBadge();
}

void SdDock::toggleAuto() {
    setAuto(!autoEnabled_);
}

void SdDock::openAssistant() {
    // Parente la modale a la fenetre principale d'OBS (centrage + modalite propre).
    auto* mainWin = static_cast<QWidget*>(obs_frontend_get_main_window());
    SdAssistant dlg(mainWin ? mainWin : this);
    if (dlg.exec() == QDialog::Accepted) {
        // L'assistant a ecrit le config.json -> on recharge tout (intervenants,
        // scenes, reglages). Puis, s'il a termine par "Activer", on lance le pilotage.
        reload();
        if (dlg.activateAutoRequested()) {
            setAuto(true);
        }
    }
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
    sceneSwitcher_.switchTo(decision.scene.empty() ? wide : decision.scene);
}

void SdDock::forceSpeakerByIndex(int index) {
    if (!director_ || index < 0 || index >= static_cast<int>(displaySpeakers_.size())) {
        return;
    }
    const sd::core::Decision decision =
        director_->forceSpeaker(nowSeconds(), displaySpeakers_[static_cast<size_t>(index)].id);
    if (!decision.scene.empty()) {
        sceneSwitcher_.switchTo(decision.scene);
    }
}

void SdDock::tick() {
    if (!director_) {
        return;
    }

    const double now = nowSeconds();
    const std::map<std::string, double> levelsBySource = monitor_.snapshot(now);
    std::map<std::string, double> levelsById;
    for (const auto& ds : displaySpeakers_) {
        const auto it = levelsBySource.find(ds.audioSource);
        levelsById[ds.id] =
            (it != levelsBySource.end()) ? it->second : static_cast<double>(kFloorDb);
    }

    const sd::core::Decision decision = director_->update(now, levelsById);

    const std::string onAir = sceneSwitcher_.currentProgramScene();
    applyDecision(decision, onAir);

    const std::vector<std::string> speaking = director_->speakingIds();
    const std::unordered_set<std::string> speakingSet(speaking.begin(), speaking.end());

    for (auto& row : rows_) {
        const auto it = levelsBySource.find(row.audioSource);
        const double db = (it != levelsBySource.end()) ? it->second : static_cast<double>(kFloorDb);

        const int dbInt = dbToInt(db);
        if (dbInt != row.shownDb) {
            row.shownDb = dbInt;
            row.meter->setLevelDb(db);
        }

        const int speakingNow = speakingSet.count(row.id) ? 1 : 0;
        if (speakingNow != row.shownSpeaking) {
            row.shownSpeaking = speakingNow;
            row.stateLabel->setText(speakingNow ? i18n("Speaker.State.Speaking")
                                                : i18n("Speaker.State.Silent"));
            row.stateLabel->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700;")
                                              .arg(speakingNow ? th::kSuccess : th::kTextTertiary)
                                              .arg(th::kFontSmall));
            styleSpeakerCard(row, speakingNow != 0);
        }
    }

    if (onAir != shownOnAir_) {
        shownOnAir_ = onAir;
        onAirLabel_->setText(onAir.empty() ? i18n("OnAir.Unavailable")
                                           : QString::fromStdString(onAir));
    }
}

}  // namespace sd::ui
