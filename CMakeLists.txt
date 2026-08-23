cmake_minimum_required(VERSION 3.20)
project(bbtracker LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(WIN32)
  foreach(dep imgui kiero minhook inipp)
    if(NOT DEFINED ENV{${dep}})
      message(FATAL_ERROR "source for '${dep}' not provided as env var (set by flake.nix)")
    endif()
  endforeach()

  set(IMGUI_DIR "$ENV{imgui}")
  set(KIERO_DIR "$ENV{kiero}")
  set(MINHOOK_DIR "$ENV{minhook}")
  set(INIPP_DIR "$ENV{inipp}")

  add_link_options(-static -static-libgcc -static-libstdc++)

  file(WRITE "${CMAKE_BINARY_DIR}/shim/Windows.h" "#pragma once
#include <windows.h>
")

  add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp)
  target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

  add_library(minhook STATIC
    ${MINHOOK_DIR}/src/buffer.c
    ${MINHOOK_DIR}/src/hook.c
    ${MINHOOK_DIR}/src/trampoline.c
    ${MINHOOK_DIR}/src/hde/hde64.c)
  target_include_directories(minhook PUBLIC ${MINHOOK_DIR}/include)
  target_include_directories(minhook PRIVATE ${MINHOOK_DIR}/src ${MINHOOK_DIR}/src/hde)

  add_library(core STATIC
    src/common/log.cpp
    src/common/config.cpp
    src/common/codename/codename.cpp
    src/common/codename/rules_mgs3.cpp
    src/overlay/overlay.cpp
    ${KIERO_DIR}/kiero.cpp)
  target_include_directories(core PUBLIC src ${KIERO_DIR} ${INIPP_DIR}/inipp)
  target_include_directories(core PRIVATE ${CMAKE_BINARY_DIR}/shim)
  target_compile_definitions(core PUBLIC KIERO_INCLUDE_D3D11=1 KIERO_USE_MINHOOK=1 _M_X64=1)
  target_link_libraries(core PUBLIC imgui minhook d3d11 dxgi d3dcompiler dwmapi)

  function(bb_asi name label)
    add_library(${name} SHARED src/games/${name}/dllmain.cpp)
    set_target_properties(${name} PROPERTIES PREFIX "" OUTPUT_NAME "bbtracker_${name}" SUFFIX ".asi")
    target_link_libraries(${name} PRIVATE core)
    target_compile_definitions(${name} PRIVATE BB_GAME_LABEL="${label}")
  endfunction()

  bb_asi(mgs2 "METAL GEAR SOLID 2")
  bb_asi(mgs3 "METAL GEAR SOLID 3")

  install(TARGETS mgs2 mgs3 RUNTIME DESTINATION bin)
endif()

if(UNIX)
  add_executable(rules_tests
    src/test/test_rules.cpp
    src/common/codename/codename.cpp
    src/common/codename/rules_mgs3.cpp)
  target_include_directories(rules_tests PRIVATE src)
  enable_testing()
  add_test(NAME rules COMMAND rules_tests)
endif()
