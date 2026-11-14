cmake_minimum_required(VERSION 3.16)
project(Strategizer2D VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(STRATEGIZER_BUILD_TESTS "Build the headless logic tests" ON)

# --- SFML 2.x: use the system package if present, otherwise download it -------
find_package(SFML 2.5 COMPONENTS graphics window system QUIET)
if(NOT SFML_FOUND)
  message(STATUS "SFML not found on the system - fetching SFML 2.6.1")
  include(FetchContent)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(sfml
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 2.6.1)
  FetchContent_MakeAvailable(sfml)
endif()

# --- Game logic (no graphics dependency, so it can be tested headless) --------
add_library(strategizer_core STATIC
  src/Csv.cpp
  src/Data.cpp
  src/Battle.cpp)
target_include_directories(strategizer_core PUBLIC src)
target_compile_definitions(strategizer_core PUBLIC
  STRATEGIZER_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(NOT MSVC)
  target_compile_options(strategizer_core PRIVATE -Wall -Wextra)
endif()

# --- The game -----------------------------------------------------------------
add_executable(Strategizer2D
  src/main.cpp
  src/App.cpp
  src/Screens.cpp
  src/BattleScreen.cpp)
target_compile_definitions(Strategizer2D PRIVATE STRATEGIZER_VERSION="${PROJECT_VERSION}")
target_link_libraries(Strategizer2D PRIVATE strategizer_core
  sfml-graphics sfml-window sfml-system)
if(NOT MSVC)
  target_compile_options(Strategizer2D PRIVATE -Wall -Wextra)
endif()

# --- Tests --------------------------------------------------------------------
if(STRATEGIZER_BUILD_TESTS)
  enable_testing()
  add_executable(core_tests tests/test_core.cpp)
  target_link_libraries(core_tests PRIVATE strategizer_core)
  add_test(NAME core_tests COMMAND core_tests ${CMAKE_SOURCE_DIR})
endif()
