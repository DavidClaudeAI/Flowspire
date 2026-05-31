// StreamDirector — helpers de style QSS partages (dock + assistant).
//
// rgba() : Qt interprete un hex a 8 chiffres dans une feuille QSS comme
// #AARRGGBB (alpha en PREMIER), alors que nos jetons (issus de la maquette
// Pencil) sont en #RRGGBBAA. Une couleur translucide serait donc completement
// faussee (vert -> rouge, etc.). On passe TOUJOURS par rgba(r,g,b,a), sans
// ambiguite. Source unique : utilise par sd_dock.cpp ET sd_assistant.cpp.
#pragma once

#include <QColor>
#include <QString>

namespace sd::ui {

inline QString rgba(const char* hex6, double alpha) {
    const QColor c(QString::fromUtf8(hex6));
    return QString("rgba(%1,%2,%3,%4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(alpha, 0, 'f', 3);
}

}  // namespace sd::ui
