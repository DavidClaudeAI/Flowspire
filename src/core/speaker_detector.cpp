#include "core/speaker_detector.hpp"

namespace sd::core {

SpeakerDetector::SpeakerDetector(double thresholdDb, int attackFrames, int releaseFrames)
    : thresholdDb_(thresholdDb),
      attackFrames_(attackFrames < 1 ? 1 : attackFrames),
      releaseFrames_(releaseFrames < 1 ? 1 : releaseFrames) {}

bool SpeakerDetector::update(double db) {
    lastDb_ = db;
    if (db >= thresholdDb_) {
        ++above_;
        below_ = 0;
        if (!speaking_ && above_ >= attackFrames_) {
            speaking_ = true;
        }
    } else {
        ++below_;
        above_ = 0;
        if (speaking_ && below_ >= releaseFrames_) {
            speaking_ = false;
        }
    }
    return speaking_;
}

}  // namespace sd::core
