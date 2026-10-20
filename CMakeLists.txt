cmake_minimum_required(VERSION 3.16)
project(aztec LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_library(aztec_core
  src/core/archive.cpp src/core/sprite.cpp src/core/palette.cpp src/core/map.cpp src/core/ini.cpp
  src/core/ugi.cpp src/core/script.cpp src/core/visfile.cpp src/core/userfiles.cpp src/core/audio.cpp src/core/mp.cpp src/core/sprite_alpha.cpp src/core/mapfiles.cpp src/core/text.cpp)
target_include_directories(aztec_core PUBLIC src)
target_compile_options(aztec_core PRIVATE -Wall -Wextra)

find_package(SDL2 REQUIRED)
add_executable(aztec_view src/app/view.cpp)
target_link_libraries(aztec_view aztec_core SDL2::SDL2)

enable_testing()
add_executable(test_formats tests/test_formats.cpp)
target_link_libraries(test_formats aztec_core)
add_test(NAME formats COMMAND test_formats ${CMAKE_SOURCE_DIR}/game)

# ---- clean C++20 engine (src/engine): loads the original data files, no recompiled code
add_library(aztec_engine_lib
  src/engine/assets/game_data.cpp src/engine/assets/mission.cpp src/engine/assets/anim_provider.cpp
  src/engine/platform/input.cpp src/engine/platform/window.cpp
  src/engine/world/world.cpp src/engine/world/camera.cpp src/engine/world/type_info.cpp
  src/engine/sim/simulation.cpp
  src/engine/render/atlas.cpp src/engine/render/sprite_store.cpp src/engine/render/font.cpp
  src/engine/render/png.cpp src/engine/render/scene_renderer.cpp
  src/engine/ui/cursor.cpp src/engine/ui/minimap.cpp src/engine/ui/hud.cpp
  src/engine/app/options.cpp src/engine/app/engine.cpp)
target_compile_features(aztec_engine_lib PUBLIC cxx_std_20)
target_include_directories(aztec_engine_lib PUBLIC src)
target_compile_options(aztec_engine_lib PRIVATE -Wall -Wextra)
target_link_libraries(aztec_engine_lib PUBLIC aztec_core SDL2::SDL2)
add_executable(aztec_engine src/engine/app/main.cpp)
target_link_libraries(aztec_engine aztec_engine_lib)
add_executable(test_engine tests/test_engine.cpp)
target_link_libraries(test_engine aztec_engine_lib)
add_test(NAME engine COMMAND test_engine ${CMAKE_SOURCE_DIR}/game)

# ---- static recompilation runtime: needs generated C from tools/ghidra_scripts/LiftAll.java
# cmake -DAZTEC_GEN_DIR=work/gen ..   (generated code is derived from the user's own Aztec.exe; never commit it)
set(AZTEC_GEN_DIR "" CACHE PATH "Directory with gen_*.c produced by LiftAll.java")
if(AZTEC_GEN_DIR)
  file(GLOB AZTEC_GEN_SRC ${AZTEC_GEN_DIR}/gen_*.c)
  add_executable(aztec_recomp runtime/main.c runtime/core.c runtime/win32.c runtime/backend.c
                 runtime/ddraw.c runtime/dsound.c runtime/dinput.c runtime/directx.c runtime/video.c runtime/settings.c runtime/overlay.c runtime/dbg.c runtime/winsock.c runtime/kernels.c runtime/oracle.c runtime/pathlog.c ${AZTEC_GEN_SRC})
  set_source_files_properties(${AZTEC_GEN_DIR}/gen_069.c PROPERTIES COMPILE_DEFINITIONS "f_0043d120=f_0043d120_orig")
  set_source_files_properties(${AZTEC_GEN_DIR}/gen_117.c PROPERTIES COMPILE_DEFINITIONS "f_0046c470=f_0046c470_orig")
  if(NOT AZTEC_NO_NATIVE_KERNELS)
    set_source_files_properties(${AZTEC_GEN_DIR}/gen_135.c ${AZTEC_GEN_DIR}/gen_136.c PROPERTIES COMPILE_DEFINITIONS "f_004805e2=f_004805e2_orig;f_00480a36=f_00480a36_orig")
  endif()
  target_include_directories(aztec_recomp PRIVATE runtime ${AZTEC_GEN_DIR})
  target_compile_options(aztec_recomp PRIVATE -O2 -fno-strict-aliasing -w)
  target_link_libraries(aztec_recomp SDL2::SDL2 m pthread)
endif()
option(AZTEC_HEAPDEBUG "Check guest heap for overlapping/invalid blocks" OFF)
if(AZTEC_GEN_DIR AND AZTEC_HEAPDEBUG)
  target_compile_definitions(aztec_recomp PRIVATE RT_HEAPDEBUG)
endif()
option(AZTEC_TRACE_CALLS "Count guest function entries (RT_CALLS_FILE, default calls.txt)" OFF)
if(AZTEC_GEN_DIR AND AZTEC_TRACE_CALLS)
  target_compile_definitions(aztec_recomp PRIVATE RT_TRACE_CALLS)
endif()
