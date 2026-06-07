// Flowspire — icones (implementation). SVG lucide teints via QSvgRenderer.
#include "ui/sd_icons.hpp"

#include "ui/sd_logo_data.hpp"

#include <QByteArray>
#include <QGuiApplication>
#include <QPainter>
#include <QSvgRenderer>

#include <cmath> // std::lround (icon() + logoFlowspire) — pas d'include transitif implicite

namespace sd::ui {

namespace {
// Corps des icones lucide (viewBox 24x24, style stroke). Source: lucide.dev (ISC).
// `%1` recevra la couleur de trait. fill:none + stroke-width 2 + bouts arrondis
// = rendu fidele a lucide.
const char* iconBody(Icon which) {
    switch (which) {
    case Icon::Clapperboard:
        return "<path d='M20.2 6 3 11l-.9-2.4c-.3-1.1.3-2.2 1.3-2.5l13.5-4c1.1-.3 2.2.3 2.5 1.3Z'/>"
               "<path d='m6.2 5.3 3.1 3.9'/>"
               "<path d='m12.4 3.4 3.1 4'/>"
               "<path d='M3 11h18v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2Z'/>";
    case Icon::User:
        return "<path d='M19 21v-2a4 4 0 0 0-4-4H9a4 4 0 0 0-4 4v2'/>"
               "<circle cx='12' cy='7' r='4'/>";
    case Icon::Settings:
        return "<path d='M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0"
               "l-.15-.08a2 2 0 0 0-2.73.73l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51"
               "a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0"
               "l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25"
               "a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.39a2 2 0 0 0-.73-2.73l-.15-.08a2 2 0 0 1-1-1.74"
               "v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08"
               "a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2Z'/>"
               "<circle cx='12' cy='12' r='3'/>";
    case Icon::Sparkles:
        return "<path d='M9.937 15.5A2 2 0 0 0 8.5 14.063l-6.135-1.582a.5.5 0 0 1 0-.962L8.5 9.936"
               "A2 2 0 0 0 9.937 8.5l1.582-6.135a.5.5 0 0 1 .963 0L14.063 8.5A2 2 0 0 0 15.5 9.937"
               "l6.135 1.581a.5.5 0 0 1 0 .964L15.5 14.063a2 2 0 0 0-1.437 1.437l-1.582 6.135"
               "a.5.5 0 0 1-.963 0Z'/>"
               "<path d='M20 3v4'/><path d='M22 5h-4'/><path d='M4 17v2'/><path d='M5 18H3'/>";
    case Icon::LayoutGrid:
        return "<rect width='7' height='7' x='3' y='3' rx='1'/>"
               "<rect width='7' height='7' x='14' y='3' rx='1'/>"
               "<rect width='7' height='7' x='14' y='14' rx='1'/>"
               "<rect width='7' height='7' x='3' y='14' rx='1'/>";
    case Icon::X:
        return "<path d='M18 6 6 18'/><path d='m6 6 12 12'/>";
    case Icon::Trash2:
        return "<path d='M3 6h18'/>"
               "<path d='M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2'/>"
               "<path d='M10 11v6'/><path d='M14 11v6'/>";
    case Icon::Plus:
        return "<path d='M5 12h14'/><path d='M12 5v14'/>";
    case Icon::ArrowRight:
        return "<path d='M5 12h14'/><path d='m12 5 7 7-7 7'/>";
    case Icon::ChevronDown:
        return "<path d='m6 9 6 6 6-6'/>";
    case Icon::ChevronRight:
        return "<path d='m9 18 6-6-6-6'/>";
    case Icon::Play:
        return "<polygon points='6 3 20 12 6 21 6 3'/>";
    case Icon::Pencil:
        return "<path d='M21.174 6.812a1 1 0 0 0-3.986-3.987L3.842 16.174a2 2 0 0 0-.5.83l-1.321 4.352"
               "a.5.5 0 0 0 .623.622l4.353-1.32a2 2 0 0 0 .83-.497z'/>"
               "<path d='m15 5 4 4'/>";
    case Icon::Users:
        return "<path d='M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2'/>"
               "<circle cx='9' cy='7' r='4'/>"
               "<path d='M22 21v-2a4 4 0 0 0-3-3.87'/>"
               "<path d='M16 3.13a4 4 0 0 1 0 7.75'/>";
    case Icon::Video:
        return "<path d='m22 8-6 4 6 4V8Z'/>"
               "<rect width='14' height='12' x='2' y='6' rx='2' ry='2'/>";
    case Icon::Clock:
        return "<circle cx='12' cy='12' r='10'/><polyline points='12 6 12 12 16 14'/>";
    case Icon::Mic:
        return "<path d='M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z'/>"
               "<path d='M19 10v2a7 7 0 0 1-14 0v-2'/>"
               "<line x1='12' x2='12' y1='19' y2='22'/>";
    case Icon::Info:
        return "<circle cx='12' cy='12' r='10'/><path d='M12 16v-4'/><path d='M12 8h.01'/>";
    case Icon::Bookmark:
        return "<path d='m19 21-7-4-7 4V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z'/>";
    case Icon::Heart:
        return "<path d='M19 14c1.49-1.46 3-3.21 3-5.5A5.5 5.5 0 0 0 16.5 3c-1.76 0-3 .5-4.5 2"
               "-1.5-1.5-2.74-2-4.5-2A5.5 5.5 0 0 0 2 8.5c0 2.29 1.51 4.04 3 5.5l7 7Z'/>";
    case Icon::EllipsisV:
        return "<circle cx='12' cy='12' r='1'/><circle cx='12' cy='5' r='1'/>"
               "<circle cx='12' cy='19' r='1'/>";
    case Icon::Github:
        return "<path d='M15 22v-4a4.8 4.8 0 0 0-1-3.5c3 0 6-2 6-5.5.08-1.25-.27-2.48-1-3.5"
               ".28-1.15.28-2.35 0-3.5 0 0-1 0-3 1.5-2.64-.5-5.36-.5-8 0C6 2 5 2 5 2c-.3 1.15-.3 2.35 0 3.5"
               "A5.4 5.4 0 0 0 4 9c0 3.5 3 5.5 6 5.5-.39.49-.68 1.05-.85 1.65-.17.6-.22 1.23-.15 1.85v4'/>"
               "<path d='M9 18c-4.51 2-5-2-7-2'/>";
    case Icon::Coffee:
        return "<path d='M10 2v2'/><path d='M14 2v2'/>"
               "<path d='M16 8a1 1 0 0 1 1 1v8a4 4 0 0 1-4 4H7a4 4 0 0 1-4-4V9a1 1 0 0 1 1-1h14a4 4 0 1 1 0 8h-1'/>"
               "<path d='M6 2v2'/>";
    case Icon::CreditCard:
        return "<rect width='20' height='14' x='2' y='5' rx='2'/><line x1='2' x2='22' y1='10' y2='10'/>";
    case Icon::SlidersHorizontal:
        return "<line x1='21' x2='14' y1='4' y2='4'/><line x1='10' x2='3' y1='4' y2='4'/>"
               "<line x1='21' x2='12' y1='12' y2='12'/><line x1='8' x2='3' y1='12' y2='12'/>"
               "<line x1='21' x2='16' y1='20' y2='20'/><line x1='12' x2='3' y1='20' y2='20'/>"
               "<line x1='14' x2='14' y1='2' y2='6'/><line x1='8' x2='8' y1='10' y2='14'/>"
               "<line x1='16' x2='16' y1='18' y2='22'/>";
    case Icon::Wand:
        // lucide "wand-sparkles" : baguette magique (metaphore "reglage automatique").
        return "<path d='m21.64 3.64-1.28-1.28a1.21 1.21 0 0 0-1.72 0L2.36 18.64a1.21 1.21 0 0 0 0 1.72"
               "l1.28 1.28a1.2 1.2 0 0 0 1.72 0L21.64 5.36a1.2 1.2 0 0 0 0-1.72'/>"
               "<path d='m14 7 3 3'/><path d='M5 6v4'/><path d='M19 14v4'/><path d='M10 2v2'/>"
               "<path d='M7 8H3'/><path d='M21 16h-4'/><path d='M11 3H9'/>";
    case Icon::Check:
        // lucide "check" : etat "cale".
        return "<path d='M20 6 9 17l-5-5'/>";
    }
    return "";
}
} // namespace

QPixmap icon(Icon which, const QString& colorHex, int sizePx) {
    const QString svg =
        QStringLiteral("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
                       "stroke='%1' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'>%2</svg>")
            .arg(colorHex, QString::fromUtf8(iconBody(which)));

    // Rendu a la resolution physique de l'ecran (net sur HiDPI).
    qreal dpr = 1.0;
    if (auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        dpr = app->devicePixelRatio();
    }
    if (dpr < 1.0) {
        dpr = 1.0;
    }

    // lround (pas troncature) pour un pixel-size net sur scale fractionnaire (150%).
    const int px = static_cast<int>(std::lround(sizePx * dpr));
    QSvgRenderer renderer(QByteArray(svg.toUtf8()));
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    renderer.render(&painter);
    painter.end();
    pm.setDevicePixelRatio(dpr);
    return pm;
}

QPixmap logoFlowspire(int heightPx) {
    // Meme principe que icon() : rendu a la resolution physique (net HiDPI). On
    // deduit la largeur du ratio d'origine (646x169) pour ne pas deformer le mot.
    qreal dpr = 1.0;
    if (auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        dpr = app->devicePixelRatio();
    }
    if (dpr < 1.0) {
        dpr = 1.0;
    }

    constexpr double kAspect = 646.0 / 169.0; // largeur / hauteur du wordmark
    const int h = static_cast<int>(std::lround(heightPx * dpr));
    const int w = static_cast<int>(std::lround(heightPx * kAspect * dpr));
    const QByteArray svg(kFlowspireLogoSvg); // variable nommee -> evite le "most vexing parse"
    QSvgRenderer renderer(svg);
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    renderer.render(&painter);
    painter.end();
    pm.setDevicePixelRatio(dpr);
    return pm;
}

} // namespace sd::ui
