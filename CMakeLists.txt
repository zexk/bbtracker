cmake_minimum_required(VERSION 3.20)
project(bbtracker VERSION 0.4.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Non-ASCII UI strings (accents, ✓/✗) must survive MSVC's codepage default.
if(MSVC)
  add_compile_options(/utf-8)
endif()

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

  # The flake passes nixpkgs' inter.src (the upstream release archive, whose
  # extras/ttf holds the static cuts). Other builds download the same pinned
  # release.
  set(INTER_DIR "$ENV{interSrc}" CACHE PATH "Path to the Inter font release")
  if(NOT INTER_DIR)
    FetchContent_Declare(inter_source
      URL https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip
      URL_HASH SHA256=9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e)
    FetchContent_MakeAvailable(inter_source)
    set(INTER_DIR "${inter_source_SOURCE_DIR}")
  endif()

  set(BB_FONT_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/inter_fonts.cpp")
  add_custom_command(
    OUTPUT "${BB_FONT_CPP}"
    COMMAND "${CMAKE_COMMAND}"
      "-DFONTS=inter_regular=${INTER_DIR}/extras/ttf/Inter-Regular.ttf|inter_semibold=${INTER_DIR}/extras/ttf/Inter-SemiBold.ttf|inter_bold=${INTER_DIR}/extras/ttf/Inter-Bold.ttf"
      "-DOUTPUT=${BB_FONT_CPP}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_fonts.cmake"
    DEPENDS
      "${INTER_DIR}/extras/ttf/Inter-Regular.ttf"
      "${INTER_DIR}/extras/ttf/Inter-SemiBold.ttf"
      "${INTER_DIR}/extras/ttf/Inter-Bold.ttf"
    COMMENT "Embedding Inter font data"
    VERBATIM)

  if(MINGW)
    # --exclude-all-symbols: PE auto-export of every global bloats .edata and
    # pins dead code; DllMain is the entry point, nothing needs exporting.
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options(-static -static-libgcc -static-libstdc++
                     -Wl,--exclude-all-symbols -Wl,--gc-sections)
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

  # FreeType rasterizer with the auto-hinter, for crisp glyphs at UI sizes.
  # The flake builds a trimmed static copy (see cmake/freetype_modules.h);
  # other builds fetch and patch the same release.
  set(FREETYPE_DIR "$ENV{freetype}" CACHE PATH "Path to the FreeType install")
  if(FREETYPE_DIR)
    add_library(bb_freetype STATIC IMPORTED)
    set_target_properties(bb_freetype PROPERTIES
      IMPORTED_LOCATION "${FREETYPE_DIR}/lib/libfreetype.a"
      INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_DIR}/include/freetype2")
  else()
    set(FT_DISABLE_ZLIB TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)
    FetchContent_Declare(freetype_source
      GIT_REPOSITORY https://github.com/freetype/freetype.git
      GIT_TAG VER-2-14-3
      GIT_SHALLOW TRUE
      PATCH_COMMAND "${CMAKE_COMMAND}" -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/freetype_modules.h"
        include/freetype/config/ftmodule.h)
    FetchContent_MakeAvailable(freetype_source)
    add_library(bb_freetype ALIAS freetype)
  endif()
  target_sources(imgui PRIVATE ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp)
  target_link_libraries(imgui PUBLIC bb_freetype)
  target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_FREETYPE)

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
    ${BB_FONT_CPP}
    src/overlay/overlay.cpp)
  target_include_directories(core PUBLIC src)
  target_link_libraries(core PUBLIC imgui minhook d3d11 d3d12 dxgi d3dcompiler dwmapi)
  target_compile_definitions(core PUBLIC BB_VERSION="${PROJECT_VERSION}")

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
  install(FILES "${INTER_DIR}/LICENSE.txt" DESTINATION bin RENAME Inter-OFL-1.1.txt)
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
