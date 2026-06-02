// StreamDirector — preferences GLOBALES de l'application (couche UI).
//
// Reglages d'APP, distincts de la config de realisation (profils). GLOBAUX (pas par
// profil), persistes dans prefs.json (dossier de config du plugin) via ObsFileStore,
// et appliques IMMEDIATEMENT (pas de modele "Enregistrer"). Lus par le dock (verif
// MAJ au demarrage ; comportement d'une scene hors regie) et edites dans l'onglet
// "Parametres generaux" des parametres avances.
//
// Header OBS-free (aucun entete OBS/Qt) -> inclus librement par le dock et les
// parametres. L'implementation, elle, parle a ObsFileStore.
#pragma once

namespace sd::ui {

// Que faire quand l'utilisateur passe MANUELLEMENT (dock natif d'OBS) sur une scene
// QUI N'EST PAS dans la regie (ni le pool d'un intervenant, ni le plan large) ?
enum class OffSceneBehavior {
    ForceAsShot,  // la compter comme un plan : forcage TEMPORAIRE, l'auto reprend
    PauseAuto,    // mettre la regie EN PAUSE : on reste dessus jusqu'a reactivation
};

struct GlobalPrefs {
    // Verifier l'existence d'une mise a jour au demarrage (opt-out : appel reseau).
    bool checkUpdatesOnStartup = true;
    // Defaut = pause (le plus sur : on ne "vole" jamais une scene hors regie).
    OffSceneBehavior offSceneBehavior = OffSceneBehavior::PauseAuto;
};

// Lit prefs.json. Toute absence / illisibilite -> valeurs par defaut (jamais d'echec,
// jamais d'exception : un fichier corrompu retombe simplement sur les defauts).
GlobalPrefs loadPrefs();

// Ecrit prefs.json (atomique + .bak via ObsFileStore). Best-effort : un echec disque
// est trace (obs_log) sans bloquer l'UI ni le pilotage en cours.
void savePrefs(const GlobalPrefs& prefs);

}  // namespace sd::ui
