cmake_minimum_required(VERSION 3.16)
project(aztec LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_library(aztec_core
  src/core/savegame.cpp src/core/archive.cpp src/core/sprite.cpp src/core/palette.cpp src/core/map.cpp src/core/ini.cpp
  src/core/ugi.cpp src/core/script.cpp src/core/visfile.cpp src/core/userfiles.cpp src/core/audio.cpp src/core/mp.cpp src/core/sprite_alpha.cpp src/core/mapfiles.cpp src/core/text.cpp src/core/strbase.cpp)
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
  src/engine/save_import.cpp
  src/engine/assets/game_data.cpp src/engine/assets/mission.cpp src/engine/assets/anim_provider.cpp src/engine/assets/daylight.cpp
  src/engine/platform/input.cpp src/engine/platform/window.cpp
  src/engine/world/world.cpp src/engine/world/camera.cpp src/engine/world/type_info.cpp
  src/engine/sim/simulation.cpp src/engine/sim/commands.cpp src/engine/sim/savegame.cpp src/engine/sim/rules.cpp src/engine/sim/game_sim.cpp src/engine/sim/script_runner.cpp src/engine/sim/ai.cpp src/engine/ai/planner.cpp
  src/engine/render/atlas.cpp src/engine/render/sprite_store.cpp src/engine/render/font.cpp
  src/engine/render/png.cpp src/engine/render/fog.cpp src/engine/render/scene_renderer.cpp
  src/engine/ui/cursor.cpp src/engine/ui/panel.cpp src/engine/ui/names.cpp src/engine/ui/selection_ui.cpp src/engine/ui/commands_ui.cpp src/engine/ui/frontend.cpp src/engine/ui/minimap.cpp src/engine/ui/hud.cpp
  src/engine/app/options.cpp src/engine/app/headless.cpp src/engine/audio/mixer.cpp src/engine/audio/audio_system.cpp src/engine/app/playtest.cpp src/engine/app/sheet.cpp src/engine/app/engine.cpp)
target_compile_features(aztec_engine_lib PUBLIC cxx_std_20)
target_include_directories(aztec_engine_lib PUBLIC src)
target_compile_options(aztec_engine_lib PRIVATE -Wall -Wextra)
target_link_libraries(aztec_engine_lib PUBLIC aztec_core aztec_path SDL2::SDL2)
add_executable(aztec_engine src/engine/app/main.cpp)
target_link_libraries(aztec_engine aztec_engine_lib)

# Independent two-peer lockstep and plain POSIX UDP transport.
add_library(aztec_net src/engine/net/lockstep.cpp src/engine/net/udp.cpp)
target_link_libraries(aztec_net PUBLIC aztec_engine_lib)
target_compile_options(aztec_net PRIVATE -Wall -Wextra)
add_executable(test_lockstep tests/test_lockstep.cpp)
target_link_libraries(test_lockstep aztec_net)
add_test(NAME lockstep COMMAND test_lockstep)
set_tests_properties(lockstep PROPERTIES LABELS "unit;network" TIMEOUT 20)
add_executable(lockstep_peer tests/lockstep_peer.cpp)
target_link_libraries(lockstep_peer aztec_net)

add_executable(aztec_map_viewer src/tools_viewer/main.cpp)
target_link_libraries(aztec_map_viewer aztec_engine_lib)
target_compile_options(aztec_map_viewer PRIVATE -Wall -Wextra)
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

# ---- deterministic pathfinding / movement module (src/engine/sim/path): no dependencies
add_library(aztec_path src/engine/sim/path/navgrid.cpp src/engine/sim/path/flowfield.cpp src/engine/sim/path/astar.cpp src/engine/sim/path/courses.cpp
  src/engine/sim/path/mover.cpp)
target_include_directories(aztec_path PUBLIC src)
target_compile_features(aztec_path PUBLIC cxx_std_20)
target_compile_options(aztec_path PRIVATE -Wall -Wextra)
add_executable(test_path tests/test_path.cpp)
target_link_libraries(test_path aztec_path)
add_test(NAME path COMMAND test_path)
add_executable(path_bench tools/path_bench.cpp)
target_link_libraries(path_bench aztec_path)
add_test(NAME path_bench_smoke COMMAND path_bench --smoke)
set_tests_properties(path_bench_smoke PROPERTIES LABELS "unit;benchmark" TIMEOUT 20)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME lockstep_processes COMMAND ${Python3_EXECUTABLE} -I
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lockstep_processes.py $<TARGET_FILE:lockstep_peer>)
  set_tests_properties(lockstep_processes PROPERTIES LABELS "network" TIMEOUT 25 SKIP_RETURN_CODE 77)
  add_test(NAME map_viewer COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_map_viewer.py $<TARGET_FILE:aztec_map_viewer>)
  set_tests_properties(map_viewer PROPERTIES LABELS "unit" TIMEOUT 20)
  add_test(NAME zpath_tools COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_zpath_tools.py $<TARGET_FILE:path_bench>)
  set_tests_properties(zpath_tools PROPERTIES LABELS "unit" TIMEOUT 20)
  add_test(NAME playthrough COMMAND ${Python3_EXECUTABLE} -I
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_playthrough.py $<TARGET_FILE:aztec_engine>)
  set_tests_properties(playthrough PROPERTIES LABELS "unit" TIMEOUT 30)
endif()

# ---- release test configuration (the path target above is already registered)
set(GAME_DIR "$ENV{GAME_DIR}" CACHE PATH "Local game installation for data-dependent tests")
# Keep the original game/ argument as a fallback for existing local setups.
# A configured cache value takes precedence; otherwise tests read GAME_DIR at run time.
if(GAME_DIR)
  set_tests_properties(formats engine PROPERTIES ENVIRONMENT "GAME_DIR=${GAME_DIR}")
endif()
set_tests_properties(formats engine PROPERTIES SKIP_RETURN_CODE 77 LABELS "game-data")
add_test(NAME engine_unit COMMAND test_engine --pure)
set_tests_properties(engine_unit path PROPERTIES LABELS "unit")
target_compile_definitions(test_engine PRIVATE AZTEC_TEST_RULES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/docs/data")

# Synthetic malformed-input coverage, independent of user game files.
add_executable(test_fuzz tests/test_fuzz.cpp)
target_link_libraries(test_fuzz aztec_core)
add_test(NAME fuzz COMMAND test_fuzz)
set_tests_properties(fuzz PROPERTIES TIMEOUT 55)

add_executable(test_savegame tests/test_savegame.cpp)
target_link_libraries(test_savegame aztec_core)
add_test(NAME savegame COMMAND test_savegame)
set_tests_properties(savegame PROPERTIES LABELS "unit")

# Synthetic renderer integration controls, independent of installed resources.
add_executable(test_render_contracts tests/test_render_contracts.cpp)
target_link_libraries(test_render_contracts aztec_engine_lib)
target_compile_definitions(test_render_contracts PRIVATE AZTEC_TEST_RULES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/docs/data")
add_test(NAME render_contracts COMMAND test_render_contracts)
set_tests_properties(render_contracts PROPERTIES LABELS "unit")
add_test(NAME animation_headless COMMAND test_render_contracts --headless)
set_tests_properties(animation_headless PROPERTIES LABELS "unit"
  ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
if(Python3_Interpreter_FOUND)
  add_test(NAME zspr_tools COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_zspr_tools.py)
  set_tests_properties(zspr_tools PROPERTIES LABELS "unit")
endif()

# Synthetic deterministic AI checks; twenty simulated minutes without renderer or game assets.
add_executable(test_ai tests/test_ai.cpp)
target_link_libraries(test_ai aztec_engine_lib)
target_compile_definitions(test_ai PRIVATE AZTEC_TEST_RULES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/docs/data"
  AZTEC_AI_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/tests/scenarios/ai_orders.scn")
add_test(NAME ai_unit COMMAND test_ai)
add_test(NAME ai_skirmish COMMAND test_ai --smoke)
set_tests_properties(ai_unit PROPERTIES LABELS "unit")
set_tests_properties(ai_skirmish PROPERTIES LABELS "smoke" TIMEOUT 180
  ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

# Original-save adapter checks: synthetic stream plus optional private installation.
add_executable(test_save_import tests/test_save_import.cpp)
target_link_libraries(test_save_import aztec_engine_lib)
add_test(NAME save_import COMMAND test_save_import)
set_tests_properties(save_import PROPERTIES LABELS "unit")
add_test(NAME save_import_private COMMAND test_save_import --private ${CMAKE_SOURCE_DIR}/game)
set_tests_properties(save_import_private PROPERTIES SKIP_RETURN_CODE 77 LABELS "game-data")
if(GAME_DIR)
  set_tests_properties(save_import_private PROPERTIES ENVIRONMENT "GAME_DIR=${GAME_DIR}")
endif()
