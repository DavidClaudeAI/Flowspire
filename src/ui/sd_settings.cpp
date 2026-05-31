// StreamDirector — fenetre Parametres avances (implementation, Run 6).
// Voir sd_settings.hpp. Fenetre modale frameless a sidebar gauche (maquette `QbX5t`).
// Edite une config existante via les panneaux PARTAGES (sd_config_panels) -> meme
// rendu que l'assistant. "Termine" ecrit le config.json (atomique) ; l'appelant
// (le dock) recharge.
#include "ui/sd_settings.hpp"

#include <QColor>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QString>
#include <QVBoxLayout>

#include <memory>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "obs/config_loader.hpp"
#include "obs/obs_inventory.hpp"
#include "ui/sd_config_panels.hpp"
#include "ui/sd_i18n.hpp"
#include "ui/sd_icons.hpp"
#include "ui/sd_style.hpp"
#include "ui/sd_support.hpp"
#include "ui/sd_theme.hpp"
#include "ui/sd_widgets.hpp"

namespace sd::ui {

namespace th = sd::ui::theme;
using namespace sd::ui::widgets;

namespace {
// Entrees de la sidebar (ordre maquette). Profils = livre au Run 7 (entree visible,
// panneau "a venir" pour l'instant). Soutenir = action (ouvre la pop-up de don).
enum Panel { PanelSpeakers, PanelCameras, PanelWide, PanelRhythm, PanelProfiles };
}  // namespace

struct SdSettings::Impl {
    SdSettings* q = nullptr;

    sd::core::Config working;
    std::vector<std::string> audioSources;
    std::vector<std::string> scenes;
    std::unique_ptr<ConfigPanels> panels;
    bool saved = false;

    QWidget* root = nullptr;
    QWidget* header = nullptr;
    QVBoxLayout* navLay = nullptr;
    QLabel* contentTitle = nullptr;
    QVBoxLayout* contentLay = nullptr;  // layout du widget scrollable de contenu
    int selPanel = PanelSpeakers;

    bool dragging = false;
    QPoint dragOffset;
    bool centered = false;

    QString panelTitle(int p) const;
    QWidget* makeNavItem(Icon ic, const QString& label, bool active, std::function<void()> onClick,
                         const char* tint = nullptr);
    void rebuildNav();
    void showPanel(int p);
    void resetToDefaults();
    void finish();
};

QString SdSettings::Impl::panelTitle(int p) const {
    switch (p) {
    case PanelSpeakers: return i18n("Settings.Nav.Speakers");
    case PanelCameras: return i18n("Settings.Nav.Cameras");
    case PanelWide: return i18n("Settings.Nav.Wide");
    case PanelRhythm: return i18n("Settings.Nav.Rhythm");
    case PanelProfiles: return i18n("Settings.Nav.Profiles");
    default: return QString();
    }
}

// Ligne de sidebar : icone + label cales a GAUCHE, fond accent translucide si
// active. `tint` force une couleur (ex. rouge pour "Soutenir") sinon accent (actif)
// ou secondaire (inactif).
QWidget* SdSettings::Impl::makeNavItem(Icon ic, const QString& label, bool active,
                                       std::function<void()> onClick, const char* tint) {
    auto* item = new ClickButton();
    item->setMargins(10, 9);
    item->setFillLeft();  // pleine largeur, contenu a gauche
    const char* fg = tint ? tint : (active ? th::kAccent : th::kTextSecondary);
    item->setIconPix(icon(ic, fg, 15));
    item->setLabel(label, fg, 12, active ? 700 : 600);
    const QString base = QStringLiteral("#clickbtn { background:%1; border-radius:6px; }");
    item->setBox(base.arg(active ? rgba(th::kAccent, 0.15) : QStringLiteral("transparent")),
                 base.arg(active ? rgba(th::kAccent, 0.15) : rgba("#FFFFFF", 0.05)));
    item->setOnClick(std::move(onClick));
    return item;
}

void SdSettings::Impl::rebuildNav() {
    clearLayout(navLay);
    const struct {
        Icon ic;
        int panel;
    } items[] = {
        {Icon::Users, PanelSpeakers},
        {Icon::Video, PanelCameras},
        {Icon::LayoutGrid, PanelWide},
        {Icon::Clock, PanelRhythm},
        {Icon::Bookmark, PanelProfiles},
    };
    for (const auto& it : items) {
        const int p = it.panel;
        navLay->addWidget(makeNavItem(it.ic, panelTitle(p), selPanel == p, [this, p]() {
            selPanel = p;
            rebuildNav();
            showPanel(p);
        }));
    }
    // Separateur + "Soutenir le projet" (action : ouvre la pop-up de don).
    auto* sep = new QFrame();
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(th::kBorder));
    navLay->addWidget(sep);
    navLay->addWidget(makeNavItem(Icon::Heart, i18n("Settings.Nav.Support"), false,
                                  [this]() { showSupportDialog(q); }, th::kDanger));
    navLay->addStretch();
}

void SdSettings::Impl::showPanel(int p) {
    clearLayout(contentLay);
    contentTitle->setText(panelTitle(p));

    // Hote du panneau (sans marges) ; les panneaux partages s'y montent.
    auto* hostW = new QWidget();
    auto* host = new QVBoxLayout(hostW);
    host->setContentsMargins(0, 0, 0, 0);
    host->setSpacing(p == PanelSpeakers ? 10 : 14);

    switch (p) {
    case PanelSpeakers: panels->mountSpeakers(host); break;
    case PanelCameras: panels->mountCameras(host); break;
    case PanelWide: panels->mountWide(host); break;
    case PanelRhythm: panels->mountRhythm(host); break;
    case PanelProfiles:
        // Profils : fenetre dediee livree au Run 7. Panneau d'attente clair.
        host->addWidget(makeHint(i18n("Settings.Profiles.ComingSoon")));
        break;
    default: break;
    }
    contentLay->addWidget(hostW);
    contentLay->addStretch();
}

void SdSettings::Impl::resetToDefaults() {
    // Reinitialise les REGLAGES FINS (rythme, sensibilite, poids) aux defauts, en
    // conservant intervenants / scenes / plan large (la structure du plateau).
    const sd::core::Config def;  // valeurs par defaut de la table de reglage
    working.timing = def.timing;
    working.audio = def.audio;
    working.whenMultiple = def.whenMultiple;
    working.whenSilence = def.whenSilence;
    showPanel(selPanel);
}

void SdSettings::Impl::finish() {
    const sd::obsbridge::ConfigSaveResult res = sd::obsbridge::saveConfig(sanitizedConfig(working));
    if (res.saved) {
        saved = true;
        q->accept();
    } else {
        q->reject();  // echec d'ecriture : on n'affirme pas un enregistrement faux
    }
}

// ===========================================================================
SdSettings::SdSettings(QWidget* parent) : QDialog(parent), d_(new Impl) {
    d_->q = this;
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setWindowTitle(QStringLiteral("StreamDirector"));

    d_->audioSources = sd::obsbridge::audioSourceNames();
    d_->scenes = sd::obsbridge::sceneNames();
    const sd::obsbridge::ConfigLoadResult loaded = sd::obsbridge::loadConfig();
    if (loaded.parsed) {
        d_->working = loaded.config;
    }
    d_->panels = std::make_unique<ConfigPanels>(d_->working, d_->audioSources, d_->scenes);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 18, 24, 30);

    d_->root = new QWidget(this);
    d_->root->setObjectName(QStringLiteral("settingsRoot"));
    d_->root->setFixedWidth(680);
    d_->root->setStyleSheet(QString("#settingsRoot { background:%1; border:1px solid %2;"
                                    " border-radius:12px; }")
                                .arg(th::kSurface1)
                                .arg(th::kBorder));
    auto* shadow = new QGraphicsDropShadowEffect(d_->root);
    shadow->setBlurRadius(50);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 18);
    d_->root->setGraphicsEffect(shadow);
    outer->addWidget(d_->root);

    d_->root->setMinimumHeight(520);  // place pour la sidebar + un panneau confortable

    auto* rootLay = new QVBoxLayout(d_->root);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    // --- En-tete ---
    d_->header = new QWidget();
    d_->header->setObjectName(QStringLiteral("setHeader"));
    d_->header->setStyleSheet(QString("#setHeader { background:%1; border-top-left-radius:12px;"
                                      " border-top-right-radius:12px; }")
                                  .arg(th::kSurfaceBar));
    auto* hLay = new QHBoxLayout(d_->header);
    hLay->setContentsMargins(20, 16, 20, 16);
    hLay->setSpacing(8);
    auto* hIcon = new QLabel();
    hIcon->setPixmap(icon(Icon::Settings, th::kAccent, 18));
    auto* hTitle = new QLabel(i18n("Settings.Title"));
    hTitle->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700;").arg(th::kTextPrimary));
    auto* close = new QPushButton();
    close->setIcon(icon(Icon::X, th::kTextTertiary, 16));
    close->setCursor(Qt::PointingHandCursor);
    close->setFlat(true);
    close->setStyleSheet(iconBtnQss());
    close->setToolTip(i18n("Assistant.Close"));
    connect(close, &QPushButton::clicked, this, [this]() { reject(); });
    hLay->addWidget(hIcon);
    hLay->addWidget(hTitle);
    hLay->addStretch();
    hLay->addWidget(close);
    rootLay->addWidget(d_->header);

    auto* sepTop = new QFrame();
    sepTop->setFixedHeight(1);
    sepTop->setStyleSheet(QString("background:%1;").arg(th::kBorder));
    rootLay->addWidget(sepTop);

    // --- Corps : sidebar + contenu ---
    auto* body = new QWidget();
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);

    auto* nav = new QWidget();
    nav->setObjectName(QStringLiteral("setNav"));
    nav->setFixedWidth(200);
    nav->setStyleSheet(QString("#setNav { background:%1; }").arg(th::kBg));
    d_->navLay = new QVBoxLayout(nav);
    d_->navLay->setContentsMargins(12, 12, 12, 12);
    d_->navLay->setSpacing(4);
    bodyLay->addWidget(nav);

    // Zone de contenu : titre + zone scrollable.
    auto* contentOuter = new QWidget();
    auto* contentOuterLay = new QVBoxLayout(contentOuter);
    contentOuterLay->setContentsMargins(20, 18, 20, 18);
    contentOuterLay->setSpacing(14);
    d_->contentTitle = new QLabel();
    d_->contentTitle->setStyleSheet(
        QString("color:%1; font-size:15px; font-weight:700;").arg(th::kTextPrimary));
    contentOuterLay->addWidget(d_->contentTitle);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background:transparent; border:none; }"));
    auto* scrollContent = new QWidget();
    scrollContent->setStyleSheet(QStringLiteral("background:transparent;"));
    d_->contentLay = new QVBoxLayout(scrollContent);
    d_->contentLay->setContentsMargins(0, 0, 0, 0);
    d_->contentLay->setSpacing(14);
    scroll->setWidget(scrollContent);
    contentOuterLay->addWidget(scroll, 1);
    bodyLay->addWidget(contentOuter, 1);

    rootLay->addWidget(body, 1);

    // --- Pied : Reinitialiser (gauche) + Termine (droite) ---
    auto* sepBot = new QFrame();
    sepBot->setFixedHeight(1);
    sepBot->setStyleSheet(QString("background:%1;").arg(th::kBorder));
    rootLay->addWidget(sepBot);
    auto* foot = new QWidget();
    foot->setObjectName(QStringLiteral("setFooter"));
    foot->setStyleSheet(QString("#setFooter { background:%1; border-bottom-left-radius:12px;"
                                " border-bottom-right-radius:12px; }")
                            .arg(th::kSurfaceBar));
    auto* fLay = new QHBoxLayout(foot);
    fLay->setContentsMargins(20, 14, 20, 14);
    fLay->setSpacing(12);
    auto* reset = new QPushButton(i18n("Settings.Reset"));
    reset->setIcon(icon(Icon::RotateCcw, th::kTextSecondary, 13));
    reset->setCursor(Qt::PointingHandCursor);
    reset->setStyleSheet(secondaryBtnQss());
    connect(reset, &QPushButton::clicked, this, [this]() { d_->resetToDefaults(); });
    auto* done = new QPushButton(i18n("Settings.Done"));
    done->setCursor(Qt::PointingHandCursor);
    done->setStyleSheet(primaryBtnQss());
    connect(done, &QPushButton::clicked, this, [this]() { d_->finish(); });
    fLay->addWidget(reset);
    fLay->addStretch();
    fLay->addWidget(done);
    rootLay->addWidget(foot);

    d_->rebuildNav();
    d_->showPanel(d_->selPanel);
}

SdSettings::~SdSettings() = default;

bool SdSettings::savedConfig() const {
    return d_->saved;
}

void SdSettings::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        for (QWidget* w = childAt(event->position().toPoint()); w; w = w->parentWidget()) {
            if (w == d_->header) {
                d_->dragging = true;
                d_->dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
                break;
            }
        }
    }
    QDialog::mousePressEvent(event);
}

void SdSettings::mouseMoveEvent(QMouseEvent* event) {
    if (d_->dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - d_->dragOffset);
    }
    QDialog::mouseMoveEvent(event);
}

void SdSettings::mouseReleaseEvent(QMouseEvent* event) {
    d_->dragging = false;
    QDialog::mouseReleaseEvent(event);
}

void SdSettings::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (!d_->centered) {
        if (QWidget* p = parentWidget()) {
            const QRect pg = p->geometry();
            move(pg.center().x() - width() / 2, pg.center().y() - height() / 2);
        }
        d_->centered = true;
    }
}

}  // namespace sd::ui
