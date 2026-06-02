#include "core/version.hpp"

#include <cstddef>

namespace sd::core {

std::optional<SemVer> parseSemVer(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::nullopt;
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    std::string s = text.substr(begin, end - begin + 1);

    if (!s.empty() && (s.front() == 'v' || s.front() == 'V'))
        s.erase(0, 1);

    int parts[3] = {0, 0, 0};
    int index = 0; // segment courant (0..2)
    bool digitsInPart = false;

    for (const char c : s) {
        if (c == '.') {
            if (!digitsInPart)
                return std::nullopt; // segment vide (".", "1..2")
            if (index == 2)
                return std::nullopt; // plus de 3 segments
            ++index;
            digitsInPart = false;
        } else if (c >= '0' && c <= '9') {
            parts[index] = parts[index] * 10 + (c - '0');
            if (parts[index] > 100000000)
                return std::nullopt; // garde-fou overflow
            digitsInPart = true;
        } else {
            return std::nullopt; // tout autre caractere (dont suffixe "-rc1") -> invalide
        }
    }

    if (index != 2 || !digitsInPart)
        return std::nullopt; // pas exactement 3 segments pleins
    return SemVer{parts[0], parts[1], parts[2]};
}

bool isNewerVersion(const std::string& latest, const std::string& current) {
    const auto l = parseSemVer(latest);
    const auto c = parseSemVer(current);
    if (!l || !c)
        return false;
    return *c < *l;
}

} // namespace sd::core
