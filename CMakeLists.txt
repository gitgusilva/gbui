cmake_minimum_required(VERSION 3.20)

project(gbui
    VERSION 0.2.0
    DESCRIPTION "A retained-mode UI toolkit with flexbox layout and themeable components"
    LANGUAGES CXX)

# Standalone builds get the developer defaults; consumed as a subdirectory, the
# parent project decides.
set(GBUI_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(GBUI_STANDALONE ON)
endif()

option(GBUI_BUILD_TESTS "Build the gbui test suite" ${GBUI_STANDALONE})
option(GBUI_BUILD_EXAMPLES "Build the gbui examples" ${GBUI_STANDALONE})
option(GBUI_WERROR "Treat warnings as errors" ${GBUI_STANDALONE})
option(GBUI_PLATFORM_SDL2 "Build the SDL2 window backend when SDL2 is present" ON)

# ---- the library -----------------------------------------------------------
# Sources are grouped by module. Everything above `platform` is free of any
# dependency beyond the standard library; `platform` is where the machine
# starts to matter.
set(GBUI_SOURCES
    src/anim/animator.cpp
    src/anim/easing.cpp
    src/core/color.cpp
    src/core/json.cpp
    src/core/path.cpp
    src/input/interaction.cpp
    src/input/textEdit.cpp
    src/overlay/placement.cpp
    src/style/design.cpp
    src/style/theme.cpp
    src/scene/tree.cpp
    src/scene/ui.cpp
    src/layout/layout.cpp
    src/layout/textWrap.cpp
    src/paint/paint.cpp
    src/paint/canvas.cpp
    src/widgets/detail.cpp
    src/widgets/badge.cpp
    src/widgets/box.cpp
    src/widgets/button.cpp
    src/widgets/chart.cpp
    src/widgets/checkbox.cpp
    src/widgets/colorPicker.cpp
    src/widgets/datePicker.cpp
    src/widgets/dateTimePicker.cpp
    src/widgets/icon.cpp
    src/widgets/icons.cpp
    src/widgets/label.cpp
    src/widgets/hyperlink.cpp
    src/widgets/listRow.cpp
    src/widgets/menu.cpp
    src/widgets/modal.cpp
    src/widgets/numberField.cpp
    src/widgets/panel.cpp
    src/widgets/popover.cpp
    src/widgets/progress.cpp
    src/widgets/radio.cpp
    src/widgets/richEditor.cpp
    src/widgets/scroll.cpp
    src/widgets/select.cpp
    src/widgets/slider.cpp
    src/widgets/spacing.cpp
    src/widgets/switchToggle.cpp
    src/widgets/table.cpp
    src/widgets/tabs.cpp
    src/widgets/text.cpp
    src/widgets/textField.cpp
    src/widgets/timePicker.cpp
    src/widgets/toolbar.cpp
    src/widgets/tooltip.cpp
    src/widgets/virtualList.cpp
    src/platform/font.cpp
    src/platform/shell.cpp
    src/platform/stbTruetypeImpl.cpp)

find_package(SDL2 QUIET)
set(GBUI_HAVE_SDL2 OFF)
if(GBUI_PLATFORM_SDL2 AND SDL2_FOUND)
    set(GBUI_HAVE_SDL2 ON)
    list(APPEND GBUI_SOURCES src/platform/sdl2/window.cpp)
else()
    list(APPEND GBUI_SOURCES src/platform/nullWindow.cpp)
endif()

# Static by default, shared when the caller asks — `-DBUILD_SHARED_LIBS=ON`,
# which is the switch every other CMake project uses and therefore the one a
# distribution already passes to all of them. Nothing else in this file has an
# opinion about which it is.
add_library(gbui ${GBUI_SOURCES})
add_library(gbui::gbui ALIAS gbui)

target_compile_features(gbui PUBLIC cxx_std_20)

target_include_directories(gbui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

if(GBUI_HAVE_SDL2)
    target_link_libraries(gbui PUBLIC SDL2::SDL2)
    target_compile_definitions(gbui PUBLIC GBUI_HAVE_SDL2=1)
endif()

if(MSVC)
    target_compile_options(gbui PRIVATE /W4 $<$<BOOL:${GBUI_WERROR}>:/WX>)
else()
    target_compile_options(gbui PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wnon-virtual-dtor -Woverloaded-virtual
        $<$<BOOL:${GBUI_WERROR}>:-Werror>)
    # Third-party code was not written for those flags.
    set_source_files_properties(src/platform/stbTruetypeImpl.cpp PROPERTIES
        COMPILE_OPTIONS "-w")

    # GCC 13 reports maybe-uninitialized from inside libstdc++ when a
    # json::Value is copied at -O3: the member it names belongs to a variant
    # alternative that is not the active one, and reading it is the thing
    # std::variant makes impossible. Clang does not report it and neither does
    # a newer GCC, so it is suppressed for this file rather than for the build —
    # a warning that is wrong gets the narrowest suppression that silences it.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_source_files_properties(src/core/json.cpp PROPERTIES
            COMPILE_OPTIONS "-Wno-maybe-uninitialized")
    endif()
endif()

set_target_properties(gbui PROPERTIES
    # libgbui.so.0.2.0 with an SONAME of libgbui.so.0, so a consumer keeps
    # working across a patch release and is asked to relink across a major one.
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    # MSVC exports nothing from a DLL unless each symbol says so, and this
    # library's surface is several hundred free functions across sixty headers.
    # The linker can work them out instead.
    #
    # It covers functions and not data, which is the one thing to know before
    # adding a public constant: every one here is `inline constexpr` in a
    # header — `kAuto`, `kMenuItemHeight` — so nothing crosses the boundary as
    # a data symbol. A `extern const` in a source file would link on Linux and
    # fail on Windows, which is the worst way to find out.
    WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(GBUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(GBUI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_SHARED_LIBS)
    set(GBUI_LIBRARY_KIND shared)
else()
    set(GBUI_LIBRARY_KIND static)
endif()
message(STATUS "gbui ${PROJECT_VERSION}: ${GBUI_LIBRARY_KIND} library, SDL2 backend ${GBUI_HAVE_SDL2}")

# ---- install ---------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS gbui EXPORT gbuiTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT gbuiTargets
    FILE gbuiTargets.cmake
    NAMESPACE gbui::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gbui)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/gbuiConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

# The installed package has to declare what it needs. `gbui::gbui` links
# SDL2::SDL2 when the backend was built, and CMake will not invent that target
# for whoever calls find_package(gbui) — without this the consumer fails on a
# missing target, in a message that names SDL2 and not the reason.
if(GBUI_HAVE_SDL2)
    set(GBUI_CONFIG_DEPENDENCIES
"include(CMakeFindDependencyMacro)
find_dependency(SDL2)
")
else()
    set(GBUI_CONFIG_DEPENDENCIES "")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gbuiConfig.cmake"
"${GBUI_CONFIG_DEPENDENCIES}include(\"\${CMAKE_CURRENT_LIST_DIR}/gbuiTargets.cmake\")\n")

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/gbuiConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/gbuiConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gbui)
