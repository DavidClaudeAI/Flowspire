// StreamDirector — dock du plugin (couche UI).
// Run 2 (lecture seule) : affiche un vumetre par source audio, l'etat
// parle/silence, et "a l'antenne (calcule)" = le locuteur que le realisateur
// suivrait. NE PILOTE PAS ENCORE les scenes OBS (read-only). Reutilise le coeur
// teste (sd::core::Director) alimente par les niveaux audio internes d'OBS.
#pragma once

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

#include "core/director.hpp"
#include "obs/audio_level_monitor.hpp"

class QLabel;
class QProgressBar;
class QTimer;
class QVBoxLayout;

namespace sd::ui {

class SdDock : public QWidget {
public:
    explicit SdDock(QWidget* parent = nullptr);
    ~SdDock() override;

private:
    struct Row {
        std::string id;
        QWidget* line = nullptr;  // conteneur de la ligne (detruit ses enfants)
        QProgressBar* bar = nullptr;
        QLabel* dbLabel = nullptr;
        QLabel* stateLabel = nullptr;
    };

    void rebuildSources();  // (re)scanne les sources audio et reconstruit l'UI
    void tick();            // appele par le timer : lit l'audio, nourrit le coeur, rafraichit
    static double nowSeconds();

    sd::obsbridge::AudioLevelMonitor monitor_;
    std::unique_ptr<sd::core::Director> director_;

    QVBoxLayout* rowsLayout_ = nullptr;
    std::vector<Row> rows_;
    QLabel* onAirLabel_ = nullptr;
    QLabel* emptyLabel_ = nullptr;
    QTimer* timer_ = nullptr;
};

}  // namespace sd::ui
