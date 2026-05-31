// StreamDirector — panneau "Soutenir le projet" (implementation).
// Voir sd_support.hpp. Monte le corps dans un layout hote (plus de pop-up : Run 7).
#include "ui/sd_support.hpp"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/sd_i18n.hpp"
#include "ui/sd_icons.hpp"
#include "ui/sd_style.hpp"
#include "ui/sd_theme.hpp"
#include "ui/sd_widgets.hpp"

namespace sd::ui {

namespace th = sd::ui::theme;
using namespace sd::ui::widgets;

namespace {
// URLs de soutien. ⚠️ A REMPLACER par les vrais liens de David quand ils existent.
// Par defaut : la page du projet (lien reel, non trompeur). cf. todo (dette).
constexpr const char* kUrlDonate = "https://github.com/DavidClaudeAI/StreamDirector";
constexpr const char* kUrlSponsors = "https://github.com/sponsors/DavidClaudeAI";
constexpr const char* kUrlKofi = "https://ko-fi.com/";
constexpr const char* kUrlPaypal = "https://www.paypal.com/";

// Petit lien : icone + libelle, fond surface2, bordure border (hover rouge).
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

void mountSupport(QVBoxLayout* host) {
    clearLayout(host);

    // Gros coeur centre.
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
    host->addWidget(bigWrap);

    auto* headline = new QLabel(i18n("Support.Headline"));
    headline->setWordWrap(true);
    headline->setAlignment(Qt::AlignCenter);
    headline->setStyleSheet(
        QString("color:%1; font-size:17px; font-weight:700;").arg(th::kTextPrimary));
    host->addWidget(headline);

    auto* bodyTxt = new QLabel(i18n("Support.Body"));
    bodyTxt->setWordWrap(true);
    bodyTxt->setAlignment(Qt::AlignCenter);
    bodyTxt->setStyleSheet(QString("color:%1; font-size:13px;").arg(th::kTextSecondary));
    host->addWidget(bodyTxt);

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
    host->addWidget(donateWrap);

    auto* links = new QWidget();
    auto* lkl = new QHBoxLayout(links);
    lkl->setContentsMargins(0, 0, 0, 0);
    lkl->setSpacing(8);
    lkl->addStretch();
    lkl->addWidget(linkButton(Icon::Github, i18n("Support.Sponsors"), kUrlSponsors));
    lkl->addWidget(linkButton(Icon::Coffee, i18n("Support.Kofi"), kUrlKofi));
    lkl->addWidget(linkButton(Icon::CreditCard, i18n("Support.Paypal"), kUrlPaypal));
    lkl->addStretch();
    host->addWidget(links);
}

}  // namespace sd::ui
