// Flowspire — calibration automatique du seuil de voix d'UNE personne (pur, sans OBS).
//
// Principe (inspire de Mumble "rapport signal/bruit" + Discord "sensibilite auto",
// mais on FIGE apres au lieu de deriver en continu) : on observe les niveaux dB de la
// personne pendant qu'elle parle naturellement, on estime son PLANCHER (silence / bruit
// de fond = percentile bas) et son niveau de VOIX (percentile haut), puis on pose le
// seuil PROPORTIONNELLEMENT dans l'ecart entre les deux. Le placement en % (et non
// "plancher + marge fixe") le rend robuste aux deux extremes :
//   - micro GATE : ecart enorme (ex. -60 -> -15) -> seuil bas mais PAS colle au fond ;
//   - micro OUVERT bruyant : petit ecart -> seuil juste au-dessus du bruit.
// On ne "cale" que si l'ecart voix-plancher est franc (sinon : pas assez parle, ou pas
// de silence capte -> on ne touche a rien, le seuil de la personne reste inchange).
//
// Header-only (logique pure, comme weighted_pick.hpp / audio_util.hpp) -> aucune entree
// CMake : il suffit de l'#include cote tests et cote dock.
#pragma once

#include "core/audio_util.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace sd::core {

// Parametres de calibration. Defauts a SENTIR EN REEL. Convention de frame du projet :
// 1 frame = 1 tick (~50 ms, cf. SpeakerDetector / kTickMs), donc 20 frames ~= 1 s.
struct CalibrationParams {
    double floorPercentile = 0.15; // estimation du plancher (silence / bruit de fond)
    double voicePercentile = 0.90; // estimation du niveau de voix (robuste si parole rare)
    double fraction = 0.30;        // placement du seuil dans l'ecart [plancher, voix]
    double minGapDb = 12.0;        // ecart voix-plancher minimum pour conclure "cale"
    double safetyDb = 3.0;         // le seuil reste au moins safetyDb SOUS la voix
    int minFrames = 100;           // ~5 s d'observation avant de pouvoir conclure
    int maxFrames = 600;           // fenetre glissante ~30 s (borne memoire)
};

// Percentile (p dans [0,1]) d'un echantillon. Prend une COPIE qu'on reordonne
// partiellement (nth_element). Precondition : v non vide.
inline double percentileDb(std::vector<double> v, double p) {
    const long n = static_cast<long>(v.size());
    long idx = std::lround(p * static_cast<double>(n - 1));
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= n) {
        idx = n - 1;
    }
    auto nth = v.begin() + idx;
    std::nth_element(v.begin(), nth, v.end());
    return *nth;
}

// Calibre le seuil d'UNE personne a partir de ses niveaux dB (parole naturelle).
// Passif : on injecte un niveau par tick ; des que l'estimation est fiable -> done()
// avec un seuil FIGE (plus aucune evolution). Aucune dependance OBS.
class ThresholdCalibrator {
public:
    ThresholdCalibrator() = default;
    explicit ThresholdCalibrator(const CalibrationParams& params) : params_(params) {}

    // Injecte un niveau dB (un par tick). No-op une fois fige (done()).
    void addSample(double db) {
        if (done_) {
            return;
        }
        samples_.push_back(db);
        if (static_cast<int>(samples_.size()) > params_.maxFrames) {
            samples_.erase(samples_.begin()); // fenetre glissante : on jette le plus ancien.
        }
        if (static_cast<int>(samples_.size()) >= params_.minFrames) {
            double t = 0.0;
            if (computeThreshold(t)) {
                thresholdDb_ = t;
                done_ = true;
            }
        }
    }

    // Force la conclusion (bouton "Terminer") : fige si on a assez de donnees ET un ecart
    // franc. Renvoie true si on a pu caler ; false sinon -> l'appelant laisse le seuil tel quel.
    bool finalizeNow() {
        if (done_) {
            return true;
        }
        double t = 0.0;
        if (samples_.size() >= 2 && computeThreshold(t)) {
            thresholdDb_ = t;
            done_ = true;
            return true;
        }
        return false;
    }

    bool done() const { return done_; }
    double thresholdDb() const { return thresholdDb_; } // valable seulement si done()
    int sampleCount() const { return static_cast<int>(samples_.size()); }

private:
    // Calcule le seuil si l'ecart voix-plancher est suffisant. Precondition : samples_ non vide.
    bool computeThreshold(double& out) const {
        const double floor = percentileDb(samples_, params_.floorPercentile);
        const double voice = percentileDb(samples_, params_.voicePercentile);
        const double gap = voice - floor;
        if (gap < params_.minGapDb) {
            return false; // pas assez d'ecart : pas assez parle, ou aucun silence capte.
        }
        double t = floor + params_.fraction * gap;
        const double maxT = voice - params_.safetyDb; // jamais au-dessus de la voix
        if (t > maxT) {
            t = maxT;
        }
        if (t < kDbFloor + 1.0) { // jamais au plancher absolu (cf. borne du slider)
            t = kDbFloor + 1.0;
        }
        out = t;
        return true;
    }

    CalibrationParams params_{};
    std::vector<double> samples_{};
    bool done_ = false;
    double thresholdDb_ = 0.0;
};

} // namespace sd::core
