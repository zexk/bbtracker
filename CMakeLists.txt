cmake_minimum_required(VERSION 3.20)
project(bbtracker LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(WIN32)
  foreach(dep imgui minhook)
    if(NOT DEFINED ENV{${dep}})
      message(FATAL_ERROR "source for '${dep}' not provided as env var (set by flake.nix)")
    endif()
  endforeach()

  set(IMGUI_DIR "$ENV{imgui}")
  set(MINHOOK_DIR "$ENV{minhook}")

  add_link_options(-static -static-libgcc -static-libstdc++)

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
    src/common/codename/codename.cpp
    src/common/codename/rules_mgs3.cpp
    src/common/codename/rules_mgs2.cpp
    src/common/codename/rules_mgs1.cpp
    src/common/codename/rules_mgs4.cpp
    src/overlay/overlay.cpp)
  target_include_directories(core PUBLIC src)
  target_link_libraries(core PUBLIC imgui minhook d3d11 dxgi d3dcompiler dwmapi)

  function(bb_asi name label sources)
    add_library(${name} SHARED ${sources})
    set_target_properties(${name} PROPERTIES PREFIX "" OUTPUT_NAME "bbtracker_${name}" SUFFIX ".asi")
    target_link_libraries(${name} PRIVATE core)
    target_compile_definitions(${name} PRIVATE BB_GAME_LABEL="${label}")
  endfunction()

  file(GLOB MGS1_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/games/mgs1/*.cpp)
  file(GLOB MGS2_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/games/mgs2/*.cpp)
  file(GLOB MGS3_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/games/mgs3/*.cpp)
  file(GLOB MGS4_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/games/mgs4/*.cpp)
  bb_asi(mgs1 "METAL GEAR SOLID" "${MGS1_SOURCES}")
  bb_asi(mgs2 "METAL GEAR SOLID 2" "${MGS2_SOURCES}")
  bb_asi(mgs3 "METAL GEAR SOLID 3" "${MGS3_SOURCES}")
  bb_asi(mgs4 "METAL GEAR SOLID 4" "${MGS4_SOURCES}")

  install(TARGETS mgs1 mgs2 mgs3 mgs4 RUNTIME DESTINATION bin)
endif()

if(UNIX)
  add_executable(rules_mgs3_tests
    src/test/test_rules.cpp
    src/common/codename/codename.cpp
    src/common/codename/rules_mgs3.cpp
    src/common/codename/rules_mgs2.cpp
    src/common/codename/rules_mgs1.cpp)
  target_include_directories(rules_mgs3_tests PRIVATE src)

  add_executable(rules_mgs2_tests
    src/test/test_rules_mgs2.cpp
    src/common/codename/codename.cpp
    src/common/codename/rules_mgs2.cpp
    src/common/codename/rules_mgs1.cpp
    src/common/codename/rules_mgs3.cpp)
  target_include_directories(rules_mgs2_tests PRIVATE src)

  add_executable(rules_mgs1_tests
    src/test/test_rules_mgs1.cpp
    src/common/codename/codename.cpp
    src/common/codename/rules_mgs1.cpp
    src/common/codename/rules_mgs2.cpp
    src/common/codename/rules_mgs3.cpp)
  target_include_directories(rules_mgs1_tests PRIVATE src)

  enable_testing()
  add_test(NAME mgs3_rules COMMAND rules_mgs3_tests)
  add_test(NAME mgs2_rules COMMAND rules_mgs2_tests)
  add_test(NAME mgs1_rules COMMAND rules_mgs1_tests)
endif()
