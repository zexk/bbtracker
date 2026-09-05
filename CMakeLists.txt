cmake_minimum_required(VERSION 3.20)
project(bbtracker LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
    src/common/codename/codename.cpp
    src/common/codename/rules_mgs4.cpp
    src/common/codename/rules_mgspw.cpp
    src/common/codename/rules_mgs3.cpp
    src/common/codename/rules_mgs2.cpp
    src/common/codename/rules_mgs1.cpp
    src/common/codename/rules_mgs1_integral.cpp
    src/overlay/overlay.cpp)
  target_include_directories(core PUBLIC src)
  target_link_libraries(core PUBLIC imgui minhook d3d11 d3d12 dxgi d3dcompiler dwmapi)

  function(bb_asi name label sources)
    add_library(${name} SHARED ${sources})
    set_target_properties(${name} PROPERTIES PREFIX "" OUTPUT_NAME "bbtracker_${name}" SUFFIX ".asi")
    target_link_libraries(${name} PRIVATE core)
    target_compile_definitions(${name} PRIVATE BB_GAME_LABEL="${label}")
  endfunction()

  file(GLOB MGS1_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/games/mgs1/*.cpp)
  file(GLOB MG12_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/games/mg12/*.cpp)
  file(GLOB MGS2_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/games/mgs2/*.cpp)
  file(GLOB MGS3_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/games/mgs3/*.cpp)
  file(GLOB MGS4_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/games/mgs4/*.cpp)
  file(GLOB MGSPW_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/games/mgspw/*.cpp)
  bb_asi(mg12 "METAL GEAR / METAL GEAR 2" "${MG12_SOURCES}")
  bb_asi(mgs1 "METAL GEAR SOLID" "${MGS1_SOURCES}")
  bb_asi(mgs2 "METAL GEAR SOLID 2" "${MGS2_SOURCES}")
  bb_asi(mgs3 "METAL GEAR SOLID 3" "${MGS3_SOURCES}")
  bb_asi(mgs4 "METAL GEAR SOLID 4" "${MGS4_SOURCES}")
  bb_asi(mgspw "METAL GEAR SOLID: PEACE WALKER" "${MGSPW_SOURCES}")

  install(TARGETS mg12 mgs1 mgs2 mgs3 mgs4 mgspw RUNTIME DESTINATION bin)
endif()

if(UNIX)
  # Every suite pulls in the whole rule set: the evaluators share the stat
  # accessors in codename.cpp, so trimming the list per game buys nothing.
  set(RULES_SOURCES
    src/common/codename/codename.cpp
    src/common/codename/rules_mgs1.cpp
    src/common/codename/rules_mgs1_integral.cpp
    src/common/codename/rules_mgs2.cpp
    src/common/codename/rules_mgs3.cpp
    src/common/codename/rules_mgs4.cpp
    src/common/codename/rules_mgspw.cpp)

  function(bb_rules_test game)
    add_executable(rules_${game}_tests src/test/test_rules_${game}.cpp ${RULES_SOURCES})
    target_include_directories(rules_${game}_tests PRIVATE src)
    add_test(NAME ${game}_rules COMMAND rules_${game}_tests)
  endfunction()

  enable_testing()
  foreach(game mg12 mgs1 mgs2 mgs3 mgs4 mgspw)
    bb_rules_test(${game})
  endforeach()
endif()
