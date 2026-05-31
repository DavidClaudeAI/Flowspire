// StreamDirector — assistant de configuration visuel (implementation, Run 5/6).
// Voir sd_assistant.hpp pour le contrat. Fenetre modale frameless, 6 ecrans
// (QStackedWidget), fidele a la maquette Pencil. Ecrit le config.json a la fin.
//
// Run 6 : les 4 etapes editables (Intervenants / Scenes / Plan large / Rythme)
// sont desormais rendues par les PANNEAUX PARTAGES (sd_config_panels), eux-memes
// batis sur les briques partagees (sd_widgets) -> meme rendu que les parametres
// avances, zero duplication. L'assistant ne garde que son habillage : coquille
// (en-tete, barre de progression, pied Precedent/Suivant), prerequis, resume.
#include "ui/sd_assistant.hpp"

#include <QColor>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "obs/config_loader.hpp"
#include "obs/obs_inventory.hpp"
#include "obs/profiles_store.hpp"
#include "ui/sd_config_panels.hpp"
#include "ui/sd_i18n.hpp"
#include "ui/sd_icons.hpp"
#include "ui/sd_style.hpp"
#include "ui/sd_theme.hpp"
#include "ui/sd_widgets.hpp"

namespace sd::ui {

namespace th = sd::ui::theme;

// Briques d'UI partagees (widgets, helpers, QSS) : voir ui/sd_widgets.hpp.
using namespace sd::ui::widgets;

// ===========================================================================
// Etat interne (pimpl).
// ===========================================================================
struct SdAssistant::Impl {
    SdAssistant* q = nullptr;

    // Coquille
    QWidget* root = nullptr;
    QWidget* header = nullptr;
    QLabel* headerIcon = nullptr;
    QLabel* headerTitle = nullptr;
    QPushButton* closeBtn = nullptr;
    QWidget* progressRow = nullptr;
    std::vector<QFrame*> segments;
    QStackedWidget* stack = nullptr;
    QVBoxLayout* contentLay[6] = {};
    QWidget* footer = nullptr;
    QPushButton* backBtn = nullptr;
    QPushButton* nextBtn = nullptr;

    // Donnees
    sd::core::Config working;
    std::vector<std::string> audioSources;
    std::vector<std::string> scenes;
    bool activateAuto = false;
    // Nom du nouveau profil a creer A LA FIN (vide -> edition du profil actif).
    QString newProfileName;

    // Panneaux d'edition PARTAGES avec les parametres avances : editent `working`
    // par reference -> source unique du rendu, zero duplication.
    std::unique_ptr<ConfigPanels> panels;

    // Navigation / etat de page
    int current = 0;
    QLabel* summaryError = nullptr;

    // Fenetre frameless : deplacement par la barre de titre.
    bool dragging = false;
    QPoint dragOffset;
    bool centered = false;

    // Libelle d'un intervenant (nom, ou "Intervenant N" si vide) — pour le resume.
    QString speakerLabel(std::size_t i) const {
        const std::string& n = working.speakers[i].name;
        return n.empty() ? i18n("Speakers.DefaultName").arg(static_cast<int>(i) + 1)
                         : QString::fromStdString(n);
    }

    // Cree un hote (QVBoxLayout sans marges) dans la page pour y monter un panneau
    // partage ; la page garde la main sur titre/intro/banniere + son stretch.
    QVBoxLayout* addPanelHost(QVBoxLayout* page, int spacing);

    // --- navigation ---
    void goTo(int i);
    void updateHeader();
    void updateFooter();
    void populate(int i);

    // --- pages ---
    void populatePrereq();
    void populateSpeakers();
    void populateCameras();
    void populateWide();
    void populateRhythm();
    void populateSummary();
    QWidget* recapCard(Icon ic, const QString& title, int page, const QStringList& lines);
    QString stepName(int step) const;

    // --- fin ---
    void finish();
    void showError(const QString& msg);
};

QVBoxLayout* SdAssistant::Impl::addPanelHost(QVBoxLayout* page, int spacing) {
    auto* hostW = new QWidget();
    auto* hostLay = new QVBoxLayout(hostW);
    hostLay->setContentsMargins(0, 0, 0, 0);
    hostLay->setSpacing(spacing);
    page->addWidget(hostW);
    return hostLay;
}

QString SdAssistant::Impl::stepName(int step) const {
    switch (step) {
    case 1: return i18n("Assistant.Step.Speakers");
    case 2: return i18n("Assistant.Step.Cameras");
    case 3: return i18n("Assistant.Step.Wide");
    case 4: return i18n("Assistant.Step.Rhythm");
    case 5: return i18n("Assistant.Step.Summary");
    default: return QString();
    }
}

void SdAssistant::Impl::goTo(int i) {
    if (i < 0 || i > 5) {
        return;
    }
    current = i;
    populate(i);
    stack->setCurrentIndex(i);
    updateHeader();
    updateFooter();
    // Fenetre a FOND TRANSLUCIDE (coins arrondis + ombre). Changer le libelle d'un
    // bouton du pied modifie sa largeur ; la zone liberee n'est pas re-invalidee
    // toute seule sur un top-level translucide -> fantome du texte precedent.
    // update() est ASYNCHRONE (peut s'executer avant le recalcul de disposition) :
    // on force donc la disposition (activate) puis une re-peinture SYNCHRONE.
    if (q->layout()) {
        q->layout()->activate();
    }
    q->repaint();
}

void SdAssistant::Impl::populate(int i) {
    switch (i) {
    case 0: populatePrereq(); break;
    case 1: populateSpeakers(); break;
    case 2: populateCameras(); break;
    case 3: populateWide(); break;
    case 4: populateRhythm(); break;
    case 5: populateSummary(); break;
    default: break;
    }
}

void SdAssistant::Impl::updateHeader() {
    if (current == 0) {
        headerIcon->setVisible(true);
        headerTitle->setText(i18n("Assistant.Title"));
        headerTitle->setStyleSheet(
            QString("color:%1; font-size:13px; font-weight:600;").arg(th::kTextPrimary));
        progressRow->setVisible(false);
    } else {
        headerIcon->setVisible(false);
        headerTitle->setText(i18n("Assistant.StepCounter").arg(current).arg(stepName(current)));
        headerTitle->setStyleSheet(
            QString("color:%1; font-size:13px; font-weight:600;").arg(th::kTextSecondary));
        progressRow->setVisible(true);
        for (int s = 0; s < static_cast<int>(segments.size()); ++s) {
            segments[s]->setStyleSheet(segQss(s < current));
        }
    }
}

void SdAssistant::Impl::updateFooter() {
    backBtn->setVisible(current != 0);
    if (current == 5) {
        backBtn->setText(i18n("Nav.Restart"));
        backBtn->setVisible(true);
        nextBtn->setText(i18n("Nav.Activate"));
        nextBtn->setIcon(icon(Icon::Play, th::kOnAccent, 15));
        nextBtn->setStyleSheet(greenBtnQss());
    } else {
        backBtn->setText(i18n("Nav.Back"));
        nextBtn->setStyleSheet(primaryBtnQss());
        if (current == 0) {
            nextBtn->setText(i18n("Nav.Start"));
        } else if (current == 4) {
            nextBtn->setText(i18n("Nav.ToSummary"));
        } else {
            nextBtn->setText(i18n("Nav.Next"));
        }
        nextBtn->setIcon(icon(Icon::ArrowRight, th::kOnAccent, 15));
    }
}

// --- Etape 0 : prerequis -----------------------------------------------------
void SdAssistant::Impl::populatePrereq() {
    clearLayout(contentLay[0]);
    auto* big = new QLabel();
    big->setPixmap(icon(Icon::Clapperboard, th::kAccent, 40));
    contentLay[0]->addWidget(big);
    contentLay[0]->addWidget(makeTitle(i18n("Prereq.Title")));
    contentLay[0]->addWidget(makeSub(i18n("Prereq.Intro")));

    auto* card = makeCard(QStringLiteral("prereqCard"));
    auto* cl = new QVBoxLayout(card);
    cl->setContentsMargins(14, 14, 14, 14);
    cl->setSpacing(14);
    const struct {
        Icon ic;
        const char* title;
        const char* desc;
    } items[] = {
        {Icon::LayoutGrid, "Prereq.Scenes.Title", "Prereq.Scenes.Desc"},
        {Icon::Mic, "Prereq.Audio.Title", "Prereq.Audio.Desc"},
    };
    for (const auto& it : items) {
        auto* row = new QWidget();
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(10);
        auto* ic = new QLabel();
        ic->setPixmap(icon(it.ic, th::kSuccess, 18));
        ic->setAlignment(Qt::AlignTop);
        auto* txt = new QWidget();
        auto* tl = new QVBoxLayout(txt);
        tl->setContentsMargins(0, 0, 0, 0);
        tl->setSpacing(2);
        auto* t = new QLabel(i18n(it.title));
        t->setStyleSheet(QString("color:%1; font-size:13px; font-weight:600;").arg(th::kTextPrimary));
        auto* d = new QLabel(i18n(it.desc));
        d->setWordWrap(true);
        d->setStyleSheet(QString("color:%1; font-size:12px;").arg(th::kTextSecondary));
        tl->addWidget(t);
        tl->addWidget(d);
        rl->addWidget(ic);
        rl->addWidget(txt, 1);
        cl->addWidget(row);
    }
    contentLay[0]->addWidget(card);
    contentLay[0]->addStretch();
}

// --- Etapes 1-4 : panneaux d'edition PARTAGES (sd_config_panels) -------------
// L'assistant ajoute son habillage (titre, intro, banniere, stretch) autour d'un
// hote, puis monte le panneau partage dedans. Meme rendu que les parametres avances.

void SdAssistant::Impl::populateSpeakers() {
    clearLayout(contentLay[1]);
    contentLay[1]->addWidget(makeTitle(i18n("Speakers.Title")));
    contentLay[1]->addWidget(makeSub(i18n("Speakers.Intro")));
    panels->mountSpeakers(addPanelHost(contentLay[1], 10));
    contentLay[1]->addStretch();
}

void SdAssistant::Impl::populateCameras() {
    clearLayout(contentLay[2]);
    contentLay[2]->addWidget(makeTitle(i18n("Cameras.Title")));
    contentLay[2]->addWidget(makeSub(i18n("Cameras.Intro")));
    panels->mountCameras(addPanelHost(contentLay[2], 14));
    contentLay[2]->addStretch();
}

void SdAssistant::Impl::populateWide() {
    clearLayout(contentLay[3]);
    contentLay[3]->addWidget(makeTitle(i18n("Wide.Title")));
    contentLay[3]->addWidget(makeSub(i18n("Wide.Intro")));
    panels->mountWide(addPanelHost(contentLay[3], 14));
    contentLay[3]->addStretch();
}

void SdAssistant::Impl::populateRhythm() {
    clearLayout(contentLay[4]);
    contentLay[4]->addWidget(makeTitle(i18n("Rhythm.Title")));
    contentLay[4]->addWidget(makeSub(i18n("Rhythm.Intro")));
    panels->mountRhythm(addPanelHost(contentLay[4], 14));

    // Banniere d'info accent (specifique a l'assistant ; voir maquette etape Rythme).
    auto* banner = new QWidget();
    banner->setObjectName(QStringLiteral("infoBanner"));
    banner->setStyleSheet(QString("#infoBanner { background:%1; border-radius:6px; }")
                              .arg(rgba(th::kAccent, 0.10)));
    auto* bl = new QHBoxLayout(banner);
    bl->setContentsMargins(12, 10, 12, 10);
    bl->setSpacing(8);
    auto* bIcon = new QLabel();
    bIcon->setPixmap(icon(Icon::Info, th::kAccent, 15));
    bIcon->setAlignment(Qt::AlignTop);
    auto* bText = new QLabel(i18n("Rhythm.Banner"));
    bText->setWordWrap(true);
    bText->setStyleSheet(QString("color:%1; font-size:12px;").arg(th::kAccent));
    bl->addWidget(bIcon);
    bl->addWidget(bText, 1);
    contentLay[4]->addWidget(banner);
    contentLay[4]->addStretch();
}

// --- Etape 5 : resume --------------------------------------------------------
QWidget* SdAssistant::Impl::recapCard(Icon ic, const QString& title, int page,
                                      const QStringList& lines) {
    auto* card = makeCard(QStringLiteral("recapCard"));
    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(14, 12, 14, 12);
    lay->setSpacing(8);
    auto* top = new QWidget();
    auto* tl = new QHBoxLayout(top);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(8);
    auto* ico = new QLabel();
    ico->setPixmap(icon(ic, th::kAccent, 15));
    auto* t = new QLabel(title);
    t->setStyleSheet(QString("color:%1; font-size:13px; font-weight:700;").arg(th::kTextPrimary));
    auto* edit = new QPushButton();
    edit->setIcon(icon(Icon::Pencil, th::kTextTertiary, 14));
    edit->setCursor(Qt::PointingHandCursor);
    edit->setFlat(true);
    edit->setStyleSheet(iconBtnQss());
    edit->setToolTip(i18n("Common.Edit"));
    QObject::connect(edit, &QPushButton::clicked, q, [this, page]() { goTo(page); });
    tl->addWidget(ico);
    tl->addWidget(t);
    tl->addStretch();
    tl->addWidget(edit);
    lay->addWidget(top);
    for (const QString& ln : lines) {
        auto* l = new QLabel(ln);
        l->setWordWrap(true);
        l->setStyleSheet(QString("color:%1; font-size:12px;").arg(th::kTextSecondary));
        lay->addWidget(l);
    }
    return card;
}

void SdAssistant::Impl::populateSummary() {
    clearLayout(contentLay[5]);
    summaryError = nullptr;
    contentLay[5]->addWidget(makeTitle(i18n("Summary.Title")));
    contentLay[5]->addWidget(makeSub(i18n("Summary.Intro")));

    // Intervenants
    QStringList spLines;
    for (std::size_t i = 0; i < working.speakers.size(); ++i) {
        const auto& s = working.speakers[i];
        const QString src = s.audioSource.empty() ? i18n("Summary.NoSource")
                                                   : QString::fromStdString(s.audioSource);
        spLines << speakerLabel(i) + QStringLiteral(" · ") + src;
    }
    if (spLines.isEmpty()) {
        spLines << i18n("Summary.NoSpeaker");
    }
    contentLay[5]->addWidget(recapCard(Icon::Users, i18n("Summary.Speakers"), 1, spLines));

    // Scenes (une ligne par intervenant : scenes + %)
    QStringList camLines;
    for (std::size_t i = 0; i < working.speakers.size(); ++i) {
        const auto& s = working.speakers[i];
        int sum = 0;
        for (const auto& w : s.scenes) {
            sum += w.weight;
        }
        QStringList parts;
        for (const auto& w : s.scenes) {
            if (!w.scene.empty()) {
                parts << QString::fromStdString(w.scene) + QStringLiteral(" ") +
                             QString::number(pctOf(w.weight, sum)) + QStringLiteral("%");
            }
        }
        const QString detail =
            parts.isEmpty() ? i18n("Summary.NoCamera") : parts.join(QStringLiteral(" · "));
        camLines << speakerLabel(i) + QStringLiteral(" : ") + detail;
    }
    if (camLines.isEmpty()) {
        camLines << i18n("Summary.NoCamera");
    }
    contentLay[5]->addWidget(recapCard(Icon::Video, i18n("Summary.Cameras"), 2, camLines));

    // Plan large & priorites
    QStringList wideLines;
    wideLines << i18n("Summary.WideScene") + QStringLiteral(" : ") +
                     (working.wideShotScene.empty() ? i18n("Summary.WideSceneNone")
                                                    : QString::fromStdString(working.wideShotScene));
    const int mSum = working.whenMultiple.loudestSpeaker + working.whenMultiple.currentSpeaker +
                     working.whenMultiple.wideShot;
    wideLines << i18n("Summary.Multiple") + QStringLiteral(" : ") + i18n("Wide.Loudest") +
                     QStringLiteral(" ") +
                     QString::number(pctOf(working.whenMultiple.loudestSpeaker, mSum)) +
                     QStringLiteral("% · ") + i18n("Wide.Current") + QStringLiteral(" ") +
                     QString::number(pctOf(working.whenMultiple.currentSpeaker, mSum)) +
                     QStringLiteral("% · ") + i18n("Wide.WideShot") + QStringLiteral(" ") +
                     QString::number(pctOf(working.whenMultiple.wideShot, mSum)) +
                     QStringLiteral("%");
    const int sSum = working.whenSilence.lastSpeaker + working.whenSilence.wideShot;
    wideLines << i18n("Summary.Silence") + QStringLiteral(" : ") + i18n("Wide.LastSpeaker") +
                     QStringLiteral(" ") +
                     QString::number(pctOf(working.whenSilence.lastSpeaker, sSum)) +
                     QStringLiteral("% · ") + i18n("Wide.WideShot") + QStringLiteral(" ") +
                     QString::number(pctOf(working.whenSilence.wideShot, sSum)) + QStringLiteral("%");
    contentLay[5]->addWidget(recapCard(Icon::LayoutGrid, i18n("Summary.Wide"), 3, wideLines));

    // Rythme
    QStringList rhyLines;
    rhyLines << i18n("Summary.RhythmLine")
                    .arg(static_cast<int>(std::lround(working.timing.minShotSeconds)))
                    .arg(static_cast<int>(std::lround(working.timing.maxShotSeconds)))
                    .arg(static_cast<int>(std::lround(working.timing.pingPongWindowSeconds)))
                    .arg(static_cast<int>(std::lround(working.audio.voiceThresholdDb)));
    contentLay[5]->addWidget(recapCard(Icon::Clock, i18n("Summary.Rhythm"), 4, rhyLines));

    summaryError = new QLabel();
    summaryError->setWordWrap(true);
    summaryError->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:600;").arg(th::kDanger));
    summaryError->setVisible(false);
    contentLay[5]->addWidget(summaryError);
    contentLay[5]->addStretch();
}

void SdAssistant::Impl::showError(const QString& msg) {
    if (summaryError) {
        summaryError->setText(msg);
        summaryError->setVisible(true);
    }
}

void SdAssistant::Impl::finish() {
    int valid = 0;
    for (const auto& s : working.speakers) {
        if (!s.name.empty() && !s.audioSource.empty()) {
            ++valid;
        }
    }
    if (valid == 0) {
        showError(i18n("Summary.Error.NoSpeaker"));
        return;
    }
    // Ecriture via le magasin de profils. Deux cas :
    //  - CREATION (newProfileName non vide) : on cree le profil + on l'active ICI, A
    //    LA FIN seulement. Avant ça, rien n'a touche config.json -> le dock n'affiche
    //    pas un profil vide "actif" pendant le remplissage (retour David).
    //  - EDITION (nom vide, compat) : on enregistre le profil ACTIF.
    // saveConfig/createProfile ecrivent config.json (copie vivante) ET le fichier du
    // profil, en phase. Le catalogue est garanti par le constructeur (loadList).
    sd::profiles::StoreResult res;
    if (!newProfileName.isEmpty()) {
        res = sd::profiles::createProfile(newProfileName.toStdString(), sanitizedConfig(working),
                                          /*makeActive=*/true);
    } else {
        res = sd::profiles::saveActive(sanitizedConfig(working));
    }
    if (!res.ok) {
        showError(i18n("Summary.Error.Save").arg(QString::fromStdString(res.error)));
        return;
    }
    activateAuto = true;
    q->accept();
}

// ===========================================================================
// SdAssistant — coquille (fenetre, drag, cycle de vie).
// ===========================================================================
SdAssistant::SdAssistant(QWidget* parent, const QString& newProfileName)
    : QDialog(parent), d_(new Impl) {
    d_->q = this;
    d_->newProfileName = newProfileName;
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setWindowTitle(QStringLiteral("StreamDirector"));

    // Inventaire OBS (modale -> listes stables pendant toute la session).
    d_->audioSources = sd::obsbridge::audioSourceNames();
    d_->scenes = sd::obsbridge::sceneNames();
    // Garantit qu'un catalogue de profils existe (migre l'existant en profil n.1 au
    // premier usage) -> createProfile/saveActive (a la fin) ont un catalogue valide.
    sd::profiles::loadList(i18n("Profiles.DefaultName").toStdString());
    // CREATION (nom fourni) -> on part d'une config VIERGE. EDITION (nom vide) -> on
    // repart de la config existante (profil actif).
    if (newProfileName.isEmpty()) {
        const sd::obsbridge::ConfigLoadResult loaded = sd::obsbridge::loadConfig();
        if (loaded.parsed) {
            d_->working = loaded.config;
        }
    }

    // Panneaux d'edition partages : editent d_->working par reference.
    d_->panels = std::make_unique<ConfigPanels>(d_->working, d_->audioSources, d_->scenes);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 18, 24, 30);  // marge pour l'ombre portee

    d_->root = new QWidget(this);
    d_->root->setObjectName(QStringLiteral("assistantRoot"));
    d_->root->setFixedWidth(520);
    d_->root->setStyleSheet(QString("#assistantRoot { background:%1; border:1px solid %2;"
                                    " border-radius:12px; }")
                                .arg(th::kSurface1)
                                .arg(th::kBorder));
    auto* shadow = new QGraphicsDropShadowEffect(d_->root);
    shadow->setBlurRadius(50);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 18);
    d_->root->setGraphicsEffect(shadow);
    outer->addWidget(d_->root);

    auto* rootLay = new QVBoxLayout(d_->root);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    // --- En-tete ---
    d_->header = new QWidget();
    d_->header->setObjectName(QStringLiteral("asHeader"));
    d_->header->setStyleSheet(QString("#asHeader { background:%1; border-top-left-radius:12px;"
                                      " border-top-right-radius:12px; }")
                                  .arg(th::kSurfaceBar));
    auto* hLay = new QVBoxLayout(d_->header);
    hLay->setContentsMargins(20, 16, 20, 14);
    hLay->setSpacing(12);
    auto* titleRow = new QWidget();
    auto* trLay = new QHBoxLayout(titleRow);
    trLay->setContentsMargins(0, 0, 0, 0);
    trLay->setSpacing(8);
    d_->headerIcon = new QLabel();
    d_->headerIcon->setPixmap(icon(Icon::Clapperboard, th::kAccent, 18));
    d_->headerTitle = new QLabel();
    d_->closeBtn = new QPushButton();
    d_->closeBtn->setIcon(icon(Icon::X, th::kTextTertiary, 16));
    d_->closeBtn->setCursor(Qt::PointingHandCursor);
    d_->closeBtn->setFlat(true);
    d_->closeBtn->setStyleSheet(iconBtnQss());
    d_->closeBtn->setToolTip(i18n("Assistant.Close"));
    connect(d_->closeBtn, &QPushButton::clicked, this, [this]() { reject(); });
    trLay->addWidget(d_->headerIcon);
    trLay->addWidget(d_->headerTitle);
    trLay->addStretch();
    trLay->addWidget(d_->closeBtn);
    hLay->addWidget(titleRow);
    d_->progressRow = new QWidget();
    auto* prLay = new QHBoxLayout(d_->progressRow);
    prLay->setContentsMargins(0, 0, 0, 0);
    prLay->setSpacing(6);
    for (int s = 0; s < 5; ++s) {
        auto* seg = new QFrame();
        seg->setFixedHeight(4);
        seg->setStyleSheet(segQss(false));
        prLay->addWidget(seg, 1);
        d_->segments.push_back(seg);
    }
    hLay->addWidget(d_->progressRow);
    rootLay->addWidget(d_->header);

    // --- Pages (chacune dans une zone scrollable, hauteur fixe) ---
    d_->stack = new QStackedWidget();
    d_->stack->setStyleSheet(QStringLiteral("QStackedWidget { background:transparent; }"));
    for (int i = 0; i < 6; ++i) {
        auto* sc = new QScrollArea();
        sc->setWidgetResizable(true);
        sc->setFrameShape(QFrame::NoFrame);
        sc->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sc->setStyleSheet(QStringLiteral("QScrollArea { background:transparent; border:none; }"));
        auto* content = new QWidget();
        content->setStyleSheet(QStringLiteral("background:transparent;"));
        auto* cl = new QVBoxLayout(content);
        cl->setContentsMargins(24, 16, 24, 16);
        cl->setSpacing(14);
        sc->setWidget(content);
        d_->contentLay[i] = cl;
        d_->stack->addWidget(sc);
    }
    d_->stack->setFixedHeight(430);
    rootLay->addWidget(d_->stack, 1);

    // --- Pied : separateur + boutons ---
    auto* sep = new QFrame();
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(th::kBorder));
    rootLay->addWidget(sep);
    d_->footer = new QWidget();
    d_->footer->setObjectName(QStringLiteral("asFooter"));
    d_->footer->setStyleSheet(QString("#asFooter { background:%1; border-bottom-left-radius:12px;"
                                      " border-bottom-right-radius:12px; }")
                                  .arg(th::kSurfaceBar));
    auto* fLay = new QHBoxLayout(d_->footer);
    fLay->setContentsMargins(20, 14, 20, 14);
    fLay->setSpacing(12);
    d_->backBtn = new QPushButton();
    d_->backBtn->setCursor(Qt::PointingHandCursor);
    d_->backBtn->setStyleSheet(secondaryBtnQss());
    connect(d_->backBtn, &QPushButton::clicked, this, [this]() {
        if (d_->current == 5) {
            d_->goTo(0);
        } else {
            d_->goTo(d_->current - 1);
        }
    });
    d_->nextBtn = new QPushButton();
    d_->nextBtn->setCursor(Qt::PointingHandCursor);
    connect(d_->nextBtn, &QPushButton::clicked, this, [this]() {
        if (d_->current == 5) {
            d_->finish();
        } else {
            d_->goTo(d_->current + 1);
        }
    });
    fLay->addWidget(d_->backBtn);
    fLay->addStretch();
    fLay->addWidget(d_->nextBtn);
    rootLay->addWidget(d_->footer);

    d_->goTo(0);
}

SdAssistant::~SdAssistant() = default;

bool SdAssistant::activateAutoRequested() const {
    return d_->activateAuto;
}

void SdAssistant::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Drag uniquement si l'appui tombe dans la barre de titre (et n'a pas ete
        // consomme par un bouton, ex la croix de fermeture).
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

void SdAssistant::mouseMoveEvent(QMouseEvent* event) {
    if (d_->dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - d_->dragOffset);
    }
    QDialog::mouseMoveEvent(event);
}

void SdAssistant::mouseReleaseEvent(QMouseEvent* event) {
    d_->dragging = false;
    QDialog::mouseReleaseEvent(event);
}

void SdAssistant::showEvent(QShowEvent* event) {
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
