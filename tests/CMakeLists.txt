cmake_minimum_required(VERSION 3.20)

# Build AUTONOME du coeur (core) + tests — SANS OBS ni Qt.
# C'est la boucle de test rapide : `cmake -S tests -B build_tests -G Ninja`.
# (Le plugin complet se build depuis la racine via l'obs-plugintemplate.)
project(FlowspireTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# REGLE DURE : warnings = erreurs (sur NOTRE code ; vendor en include systeme).
if(MSVC)
  add_compile_options(/W4 /WX /permissive- /EHsc)
  add_compile_options(/external:anglebrackets /external:W0)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

set(SD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_library(
  sd_core
  STATIC
  ${SD_ROOT}/src/core/speaker_detector.cpp
  ${SD_ROOT}/src/core/config.cpp
  ${SD_ROOT}/src/core/director.cpp
  ${SD_ROOT}/src/core/profiles.cpp
  ${SD_ROOT}/src/core/version.cpp
  # OBS-free : la LOGIQUE de gestion des profils parle a un FileStore injecte
  # (cf. src/obs/file_store.hpp) -> testable hors OBS avec un FakeFileStore.
  ${SD_ROOT}/src/obs/profiles_store.cpp
)
target_include_directories(sd_core PUBLIC ${SD_ROOT}/src)
target_include_directories(sd_core SYSTEM PUBLIC ${SD_ROOT}/vendor)

enable_testing()
add_executable(sd_tests ${SD_ROOT}/tests/test_core.cpp ${SD_ROOT}/tests/test_profiles_store.cpp)
target_link_libraries(sd_tests PRIVATE sd_core)
target_include_directories(sd_tests SYSTEM PRIVATE ${SD_ROOT}/vendor)
add_test(NAME sd_tests COMMAND sd_tests)
