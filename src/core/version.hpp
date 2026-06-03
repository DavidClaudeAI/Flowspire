// Flowspire — comparaison de versions semantiques (PUR : aucune dependance
// OBS/Qt/reseau, donc testable hors OBS comme le reste de src/core).
//
// Sert au systeme de mise a jour : comparer la version locale (PLUGIN_VERSION) a la
// derniere version publiee sur GitHub. On ne raisonne QUE sur des versions stables
// "X.Y.Z" ; toute pre-release ("1.2.3-rc1") ou format inattendu ne parse pas -> on
// ne notifie alors pas (pas de fausse alerte).
#pragma once

#include <optional>
#include <string>

namespace sd::core {

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;

    bool operator==(const SemVer& o) const { return major == o.major && minor == o.minor && patch == o.patch; }
    bool operator<(const SemVer& o) const {
        if (major != o.major)
            return major < o.major;
        if (minor != o.minor)
            return minor < o.minor;
        return patch < o.patch;
    }
};

// Parse "X.Y.Z" (tolere un prefixe 'v'/'V' et des espaces autour). Renvoie nullopt si
// ce n'est pas EXACTEMENT trois entiers separes par des points (un suffixe de
// pre-release "-rc1" rend donc le parse nul : voulu, cf. en-tete).
std::optional<SemVer> parseSemVer(const std::string& text);

// Vrai si `latest` est une version stable STRICTEMENT superieure a `current`.
// Tolerant : si l'un des deux ne parse pas, renvoie false (jamais de fausse notif).
bool isNewerVersion(const std::string& latest, const std::string& current);

} // namespace sd::core
