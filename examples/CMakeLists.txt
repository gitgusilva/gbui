# The examples.
#
# **Demonstration material, not a library** — the same rule the demos follow.
# Nothing here is installed and nothing outside this directory links it; the
# interface target below is plumbing so that four executables can share one
# header, not an artefact anyone is meant to consume.
#
# Every example shares the screen in examples/shared, so the windowed
# application and the SVG sweeps are provably the same UI.
add_library(gbui_example_screen INTERFACE)
target_include_directories(gbui_example_screen INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/shared)
target_link_libraries(gbui_example_screen INTERFACE gbui::gbui)

add_executable(gbui_panel panel/main.cpp)
target_link_libraries(gbui_panel PRIVATE gbui::gbui)

add_executable(gbui_gallery gallery/main.cpp)
target_link_libraries(gbui_gallery PRIVATE gbui_example_screen)

# The application links whatever backend the library was built with; with none,
# it still runs --shot and writes a frame to a file.
add_executable(gbui_app app/main.cpp)
target_link_libraries(gbui_app PRIVATE gbui_example_screen)

# Every interactive component in one window: the widget gallery.
add_executable(gbui_controls controls/main.cpp)
target_link_libraries(gbui_controls PRIVATE gbui::gbui)

# The DLLs these need, beside them — see the root CMakeLists.
gbui_place_runtime_dlls(gbui_panel)
gbui_place_runtime_dlls(gbui_gallery)
gbui_place_runtime_dlls(gbui_app)
gbui_place_runtime_dlls(gbui_controls)
