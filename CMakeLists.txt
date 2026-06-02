cmake_minimum_required(VERSION 3.28...3.30)

# StreamDirector — plugin OBS natif.
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
    src/ui/sd_support.cpp
    src/ui/sd_update_check.cpp
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)
# Fichiers d'en-tete seuls (pas de .cpp) — inclus par les .cpp ci-dessus :
# sd_i18n.hpp, sd_theme.hpp, sd_style.hpp, sd_runtime.hpp, sd_widgets.hpp.
target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PRIVATE vendor)
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
