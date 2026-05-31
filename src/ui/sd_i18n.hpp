// StreamDirector — helper d'internationalisation (couche UI).
// Centralise l'acces a l'i18n NATIVE d'OBS : obs_module_text(cle) lit la chaine
// dans le fichier de langue actif (data/locale/<langue>.ini), avec fallback sur
// la locale par defaut (en-US) declaree via OBS_MODULE_USE_DEFAULT_LOCALE.
// OBS choisit la langue selon les reglages d'OBS -> le dock suit automatiquement.
//
// Toute chaine visible par l'utilisateur DOIT passer par sd::ui::i18n() (jamais
// de texte en dur), pour rester traduisible. Les cles sont definies dans
// data/locale/en-US.ini (source) et traduites dans fr-FR.ini, etc.
//
// IMPORTANT : la fonction s'appelle i18n() et NON tr(). Dans une sous-classe de
// QObject/QWidget (notre dock), un appel non qualifie a `tr(...)` resout vers
// QObject::tr (la traduction Qt) AVANT toute fonction de namespace -> il
// renverrait la cle brute (aucun .qm Qt charge). Le nom i18n evite toute
// collision avec QObject::tr.
#pragma once

#include <obs-module.h>

#include <QString>

namespace sd::ui {

// Traduit une cle i18n -> QString UTF-8 (OBS renvoie de l'UTF-8 ; QString::fromUtf8
// est obligatoire pour les accents). Si la cle est absente, OBS renvoie la cle
// elle-meme (comportement standard, pratique pour reperer un oubli).
inline QString i18n(const char* key) {
    return QString::fromUtf8(obs_module_text(key));
}

}  // namespace sd::ui
