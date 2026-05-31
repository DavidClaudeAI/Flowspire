// StreamDirector — icones (implementation). SVG lucide teints via QSvgRenderer.
#include "ui/sd_icons.hpp"

#include <QByteArray>
#include <QGuiApplication>
#include <QPainter>
#include <QSvgRenderer>

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
    }
    return "";
}
}  // namespace

QPixmap icon(Icon which, const QString& colorHex, int sizePx) {
    const QString svg =
        QStringLiteral(
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
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

}  // namespace sd::ui
