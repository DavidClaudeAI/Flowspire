// Flowspire — catalogue de profils (implementation pure).
#include "core/profiles.hpp"

#include <nlohmann/json.hpp>

namespace sd::core {

using nlohmann::json;

std::string profileIndexToJson(const ProfileIndex& idx) {
    json profiles = json::array();
    for (const auto& e : idx.profiles) {
        profiles.push_back({{"id", e.id}, {"name", e.name}});
    }
    json j = {
        {"schemaVersion", 1},
        {"activeId", idx.activeId},
        {"nextId", idx.nextId},
        {"profiles", profiles},
    };
    return j.dump(2);
}

ProfileIndex profileIndexFromJson(const std::string& text) {
    const json j = json::parse(text);
    ProfileIndex idx;
    idx.activeId = j.value("activeId", 0);
    idx.nextId = j.value("nextId", 1);

    if (j.contains("profiles") && j.at("profiles").is_array()) {
        for (const auto& jp : j.at("profiles")) {
            ProfileEntry e;
            e.id = jp.value("id", 0);
            e.name = jp.value("name", std::string{});
            if (e.id > 0) { // ignore les entrees sans id valide
                idx.profiles.push_back(e);
            }
        }
    }

    // Normalisation des invariants (source unique de verite, quelle que soit
    // l'origine du JSON) :
    //   - nextId doit etre strictement au-dessus de tous les ids existants,
    //     sinon addProfile reattribuerait un id deja utilise (-> ecrasement de
    //     fichier d'un autre profil).
    //   - activeId doit pointer un profil existant ; sinon on retombe sur le
    //     premier (il y a toujours au moins un profil en pratique).
    for (const auto& e : idx.profiles) {
        if (e.id >= idx.nextId) {
            idx.nextId = e.id + 1;
        }
    }
    if (findProfile(idx, idx.activeId) == nullptr && !idx.profiles.empty()) {
        idx.activeId = idx.profiles.front().id;
    }
    return idx;
}

const ProfileEntry* findProfile(const ProfileIndex& idx, int id) {
    for (const auto& e : idx.profiles) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

bool nameExists(const ProfileIndex& idx, const std::string& name, int exceptId) {
    for (const auto& e : idx.profiles) {
        if (e.id != exceptId && e.name == name) {
            return true;
        }
    }
    return false;
}

std::string makeUniqueName(const ProfileIndex& idx, const std::string& desired, int exceptId) {
    if (!nameExists(idx, desired, exceptId)) {
        return desired;
    }
    for (int n = 2;; ++n) {
        const std::string candidate = desired + " (" + std::to_string(n) + ")";
        if (!nameExists(idx, candidate, exceptId)) {
            return candidate;
        }
    }
}

int addProfile(ProfileIndex& idx, const std::string& desiredName) {
    ProfileEntry e;
    e.id = idx.nextId++;
    e.name = makeUniqueName(idx, desiredName);
    idx.profiles.push_back(e);
    return e.id;
}

bool renameProfile(ProfileIndex& idx, int id, const std::string& desiredName) {
    for (auto& e : idx.profiles) {
        if (e.id == id) {
            e.name = makeUniqueName(idx, desiredName, id);
            return true;
        }
    }
    return false;
}

bool removeProfile(ProfileIndex& idx, int id) {
    if (id == idx.activeId) {
        return false; // on ne supprime pas le profil actif (basculer d'abord)
    }
    if (idx.profiles.size() <= 1) {
        return false; // toujours au moins un profil
    }
    for (auto it = idx.profiles.begin(); it != idx.profiles.end(); ++it) {
        if (it->id == id) {
            idx.profiles.erase(it);
            return true;
        }
    }
    return false; // id absent
}

bool setActiveProfile(ProfileIndex& idx, int id) {
    if (findProfile(idx, id) == nullptr) {
        return false;
    }
    idx.activeId = id;
    return true;
}

} // namespace sd::core
