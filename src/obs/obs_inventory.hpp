// Flowspire — pont OBS : inventaire des ressources (scenes + sources audio).
// Couche "obs" : enumere ce qui existe dans OBS pour alimenter les listes
// deroulantes de l'assistant de configuration. A n'appeler QUE depuis le thread
// UI (l'API frontend d'OBS n'est pas concue pour un thread arbitraire).
#pragma once

#include <string>
#include <vector>

namespace sd::obsbridge {

// Noms des sources AUDIO d'OBS (memes criteres que AudioLevelMonitor :
// OBS_SOURCE_AUDIO, dedoublonnees par nom, dans l'ordre d'enumeration OBS).
// Garantit que l'assistant propose exactement les sources que le moteur sait
// ecouter.
std::vector<std::string> audioSourceNames();

// Noms des SCENES de la liste OBS (obs_frontend_get_scenes : exactement les
// scenes utilisateur, ni groupes ni scenes privees). Memes scenes que celles
// que SceneSwitcher sait activer.
std::vector<std::string> sceneNames();

} // namespace sd::obsbridge
