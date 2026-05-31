// StreamDirector — pop-up "Soutenir le projet" (implementation, Run 6).
#include "ui/sd_support.hpp"

#include <QColor>
#include <QDesktopServices>
#include <QDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QUrl>
#include <QVBoxLayout>

#include "ui/sd_icons.hpp"
#include "ui/sd_i18n.hpp"
#include "ui/sd_style.hpp"
#include "ui/sd_theme.hpp"
#include "ui/sd_widgets.hpp"

namespace sd::ui {

namespace th = sd::ui::theme;
using namespace sd::ui::widgets;

namespace {
// URLs de soutien. ⚠️ A REMPLACER par les vrais liens de David quand ils existent.
// Par defaut : la page du projet (lien reel, non trompeur). cf. todo Run 6.
constexpr const char* kUrlDonate = "https://github.com/DavidClaudeAI/StreamDirector";
constexpr const char* kUrlSponsors = "https://github.com/sponsors/DavidClaudeAI";
constexpr const char* kUrlKofi = "https://ko-fi.com/";
constexpr const char* kUrlPaypal = "https://www.paypal.com/";

// Petit lien bas de pop-up : icone + libelle, fond surface2, bordure border.
QPushButton* linkButton(Icon ic, const QString& label, const char* url) {
    auto* b = new QPushButton(label);
    b->setIcon(icon(ic, th::kTextSecondary, 13));
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QString("QPushButton { background:%1; border:1px solid %2; border-radius:6px;"
                             " color:%3; font-size:12px; font-weight:600; padding:8px 12px; }"
                             "QPushButton:hover { border-color:%4; }")
                         .arg(th::kSurface2)
                         .arg(th::kBorder)
                         .arg(th::kTextSecondary)
                         .arg(rgba(th::kDanger, 0.5)));
    QObject::connect(b, &QPushButton::clicked, b,
                     [url]() { QDesktopServices::openUrl(QUrl(QString::fromUtf8(url))); });
    return b;
}
}  // namespace

void showSupportDialog(QWidget* parent) {
    QDialog dlg(parent);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground);
    dlg.setModal(true);
    dlg.setWindowTitle(QStringLiteral("StreamDirector"));

    auto* outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(24, 18, 24, 30);  // marge pour l'ombre

    auto* root = new QWidget(&dlg);
    root->setObjectName(QStringLiteral("supportRoot"));
    root->setFixedWidth(460);
    root->setStyleSheet(QString("#supportRoot { background:%1; border:1px solid %2;"
                                " border-radius:12px; }")
                            .arg(th::kSurface1)
                            .arg(th::kBorder));
    auto* shadow = new QGraphicsDropShadowEffect(root);
    shadow->setBlurRadius(50);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 18);
    root->setGraphicsEffect(shadow);
    outer->addWidget(root);

    auto* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    // En-tete : coeur + titre + croix.
    auto* header = new QWidget();
    auto* hLay = new QHBoxLayout(header);
    hLay->setContentsMargins(20, 16, 20, 16);
    hLay->setSpacing(8);
    auto* hIcon = new QLabel();
    hIcon->setPixmap(icon(Icon::Heart, th::kDanger, 18));
    auto* hTitle = new QLabel(i18n("Support.Title"));
    hTitle->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700;").arg(th::kTextPrimary));
    auto* close = new QPushButton();
    close->setIcon(icon(Icon::X, th::kTextTertiary, 16));
    close->setCursor(Qt::PointingHandCursor);
    close->setFlat(true);
    close->setStyleSheet(iconBtnQss());
    QObject::connect(close, &QPushButton::clicked, &dlg, [&dlg]() { dlg.reject(); });
    hLay->addWidget(hIcon);
    hLay->addWidget(hTitle);
    hLay->addStretch();
    hLay->addWidget(close);
    rootLay->addWidget(header);

    auto* sep = new QFrame();
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(th::kBorder));
    rootLay->addWidget(sep);

    // Corps centre : gros coeur, titre, texte, bouton don, liens.
    auto* body = new QWidget();
    auto* bLay = new QVBoxLayout(body);
    bLay->setContentsMargins(24, 24, 24, 24);
    bLay->setSpacing(16);

    auto* bigWrap = new QWidget();
    auto* bwl = new QHBoxLayout(bigWrap);
    bwl->setContentsMargins(0, 0, 0, 0);
    auto* big = new QLabel();
    big->setFixedSize(60, 60);
    big->setAlignment(Qt::AlignCenter);
    big->setPixmap(icon(Icon::Heart, th::kDanger, 28));
    big->setStyleSheet(QString("background:%1; border:1px solid %2; border-radius:30px;")
                           .arg(rgba(th::kDanger, 0.14))
                           .arg(rgba(th::kDanger, 0.5)));
    bwl->addStretch();
    bwl->addWidget(big);
    bwl->addStretch();
    bLay->addWidget(bigWrap);

    auto* headline = new QLabel(i18n("Support.Headline"));
    headline->setWordWrap(true);
    headline->setAlignment(Qt::AlignCenter);
    headline->setStyleSheet(QString("color:%1; font-size:17px; font-weight:700;").arg(th::kTextPrimary));
    bLay->addWidget(headline);

    auto* bodyTxt = new QLabel(i18n("Support.Body"));
    bodyTxt->setWordWrap(true);
    bodyTxt->setAlignment(Qt::AlignCenter);
    bodyTxt->setStyleSheet(QString("color:%1; font-size:13px;").arg(th::kTextSecondary));
    bLay->addWidget(bodyTxt);

    auto* donateWrap = new QWidget();
    auto* dwl = new QHBoxLayout(donateWrap);
    dwl->setContentsMargins(0, 0, 0, 0);
    auto* donate = new QPushButton(i18n("Support.Donate"));
    donate->setIcon(icon(Icon::Heart, "#FFFFFF", 15));
    donate->setCursor(Qt::PointingHandCursor);
    donate->setStyleSheet(QString("QPushButton { background:%1; border:none; border-radius:8px;"
                                  " color:#FFFFFF; font-size:13px; font-weight:700; padding:11px 20px; }"
                                  "QPushButton:hover { background:%2; }")
                              .arg(th::kDanger)
                              .arg(rgba(th::kDanger, 0.85)));
    QObject::connect(donate, &QPushButton::clicked, donate,
                     []() { QDesktopServices::openUrl(QUrl(QString::fromUtf8(kUrlDonate))); });
    dwl->addStretch();
    dwl->addWidget(donate);
    dwl->addStretch();
    bLay->addWidget(donateWrap);

    auto* links = new QWidget();
    auto* lkl = new QHBoxLayout(links);
    lkl->setContentsMargins(0, 0, 0, 0);
    lkl->setSpacing(8);
    lkl->addStretch();
    lkl->addWidget(linkButton(Icon::Github, i18n("Support.Sponsors"), kUrlSponsors));
    lkl->addWidget(linkButton(Icon::Coffee, i18n("Support.Kofi"), kUrlKofi));
    lkl->addWidget(linkButton(Icon::CreditCard, i18n("Support.Paypal"), kUrlPaypal));
    lkl->addStretch();
    bLay->addWidget(links);

    rootLay->addWidget(body);

    // Centrage sur le parent.
    if (parent) {
        const QRect pg = parent->geometry();
        dlg.move(pg.center().x() - 254, pg.center().y() - 200);
    }
    dlg.exec();
}

}  // namespace sd::ui
