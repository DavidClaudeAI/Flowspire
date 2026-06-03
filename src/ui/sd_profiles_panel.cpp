// Flowspire — panneau "Profils" (implementation, Run 7). Voir le .hpp.
#include "ui/sd_profiles_panel.hpp"

#include <QAction>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <string>

#include "core/config.hpp"
#include "core/profiles.hpp"
#include "obs/profiles_store.hpp"
#include "ui/sd_i18n.hpp"
#include "ui/sd_icons.hpp"
#include "ui/sd_style.hpp"
#include "ui/sd_theme.hpp"
#include "ui/sd_widgets.hpp"

namespace sd::ui {

namespace th = sd::ui::theme;
using namespace sd::ui::widgets;

namespace {
// Resume d'une ligne : "n sources · plan large : X" ou "... · sans plan large".
QString summarize(const sd::core::Config& cfg) {
    int sources = 0;
    for (const auto& s : cfg.speakers) {
        if (!s.audioSource.empty()) {
            ++sources;
        }
    }
    const QString left = sources == 1 ? i18n("Profiles.Card.Source1") : i18n("Profiles.Card.SourceN").arg(sources);
    const QString right = cfg.wideShotScene.empty()
                              ? i18n("Profiles.Card.NoWide")
                              : i18n("Profiles.Card.Wide").arg(QString::fromStdString(cfg.wideShotScene));
    return left + QStringLiteral(" · ") + right;
}
} // namespace

ProfilesPanel::ProfilesPanel(QWidget* dialogParent, Callbacks callbacks)
    : dialogParent_(dialogParent),
      cb_(std::move(callbacks)) {}

void ProfilesPanel::mount(QVBoxLayout* host) {
    host_ = host;
    rebuild();
}

void ProfilesPanel::rebuild() {
    if (!host_) {
        return;
    }
    clearLayout(host_);

    host_->addWidget(buildHeaderRow());

    const sd::profiles::ListResult list = sd::profiles::loadList(i18n("Profiles.DefaultName").toStdString());
    if (!list.ok) {
        host_->addWidget(makeHint(i18n("Profiles.Error.Generic").arg(QString::fromStdString(list.error))));
        return;
    }

    for (const auto& e : list.index.profiles) {
        const bool active = e.id == list.index.activeId;
        QString summary;
        const sd::profiles::ConfigResult pc = sd::profiles::loadProfile(e.id);
        if (pc.ok) {
            summary = summarize(pc.config);
        }
        host_->addWidget(buildCard(e.id, QString::fromStdString(e.name), active, summary));
    }
}

QWidget* ProfilesPanel::buildHeaderRow() {
    auto* row = new QWidget();
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(12);

    auto* sub = new QLabel(i18n("Profiles.Subtitle"));
    sub->setWordWrap(true);
    sub->setStyleSheet(QString("color:%1; font-size:13px;").arg(th::kTextSecondary));

    // Bouton "+ Nouveau" (menu : avec / sans assistant) — en haut a droite.
    auto* newBtn = new QPushButton(i18n("Profiles.New"));
    newBtn->setIcon(icon(Icon::Plus, th::kOnAccent, 14));
    newBtn->setCursor(Qt::PointingHandCursor);
    newBtn->setStyleSheet(primaryBtnQss());
    QObject::connect(newBtn, &QPushButton::clicked, newBtn, [this, newBtn]() {
        QMenu menu;
        menu.setStyleSheet(menuQss());
        QAction* withA = menu.addAction(i18n("Profiles.New.WithAssistant"));
        QAction* blank = menu.addAction(i18n("Profiles.New.Blank"));
        QAction* chosen = menu.exec(newBtn->mapToGlobal(QPoint(0, newBtn->height() + 4)));
        if (chosen == withA && cb_.onNewWithAssistant) {
            cb_.onNewWithAssistant();
        } else if (chosen == blank && cb_.onNewBlank) {
            cb_.onNewBlank();
        }
    });

    lay->addWidget(sub, 1);
    lay->addWidget(newBtn, 0, Qt::AlignTop);
    return row;
}

QWidget* ProfilesPanel::buildCard(int id, const QString& name, bool active, const QString& summary) {
    auto* card = new QWidget();
    card->setObjectName(QStringLiteral("profCard"));
    card->setStyleSheet(QString("#profCard { background:%1; border:1px solid %2; border-radius:%3px; }")
                            .arg(th::kSurface2)
                            .arg(active ? rgba(th::kAccent, 0.6) : QString::fromUtf8(th::kBorder))
                            .arg(th::kRadiusCard));
    auto* lay = new QHBoxLayout(card);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(12);

    // Pastille icone (accent translucide si actif).
    auto* iconBox = new QLabel();
    iconBox->setFixedSize(34, 34);
    iconBox->setAlignment(Qt::AlignCenter);
    iconBox->setPixmap(icon(Icon::Users, active ? th::kAccent : th::kTextSecondary, 16));
    iconBox->setStyleSheet(QString("background:%1; border-radius:8px;")
                               .arg(active ? rgba(th::kAccent, 0.14) : QString::fromUtf8(th::kSurface3)));

    // Bloc texte : nom + resume.
    auto* mid = new QWidget();
    auto* midLay = new QVBoxLayout(mid);
    midLay->setContentsMargins(0, 0, 0, 0);
    midLay->setSpacing(2);
    auto* nameLbl = new QLabel(name);
    nameLbl->setStyleSheet(QString("color:%1; font-size:13px; font-weight:600;").arg(th::kTextPrimary));
    auto* sumLbl = new QLabel(summary);
    sumLbl->setStyleSheet(QString("color:%1; font-size:11px;").arg(th::kTextTertiary));
    midLay->addWidget(nameLbl);
    midLay->addWidget(sumLbl);

    lay->addWidget(iconBox, 0, Qt::AlignVCenter);
    lay->addWidget(mid, 1);

    if (active) {
        // Badge "● Actif".
        auto* badge = new QWidget();
        badge->setObjectName(QStringLiteral("profActive"));
        badge->setStyleSheet(QString("#profActive { background:%1; border:1px solid %2;"
                                     " border-radius:10px; }")
                                 .arg(rgba(th::kSuccess, 0.14))
                                 .arg(rgba(th::kSuccess, 0.5)));
        auto* bl = new QHBoxLayout(badge);
        bl->setContentsMargins(10, 5, 10, 5);
        bl->setSpacing(5);
        auto* dot = new QLabel(QStringLiteral("●"));
        dot->setStyleSheet(QString("color:%1; font-size:9px;").arg(th::kSuccess));
        auto* txt = new QLabel(i18n("Profiles.Active"));
        txt->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700;").arg(th::kSuccess));
        bl->addWidget(dot);
        bl->addWidget(txt);
        lay->addWidget(badge, 0, Qt::AlignVCenter);
    } else {
        // Bouton "Charger".
        auto* load = new QPushButton(i18n("Profiles.Load"));
        load->setCursor(Qt::PointingHandCursor);
        load->setStyleSheet(QString("QPushButton { background:%1; border:none; border-radius:6px;"
                                    " color:%2; font-size:11px; font-weight:600; padding:6px 12px; }"
                                    "QPushButton:hover { background:%3; }")
                                .arg(th::kSurface3)
                                .arg(th::kTextSecondary)
                                .arg(rgba(th::kAccent, 0.2)));
        QObject::connect(load, &QPushButton::clicked, load, [this, id]() {
            if (cb_.onLoad) {
                cb_.onLoad(id);
            }
        });
        lay->addWidget(load, 0, Qt::AlignVCenter);
    }

    // Menu "⋮".
    auto* more = new QPushButton();
    more->setIcon(icon(Icon::EllipsisV, th::kTextTertiary, 16));
    more->setCursor(Qt::PointingHandCursor);
    more->setFlat(true);
    more->setStyleSheet(iconBtnQss());
    QObject::connect(more, &QPushButton::clicked, more, [this, id, active, more]() { showRowMenu(id, active, more); });
    lay->addWidget(more, 0, Qt::AlignVCenter);

    return card;
}

void ProfilesPanel::showRowMenu(int id, bool active, QWidget* anchor) {
    QMenu menu;
    menu.setStyleSheet(menuQss());
    QAction* rename = menu.addAction(i18n("Profiles.Menu.Rename"));
    QAction* duplicate = menu.addAction(i18n("Profiles.Menu.Duplicate"));
    QAction* edit = menu.addAction(i18n("Profiles.Menu.Edit"));
    menu.addSeparator();
    QAction* del = menu.addAction(i18n("Profiles.Menu.Delete"));
    // On ne supprime jamais le profil actif (basculer d'abord) -> entree grisee.
    del->setEnabled(!active);

    QAction* chosen = menu.exec(anchor->mapToGlobal(QPoint(anchor->width(), anchor->height())));
    if (chosen == nullptr) {
        return;
    }
    if (chosen == rename) {
        doRename(id);
    } else if (chosen == duplicate) {
        doDuplicate(id);
    } else if (chosen == edit) {
        if (cb_.onEdit) {
            cb_.onEdit(id);
        }
    } else if (chosen == del) {
        // Nom courant pour la confirmation.
        const sd::profiles::ListResult list = sd::profiles::loadList(i18n("Profiles.DefaultName").toStdString());
        QString name;
        if (list.ok) {
            if (const sd::core::ProfileEntry* e = sd::core::findProfile(list.index, id)) {
                name = QString::fromStdString(e->name);
            }
        }
        doDelete(id, name);
    }
}

void ProfilesPanel::doRename(int id) {
    // Nom courant pour pre-remplir.
    const sd::profiles::ListResult list = sd::profiles::loadList(i18n("Profiles.DefaultName").toStdString());
    QString current;
    if (list.ok) {
        if (const sd::core::ProfileEntry* e = sd::core::findProfile(list.index, id)) {
            current = QString::fromStdString(e->name);
        }
    }
    bool ok = false;
    const QString name = QInputDialog::getText(dialogParent_, i18n("Profiles.NamePrompt.Title"),
                                               i18n("Profiles.NamePrompt.Body"), QLineEdit::Normal, current, &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    const sd::profiles::StoreResult res = sd::profiles::renameProfile(id, name.trimmed().toStdString());
    if (!res.ok) {
        QMessageBox::warning(dialogParent_, i18n("Settings.Title"),
                             i18n("Profiles.Error.Generic").arg(QString::fromStdString(res.error)));
    }
    rebuild();
}

void ProfilesPanel::doDuplicate(int id) {
    const sd::profiles::StoreResult res = sd::profiles::duplicateProfile(id, i18n("Profiles.CopySuffix").toStdString());
    if (!res.ok) {
        QMessageBox::warning(dialogParent_, i18n("Settings.Title"),
                             i18n("Profiles.Error.Generic").arg(QString::fromStdString(res.error)));
    }
    rebuild();
}

void ProfilesPanel::doDelete(int id, const QString& name) {
    const QMessageBox::StandardButton answer =
        QMessageBox::question(dialogParent_, i18n("Profiles.Delete.Title"), i18n("Profiles.Delete.Body").arg(name),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    const sd::profiles::StoreResult res = sd::profiles::removeProfile(id);
    if (!res.ok) {
        QMessageBox::warning(dialogParent_, i18n("Settings.Title"),
                             i18n("Profiles.Error.Generic").arg(QString::fromStdString(res.error)));
    }
    rebuild();
}

} // namespace sd::ui
