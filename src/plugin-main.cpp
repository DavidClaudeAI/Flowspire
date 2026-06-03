/*
Flowspire — plugin OBS (point d'entree).
Copyright (C) 2026 David

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <atomic>
#include <cstdio>
#include <exception>
#include <functional>
#include <utility>

#include "ui/sd_dock.hpp"

// OBS fournit Qt6Network mais PAS de backend TLS sur Windows ET macOS (cf.
// registerBundledTlsBackend) : il faut Qt + l'API systeme pour localiser notre binaire.
#if defined(_WIN32) || defined(__APPLE__)
#include <QCoreApplication>
#include <QFileInfo>
#include <QString>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef __APPLE__
#include <dlfcn.h>
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

// Identifiant unique du dock (utilise pour l'ajout ET le retrait).
static constexpr const char* kDockId = "FlowspireDock";

// Nombre de hotkeys "forcer l'intervenant N" exposees. Volontairement fixe :
// les hotkeys sont enregistrees une fois au chargement (les bindings clavier /
// Stream Deck sont memorises par OBS via le nom). Les N premiers intervenants de
// la config sont mappes sur ces hotkeys ; au-dela, on force via le dock.
static constexpr int kMaxSpeakerHotkeys = 8;

// Le dock vit cote OBS apres add_dock. On garde un pointeur pour router les
// hotkeys vers lui. atomic : une callback de hotkey peut etre invoquee depuis
// un thread != UI, alors que l'unload (thread UI) remet le pointeur a nullptr.
static std::atomic<sd::ui::SdDock*> g_dock{nullptr};

static obs_hotkey_id g_hkToggleAuto = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id g_hkForceWide = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id g_hkForceSpeaker[kMaxSpeakerHotkeys];

// Contexte passe a la callback "forcer intervenant N" : porte l'index (0-based).
struct SpeakerHotkeyCtx {
    int index;
};
static SpeakerHotkeyCtx g_speakerHkCtx[kMaxSpeakerHotkeys];

// Marshale une action vers le thread UI (celui du dock). Les callbacks de
// hotkey OBS peuvent etre invoquees depuis un thread quelconque : on delegue au
// dock qui poste l'action dans sa propre file d'evenements (postEvent, API Qt
// stable independante de la version). Qt supprime les evenements d'un objet
// detruit -> aucune execution sur un dock mort.
static void postToDock(std::function<void(sd::ui::SdDock*)> action) {
    sd::ui::SdDock* dock = g_dock.load(std::memory_order_acquire);
    if (!dock) {
        return;
    }
    dock->runOnUiThread([dock, action = std::move(action)]() { action(dock); });
}

static void hkToggleAuto(void*, obs_hotkey_id, obs_hotkey_t*, bool pressed) {
    if (!pressed) {
        return; // on agit a l'appui, pas au relachement.
    }
    postToDock([](sd::ui::SdDock* dock) { dock->toggleAuto(); });
}

static void hkForceWide(void*, obs_hotkey_id, obs_hotkey_t*, bool pressed) {
    if (!pressed) {
        return;
    }
    postToDock([](sd::ui::SdDock* dock) { dock->forceWide(); });
}

static void hkForceSpeaker(void* data, obs_hotkey_id, obs_hotkey_t*, bool pressed) {
    if (!pressed || !data) {
        return;
    }
    const int index = static_cast<SpeakerHotkeyCtx*>(data)->index;
    postToDock([index](sd::ui::SdDock* dock) { dock->forceSpeakerByIndex(index); });
}

static void registerHotkeys(void) {
    g_hkToggleAuto = obs_hotkey_register_frontend(
        "flowspire.toggle_auto", "Flowspire : activer/desactiver le pilotage auto", hkToggleAuto, nullptr);
    g_hkForceWide =
        obs_hotkey_register_frontend("flowspire.force_wide", "Flowspire : forcer le plan large", hkForceWide, nullptr);

    for (int i = 0; i < kMaxSpeakerHotkeys; ++i) {
        g_speakerHkCtx[i].index = i;
        char name[64];
        char desc[96];
        std::snprintf(name, sizeof(name), "flowspire.force_speaker_%d", i + 1);
        std::snprintf(desc, sizeof(desc), "Flowspire : forcer l'intervenant %d", i + 1);
        g_hkForceSpeaker[i] = obs_hotkey_register_frontend(name, desc, hkForceSpeaker, &g_speakerHkCtx[i]);
    }
    obs_log(LOG_INFO, "Flowspire hotkeys registered");
}

static void unregisterHotkeys(void) {
    if (g_hkToggleAuto != OBS_INVALID_HOTKEY_ID) {
        obs_hotkey_unregister(g_hkToggleAuto);
        g_hkToggleAuto = OBS_INVALID_HOTKEY_ID;
    }
    if (g_hkForceWide != OBS_INVALID_HOTKEY_ID) {
        obs_hotkey_unregister(g_hkForceWide);
        g_hkForceWide = OBS_INVALID_HOTKEY_ID;
    }
    for (int i = 0; i < kMaxSpeakerHotkeys; ++i) {
        if (g_hkForceSpeaker[i] != OBS_INVALID_HOTKEY_ID) {
            obs_hotkey_unregister(g_hkForceSpeaker[i]);
            g_hkForceSpeaker[i] = OBS_INVALID_HOTKEY_ID;
        }
    }
}

#if defined(_WIN32) || defined(__APPLE__)
// OBS fournit Qt6Network mais PAS de backend TLS sur Windows ET macOS -> les requetes
// HTTPS de Qt (notre verification de mise a jour) echouent avec "No functional TLS
// backend". On embarque le backend TLS de Qt (version alignee sur le Qt d'OBS) a cote du
// plugin et on ajoute son dossier aux chemins de plugins de Qt pour qu'il l'y trouve :
//   Windows : <plugin>/bin/64bit/tls/qschannelbackend.dll   (Schannel, natif)
//   macOS   : <bundle>.plugin/Contents/PlugIns/tls/...       (SecureTransport/OpenSSL)
// (Linux : OpenSSL systeme, rien a embarquer.)
static void registerBundledTlsBackend(void) {
    QString libDir;
#ifdef _WIN32
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&registerBundledTlsBackend), &self)) {
        obs_log(LOG_WARNING, "Flowspire: module introuvable, backend TLS Qt non enregistre");
        return;
    }
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        obs_log(LOG_WARNING, "Flowspire: chemin du module non resolu, backend TLS Qt non enregistre");
        return;
    }
    // .../bin/64bit/flowspire.dll -> on ajoute .../bin/64bit (Qt cherchera ./tls/).
    libDir = QFileInfo(QString::fromWCharArray(path, static_cast<int>(n))).absolutePath();
#elif defined(__APPLE__)
    Dl_info info;
    if (dladdr(reinterpret_cast<const void*>(&registerBundledTlsBackend), &info) == 0 || info.dli_fname == nullptr) {
        obs_log(LOG_WARNING, "Flowspire: plugin introuvable (dladdr), backend TLS Qt non enregistre");
        return;
    }
    // .../flowspire.plugin/Contents/MacOS/flowspire -> on ajoute .../Contents/PlugIns
    // (Qt cherchera ./tls/). On remonte de MacOS a Contents.
    const QString bin = QString::fromUtf8(info.dli_fname);
    libDir = QFileInfo(QFileInfo(bin).absolutePath()).absolutePath() + QStringLiteral("/PlugIns");
#endif
    QCoreApplication::addLibraryPath(libDir);
    // Trace le chemin enregistre : si le backend TLS ne se charge pas un jour, le log indique
    // immediatement si c'est ce chemin (vide/faux) ou le backend lui-meme qui est en cause.
    obs_log(LOG_INFO, "Flowspire: chemin backend TLS Qt enregistre: %s", libDir.toUtf8().constData());
}
#endif

bool obs_module_load(void) {
    for (int i = 0; i < kMaxSpeakerHotkeys; ++i) {
        g_hkForceSpeaker[i] = OBS_INVALID_HOTKEY_ID;
    }
    obs_log(LOG_INFO, "Flowspire loaded successfully (version %s)", PLUGIN_VERSION);
    return true;
}

void obs_module_post_load(void) {
    // obs_module_post_load a une ABI C : une exception C++ qui s'en echapperait
    // traverserait du code C d'OBS -> std::terminate (crash d'OBS au demarrage).
    // On isole donc toute la construction du dock derriere un try/catch.
    try {
#if defined(_WIN32) || defined(__APPLE__)
        // Doit preceder toute requete HTTPS de Qt (la verif MAJ part a la creation du
        // dock juste apres) -> on enregistre le backend TLS embarque maintenant.
        registerBundledTlsBackend();
#endif
        // A ce stade, l'interface Qt d'OBS est prete : on peut creer le dock.
        auto* dock = new sd::ui::SdDock();

        // add_dock_by_id renvoie false si l'id est deja pris (ex: rechargement
        // du plugin). Dans ce cas OBS ne prend PAS le widget en charge : on le
        // libere nous-memes. En cas de succes, OBS en devient proprietaire.
        if (!obs_frontend_add_dock_by_id(kDockId, "Flowspire", dock)) {
            obs_log(LOG_WARNING, "Flowspire dock already registered, skipping");
            delete dock;
            return;
        }

        g_dock.store(dock, std::memory_order_release);
        obs_log(LOG_INFO, "Flowspire dock registered");

        // Les hotkeys ne sont utiles qu'avec un dock vivant pour les router.
        registerHotkeys();
    } catch (const std::exception& e) {
        obs_log(LOG_ERROR, "Flowspire dock creation failed: %s", e.what());
    } catch (...) {
        obs_log(LOG_ERROR, "Flowspire dock creation failed (unknown error)");
    }
}

void obs_module_unload(void) {
    // Ordre important : on coupe d'abord les hotkeys, puis on oublie le dock,
    // puis OBS le detruit. SURETE : obs_hotkey_unregister est synchrone (il prend
    // le mutex hotkey interne d'OBS), donc apres son retour AUCUNE callback de
    // hotkey ne peut etre en vol -> g_dock ne sera plus lu, et la destruction du
    // dock par remove_dock ne peut pas courir avec un postToDock().
    unregisterHotkeys();
    g_dock.store(nullptr, std::memory_order_release);

    // Retrait propre du dock : sans cela, au rechargement/dechargement du
    // plugin, OBS garderait un dock pointant vers du code libere. Sans effet si
    // le dock n'avait pas ete ajoute.
    obs_frontend_remove_dock(kDockId);
    obs_log(LOG_INFO, "Flowspire unloaded");
}
