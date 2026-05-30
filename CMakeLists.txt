cmake_minimum_required(VERSION 3.20)

# StreamDirector — build du COEUR (core) et de ses TESTS.
# Le plugin OBS natif (couches obs/ + ui/) sera integre au scaffold
# obs-plugintemplate dans une etape ulterieure ; le code de core/ y sera
# reutilise tel quel (architecture en couches).
project(StreamDirectorCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# REGLE DURE : warnings = erreurs. On ne laisse jamais un warning derriere.
if(MSVC)
  add_compile_options(/W4 /WX /permissive- /EHsc)
  # Les en-tetes tiers (vendor) sont traites comme "externes" : pas de /WX dessus.
  add_compile_options(/external:anglebrackets /external:W0)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# --- Coeur : bibliotheque pure, aucune dependance OBS/Qt ---
add_library(sd_core STATIC
  src/core/speaker_detector.cpp
  src/core/config.cpp
  src/core/director.cpp
)
target_include_directories(sd_core PUBLIC src)
# vendor en include "systeme" -> les warnings des libs tierces sont ignores.
target_include_directories(sd_core SYSTEM PUBLIC vendor)

# --- Tests ---
enable_testing()
add_executable(sd_tests tests/test_core.cpp)
target_link_libraries(sd_tests PRIVATE sd_core)
target_include_directories(sd_tests SYSTEM PRIVATE vendor)
add_test(NAME sd_tests COMMAND sd_tests)
