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

// Run 1 — dock minimal de validation : prouve que le plugin se charge dans OBS
// et qu'il sait enregistrer un panneau Qt. Le vrai dock (vumetres + pilotage)
// viendra ensuite, en reutilisant le coeur teste (src/core).
static QWidget *sd_create_placeholder_dock()
{
	auto *widget = new QWidget();
	widget->setObjectName("StreamDirectorDock");
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
	obs_frontend_add_dock_by_id("StreamDirectorDock", "StreamDirector",
				    sd_create_placeholder_dock());
	obs_log(LOG_INFO, "StreamDirector dock registered");
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "StreamDirector unloaded");
}
