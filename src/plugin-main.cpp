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

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

// Identifiant unique du dock (utilise pour l'ajout ET le retrait).
static constexpr const char *kDockId = "StreamDirectorDock";

// Run 1 — dock minimal de validation : prouve que le plugin se charge dans OBS
// et qu'il sait enregistrer un panneau Qt. Le vrai dock (vumetres + pilotage)
// viendra ensuite, en reutilisant le coeur teste (src/core).
static QWidget *sd_create_placeholder_dock()
{
	auto *widget = new QWidget();
	widget->setObjectName(kDockId);
	auto *layout = new QVBoxLayout(widget);
	auto *title = new QLabel(QStringLiteral("StreamDirector"));
	auto *subtitle =
		new QLabel(QStringLiteral("Run 1 — le plugin est charge. Dock complet a venir."));
	subtitle->setWordWrap(true);
	layout->addWidget(title);
	layout->addWidget(subtitle);
	layout->addStretch();
	return widget;
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "StreamDirector loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_post_load(void)
{
	// A ce stade, l'interface Qt d'OBS est prete : on peut enregistrer le dock.
	QWidget *dock = sd_create_placeholder_dock();

	// add_dock_by_id renvoie false si l'id est deja pris (ex: rechargement du
	// plugin). Dans ce cas OBS ne prend PAS le widget en charge : on le libere
	// nous-memes pour eviter une fuite. En cas de succes, OBS en devient
	// proprietaire (widget reparente dans un QDockWidget).
	if (!obs_frontend_add_dock_by_id(kDockId, "StreamDirector", dock)) {
		obs_log(LOG_WARNING, "StreamDirector dock already registered, skipping");
		delete dock;
		return;
	}

	obs_log(LOG_INFO, "StreamDirector dock registered");
}

void obs_module_unload(void)
{
	// Retrait propre du dock : sans cela, au rechargement/dechargement du
	// plugin, OBS garderait un dock pointant vers du code libere (risque de
	// crash). Sans effet si le dock n'avait pas ete ajoute.
	obs_frontend_remove_dock(kDockId);
	obs_log(LOG_INFO, "StreamDirector unloaded");
}
