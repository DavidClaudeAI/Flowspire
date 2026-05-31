// StreamDirector — assistant de configuration visuel (implementation, Run 5).
// Voir sd_assistant.hpp pour le contrat. Fenetre modale frameless, 6 ecrans
// (QStackedWidget), fidele a la maquette Pencil. Ecrit le config.json a la fin.
#include "ui/sd_assistant.hpp"

#include <QColor>
#include <QEnterEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QSizePolicy>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSlider>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "obs/config_loader.hpp"
#include "obs/obs_inventory.hpp"
#include "ui/sd_i18n.hpp"
#include "ui/sd_icons.hpp"
#include "ui/sd_runtime.hpp"
#include "ui/sd_style.hpp"
#include "ui/sd_theme.hpp"

namespace sd::ui {

namespace th = sd::ui::theme;

namespace {

// ---------------------------------------------------------------------------
// Feuilles de style (QSS) — traduisent les jetons de la maquette.
// ---------------------------------------------------------------------------
QString menuQss() {
    return QString("QMenu { background:%1; border:1px solid %2; border-radius:6px; padding:4px; }"
                   "QMenu::item { color:%3; padding:6px 22px 6px 12px; border-radius:4px; }"
                   "QMenu::item:selected { background:%4; }"
                   "QMenu::item:disabled { color:%5; }")
        .arg(th::kSurface2)
        .arg(th::kBorder)
        .arg(th::kTextPrimary)
        .arg(rgba(th::kAccent, 0.25))
        .arg(th::kTextTertiary);
}

QString accentSliderQss() {
    return QString(
               "QSlider::groove:horizontal { height:6px; background:%1; border-radius:3px; }"
               "QSlider::sub-page:horizontal { height:6px; background:%2; border-radius:3px; }"
               "QSlider::add-page:horizontal { height:6px; background:%1; border-radius:3px; }"
               "QSlider::handle:horizontal { width:14px; height:14px; margin:-4px 0;"
               " border-radius:7px; background:%3; border:2px solid %2; }")
        .arg(th::kSurface3)
        .arg(th::kAccent)
        .arg(th::kTextPrimary);
}

QString badgeQss() {
    return QString("color:%1; background:%2; border-radius:5px; font-size:11px;"
                   " font-weight:600; padding:2px 0;")
        .arg(th::kAccent)
        .arg(rgba(th::kAccent, 0.15));
}

QString lineEditQss() {
    return QString("QLineEdit { background:%1; border:1px solid %2; border-radius:%3px;"
                   " color:%4; font-size:13px; padding:6px 10px; }"
                   "QLineEdit:focus { border-color:%5; }")
        .arg(th::kSurface3)
        .arg(th::kBorder)
        .arg(th::kRadiusButton)
        .arg(th::kTextPrimary)
        .arg(rgba(th::kAccent, 0.7));
}

QString iconBtnQss() {
    return QStringLiteral("QPushButton { background:transparent; border:none; padding:3px; }"
                          "QPushButton:hover { background:rgba(255,255,255,0.06);"
                          " border-radius:4px; }");
}

QString primaryBtnQss() {
    return QString("QPushButton { background:%1; border:none; border-radius:6px; color:%2;"
                   " font-size:13px; font-weight:700; padding:10px 16px; }"
                   "QPushButton:hover { background:%3; }")
        .arg(th::kAccent)
        .arg(th::kOnAccent)
        .arg(rgba(th::kAccent, 0.85));
}

QString greenBtnQss() {
    return QString("QPushButton { background:%1; border:none; border-radius:6px; color:%2;"
                   " font-size:13px; font-weight:700; padding:10px 16px; }"
                   "QPushButton:hover { background:%3; }")
        .arg(th::kSuccess)
        .arg(th::kOnAccent)
        .arg(rgba(th::kSuccess, 0.85));
}

QString secondaryBtnQss() {
    return QString("QPushButton { background:%1; border:none; border-radius:6px; color:%2;"
                   " font-size:13px; font-weight:600; padding:10px 14px; }"
                   "QPushButton:hover { color:%3; }")
        .arg(th::kSurface3)
        .arg(th::kTextSecondary)
        .arg(th::kTextPrimary);
}

// Style d'un onglet "dossier" (etape "scenes"), repris de la maquette Paramètres
// avancés : coins arrondis EN HAUT seulement, l'onglet actif a la MÊME couleur que
// la carte (surface2) -> effet "rattaché", l'inactif est plus sombre (surface app).
// CONTOUR ajouté par rapport à la maquette (demande de David). L'actif n'a pas de
// bordure basse : il fusionne avec la carte posée juste en dessous (gap 0).
QString tabBoxQss(bool active) {
    if (active) {
        return QString("#clickbtn { background:%1; border:1px solid %2; border-bottom:none;"
                       " border-top-left-radius:8px; border-top-right-radius:8px; }")
            .arg(th::kSurface2)
            .arg(rgba(th::kAccent, 0.55));
    }
    return QString("#clickbtn { background:%1; border:1px solid %2; border-bottom:none;"
                   " border-top-left-radius:8px; border-top-right-radius:8px; }")
        .arg(th::kBg)
        .arg(rgba(th::kBorder, 1.0));
}

// Bouton cliquable custom : icone (optionnelle) + texte CENTRES via un layout
// interne. On n'utilise PAS QPushButton::setIcon : sur un bouton LARGE (pleine
// largeur), Qt aligne l'icone a gauche et centre le texte separement -> gros trou
// entre les deux (bug visuel signale par David sur "Ajouter..."). Avec un
// QHBoxLayout [stretch | icone | texte | stretch] le rendu est fidele a la maquette.
// Meme principe que ComboField : pas de signal Qt custom (pas de moc), callback
// std::function ; les labels enfants sont transparents a la souris -> le clic
// remonte toujours au ClickButton.
class ClickButton : public QFrame {
public:
    explicit ClickButton(QWidget* parent = nullptr) : QFrame(parent) {
        setObjectName(QStringLiteral("clickbtn"));
        setAttribute(Qt::WA_StyledBackground, true);  // fond/bordure QSS reellement peints
        setCursor(Qt::PointingHandCursor);
        lay_ = new QHBoxLayout(this);
        lay_->setContentsMargins(12, 9, 12, 9);
        lay_->setSpacing(6);
        iconLabel_ = new QLabel(this);
        iconLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        iconLabel_->setVisible(false);
        textLabel_ = new QLabel(this);
        textLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        lay_->addWidget(iconLabel_);
        lay_->addWidget(textLabel_);
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    }
    void setMargins(int h, int v) { lay_->setContentsMargins(h, v, h, v); }
    void setExpanding() {  // contenu centre sur toute la largeur (boutons "Ajouter")
        lay_->insertStretch(0);
        lay_->addStretch();
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setIconPix(const QPixmap& p) {
        iconLabel_->setPixmap(p);
        iconLabel_->setVisible(!p.isNull());
    }
    void setLabel(const QString& t, const char* color, int size, int weight) {
        textLabel_->setText(t);
        textLabel_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:%3; background:transparent;")
                .arg(QString::fromUtf8(color))
                .arg(size)
                .arg(weight));
    }
    void setBox(const QString& normal, const QString& hover = QString()) {
        normalQss_ = normal;
        hoverQss_ = hover;
        setStyleSheet(normalQss_);
    }
    void setOnClick(std::function<void()> cb) { cb_ = std::move(cb); }

protected:
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && cb_) {
            cb_();
        }
    }
    void enterEvent(QEnterEvent*) override {
        if (!hoverQss_.isEmpty()) {
            setStyleSheet(hoverQss_);
        }
    }
    void leaveEvent(QEvent*) override {
        if (!hoverQss_.isEmpty()) {
            setStyleSheet(normalQss_);
        }
    }

private:
    QHBoxLayout* lay_ = nullptr;
    QLabel* iconLabel_ = nullptr;
    QLabel* textLabel_ = nullptr;
    std::function<void()> cb_;
    QString normalQss_;
    QString hoverQss_;
};

QString segQss(bool filled) {
    return QString("QFrame { background:%1; border-radius:2px; }")
        .arg(filled ? QString::fromUtf8(th::kAccent) : QString::fromUtf8(th::kSurface3));
}

// ---------------------------------------------------------------------------
// Fabriques de petits widgets reutilisables.
// ---------------------------------------------------------------------------
QLabel* makeTitle(const QString& t) {
    auto* l = new QLabel(t);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:20px; font-weight:700;").arg(th::kTextPrimary));
    return l;
}

QLabel* makeSub(const QString& t) {
    auto* l = new QLabel(t);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:13px;").arg(th::kTextSecondary));
    return l;
}

QLabel* makeSectionLabel(const QString& t) {
    auto* l = new QLabel(t);
    l->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px;")
                         .arg(th::kTextTertiary));
    return l;
}

QLabel* makeHint(const QString& t) {
    auto* l = new QLabel(t);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:13px;").arg(th::kTextTertiary));
    return l;
}

QLabel* makeGroupHeader(const QString& t) {
    auto* l = new QLabel(t);
    l->setStyleSheet(QString("color:%1; font-size:13px; font-weight:600;").arg(th::kTextSecondary));
    return l;
}

// Bouton "Ajouter ..." pleine largeur : icone + texte CENTRES (ClickButton), fond
// surface2 + bordure border, liseré accent au survol. Callback std::function.
ClickButton* makeAddButton(const QString& text, std::function<void()> onClick) {
    auto* b = new ClickButton();
    b->setExpanding();
    b->setMargins(12, 11);
    b->setIconPix(icon(Icon::Plus, th::kAccent, 15));
    b->setLabel(text, th::kAccent, 13, 600);
    const QString base =
        QStringLiteral("#clickbtn { background:%1; border:1px solid %2; border-radius:8px; }");
    b->setBox(base.arg(th::kSurface2).arg(th::kBorder),
              base.arg(th::kSurface2).arg(rgba(th::kAccent, 0.6)));
    b->setOnClick(std::move(onClick));
    return b;
}

// Carte (fond surface2 + bordure arrondie). `obj` = objectName UNIQUE par type de
// carte -> le selecteur #obj ne cascade pas vers les enfants (lecon Run 4).
QWidget* makeCard(const QString& obj) {
    auto* w = new QWidget();
    w->setObjectName(obj);
    w->setStyleSheet(QString("#%1 { background:%2; border:1px solid %3; border-radius:%4px; }")
                         .arg(obj)
                         .arg(th::kSurface2)
                         .arg(th::kBorder)
                         .arg(th::kRadiusCard));
    return w;
}

void clearLayout(QLayout* lay) {
    if (!lay) {
        return;
    }
    while (QLayoutItem* item = lay->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            // Suppression DIFFEREE (et non `delete` immediat). Un widget peut
            // declencher sa PROPRE destruction depuis son gestionnaire d'evenement :
            // un onglet (ClickButton) ou un bouton corbeille dont le clic appelle
            // populate*(), qui vide ce layout et detruirait donc le widget alors que
            // Qt deroule encore son event dispatch sur lui -> use-after-free. On le
            // cache (il ne peint plus, pas de doublon a l'ecran avec le nouveau
            // contenu) puis on confie sa destruction a la boucle d'evenements une fois
            // l'evenement courant entierement termine.
            w->hide();
            w->deleteLater();
        } else if (QLayout* child = item->layout()) {
            clearLayout(child);
        }
        delete item;
    }
}

int pctOf(int v, int sum) {
    return sum > 0 ? static_cast<int>(std::lround(100.0 * v / sum)) : 0;
}

// ---------------------------------------------------------------------------
// ComboField : liste deroulante stylee (texte + chevron + QMenu). On n'utilise
// PAS QComboBox : sa fleche se style via une image QSS (fragile), alors qu'ici
// on reutilise nos icones lucide rendues par QSvgRenderer (deja eprouve). Pas de
// signal Qt custom (donc pas de moc) -> callback std::function.
// ---------------------------------------------------------------------------
class ComboField : public QWidget {
public:
    explicit ComboField(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("combo"));
        setCursor(Qt::PointingHandCursor);
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(10, 7, 10, 7);
        lay->setSpacing(8);
        text_ = new QLabel(this);
        chevron_ = new QLabel(this);
        chevron_->setPixmap(icon(Icon::ChevronDown, th::kTextSecondary, 14));
        lay->addWidget(text_, 1);
        lay->addWidget(chevron_);
        setStyleSheet(QString("#combo { background:%1; border:1px solid %2; border-radius:%3px; }"
                              "#combo:hover { border:1px solid %4; }")
                          .arg(th::kSurface3)
                          .arg(th::kBorder)
                          .arg(th::kRadiusButton)
                          .arg(rgba(th::kAccent, 0.6)));
        updateText();
    }

    void setOptions(QStringList opts, const QString& current) {
        opts_ = std::move(opts);
        value_ = current;
        // Valeur stockee absente de la liste (ex : source supprimee) -> on la garde
        // visible ET re-selectionnable, pour ne pas effacer silencieusement un reglage.
        if (!value_.isEmpty() && !opts_.contains(value_)) {
            opts_.prepend(value_);
        }
        updateText();
    }
    void setPlaceholder(const QString& ph) {
        placeholder_ = ph;
        updateText();
    }
    void setAllowEmpty(bool allow, const QString& emptyLabel) {
        allowEmpty_ = allow;
        emptyLabel_ = emptyLabel;
    }
    void setEmptyHint(const QString& hint) { emptyHint_ = hint; }
    QString value() const { return value_; }
    void setOnChange(std::function<void(const QString&)> cb) { cb_ = std::move(cb); }

protected:
    void mouseReleaseEvent(QMouseEvent*) override { openMenu(); }

private:
    void updateText() {
        const bool empty = value_.isEmpty();
        text_->setText(empty ? placeholder_ : value_);
        text_->setStyleSheet(QString("color:%1; font-size:13px;")
                                 .arg(empty ? th::kTextTertiary : th::kTextPrimary));
    }
    void openMenu() {
        QMenu menu;
        menu.setStyleSheet(menuQss());
        if (allowEmpty_) {
            QAction* a = menu.addAction(emptyLabel_);
            a->setData(QString());
        }
        for (const QString& o : opts_) {
            QAction* a = menu.addAction(o);
            a->setData(o);
        }
        if (menu.actions().isEmpty()) {
            QAction* a = menu.addAction(emptyHint_.isEmpty() ? QStringLiteral("—") : emptyHint_);
            a->setEnabled(false);
        }
        QAction* chosen = menu.exec(mapToGlobal(rect().bottomLeft()));
        if (chosen && chosen->isEnabled()) {
            value_ = chosen->data().toString();
            updateText();
            if (cb_) {
                cb_(value_);
            }
        }
    }

    QStringList opts_;
    QString value_;
    QString placeholder_;
    QString emptyLabel_;
    QString emptyHint_;
    bool allowEmpty_ = false;
    QLabel* text_ = nullptr;
    QLabel* chevron_ = nullptr;
    std::function<void(const QString&)> cb_;
};

// ---------------------------------------------------------------------------
// SliderRow : libelle + slider + valeur formatee (+ badge % optionnel).
// ---------------------------------------------------------------------------
class SliderRow : public QWidget {
public:
    SliderRow(const QString& label, int min, int max, int value,
              std::function<QString(int)> fmt, bool withBadge, QWidget* parent = nullptr)
        : QWidget(parent), fmt_(std::move(fmt)) {
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(12);
        auto* lbl = new QLabel(label, this);
        lbl->setStyleSheet(QString("color:%1; font-size:13px;").arg(th::kTextSecondary));
        lbl->setMinimumWidth(132);
        slider_ = new QSlider(Qt::Horizontal, this);
        slider_->setRange(min, max);
        slider_->setValue(value);
        slider_->setStyleSheet(accentSliderQss());
        slider_->setCursor(Qt::PointingHandCursor);
        valueLabel_ = new QLabel(this);
        valueLabel_->setMinimumWidth(42);
        valueLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel_->setStyleSheet(
            QString("color:%1; font-size:13px; font-weight:600;").arg(th::kTextPrimary));
        lay->addWidget(lbl);
        lay->addWidget(slider_, 1);
        lay->addWidget(valueLabel_);
        if (withBadge) {
            badge_ = new QLabel(this);
            badge_->setAlignment(Qt::AlignCenter);
            badge_->setFixedWidth(44);
            badge_->setStyleSheet(badgeQss());
            lay->addWidget(badge_);
        }
        updateValueText();
        connect(slider_, &QSlider::valueChanged, this, [this](int v) {
            updateValueText();
            if (cb_) {
                cb_(v);
            }
        });
    }

    int value() const { return slider_->value(); }
    void setBadge(int pct) {
        if (badge_) {
            badge_->setText(QString::number(pct) + QStringLiteral("%"));
        }
    }
    void setOnChange(std::function<void(int)> cb) { cb_ = std::move(cb); }

private:
    void updateValueText() {
        valueLabel_->setText(fmt_ ? fmt_(slider_->value()) : QString::number(slider_->value()));
    }
    QSlider* slider_ = nullptr;
    QLabel* valueLabel_ = nullptr;
    QLabel* badge_ = nullptr;
    std::function<QString(int)> fmt_;
    std::function<void(int)> cb_;
};

// Formats de valeur (unites universelles, non traduites).
QString fmtSeconds(int v) { return QString::number(v) + QStringLiteral(" s"); }
QString fmtDb(int v) { return QString::number(v) + QStringLiteral(" dB"); }
QString fmtVad(int v) { return QString::number(v / 100.0, 'f', 2); }
QString fmtSilence(int frames) {
    return QString::number(frames * sd::ui::kFrameSeconds, 'f', 1) + QStringLiteral(" s");
}

}  // namespace

// ===========================================================================
// Etat interne (pimpl) : tout le gros de l'assistant vit ici.
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
    QWidget* content[6] = {};
    QVBoxLayout* contentLay[6] = {};
    QWidget* footer = nullptr;
    QPushButton* backBtn = nullptr;
    QPushButton* nextBtn = nullptr;

    // Donnees
    sd::core::Config working;
    std::vector<std::string> audioSources;
    std::vector<std::string> scenes;
    int idCounter = 0;
    bool activateAuto = false;

    // Navigation / etat de page
    int current = 0;
    int selSpeaker = 0;  // onglet actif de l'etape Cameras
    std::vector<QSlider*> sceneWeightSliders;
    std::vector<QLabel*> sceneWeightBadges;
    std::vector<SliderRow*> multRows;
    std::vector<SliderRow*> silRows;
    QLabel* summaryError = nullptr;

    // Fenetre frameless : deplacement par la barre de titre.
    bool dragging = false;
    QPoint dragOffset;
    bool centered = false;

    // --- helpers config ---
    sd::core::Speaker* findSpeaker(const std::string& id) {
        for (auto& s : working.speakers) {
            if (s.id == id) {
                return &s;
            }
        }
        return nullptr;
    }
    std::string uniqueId() {
        std::string id;
        do {
            id = "spk-" + std::to_string(++idCounter);
        } while (findSpeaker(id) != nullptr);
        return id;
    }
    void addBlankSpeaker() { working.speakers.push_back({uniqueId(), "", "", {}}); }
    void eraseSpeaker(const std::string& id) {
        working.speakers.erase(
            std::remove_if(working.speakers.begin(), working.speakers.end(),
                           [&](const sd::core::Speaker& s) { return s.id == id; }),
            working.speakers.end());
    }
    QString speakerLabel(size_t i) {
        const std::string& n = working.speakers[i].name;
        return n.empty() ? i18n("Speakers.DefaultName").arg(static_cast<int>(i) + 1)
                         : QString::fromStdString(n);
    }
    QStringList sceneOptions() const {
        QStringList o;
        for (const auto& s : scenes) {
            o << QString::fromStdString(s);
        }
        return o;
    }
    QStringList audioOptions() const {
        QStringList o;
        for (const auto& s : audioSources) {
            o << QString::fromStdString(s);
        }
        return o;
    }

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
    QWidget* buildSpeakerCard(size_t idx);
    QWidget* buildSceneRow(int spk, size_t s);
    void recomputeSceneWeights();
    void recomputeGroup(const std::vector<SliderRow*>& rows);
    QWidget* recapCard(Icon ic, const QString& title, int page, const QStringList& lines);
    QString stepName(int step) const;

    // --- fin ---
    void finish();
    void showError(const QString& msg);
    sd::core::Config sanitized() const;

    // Bandeau d'info accent (etape rythme).
    QWidget* infoBanner(const QString& text);
    // En-tete de groupe avec icone (cartes plan large).
};

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
    // bouton du pied (ex. "Suivant" -> "C'est fait, commencer") modifie sa largeur ;
    // la zone liberee n'est pas re-invalidee toute seule sur un top-level translucide
    // -> un fantome du texte precedent subsiste (vu par David : "C'est fait" a moitie
    // peint, "Suivant" dessous). update() ne suffit pas : il est ASYNCHRONE et peut
    // s'executer AVANT le recalcul de la disposition. On force donc la disposition a
    // se recalculer MAINTENANT (activate) puis une re-peinture SYNCHRONE de toute la
    // fenetre (repaint) -> le residu est efface immediatement.
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
    // La re-peinture qui efface tout fantome de libelle est faite par goTo()
    // (recalcul de disposition + repaint synchrone de la fenetre translucide).
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

// --- Etape 1 : intervenants --------------------------------------------------
void SdAssistant::Impl::populateSpeakers() {
    clearLayout(contentLay[1]);
    contentLay[1]->addWidget(makeTitle(i18n("Speakers.Title")));
    contentLay[1]->addWidget(makeSub(i18n("Speakers.Intro")));
    if (working.speakers.empty()) {
        addBlankSpeaker();  // toujours au moins une carte a remplir
    }
    auto* list = new QWidget();
    auto* ll = new QVBoxLayout(list);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(10);
    for (size_t i = 0; i < working.speakers.size(); ++i) {
        ll->addWidget(buildSpeakerCard(i));
    }
    contentLay[1]->addWidget(list);
    auto* add = makeAddButton(i18n("Speakers.Add"), [this]() {
        addBlankSpeaker();
        populateSpeakers();
    });
    contentLay[1]->addWidget(add);
    contentLay[1]->addStretch();
}

QWidget* SdAssistant::Impl::buildSpeakerCard(size_t idx) {
    const std::string sid = working.speakers[idx].id;
    auto* card = makeCard(QStringLiteral("spkEdit"));
    auto* lay = new QHBoxLayout(card);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(10);

    auto* num = new QLabel(QString::number(static_cast<int>(idx) + 1));
    num->setFixedSize(30, 30);
    num->setAlignment(Qt::AlignCenter);
    num->setStyleSheet(QString("background:%1; border-radius:15px; color:%2;"
                               " font-size:13px; font-weight:600;")
                           .arg(th::kSurface3)
                           .arg(th::kTextSecondary));

    auto* fields = new QWidget();
    auto* fl = new QVBoxLayout(fields);
    fl->setContentsMargins(0, 0, 0, 0);
    fl->setSpacing(6);
    auto* name = new QLineEdit(QString::fromStdString(working.speakers[idx].name));
    name->setPlaceholderText(i18n("Speakers.NamePlaceholder"));
    name->setStyleSheet(lineEditQss());
    QObject::connect(name, &QLineEdit::textChanged, q, [this, sid](const QString& t) {
        if (auto* s = findSpeaker(sid)) {
            s->name = t.toStdString();
        }
    });
    auto* src = new ComboField();
    src->setPlaceholder(i18n("Speakers.SourcePlaceholder"));
    src->setEmptyHint(i18n("Speakers.NoSource"));
    src->setOptions(audioOptions(), QString::fromStdString(working.speakers[idx].audioSource));
    src->setOnChange([this, sid](const QString& v) {
        if (auto* s = findSpeaker(sid)) {
            s->audioSource = v.toStdString();
        }
    });
    fl->addWidget(name);
    fl->addWidget(src);

    auto* del = new QPushButton();
    del->setIcon(icon(Icon::Trash2, th::kTextTertiary, 15));
    del->setCursor(Qt::PointingHandCursor);
    del->setFlat(true);
    del->setStyleSheet(iconBtnQss());
    QObject::connect(del, &QPushButton::clicked, q, [this, sid]() {
        eraseSpeaker(sid);
        populateSpeakers();
    });

    lay->addWidget(num, 0, Qt::AlignTop);
    lay->addWidget(fields, 1);
    lay->addWidget(del, 0, Qt::AlignTop);
    return card;
}

// --- Etape 2 : cameras (scenes + poids par intervenant) ----------------------
void SdAssistant::Impl::populateCameras() {
    clearLayout(contentLay[2]);
    sceneWeightSliders.clear();
    sceneWeightBadges.clear();
    contentLay[2]->addWidget(makeTitle(i18n("Cameras.Title")));
    contentLay[2]->addWidget(makeSub(i18n("Cameras.Intro")));

    if (working.speakers.empty()) {
        contentLay[2]->addWidget(makeHint(i18n("Cameras.Empty")));
        contentLay[2]->addStretch();
        return;
    }
    if (selSpeaker >= static_cast<int>(working.speakers.size())) {
        selSpeaker = 0;
    }

    // Onglets "dossier" + carte rattachée, dans un conteneur SANS gap : la rangée
    // d'onglets se pose pile au-dessus de la carte (effet classeur). Un par intervenant.
    auto* tabWrap = new QWidget();
    auto* twl = new QVBoxLayout(tabWrap);
    twl->setContentsMargins(0, 0, 0, 0);
    twl->setSpacing(0);

    auto* tabs = new QWidget();
    auto* tl = new QHBoxLayout(tabs);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(4);
    for (size_t i = 0; i < working.speakers.size(); ++i) {
        const bool active = static_cast<int>(i) == selSpeaker;
        auto* tb = new ClickButton();
        tb->setMargins(16, 9);
        tb->setIconPix(icon(Icon::User, active ? th::kAccent : th::kTextTertiary, 14));
        tb->setLabel(speakerLabel(i), active ? th::kAccent : th::kTextSecondary, 13,
                     active ? 700 : 600);
        tb->setBox(tabBoxQss(active));
        const int idx = static_cast<int>(i);
        tb->setOnClick([this, idx]() {
            selSpeaker = idx;
            populateCameras();
        });
        tl->addWidget(tb);
    }
    tl->addStretch();
    twl->addWidget(tabs);

    // Carte rattachée : coin haut-GAUCHE carré (sous le 1er onglet) façon dossier.
    auto* group = new QWidget();
    group->setObjectName(QStringLiteral("camGroup"));
    group->setStyleSheet(QString("#camGroup { background:%1; border:1px solid %2;"
                                 " border-top-left-radius:0px; border-top-right-radius:%3px;"
                                 " border-bottom-left-radius:%3px; border-bottom-right-radius:%3px; }")
                             .arg(th::kSurface2)
                             .arg(rgba(th::kBorder, 1.0))
                             .arg(th::kRadiusCard));
    auto* gl = new QVBoxLayout(group);
    gl->setContentsMargins(14, 14, 14, 14);
    gl->setSpacing(12);
    sd::core::Speaker& sp = working.speakers[static_cast<size_t>(selSpeaker)];
    if (sp.scenes.empty()) {
        gl->addWidget(makeHint(i18n("Cameras.NoScene")));
    }
    for (size_t s = 0; s < sp.scenes.size(); ++s) {
        gl->addWidget(buildSceneRow(selSpeaker, s));
    }
    twl->addWidget(group);
    contentLay[2]->addWidget(tabWrap);

    auto* add = makeAddButton(i18n("Cameras.Add"), [this]() {
        working.speakers[static_cast<size_t>(selSpeaker)].scenes.push_back({"", 50});
        populateCameras();
    });
    contentLay[2]->addWidget(add);
    contentLay[2]->addStretch();
    recomputeSceneWeights();
}

QWidget* SdAssistant::Impl::buildSceneRow(int spk, size_t s) {
    auto* row = new QWidget();
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    auto* combo = new ComboField();
    combo->setPlaceholder(i18n("Cameras.ScenePlaceholder"));
    combo->setEmptyHint(i18n("Cameras.NoSceneAvail"));
    combo->setOptions(sceneOptions(),
                      QString::fromStdString(working.speakers[static_cast<size_t>(spk)].scenes[s].scene));
    combo->setMinimumWidth(140);
    combo->setOnChange([this, spk, s](const QString& v) {
        working.speakers[static_cast<size_t>(spk)].scenes[s].scene = v.toStdString();
    });

    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 100);
    slider->setValue(working.speakers[static_cast<size_t>(spk)].scenes[s].weight);
    slider->setStyleSheet(accentSliderQss());
    slider->setCursor(Qt::PointingHandCursor);

    auto* val = new QLabel(QString::number(slider->value()));
    val->setMinimumWidth(28);
    val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    val->setStyleSheet(QString("color:%1; font-size:13px; font-weight:600;").arg(th::kTextPrimary));

    auto* badge = new QLabel();
    badge->setFixedWidth(44);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(badgeQss());

    auto* del = new QPushButton();
    del->setIcon(icon(Icon::Trash2, th::kTextTertiary, 15));
    del->setCursor(Qt::PointingHandCursor);
    del->setFlat(true);
    del->setStyleSheet(iconBtnQss());

    QObject::connect(slider, &QSlider::valueChanged, q, [this, spk, s, val](int v) {
        working.speakers[static_cast<size_t>(spk)].scenes[s].weight = v;
        val->setText(QString::number(v));
        recomputeSceneWeights();
    });
    QObject::connect(del, &QPushButton::clicked, q, [this, spk, s]() {
        auto& sc = working.speakers[static_cast<size_t>(spk)].scenes;
        if (s < sc.size()) {
            sc.erase(sc.begin() + static_cast<long>(s));
        }
        populateCameras();
    });

    lay->addWidget(combo, 1);
    lay->addWidget(slider, 1);
    lay->addWidget(val);
    lay->addWidget(badge);
    lay->addWidget(del);
    sceneWeightSliders.push_back(slider);
    sceneWeightBadges.push_back(badge);
    return row;
}

void SdAssistant::Impl::recomputeSceneWeights() {
    int sum = 0;
    for (auto* s : sceneWeightSliders) {
        sum += s->value();
    }
    for (size_t i = 0; i < sceneWeightSliders.size(); ++i) {
        sceneWeightBadges[i]->setText(QString::number(pctOf(sceneWeightSliders[i]->value(), sum)) +
                                      QStringLiteral("%"));
    }
}

void SdAssistant::Impl::recomputeGroup(const std::vector<SliderRow*>& rows) {
    int sum = 0;
    for (auto* r : rows) {
        sum += r->value();
    }
    for (auto* r : rows) {
        r->setBadge(pctOf(r->value(), sum));
    }
}

// --- Etape 3 : plan large & priorites ---------------------------------------
void SdAssistant::Impl::populateWide() {
    clearLayout(contentLay[3]);
    multRows.clear();
    silRows.clear();
    contentLay[3]->addWidget(makeTitle(i18n("Wide.Title")));
    contentLay[3]->addWidget(makeSub(i18n("Wide.Intro")));
    contentLay[3]->addWidget(makeSectionLabel(i18n("Wide.SceneLabel")));

    auto* wide = new ComboField();
    wide->setAllowEmpty(true, i18n("Wide.ScenePlaceholder"));
    wide->setPlaceholder(i18n("Wide.ScenePlaceholder"));
    wide->setEmptyHint(i18n("Cameras.NoSceneAvail"));
    wide->setOptions(sceneOptions(), QString::fromStdString(working.wideShotScene));
    wide->setOnChange([this](const QString& v) { working.wideShotScene = v.toStdString(); });
    contentLay[3]->addWidget(wide);

    // Groupe "plusieurs parlent".
    auto* g1 = makeCard(QStringLiteral("wideMul"));
    auto* g1l = new QVBoxLayout(g1);
    g1l->setContentsMargins(14, 12, 14, 12);
    g1l->setSpacing(10);
    g1l->addWidget(makeGroupHeader(i18n("Wide.Multiple")));
    auto* r1 = new SliderRow(i18n("Wide.Loudest"), 0, 100, working.whenMultiple.loudestSpeaker,
                             nullptr, true);
    r1->setOnChange([this](int v) {
        working.whenMultiple.loudestSpeaker = v;
        recomputeGroup(multRows);
    });
    auto* r2 = new SliderRow(i18n("Wide.Current"), 0, 100, working.whenMultiple.currentSpeaker,
                             nullptr, true);
    r2->setOnChange([this](int v) {
        working.whenMultiple.currentSpeaker = v;
        recomputeGroup(multRows);
    });
    auto* r3 = new SliderRow(i18n("Wide.WideShot"), 0, 100, working.whenMultiple.wideShot, nullptr,
                             true);
    r3->setOnChange([this](int v) {
        working.whenMultiple.wideShot = v;
        recomputeGroup(multRows);
    });
    multRows = {r1, r2, r3};
    g1l->addWidget(r1);
    g1l->addWidget(r2);
    g1l->addWidget(r3);
    contentLay[3]->addWidget(g1);

    // Groupe "personne ne parle".
    auto* g2 = makeCard(QStringLiteral("wideSil"));
    auto* g2l = new QVBoxLayout(g2);
    g2l->setContentsMargins(14, 12, 14, 12);
    g2l->setSpacing(10);
    g2l->addWidget(makeGroupHeader(i18n("Wide.Silence")));
    auto* s1 = new SliderRow(i18n("Wide.LastSpeaker"), 0, 100, working.whenSilence.lastSpeaker,
                             nullptr, true);
    s1->setOnChange([this](int v) {
        working.whenSilence.lastSpeaker = v;
        recomputeGroup(silRows);
    });
    auto* s2 = new SliderRow(i18n("Wide.WideShot"), 0, 100, working.whenSilence.wideShot, nullptr,
                             true);
    s2->setOnChange([this](int v) {
        working.whenSilence.wideShot = v;
        recomputeGroup(silRows);
    });
    silRows = {s1, s2};
    g2l->addWidget(s1);
    g2l->addWidget(s2);
    contentLay[3]->addWidget(g2);
    contentLay[3]->addStretch();

    recomputeGroup(multRows);
    recomputeGroup(silRows);
}

// --- Etape 4 : rythme & sensibilite -----------------------------------------
QWidget* SdAssistant::Impl::infoBanner(const QString& text) {
    auto* w = new QWidget();
    w->setObjectName(QStringLiteral("infoBanner"));
    w->setStyleSheet(QString("#infoBanner { background:%1; border-radius:6px; }")
                         .arg(rgba(th::kAccent, 0.10)));
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(12, 10, 12, 10);
    l->setSpacing(8);
    auto* ic = new QLabel();
    ic->setPixmap(icon(Icon::Info, th::kAccent, 15));
    ic->setAlignment(Qt::AlignTop);
    auto* t = new QLabel(text);
    t->setWordWrap(true);
    t->setStyleSheet(QString("color:%1; font-size:12px;").arg(th::kAccent));
    l->addWidget(ic);
    l->addWidget(t, 1);
    return w;
}

void SdAssistant::Impl::populateRhythm() {
    clearLayout(contentLay[4]);
    contentLay[4]->addWidget(makeTitle(i18n("Rhythm.Title")));
    contentLay[4]->addWidget(makeSub(i18n("Rhythm.Intro")));

    auto* timing = makeCard(QStringLiteral("rhyTiming"));
    auto* tlay = new QVBoxLayout(timing);
    tlay->setContentsMargins(14, 12, 14, 12);
    tlay->setSpacing(10);
    auto* minR = new SliderRow(i18n("Rhythm.MinShot"), 0, 30,
                               static_cast<int>(std::lround(working.timing.minShotSeconds)),
                               fmtSeconds, false);
    minR->setOnChange([this](int v) { working.timing.minShotSeconds = v; });
    auto* maxR = new SliderRow(i18n("Rhythm.MaxShot"), 0, 60,
                               static_cast<int>(std::lround(working.timing.maxShotSeconds)),
                               fmtSeconds, false);
    maxR->setOnChange([this](int v) { working.timing.maxShotSeconds = v; });
    auto* ppR = new SliderRow(i18n("Rhythm.PingPong"), 0, 30,
                              static_cast<int>(std::lround(working.timing.pingPongWindowSeconds)),
                              fmtSeconds, false);
    ppR->setOnChange([this](int v) { working.timing.pingPongWindowSeconds = v; });
    tlay->addWidget(minR);
    tlay->addWidget(maxR);
    tlay->addWidget(ppR);
    contentLay[4]->addWidget(timing);

    contentLay[4]->addWidget(makeSectionLabel(i18n("Rhythm.AudioSection")));
    auto* audio = makeCard(QStringLiteral("rhyAudio"));
    auto* alay = new QVBoxLayout(audio);
    alay->setContentsMargins(14, 12, 14, 12);
    alay->setSpacing(10);
    int thr = static_cast<int>(std::lround(working.audio.voiceThresholdDb));
    thr = std::max(-60, std::min(0, thr));
    auto* thrR = new SliderRow(i18n("Rhythm.Threshold"), -60, 0, thr, fmtDb, false);
    thrR->setOnChange([this](int v) { working.audio.voiceThresholdDb = v; });
    int vad = static_cast<int>(std::lround(working.audio.vadThreshold * 100.0));
    vad = std::max(0, std::min(100, vad));
    auto* vadR = new SliderRow(i18n("Rhythm.Vad"), 0, 100, vad, fmtVad, false);
    vadR->setOnChange([this](int v) { working.audio.vadThreshold = v / 100.0; });
    int rel = working.audio.releaseFrames;
    rel = std::max(1, std::min(40, rel));
    auto* silR = new SliderRow(i18n("Rhythm.SilenceDelay"), 1, 40, rel, fmtSilence, false);
    silR->setOnChange([this](int v) { working.audio.releaseFrames = v; });
    alay->addWidget(thrR);
    alay->addWidget(vadR);
    alay->addWidget(silR);
    contentLay[4]->addWidget(audio);

    contentLay[4]->addWidget(infoBanner(i18n("Rhythm.Banner")));
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
    for (size_t i = 0; i < working.speakers.size(); ++i) {
        const auto& s = working.speakers[i];
        const QString src = s.audioSource.empty() ? i18n("Summary.NoSource")
                                                   : QString::fromStdString(s.audioSource);
        spLines << speakerLabel(i) + QStringLiteral(" · ") + src;
    }
    if (spLines.isEmpty()) {
        spLines << i18n("Summary.NoSpeaker");
    }
    contentLay[5]->addWidget(recapCard(Icon::Users, i18n("Summary.Speakers"), 1, spLines));

    // Cameras (une ligne par intervenant : scenes + %)
    QStringList camLines;
    for (size_t i = 0; i < working.speakers.size(); ++i) {
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
        const QString detail = parts.isEmpty() ? i18n("Summary.NoCamera") : parts.join(QStringLiteral(" · "));
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
    wideLines << i18n("Summary.Multiple") + QStringLiteral(" : ") +
                     i18n("Wide.Loudest") + QStringLiteral(" ") +
                     QString::number(pctOf(working.whenMultiple.loudestSpeaker, mSum)) +
                     QStringLiteral("% · ") + i18n("Wide.Current") + QStringLiteral(" ") +
                     QString::number(pctOf(working.whenMultiple.currentSpeaker, mSum)) +
                     QStringLiteral("% · ") + i18n("Wide.WideShot") + QStringLiteral(" ") +
                     QString::number(pctOf(working.whenMultiple.wideShot, mSum)) +
                     QStringLiteral("%");
    const int sSum = working.whenSilence.lastSpeaker + working.whenSilence.wideShot;
    wideLines << i18n("Summary.Silence") + QStringLiteral(" : ") +
                     i18n("Wide.LastSpeaker") + QStringLiteral(" ") +
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
    summaryError->setStyleSheet(QString("color:%1; font-size:12px; font-weight:600;").arg(th::kDanger));
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

sd::core::Config SdAssistant::Impl::sanitized() const {
    sd::core::Config out = working;
    std::vector<sd::core::Speaker> kept;
    for (sd::core::Speaker s : out.speakers) {
        if (s.name.empty() || s.audioSource.empty()) {
            continue;  // intervenant incomplet -> non enregistre
        }
        std::vector<sd::core::SceneWeight> scenes;
        for (const auto& w : s.scenes) {
            if (!w.scene.empty() && w.weight > 0) {
                scenes.push_back(w);
            }
        }
        s.scenes = std::move(scenes);
        kept.push_back(std::move(s));
    }
    out.speakers = std::move(kept);
    return out;
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
    const sd::obsbridge::ConfigSaveResult res = sd::obsbridge::saveConfig(sanitized());
    if (!res.saved) {
        showError(i18n("Summary.Error.Save").arg(QString::fromStdString(res.error)));
        return;
    }
    activateAuto = true;
    q->accept();
}

// ===========================================================================
// SdAssistant — coquille (fenetre, drag, cycle de vie).
// ===========================================================================
SdAssistant::SdAssistant(QWidget* parent) : QDialog(parent), d_(new Impl) {
    d_->q = this;
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setWindowTitle(QStringLiteral("StreamDirector"));

    // Inventaire OBS (modale -> listes stables pendant toute la session).
    d_->audioSources = sd::obsbridge::audioSourceNames();
    d_->scenes = sd::obsbridge::sceneNames();
    const sd::obsbridge::ConfigLoadResult loaded = sd::obsbridge::loadConfig();
    if (loaded.parsed) {
        d_->working = loaded.config;  // on repart de la config existante
    }

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
        d_->content[i] = content;
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
