// Flowspire — catalogue de profils (modele PUR + (de)serialisation JSON).
// Un "profil" = une configuration nommee. Le CONTENU d'un profil est une
// sd::core::Config classique (meme format que config.json) ; ce fichier ne gere
// que le CATALOGUE : la liste ordonnee {id, nom}, le profil actif, et l'allocation
// d'identifiants. Toute l'IO (lecture/ecriture des fichiers) vit dans la couche
// obs (obs/profiles_store) ; ici, rien ne touche au disque -> testable hors OBS.
#pragma once

#include <string>
#include <vector>

namespace sd::core {

// Une entree du catalogue : un identifiant stable + un nom affiche.
struct ProfileEntry {
    int id = 0;
    std::string name;
};

// Le catalogue complet : profils dans l'ordre d'affichage, profil actif, et le
// prochain id a attribuer (compteur monotone -> jamais de reutilisation d'id
// apres suppression, donc pas de collision de fichier).
struct ProfileIndex {
    int activeId = 0;
    int nextId = 1;
    std::vector<ProfileEntry> profiles; // ordre d'affichage
};

// Serialise le catalogue (texte indente, pas de BOM).
std::string profileIndexToJson(const ProfileIndex& idx);

// Parse un catalogue depuis du JSON. Tolerant (cles absentes -> defaut) et
// NORMALISANT : nextId est porte au-dela de tous les ids presents, et activeId
// est ramene sur un profil existant si invalide. Leve std::exception si le JSON
// lui-meme est invalide.
ProfileIndex profileIndexFromJson(const std::string& text);

// Renvoie l'entree d'id `id`, ou nullptr si absente.
const ProfileEntry* findProfile(const ProfileIndex& idx, int id);

// Un nom est-il deja pris (comparaison exacte) ? `exceptId` permet d'ignorer une
// entree (utile au renommage : un profil peut garder son propre nom).
bool nameExists(const ProfileIndex& idx, const std::string& name, int exceptId = 0);

// Rend `desired` unique dans le catalogue en suffixant " (2)", " (3)"... au besoin.
// `exceptId` exclut une entree de la verification (renommage sur place).
std::string makeUniqueName(const ProfileIndex& idx, const std::string& desired, int exceptId = 0);

// Ajoute un profil (nom rendu unique), renvoie son id. Ne change pas l'actif.
int addProfile(ProfileIndex& idx, const std::string& desiredName);

// Renomme un profil (nom rendu unique). Renvoie false si l'id est absent.
bool renameProfile(ProfileIndex& idx, int id, const std::string& desiredName);

// Supprime un profil. Refuse si l'id est absent, s'il est ACTIF, ou s'il ne
// resterait aucun profil (on garde toujours au moins un profil). Renvoie false
// dans ces cas, true si la suppression a eu lieu.
bool removeProfile(ProfileIndex& idx, int id);

// Marque `id` comme actif. Renvoie false si l'id est absent.
bool setActiveProfile(ProfileIndex& idx, int id);

} // namespace sd::core
