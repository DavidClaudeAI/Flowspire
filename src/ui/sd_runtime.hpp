// StreamDirector — constantes runtime partagees (couche UI).
// Source UNIQUE de verite pour la cadence du moteur : le dock appelle
// Director::update() une fois par tick de timer, et le detecteur compte ses
// frames attack/release en nombre d'appels a update(). Donc 1 frame = 1 tick.
// L'assistant convertit "delai de silence" (secondes, maquette) <-> releaseFrames
// avec cette meme valeur (ex : 8 frames x 50 ms = 0,4 s). Centralise ici pour
// qu'un changement de cadence ne desynchronise jamais le dock et l'assistant.
#pragma once

namespace sd::ui {

inline constexpr int kTickMs = 50;                        // periode du timer du dock (~20 Hz)
inline constexpr double kFrameSeconds = kTickMs / 1000.0; // duree d'un frame du detecteur (s)

} // namespace sd::ui
