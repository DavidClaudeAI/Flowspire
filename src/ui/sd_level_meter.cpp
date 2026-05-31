// StreamDirector — vumetre custom avec marqueur de seuil (implementation).
#include "ui/sd_level_meter.hpp"

#include <QColor>
#include <QPainter>

#include <algorithm>

#include "core/audio_util.hpp"  // kDbFloor : bas de l'echelle (source de verite)
#include "ui/sd_theme.hpp"

namespace sd::ui {

namespace th = sd::ui::theme;

namespace {
constexpr double kFloor = sd::core::kDbFloor;  // ex -60 dB

// dB -> fraction [0,1] sur l'echelle [kFloor, 0].
double dbToFrac(double db) {
    if (db < kFloor) {
        db = kFloor;
    } else if (db > 0.0) {
        db = 0.0;
    }
    return (db - kFloor) / (0.0 - kFloor);
}
}  // namespace

LevelMeter::LevelMeter(QWidget* parent)
    : QWidget(parent), levelDb_(kFloor), thresholdDb_(-35.0) {
    setFixedHeight(th::kMeterHeight);
    setMinimumWidth(40);
}

void LevelMeter::setLevelDb(double db) {
    if (db < kFloor) {
        db = kFloor;
    } else if (db > 0.0) {
        db = 0.0;
    }
    if (db != levelDb_) {
        levelDb_ = db;
        update();
    }
}

void LevelMeter::setThresholdDb(double db) {
    if (db != thresholdDb_) {
        thresholdDb_ = db;
        update();
    }
}

void LevelMeter::setSpeaking(bool speaking) {
    if (speaking != speaking_) {
        speaking_ = speaking;
        update();
    }
}

void LevelMeter::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const double w = width();
    const double h = height();
    const double radius = h / 2.0;

    // 1) Piste de fond (surface3).
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(th::kSurface3));
    p.drawRoundedRect(QRectF(0, 0, w, h), radius, radius);

    // 2) Remplissage = niveau courant. Vert vif si "parle", vert attenue sinon.
    const double lvl = dbToFrac(levelDb_) * w;
    if (lvl > 0.5) {
        QColor fill(speaking_ ? th::kSuccess : th::kTextTertiary);
        p.setBrush(fill);
        p.drawRoundedRect(QRectF(0, 0, lvl, h), radius, radius);
    }

    // 3) Marqueur de seuil : trait vertical clair a la position du seuil. C'est
    //    le repere qui se deplace quand on bouge le slider -> on voit le point de
    //    declenchement directement sur le vumetre.
    const double tx = dbToFrac(thresholdDb_) * w;
    QColor marker(th::kTextPrimary);
    p.setPen(QPen(marker, 2.0));
    p.drawLine(QPointF(tx, -1.0), QPointF(tx, h + 1.0));
}

}  // namespace sd::ui
