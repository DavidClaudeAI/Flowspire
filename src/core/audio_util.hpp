// Flowspire — utilitaires audio (purs, sans dependance OBS).
#pragma once

#include <cmath>
#include <vector>

namespace sd::core {

// Plancher d'affichage : en dessous, on considere "silence total".
inline constexpr double kDbFloor = -60.0;

// Convertit un niveau lineaire OBS (0..1) en decibels (borne [kDbFloor, 0]).
inline double mulToDb(double mul) {
    if (mul <= 0.0) {
        return kDbFloor;
    }
    const double db = 20.0 * std::log10(mul);
    if (db < kDbFloor) {
        return kDbFloor;
    }
    if (db > 0.0) {
        return 0.0;
    }
    return db;
}

// Extrait un niveau dB a partir du champ inputLevelsMul d'OBS.
// Format : liste de canaux ; chaque canal = [magnitude, peak, inputPeak]
// (valeurs lineaires). On prend le PEAK le plus fort parmi les canaux
// (plus reactif que la magnitude pour detecter une prise de parole).
inline double levelsToDb(const std::vector<std::vector<double>>& channels) {
    double best = 0.0;
    for (const auto& ch : channels) {
        if (ch.empty()) {
            continue;
        }
        const double peak = (ch.size() > 1) ? ch[1] : ch[0];
        if (peak > best) {
            best = peak;
        }
    }
    return mulToDb(best);
}

} // namespace sd::core
