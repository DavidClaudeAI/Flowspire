// StreamDirector — vumetre custom (couche UI) avec MARQUEUR DE SEUIL.
// Remplace QProgressBar : dessine la piste, le remplissage (niveau audio en
// direct) ET un trait vertical a la position du seuil de voix. Quand on bouge le
// slider de seuil, le marqueur se deplace sur le vumetre en temps reel -> on voit
// exactement au-dela de quel niveau l'intervenant est considere comme "parle"
// (demande UX de David). Echelle commune au slider : dB dans [kDbFloor, 0].
#pragma once

#include <QWidget>

namespace sd::ui {

class LevelMeter : public QWidget {
public:
    explicit LevelMeter(QWidget* parent = nullptr);

    // Niveau audio courant (dB, borne a l'echelle). Repeint seulement si change.
    void setLevelDb(double db);
    // Seuil de voix (dB) -> position du marqueur. Repeint seulement si change.
    void setThresholdDb(double db);
    // Etat parle : change la couleur du remplissage (vert vif vs attenue).
    void setSpeaking(bool speaking);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double levelDb_;
    double thresholdDb_;
    bool speaking_ = false;
};

} // namespace sd::ui
