/*
StreamDirector — plugin OBS (point d'entree).
Copyright (C) 2026 David

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <exception>

#include "ui/sd_dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

// Identifiant unique du dock (utilise pour l'ajout ET le retrait).
static constexpr const char *kDockId = "StreamDirectorDock";

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "StreamDirector loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_post_load(void)
{
	// obs_module_post_load a une ABI C : une exception C++ qui s'en echapperait
	// traverserait du code C d'OBS -> std::terminate (crash d'OBS au demarrage).
	// On isole donc toute la construction du dock derriere un try/catch.
	try {
		// A ce stade, l'interface Qt d'OBS est prete : on peut creer le dock
		// (lecture seule des niveaux audio + detection "qui parle").
		QWidget *dock = new sd::ui::SdDock();

		// add_dock_by_id renvoie false si l'id est deja pris (ex: rechargement
		// du plugin). Dans ce cas OBS ne prend PAS le widget en charge : on le
		// libere nous-memes. En cas de succes, OBS en devient proprietaire
		// (widget reparente dans un QDockWidget).
		if (!obs_frontend_add_dock_by_id(kDockId, "StreamDirector", dock)) {
			obs_log(LOG_WARNING, "StreamDirector dock already registered, skipping");
			delete dock;
			return;
		}

		obs_log(LOG_INFO, "StreamDirector dock registered");
	} catch (const std::exception &e) {
		obs_log(LOG_ERROR, "StreamDirector dock creation failed: %s", e.what());
	} catch (...) {
		obs_log(LOG_ERROR, "StreamDirector dock creation failed (unknown error)");
	}
}

void obs_module_unload(void)
{
	// Retrait propre du dock : sans cela, au rechargement/dechargement du
	// plugin, OBS garderait un dock pointant vers du code libere (risque de
	// crash). Sans effet si le dock n'avait pas ete ajoute.
	obs_frontend_remove_dock(kDockId);
	obs_log(LOG_INFO, "StreamDirector unloaded");
}
