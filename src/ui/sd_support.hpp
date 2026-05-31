// StreamDirector — pop-up "Soutenir le projet" (couche UI, Run 6).
//
// Petite fenetre modale frameless (maquette Pencil `jJIQ7`) : rappel que le plugin
// est gratuit & open source + bouton "Faire un don" + liens Sponsors / Ko-fi /
// PayPal. Jamais ouverte automatiquement (regle OBS : les dons ne doivent pas etre
// "en focus") -> declenchee uniquement par l'utilisateur (entree des parametres
// avances ; au Run 7, aussi un petit coeur dans le dock).
//
// Les URLs reelles sont des constantes dans le .cpp (a renseigner par David).
#pragma once

class QWidget;

namespace sd::ui {

// Ouvre la pop-up de soutien en modale, parentee a `parent`. Bloquante.
void showSupportDialog(QWidget* parent);

}  // namespace sd::ui
