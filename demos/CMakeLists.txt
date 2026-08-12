# The demo screens.
#
# **Demonstration material, not a library.** These screens exist to be looked
# at — in a window, in a still, on the documentation site — and nothing outside
# this directory links them. They are not installed, they have no API stability
# and they carry no `gbui::` alias, because a `gbui::` alias is the signal that
# something is part of the product and these are not.
#
# What they *do* prove is worth the separation from `examples/`: they link
# `gbui::gbui` and nothing else, reach into no source directory, include no
# header by path, and would build unchanged against an installed copy. A change
# that makes them awkward has made the library awkward, which is the only
# reason they are built as a target rather than as six loose files.
#
# Two entry points sit on top — a native runner and a WebAssembly module — and
# both are thin: everything either one does lives in `Host`.

add_library(gbui_demos
    src/registry.cpp
    src/host.cpp
    src/analytics.cpp
    src/weather.cpp
    src/scada.cpp
    src/production.cpp
    src/grid.cpp
    src/logistics.cpp)
target_include_directories(gbui_demos PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gbui_demos PUBLIC gbui::gbui)
target_compile_features(gbui_demos PUBLIC cxx_std_20)

# `-DBUILD_SHARED_LIBS=ON` makes this a DLL too, and MSVC exports nothing from
# one unless each symbol says so — the same trap the root CMakeLists.txt
# explains for the library itself. Without this the runner links against a DLL
# with no symbols in it, and the error names the linker rather than the reason.
set_target_properties(gbui_demos PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(MSVC)
    target_compile_options(gbui_demos PRIVATE /W4 $<$<BOOL:${GBUI_WERROR}>:/WX>)
else()
    target_compile_options(gbui_demos PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wnon-virtual-dtor -Woverloaded-virtual
        $<$<BOOL:${GBUI_WERROR}>:-Werror>)
endif()

if(EMSCRIPTEN)
    # ---- the browser -------------------------------------------------------
    #
    # One module holding every screen, driven from JavaScript through the
    # handful of C entry points in runtime/wasm.cpp. See tools/build_wasm.sh,
    # which is what the documentation site calls.
    add_executable(gbui_demos_wasm runtime/wasm.cpp)
    target_link_libraries(gbui_demos_wasm PRIVATE gbui_demos)
    set_target_properties(gbui_demos_wasm PROPERTIES OUTPUT_NAME "gbui-demos" SUFFIX ".js")

    # A font has to travel with the module: there is no /usr/share/fonts in a
    # browser, and a UI toolkit with no face draws no text at all. The files
    # are gathered by tools/build_wasm.sh and preloaded into the virtual
    # filesystem at /fonts, which is where Host looks for them.
    if(NOT GBUI_WASM_FONT_DIR)
        message(FATAL_ERROR
            "GBUI_WASM_FONT_DIR is not set. Run tools/build_wasm.sh, which finds the faces "
            "to embed and passes it.")
    endif()

    target_link_options(gbui_demos_wasm PRIVATE
        # A factory function rather than a global, so a page can hold more than
        # one canvas without the two modules fighting over `Module`.
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=createGbuiDemos"
        "-sEXPORT_ES6=1"
        "-sENVIRONMENT=web"
        # The heap grows: a 1600x900 canvas at devicePixelRatio 2 is 23 MB of
        # framebuffer on its own, and the reader picks the size by resizing
        # their window.
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=64MB"
        "-sSTACK_SIZE=1MB"
        "-sFILESYSTEM=1"
        # `_free` is a compiled function and belongs in EXPORTED_FUNCTIONS; the
        # runtime methods are the JavaScript-side helpers the loader uses to
        # cross the string and heap boundary.
        "-sEXPORTED_RUNTIME_METHODS=['HEAPU8','UTF8ToString','stringToNewUTF8']"
        "-sEXPORTED_FUNCTIONS=['_malloc','_free']"
        "--preload-file" "${GBUI_WASM_FONT_DIR}@/fonts")

    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        target_link_options(gbui_demos_wasm PRIVATE -Os)
    endif()
else()
    # ---- a window ----------------------------------------------------------
    add_executable(gbui_demo runtime/native.cpp)
    target_link_libraries(gbui_demo PRIVATE gbui_demos)

    if(MSVC)
        target_compile_options(gbui_demo PRIVATE /W4 $<$<BOOL:${GBUI_WERROR}>:/WX>)
    else()
        target_compile_options(gbui_demo PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
            $<$<BOOL:${GBUI_WERROR}>:-Werror>)
    endif()
endif()
