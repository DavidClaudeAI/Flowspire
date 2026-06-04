cmake_minimum_required(VERSION 3.28...3.30)

# Flowspire — plugin OBS natif.
# Build base sur l'obs-plugintemplate officiel (telecharge libobs + Qt6 via
# buildspec.json, multi-OS). Le COEUR pur (src/core) est teste a part, sans OBS,
# via tests/CMakeLists.txt (boucle de test rapide). Voir specs/build-plan.

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

# Active des le depart : on veut le dock (Qt) et le controle des scenes (frontend-api).
option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  # Svg : pour rendre les icones lucide (SVG teintes) du dock. La DLL Qt6Svg est
  # fournie avec OBS (runtime) et dans les deps (compile) -> dependance sure.
  find_package(Qt6 REQUIRED COMPONENTS Widgets Core Svg Network)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Svg Qt6::Network)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

# --- Sources du plugin ---
#   core/ : cerveau pur (aucune dependance OBS/Qt), reutilise tel quel.
#   obs/  : pont vers l'API OBS (lecture des niveaux audio, inventaire, config).
#   ui/   : dock + fenetres Qt (assistant, parametres avances, panneaux partages).
target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/core/speaker_detector.cpp
    src/core/config.cpp
    src/core/director.cpp
    src/core/profiles.cpp
    src/core/version.cpp
    src/obs/audio_level_monitor.cpp
    src/obs/fs_atomic.cpp
    src/obs/obs_file_store.cpp
    src/obs/obs_inventory.cpp
    src/obs/profiles_store.cpp
    src/obs/profiles_store_obs.cpp
    src/obs/scene_switcher.cpp
    src/ui/sd_assistant.cpp
    src/ui/sd_config_panels.cpp
    src/ui/sd_dock.cpp
    src/ui/sd_icons.cpp
    src/ui/sd_level_meter.cpp
    src/ui/sd_prefs.cpp
    src/ui/sd_profiles_panel.cpp
    src/ui/sd_settings.cpp
    src/ui/sd_status_push.cpp
    src/ui/sd_support.cpp
    src/ui/sd_update_check.cpp
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)
# Fichiers d'en-tete seuls (pas de .cpp) — inclus par les .cpp ci-dessus :
# sd_i18n.hpp, sd_theme.hpp, sd_style.hpp, sd_runtime.hpp, sd_widgets.hpp,
# sd_logo_data.hpp (blob SVG du logo, genere depuis src/ui/sd_logo_flowspire.svg).
target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PRIVATE vendor)
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# OBS embarque Qt6Network mais PAS de backend TLS (Windows ET macOS) -> notre verification
# de mise a jour en HTTPS echouerait ("No functional TLS backend"). On embarque le backend
# TLS de Qt (version alignee sur le Qt d'OBS) a cote du plugin ; plugin-main ajoute son
# dossier aux chemins de plugins Qt au chargement. (Linux : OpenSSL systeme.)
# Windows : backend Schannel (natif) sous bin/64bit/tls.
if(WIN32 AND ENABLE_QT AND TARGET Qt6::QSchannelBackendPlugin)
  install(
    FILES "$<TARGET_FILE:Qt6::QSchannelBackendPlugin>"
    DESTINATION "${CMAKE_PROJECT_NAME}/bin/64bit/tls"
    COMPONENT Runtime
  )
endif()

# macOS : OBS ne fournit pas de backend TLS Qt -> on embarque celui de Qt (SecureTransport,
# natif macOS) DANS le bundle a Contents/PlugIns/tls. Subtilite : le backend livre par les
# obs-deps est NON signe, et Xcode refuse de sceller un bundle dont un composant interne
# (dossier PlugIns = emplacement de code) n'est pas signe. On le signe donc nous-memes en
# ad-hoc (ordre "inside-out" recommande par Apple) sur une copie locale, AVANT que
# MACOSX_PACKAGE_LOCATION ne le place dans le bundle et qu'Xcode scelle l'ensemble ->
# signature du bundle valide ET backend present dans le .pkg. (XCODE_EMBED_PLUGINS n'est pas
# utilisable : il exige une cible construite, pas un dylib importe de Qt.) plugin-main ajoute
# Contents/PlugIns aux chemins de plugins Qt au chargement.
if(APPLE AND ENABLE_QT)
  set(_tls_bundled FALSE)
  foreach(_backend QSecureTransportBackendPlugin QOpenSSLBackendPlugin)
    if(TARGET Qt6::${_backend})
      set(_bk_loc "")
      foreach(_cfg RELEASE RELWITHDEBINFO MINSIZEREL DEBUG)
        get_target_property(_loc Qt6::${_backend} IMPORTED_LOCATION_${_cfg})
        if(_loc)
          set(_bk_loc "${_loc}")
          break()
        endif()
      endforeach()
      if(NOT _bk_loc)
        get_target_property(_bk_loc Qt6::${_backend} IMPORTED_LOCATION)
      endif()
      if(_bk_loc AND EXISTS "${_bk_loc}")
        get_filename_component(_bk_name "${_bk_loc}" NAME)
        file(COPY "${_bk_loc}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/tls_backends")
        set(_bk_signed "${CMAKE_CURRENT_BINARY_DIR}/tls_backends/${_bk_name}")
        # Meme identite que le bundle (CODESIGN_IDENTITY, "-" ad-hoc par defaut) pour eviter
        # toute incoherence de signature (fork avec un vrai certificat Apple).
        # NOTE notarisation : cette signature n'ajoute ni timestamp securise ni hardened
        # runtime. Pour une distribution Apple NOTARISEE (vrai Developer ID), il faudrait
        # ajouter ici "--timestamp --options runtime". Non requis pour le flux actuel
        # (ad-hoc, non notarise) ; valide end-to-end sur macOS 26.4.
        set(_bk_sign_id "${CODESIGN_IDENTITY}")
        if(NOT _bk_sign_id)
          set(_bk_sign_id "-")
        endif()
        execute_process(
          COMMAND codesign --force --sign "${_bk_sign_id}" "${_bk_signed}"
          RESULT_VARIABLE _bk_sign_result
        )
        # Echec bloquant : un dylib non signe dans PlugIns ferait de toute facon echouer le
        # scellement du bundle par Xcode -> on arrete tot et clair plutot que tard et opaque.
        if(NOT _bk_sign_result EQUAL 0)
          message(FATAL_ERROR "Flowspire : echec signature backend TLS ${_bk_name} (codesign --sign ${_bk_sign_id})")
        endif()
        target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${_bk_signed}")
        set_source_files_properties("${_bk_signed}" PROPERTIES MACOSX_PACKAGE_LOCATION "PlugIns/tls")
        set(_tls_bundled TRUE)
      endif()
    endif()
  endforeach()
  # Garde-fou anti-regression silencieuse : si un futur changement d'obs-deps/Qt renomme ou
  # retire la cible du backend, on le SIGNALE au lieu de livrer un .pkg sans TLS (= le bug
  # d'origine) avec une CI verte.
  if(NOT _tls_bundled)
    message(
      WARNING
      "Flowspire : AUCUN backend TLS Qt embarque dans le bundle macOS -> la verification de "
      "MAJ (HTTPS) sera inoperante. Qt6::QSecureTransportBackendPlugin introuvable ou dylib "
      "absent (changement d'obs-deps / Qt ?)."
    )
  endif()
endif()
