// StreamDirector — detecteur d'activite vocale pour UNE source audio.
// Pur (aucune dependance OBS). Port de la logique validee par le POC :
// seuil de voix + hysteresis (attaque / relachement) pour eviter les faux
// declenchements (pic isole) et les coupures sur une respiration.
#pragma once

#include "core/audio_util.hpp"

namespace sd::core {

class SpeakerDetector {
public:
    SpeakerDetector() = default;
    SpeakerDetector(double thresholdDb, int attackFrames, int releaseFrames);

    // Injecte un niveau dB, met a jour l'etat, renvoie true si "parle".
    bool update(double db);

    bool speaking() const { return speaking_; }
    double lastDb() const { return lastDb_; }

    void setThresholdDb(double thresholdDb) { thresholdDb_ = thresholdDb; }
    double thresholdDb() const { return thresholdDb_; }

private:
    double thresholdDb_ = -35.0;
    int attackFrames_ = 2;  // frames consecutives au-dessus du seuil -> "parle"
    int releaseFrames_ = 8; // frames consecutives sous le seuil -> "ne parle plus"

    bool speaking_ = false;
    double lastDb_ = kDbFloor;
    int above_ = 0;
    int below_ = 0;
};

} // namespace sd::core
