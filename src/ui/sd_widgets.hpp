// Flowspire — briques d'UI partagees (couche UI).
//
// Source UNIQUE des petits widgets et helpers de style reutilises par l'assistant
// (sd_assistant) ET les parametres avances (sd_settings) -> pas de copier-coller
// entre les deux fenetres (lecon de la review Run 5 : eviter la duplication).
//
// Header-only : tout est `inline`. Les classes (ClickButton/ComboField/SliderRow)
// n'utilisent PAS Q_OBJECT (callbacks std::function, pas de signal Qt custom) ->
// aucun moc requis, definissables en en-tete sans souci d'ODR.
//
// Espace de noms `sd::ui::widgets` (imbrique dans sd::ui) : `icon`/`Icon`/`rgba`/
// `kFrameSeconds`/`theme::` y sont visibles par recherche dans l'espace englobant.
#pragma once

#include <QAction>
#include <QEnterEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QSlider>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <functional>
#include <utility>

#include "ui/sd_icons.hpp"   // icon(), Icon
#include "ui/sd_runtime.hpp" // kFrameSeconds
#include "ui/sd_style.hpp"   // rgba()
#include "ui/sd_theme.hpp"   // theme::k*

namespace sd::ui::widgets {

// ===========================================================================
// Feuilles de style (QSS) — traduisent les jetons de la maquette.
// ===========================================================================
inline QString menuQss() {
    return QString("QMenu { background:%1; border:1px solid %2; border-radius:6px; padding:4px; }"
                   "QMenu::item { color:%3; padding:6px 22px 6px 12px; border-radius:4px; }"
                   "QMenu::item:selected { background:%4; }"
                   "QMenu::item:disabled { color:%5; }")
        .arg(theme::kSurface2)
        .arg(theme::kBorder)
        .arg(theme::kTextPrimary)
        .arg(rgba(theme::kAccent, 0.25))
        .arg(theme::kTextTertiary);
}

inline QString accentSliderQss() {
    return QString("QSlider::groove:horizontal { height:6px; background:%1; border-radius:3px; }"
                   "QSlider::sub-page:horizontal { height:6px; background:%2; border-radius:3px; }"
                   "QSlider::add-page:horizontal { height:6px; background:%1; border-radius:3px; }"
                   "QSlider::handle:horizontal { width:14px; height:14px; margin:-4px 0;"
                   " border-radius:7px; background:%3; border:2px solid %2; }")
        .arg(theme::kSurface3)
        .arg(theme::kAccent)
        .arg(theme::kTextPrimary);
}

inline QString badgeQss() {
    return QString("color:%1; background:%2; border-radius:5px; font-size:11px;"
                   " font-weight:600; padding:2px 0;")
        .arg(theme::kAccent)
        .arg(rgba(theme::kAccent, 0.15));
}

inline QString lineEditQss() {
    return QString("QLineEdit { background:%1; border:1px solid %2; border-radius:%3px;"
                   " color:%4; font-size:13px; padding:6px 10px; }"
                   "QLineEdit:focus { border-color:%5; }")
        .arg(theme::kSurface3)
        .arg(theme::kBorder)
        .arg(theme::kRadiusButton)
        .arg(theme::kTextPrimary)
        .arg(rgba(theme::kAccent, 0.7));
}

inline QString iconBtnQss() {
    return QStringLiteral("QPushButton { background:transparent; border:none; padding:3px; }"
                          "QPushButton:hover { background:rgba(255,255,255,0.06);"
                          " border-radius:4px; }");
}

inline QString primaryBtnQss() {
    return QString("QPushButton { background:%1; border:none; border-radius:6px; color:%2;"
                   " font-size:13px; font-weight:700; padding:10px 16px; }"
                   "QPushButton:hover { background:%3; }")
        .arg(theme::kAccent)
        .arg(theme::kOnAccent)
        .arg(rgba(theme::kAccent, 0.85));
}

inline QString greenBtnQss() {
    return QString("QPushButton { background:%1; border:none; border-radius:6px; color:%2;"
                   " font-size:13px; font-weight:700; padding:10px 16px; }"
                   "QPushButton:hover { background:%3; }")
        .arg(theme::kSuccess)
        .arg(theme::kOnAccent)
        .arg(rgba(theme::kSuccess, 0.85));
}

inline QString secondaryBtnQss() {
    return QString("QPushButton { background:%1; border:none; border-radius:6px; color:%2;"
                   " font-size:13px; font-weight:600; padding:10px 14px; }"
                   "QPushButton:hover { color:%3; }")
        .arg(theme::kSurface3)
        .arg(theme::kTextSecondary)
        .arg(theme::kTextPrimary);
}

// Bouton "pilule" a contour accent : fond surface, bordure + texte bleus (accent), bordure
// qui s'eclaire au survol. Mutualise les boutons accent du dock (bandeau MAJ, footer, et la
// calibration #3). `padding` au format QSS, ex "5px 10px".
inline QString accentOutlineBtnQss(const QString& padding) {
    return QString("QPushButton { background:%1; border:1px solid %2; border-radius:%3px;"
                   " color:%4; font-size:%5px; font-weight:600; padding:%6; }"
                   "QPushButton:hover { border-color:%4; }")
        .arg(theme::kSurface3)
        .arg(rgba(theme::kAccent, 0.4))
        .arg(theme::kRadiusButton)
        .arg(theme::kAccent)
        .arg(theme::kFontButton)
        .arg(padding);
}

// Onglet "dossier" (assistant etape Scenes + parametres avances) : coins arrondis
// EN HAUT seulement, actif = couleur de la carte (effet rattache), inactif plus
// sombre. Contour present (demande David). Selecteur #clickbtn (ClickButton).
inline QString tabBoxQss(bool active) {
    if (active) {
        return QString("#clickbtn { background:%1; border:1px solid %2; border-bottom:none;"
                       " border-top-left-radius:8px; border-top-right-radius:8px; }")
            .arg(theme::kSurface2)
            .arg(rgba(theme::kAccent, 0.55));
    }
    return QString("#clickbtn { background:%1; border:1px solid %2; border-bottom:none;"
                   " border-top-left-radius:8px; border-top-right-radius:8px; }")
        .arg(theme::kBg)
        .arg(rgba(theme::kBorder, 1.0));
}

// Segment de barre de progression (assistant).
inline QString segQss(bool filled) {
    return QString("QFrame { background:%1; border-radius:2px; }")
        .arg(filled ? QString::fromUtf8(theme::kAccent) : QString::fromUtf8(theme::kSurface3));
}

// Case a cocher / bouton radio : on ne stylise QUE le texte et l'espacement, et on
// laisse l'INDICATEUR natif de l'OS (coche / pastille) -> rendu standard, fiable sur
// Windows/macOS/Linux et coherent avec les reglages d'OBS (eviter le piege Qt du
// `::indicator` custom qui exige une image pour la coche).
inline QString checkBoxQss() {
    return QString("QCheckBox { color:%1; font-size:13px; spacing:8px; }").arg(theme::kTextPrimary);
}
inline QString radioQss() {
    return QString("QRadioButton { color:%1; font-size:13px; spacing:8px; padding:2px 0; }").arg(theme::kTextPrimary);
}

// ===========================================================================
// Aide contextuelle (tooltips) — petit ⓘ au survol.
// ===========================================================================
// Qt affiche un tooltip de texte BRUT sur une seule longue ligne ; un tooltip en
// TEXTE ENRICHI (HTML) est auto-mis en forme et revient a la ligne (doc Qt 6).
// On enveloppe donc le texte dans un tableau a largeur fixe -> petite "boite"
// carree lisible (et largeur contrainte fiable sur toutes les versions de Qt,
// contrairement a width: sur un <p>). Le texte est echappe (HTML-safe).
inline QString richTip(const QString& text) {
    return QStringLiteral("<table><tr><td width='250'>%1</td></tr></table>").arg(text.toHtmlEscaped());
}

// Petit icone ⓘ qui affiche `tip` (en boite) au survol. Cible de survol a part
// entiere (pas transparent a la souris, sinon Qt n'affiche pas le tooltip).
inline QLabel* makeInfoIcon(const QString& tip) {
    auto* l = new QLabel();
    l->setPixmap(icon(Icon::Info, theme::kTextTertiary, 14));
    l->setToolTip(richTip(tip));
    l->setCursor(Qt::WhatsThisCursor);
    l->setFixedWidth(18);
    l->setAlignment(Qt::AlignCenter);
    return l;
}

// Enrobe un champ (combo, champ texte...) avec un ⓘ a sa DROITE -> "[champ] ⓘ".
// Pour annoter les controles qui n'ont pas de libelle propre (listes deroulantes).
inline QWidget* withInfo(QWidget* field, const QString& tip) {
    auto* row = new QWidget();
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);
    lay->addWidget(field, 1);
    lay->addWidget(makeInfoIcon(tip), 0, Qt::AlignVCenter);
    return row;
}

// ===========================================================================
// FlowLayout — dispose les enfants horizontalement et passe a la LIGNE quand la largeur
// disponible est atteinte (style "chips / tags"). Sert a ce qu'une rangee dont le nombre
// d'elements peut grandir (ex. les pastilles de style : 5 styles + "Perso") n'impose pas une
// largeur mini trop grande et ne deborde pas une fenetre etroite : elle s'enroule sur plusieurs
// lignes. Adaptation du FlowLayout canonique de Qt (hauteur calculee en fonction de la largeur).
// Pas de Q_OBJECT (aucun signal custom) -> header-only sans moc, comme les autres widgets ici.
// ===========================================================================
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = 0, int hSpacing = 8, int vSpacing = 8)
        : QLayout(parent), hSpace_(hSpacing), vSpace_(vSpacing) {
        setContentsMargins(margin, margin, margin, margin);
    }
    ~FlowLayout() override {
        QLayoutItem* item = nullptr;
        while ((item = takeAt(0)) != nullptr) {
            delete item;
        }
    }

    void addItem(QLayoutItem* item) override { items_.append(item); }
    int count() const override { return static_cast<int>(items_.size()); }
    QLayoutItem* itemAt(int index) const override { return items_.value(index); }
    QLayoutItem* takeAt(int index) override {
        return (index >= 0 && index < items_.size()) ? items_.takeAt(index) : nullptr;
    }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }
    void setGeometry(const QRect& rect) override {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override {
        QSize s;
        for (QLayoutItem* item : items_) {
            s = s.expandedTo(item->minimumSize());
        }
        const QMargins m = contentsMargins();
        return s + QSize(m.left() + m.right(), m.top() + m.bottom());
    }

private:
    int doLayout(const QRect& rect, bool testOnly) const {
        const QMargins m = contentsMargins();
        const QRect eff = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
        int x = eff.x();
        int y = eff.y();
        int lineHeight = 0;
        for (QLayoutItem* item : items_) {
            const QSize hint = item->sizeHint();
            int nextX = x + hint.width() + hSpace_;
            if (nextX - hSpace_ > eff.right() + 1 && lineHeight > 0) {
                x = eff.x();
                y += lineHeight + vSpace_;
                nextX = x + hint.width() + hSpace_;
                lineHeight = 0;
            }
            if (!testOnly) {
                item->setGeometry(QRect(QPoint(x, y), hint));
            }
            x = nextX;
            lineHeight = qMax(lineHeight, hint.height());
        }
        return y + lineHeight - rect.y() + m.bottom();
    }

    QList<QLayoutItem*> items_;
    int hSpace_;
    int vSpace_;
};

// ===========================================================================
// ClickButton — bouton cliquable custom : icone (optionnelle) + texte CENTRES via
// un layout interne. On n'utilise PAS QPushButton::setIcon : sur un bouton LARGE,
// Qt aligne l'icone a gauche et centre le texte separement -> gros trou (bug vu par
// David). Pas de signal Qt custom (pas de moc), callback std::function ; les labels
// enfants sont transparents a la souris -> le clic remonte toujours au ClickButton.
// ===========================================================================
class ClickButton : public QFrame {
public:
    explicit ClickButton(QWidget* parent = nullptr) : QFrame(parent) {
        setObjectName(QStringLiteral("clickbtn"));
        setAttribute(Qt::WA_StyledBackground, true); // fond/bordure QSS reellement peints
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
    void setExpanding() { // contenu centre sur toute la largeur (boutons "Ajouter")
        lay_->insertStretch(0);
        lay_->addStretch();
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setFillLeft() { // pleine largeur, contenu calé a GAUCHE (lignes de sidebar)
        lay_->addStretch();
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setIconPix(const QPixmap& p) {
        iconLabel_->setPixmap(p);
        iconLabel_->setVisible(!p.isNull());
    }
    void setLabel(const QString& t, const char* color, int size, int weight) {
        textLabel_->setText(t);
        textLabel_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:%3; background:transparent;")
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

// ===========================================================================
// ComboField — liste deroulante stylee (texte + chevron + QMenu). On n'utilise PAS
// QComboBox (sa fleche se style via une image QSS fragile) : on reutilise nos
// icones lucide. Pas de signal Qt custom (pas de moc) -> callback std::function.
// ===========================================================================
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
        chevron_->setPixmap(icon(Icon::ChevronDown, theme::kTextSecondary, 14));
        lay->addWidget(text_, 1);
        lay->addWidget(chevron_);
        setStyleSheet(QString("#combo { background:%1; border:1px solid %2; border-radius:%3px; }"
                              "#combo:hover { border:1px solid %4; }")
                          .arg(theme::kSurface3)
                          .arg(theme::kBorder)
                          .arg(theme::kRadiusButton)
                          .arg(rgba(theme::kAccent, 0.6)));
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
    // Selectionne `v` PAR PROGRAMME (ne declenche PAS onChange) : sert a refleter l'etat
    // courant (ex : le style perso actif) sans reboucler sur le callback.
    void setValue(const QString& v) {
        value_ = v;
        updateText();
    }
    void setOnChange(std::function<void(const QString&)> cb) { cb_ = std::move(cb); }

protected:
    void mouseReleaseEvent(QMouseEvent*) override { openMenu(); }

private:
    void updateText() {
        const bool empty = value_.isEmpty();
        text_->setText(empty ? placeholder_ : value_);
        text_->setStyleSheet(
            QString("color:%1; font-size:13px;").arg(empty ? theme::kTextTertiary : theme::kTextPrimary));
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

// ===========================================================================
// SliderRow — libelle + slider + valeur formatee (+ badge % optionnel).
// ===========================================================================
class SliderRow : public QWidget {
public:
    SliderRow(const QString& label, int min, int max, int value, std::function<QString(int)> fmt, bool withBadge,
              QWidget* parent = nullptr)
        : QWidget(parent),
          fmt_(std::move(fmt)) {
        auto* lay = new QHBoxLayout(this);
        rowLay_ = lay; // memorise pour pouvoir appendre un ⓘ via setInfo()
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(12);
        label_ = new QLabel(label, this);
        label_->setStyleSheet(QString("color:%1; font-size:13px;").arg(theme::kTextSecondary));
        label_->setMinimumWidth(132);
        slider_ = new QSlider(Qt::Horizontal, this);
        slider_->setRange(min, max);
        slider_->setValue(value);
        slider_->setStyleSheet(accentSliderQss());
        slider_->setCursor(Qt::PointingHandCursor);
        valueLabel_ = new QLabel(this);
        valueLabel_->setMinimumWidth(42);
        valueLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel_->setStyleSheet(QString("color:%1; font-size:13px; font-weight:600;").arg(theme::kTextPrimary));
        lay->addWidget(label_);
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
    // Repositionne le slider par programme (declenche valueChanged -> label + cb).
    // Sert au couplage min<=max (etape Rythme) sans contourner la logique de la ligne.
    void setValue(int v) { slider_->setValue(v); }
    void setBadge(int pct) {
        if (badge_) {
            badge_->setText(QString::number(pct) + QStringLiteral("%"));
        }
    }
    void setOnChange(std::function<void(int)> cb) { cb_ = std::move(cb); }
    // Ajoute un petit ⓘ a la FIN de la ligne ; son survol affiche `tip` en boite.
    void setInfo(const QString& tip) {
        if (info_ || !rowLay_) {
            return;
        }
        info_ = makeInfoIcon(tip);
        rowLay_->addWidget(info_, 0, Qt::AlignVCenter);
    }

private:
    void updateValueText() { valueLabel_->setText(fmt_ ? fmt_(slider_->value()) : QString::number(slider_->value())); }
    QHBoxLayout* rowLay_ = nullptr; // layout de la ligne (pour appendre le ⓘ)
    QLabel* label_ = nullptr;
    QSlider* slider_ = nullptr;
    QLabel* valueLabel_ = nullptr;
    QLabel* badge_ = nullptr;
    QLabel* info_ = nullptr; // ⓘ optionnel (setInfo), nullptr si absent
    std::function<QString(int)> fmt_;
    std::function<void(int)> cb_;
};

// ===========================================================================
// CollapsibleSection — section repliable : en-tete cliquable (chevron + titre +
// mention discrete) qui montre/cache un corps. Sert au tiroir "Parametres avances
// de realisation" de l'assistant. Header-only, sans Q_OBJECT (toggle interne, pas
// de signal custom) -> coherent avec ClickButton/ComboField. Repliee par defaut.
// ===========================================================================
class CollapsibleSection : public QWidget {
public:
    explicit CollapsibleSection(const QString& title, const QString& note = QString(), QWidget* parent = nullptr)
        : QWidget(parent) {
        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(10);

        // En-tete cliquable (chevron ▸/▾ + titre + mention grise). Les enfants sont
        // transparents a la souris -> le clic remonte au Header (comme ClickButton).
        header_ = new Header(this, [this]() { setExpanded(!expanded_); });
        auto* hl = new QHBoxLayout(header_);
        hl->setContentsMargins(2, 4, 2, 4);
        hl->setSpacing(8);
        chevron_ = new QLabel(header_);
        chevron_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        title_ = new QLabel(title, header_);
        title_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        title_->setStyleSheet(QString("color:%1; font-size:13px; font-weight:600;").arg(theme::kTextPrimary));
        hl->addWidget(chevron_);
        hl->addWidget(title_);
        if (!note.isEmpty()) {
            auto* n = new QLabel(note, header_);
            n->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            n->setStyleSheet(QString("color:%1; font-size:12px;").arg(theme::kTextTertiary));
            hl->addSpacing(6);
            hl->addWidget(n);
        }
        hl->addStretch();
        outer->addWidget(header_);

        body_ = new QWidget(this);
        bodyLay_ = new QVBoxLayout(body_);
        bodyLay_->setContentsMargins(0, 0, 0, 0);
        bodyLay_->setSpacing(14);
        body_->setVisible(expanded_);
        outer->addWidget(body_);

        updateChevron();
    }

    // Corps repliable : l'appelant y ajoute son contenu via body()->addWidget(...).
    QVBoxLayout* body() { return bodyLay_; }

    void setExpanded(bool on) {
        expanded_ = on;
        body_->setVisible(on);
        updateChevron();
    }

private:
    // En-tete cliquable a callback (meme pattern que ClickButton, sans Q_OBJECT).
    class Header : public QFrame {
    public:
        Header(QWidget* parent, std::function<void()> cb) : QFrame(parent), cb_(std::move(cb)) {
            setObjectName(QStringLiteral("collHeader"));
            setAttribute(Qt::WA_StyledBackground, true);
            setCursor(Qt::PointingHandCursor);
            setStyleSheet(QString("#collHeader { background:transparent; border:none; border-radius:6px; }"
                                  "#collHeader:hover { background:%1; }")
                              .arg(rgba(theme::kAccent, 0.08)));
        }

    protected:
        void mouseReleaseEvent(QMouseEvent* e) override {
            if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && cb_) {
                cb_();
            }
        }

    private:
        std::function<void()> cb_;
    };

    void updateChevron() {
        chevron_->setPixmap(icon(expanded_ ? Icon::ChevronDown : Icon::ChevronRight, theme::kTextSecondary, 16));
    }

    Header* header_ = nullptr;
    QLabel* chevron_ = nullptr;
    QLabel* title_ = nullptr;
    QWidget* body_ = nullptr;
    QVBoxLayout* bodyLay_ = nullptr;
    bool expanded_ = false;
};

// ===========================================================================
// Fabriques de petits widgets + helpers.
// ===========================================================================
inline QLabel* makeTitle(const QString& t) {
    auto* l = new QLabel(t);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:20px; font-weight:700;").arg(theme::kTextPrimary));
    return l;
}

inline QLabel* makeSub(const QString& t) {
    auto* l = new QLabel(t);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:13px;").arg(theme::kTextSecondary));
    return l;
}

inline QLabel* makeSectionLabel(const QString& t) {
    auto* l = new QLabel(t);
    l->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px;").arg(theme::kTextTertiary));
    return l;
}

inline QLabel* makeHint(const QString& t) {
    auto* l = new QLabel(t);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:13px;").arg(theme::kTextTertiary));
    return l;
}

// Astuce/conseil en couleur ACCENT (12px, retour a la ligne) : pendant de makeHint (gris)
// pour les conseils qu'on veut faire RESSORTIR (inciter a definir un plan large, rappel
// calibration). Mutualise le style sinon copie entre l'assistant et les panneaux partages.
inline QLabel* makeAccentTip(const QString& t) {
    auto* l = new QLabel(t);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color:%1; font-size:12px;").arg(theme::kAccent));
    return l;
}

inline QLabel* makeGroupHeader(const QString& t) {
    auto* l = new QLabel(t);
    l->setStyleSheet(QString("color:%1; font-size:13px; font-weight:600;").arg(theme::kTextSecondary));
    return l;
}

// Bouton "Ajouter ..." pleine largeur : icone + texte CENTRES (ClickButton), fond
// surface2 + bordure border, liseré accent au survol. Callback std::function.
inline ClickButton* makeAddButton(const QString& text, std::function<void()> onClick) {
    auto* b = new ClickButton();
    b->setExpanding();
    b->setMargins(12, 11);
    b->setIconPix(icon(Icon::Plus, theme::kAccent, 15));
    b->setLabel(text, theme::kAccent, 13, 600);
    const QString base = QStringLiteral("#clickbtn { background:%1; border:1px solid %2; border-radius:8px; }");
    b->setBox(base.arg(theme::kSurface2).arg(theme::kBorder),
              base.arg(theme::kSurface2).arg(rgba(theme::kAccent, 0.6)));
    b->setOnClick(std::move(onClick));
    return b;
}

// Carte (fond surface2 + bordure arrondie). `obj` = objectName UNIQUE par type de
// carte -> le selecteur #obj ne cascade pas vers les enfants (lecon Run 4).
inline QWidget* makeCard(const QString& obj) {
    auto* w = new QWidget();
    w->setObjectName(obj);
    w->setStyleSheet(QString("#%1 { background:%2; border:1px solid %3; border-radius:%4px; }")
                         .arg(obj)
                         .arg(theme::kSurface2)
                         .arg(theme::kBorder)
                         .arg(theme::kRadiusCard));
    return w;
}

// Vide un layout en differant la destruction des widgets (hide + deleteLater).
// IMPORTANT (cause racine d'un crash corrige en Run 5) : un widget peut declencher
// sa propre destruction depuis son gestionnaire d'evenement (un onglet/bouton dont
// le clic reconstruit la page) ; un `delete` immediat = use-after-free pendant que
// Qt deroule encore l'event. deleteLater() repousse la destruction a la fin de
// l'evenement courant ; hide() evite tout doublon a l'ecran entre-temps.
inline void clearLayout(QLayout* lay) {
    if (!lay) {
        return;
    }
    while (QLayoutItem* item = lay->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        } else if (QLayout* child = item->layout()) {
            clearLayout(child);
        }
        delete item;
    }
}

// Pourcentage entier d'une valeur dans une somme (0 si somme nulle).
inline int pctOf(int v, int sum) {
    return sum > 0 ? static_cast<int>(std::lround(100.0 * v / sum)) : 0;
}

// ===========================================================================
// Formats de valeur (unites universelles, non traduites).
// ===========================================================================
inline QString fmtSeconds(int v) {
    return QString::number(v) + QStringLiteral(" s");
}
inline QString fmtDb(int v) {
    return QString::number(v) + QStringLiteral(" dB");
}
// Delai en secondes a partir d'un nombre de frames (1 frame = kFrameSeconds = 50 ms).
// 2 decimales : le pas d'un cran (0,05 s) reste lisible -- sert au delai de silence
// ET au delai d'attaque.
inline QString fmtSilence(int frames) {
    return QString::number(frames * sd::ui::kFrameSeconds, 'f', 2) + QStringLiteral(" s");
}

} // namespace sd::ui::widgets
