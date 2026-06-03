// Flowspire — panneau "Soutenir le projet" (couche UI).
//
// Run 6 : c'etait une pop-up modale autonome. Run 7 : David a tranche "plus de
// pop-up sauf l'assistant" -> Soutenir devient un PANNEAU de la sidebar des
// parametres avances (comme Intervenants/Profils...). On expose donc juste le
// CORPS (gros coeur + texte + bouton don + liens), monte dans un layout hote
// fourni par la fenetre. Le titre "Soutenir le projet" est rendu par la fenetre
// (contentTitle), pas ici.
//
// Les URLs reelles sont des constantes dans le .cpp (a renseigner par David).
#pragma once

class QVBoxLayout;

namespace sd::ui {

// Monte le contenu "Soutenir le projet" dans `host` (le vide d'abord). Les liens
// ouvrent le navigateur via QDesktopServices.
void mountSupport(QVBoxLayout* host);

} // namespace sd::ui
