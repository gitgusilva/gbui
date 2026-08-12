# A project that consumes an *installed* gbui, the way anything outside this
# repository would: find_package, one target, no knowledge of how it was built.
#
# It exists to be run against a fresh install — CI does it on all three
# platforms after `cmake --install`, which is the only way to catch the things
# that only break there: a DLL that exports nothing, a package that forgets to
# declare SDL2, a header that was never installed.
#
#   cmake -S tools/consumer -B build/consumer -DCMAKE_PREFIX_PATH=<prefix>
#   cmake --build build/consumer
cmake_minimum_required(VERSION 3.20)
project(gbui_consumer CXX)

find_package(gbui 0.2 REQUIRED)

add_executable(gbui_consumer main.cpp)
target_link_libraries(gbui_consumer PRIVATE gbui::gbui)
