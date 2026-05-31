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
// Lien de don REEL de David (PayPal). Un seul canal (decision David, polish Run 7) :
// le gros bouton rouge ci-dessous y mene. Les anciens liens Sponsors/Ko-fi ont ete
// retires.
constexpr const char* kUrlDonate = "https://paypal.me/DavidZouari";
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

    // Unique bouton : "Faire un don via PayPal" -> lien PayPal de David.
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
}

}  // namespace sd::ui
