cmake_minimum_required(VERSION 3.20)
project(bbtracker LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(GAMES mg12 mgs1 mgs2 mgs3 mgs4 mgspw babel)
set(RANKED_GAMES mg12 mgs1 mgs2 mgs3 mgs4 mgspw)
set(RULE_SOURCES
  src/common/codename/codename.cpp
  src/common/codename/rules_mgs1.cpp
  src/common/codename/rules_mgs1_integral.cpp
  src/common/codename/rules_mgs2.cpp
  src/common/codename/rules_mgs3.cpp
  src/common/codename/rules_mgs4.cpp
  src/common/codename/rules_mgspw.cpp)

if(WIN32)
  include(FetchContent)

  set(IMGUI_DIR "$ENV{imgui}" CACHE PATH "Path to ImGui sources")
  if(NOT IMGUI_DIR)
    FetchContent_Declare(imgui_source
      GIT_REPOSITORY https://github.com/ocornut/imgui.git
      GIT_TAG v1.92.9b
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(imgui_source)
    set(IMGUI_DIR "${imgui_source_SOURCE_DIR}")
  endif()

  set(MINHOOK_DIR "$ENV{minhook}" CACHE PATH "Path to MinHook sources")
  if(NOT MINHOOK_DIR)
    FetchContent_Declare(minhook_source
      GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
      GIT_TAG v1.3.4
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(minhook_source)
    set(MINHOOK_DIR "${minhook_source_SOURCE_DIR}")
  endif()

  if(MINGW)
    add_link_options(-static -static-libgcc -static-libstdc++)
  endif()

  add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp
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
    ${RULE_SOURCES}
    src/overlay/overlay.cpp)
  target_include_directories(core PUBLIC src)
  target_link_libraries(core PUBLIC imgui minhook d3d11 d3d12 dxgi d3dcompiler dwmapi)

  function(bb_asi name label)
    file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/games/${name}/*.cpp)
    add_library(${name} SHARED ${sources} src/games/asi_main.cpp)
    set_target_properties(${name} PROPERTIES PREFIX "" OUTPUT_NAME "bbtracker_${name}" SUFFIX ".asi")
    target_link_libraries(${name} PRIVATE core)
    target_compile_definitions(${name} PRIVATE BB_GAME_LABEL="${label}")
  endfunction()

  bb_asi(mg12 "METAL GEAR / METAL GEAR 2")
  bb_asi(mgs1 "METAL GEAR SOLID")
  bb_asi(mgs2 "METAL GEAR SOLID 2")
  bb_asi(mgs3 "METAL GEAR SOLID 3")
  bb_asi(mgs4 "METAL GEAR SOLID 4")
  bb_asi(mgspw "METAL GEAR SOLID: PEACE WALKER")
  bb_asi(babel "METAL GEAR: GHOST BABEL")

  install(TARGETS ${GAMES} RUNTIME DESTINATION bin)
endif()

if(UNIX)
  function(bb_rules_test game)
    add_executable(rules_${game}_tests src/test/test_rules_${game}.cpp ${RULE_SOURCES})
    target_include_directories(rules_${game}_tests PRIVATE src)
    add_test(NAME ${game}_rules COMMAND rules_${game}_tests)
  endfunction()

  enable_testing()
  foreach(game IN LISTS RANKED_GAMES)
    bb_rules_test(${game})
  endforeach()

  add_executable(pw_portable_tests src/test/test_pw_portable.cpp)
  target_include_directories(pw_portable_tests PRIVATE src)
  add_test(NAME pw_portable COMMAND pw_portable_tests)
endif()
