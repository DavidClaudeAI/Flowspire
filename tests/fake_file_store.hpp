// StreamDirector — FileStore en memoire pour les tests (OBS-free).
// Mime la semantique d'os_safe_replace : a chaque reecriture, l'ancien contenu
// part en "<chemin>.bak". Permet de simuler une COUPURE (failPathContains) et
// d'inspecter/poser l'etat disque (setRaw/getRaw) pour rejouer corruptions et pannes.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "obs/file_store.hpp"

namespace sdtest {

class FakeFileStore final : public sd::obsbridge::FileStore {
public:
    bool read(const std::string& relPath, std::string& out) const override {
        const auto it = files_.find(relPath);
        if (it == files_.end()) {
            return false;
        }
        out = it->second;
        return true;
    }

    bool exists(const std::string& relPath) const override {
        return files_.find(relPath) != files_.end();
    }

    sd::obsbridge::FsResult write(const std::string& relPath, const std::string& text) override {
        ++writeCalls;
        // Coupure simulee : toute ecriture dont le chemin contient `failPathContains`
        // echoue SANS modifier l'etat (comme un kill avant la bascule atomique).
        if (!failPathContains.empty() && relPath.find(failPathContains) != std::string::npos) {
            return {false, "coupure simulee"};
        }
        // os_safe_replace : l'ancien contenu (s'il existait) part en .bak (meme
        // convention que la prod, via backupPath -> pas de divergence test/reel).
        const auto it = files_.find(relPath);
        if (it != files_.end()) {
            files_[sd::obsbridge::backupPath(relPath)] = it->second;
        }
        files_[relPath] = text;
        return {true, ""};
    }

    bool remove(const std::string& relPath) override {
        files_.erase(relPath);
        return files_.find(relPath) == files_.end();
    }

    std::vector<std::string> list(const std::string& relDir,
                                  const std::string& ext) const override {
        std::vector<std::string> names;
        const std::string prefix = relDir + "/";
        for (const auto& kv : files_) {
            const std::string& p = kv.first;
            if (p.size() <= prefix.size() || p.compare(0, prefix.size(), prefix) != 0) {
                continue;
            }
            const std::string name = p.substr(prefix.size());
            if (name.find('/') != std::string::npos) {
                continue;  // pas de descente dans les sous-dossiers
            }
            if (name.size() >= ext.size() &&
                name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
                names.push_back(name);
            }
        }
        return names;
    }

    // --- API de test : pose / inspection directe de l'etat "disque" ---
    void setRaw(const std::string& path, const std::string& content) { files_[path] = content; }
    bool hasRaw(const std::string& path) const { return files_.find(path) != files_.end(); }
    std::string getRaw(const std::string& path) const {
        const auto it = files_.find(path);
        return it == files_.end() ? std::string{} : it->second;
    }

    std::string failPathContains;  // ecritures dont le chemin contient ceci -> echec
    int writeCalls = 0;

private:
    std::map<std::string, std::string> files_;
};

}  // namespace sdtest
