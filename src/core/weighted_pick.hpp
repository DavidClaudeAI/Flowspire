// StreamDirector — tirage au sort pondere (cle du "feel" organique).
// Pur, deterministe pour un meme r : permet des tests reproductibles.
#pragma once

#include <utility>
#include <vector>

namespace sd::core {

// Choisit un element parmi `options` (paires {valeur, poids}) selon les poids.
// `r` doit etre dans [0, 1) (typiquement issu d'un RNG injectable).
// Les poids <= 0 sont ignores. Renvoie nullptr si aucune option eligible.
template <typename T>
const T* weightedPick(const std::vector<std::pair<T, int>>& options, double r) {
    // long long (et pas long) : long fait 32 bits sur Windows (LLP64) et 64 sur
    // Linux/macOS (LP64) — on fixe 64 bits partout pour un comportement identique
    // multi-OS et eviter tout depassement sur de gros cumuls de poids.
    long long total = 0;
    for (const auto& opt : options) {
        if (opt.second > 0) {
            total += opt.second;
        }
    }
    if (total <= 0) {
        return nullptr;
    }
    const double x = r * static_cast<double>(total);
    double acc = 0.0;
    for (const auto& opt : options) {
        if (opt.second <= 0) {
            continue;
        }
        acc += static_cast<double>(opt.second);
        if (x < acc) {
            return &opt.first;
        }
    }
    // Garde-fou numerique (r ~ 1.0) : renvoyer la derniere option eligible.
    for (auto it = options.rbegin(); it != options.rend(); ++it) {
        if (it->second > 0) {
            return &it->first;
        }
    }
    return nullptr;
}

}  // namespace sd::core
