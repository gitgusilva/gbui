add_executable(gbui_tests
    main.cpp
    accessibilityTest.cpp
    toastTest.cpp
    compareTest.cpp
    layoutTest.cpp
    themeTest.cpp
    treeTest.cpp
    pathTest.cpp
    interactionTest.cpp
    virtualListTest.cpp
    formTest.cpp
    textInputTest.cpp
    selectTest.cpp
    placementTest.cpp
    textEditTest.cpp
    textWrapTest.cpp
    animatorTest.cpp
    scrollTest.cpp
    fontTest.cpp
    richEditorTest.cpp
    chartTest.cpp
    imageTest.cpp
    tableTest.cpp
    sliderTest.cpp
    paintTest.cpp
    metaTest.cpp
    shellTest.cpp)

target_link_libraries(gbui_tests PRIVATE gbui::gbui gbui::meta)
target_include_directories(gbui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Where the sample pictures are. A test that reads a file has to be told where
# the source tree is: it runs from the build directory, and CTest may run it
# from anywhere at all.
target_compile_definitions(gbui_tests PRIVATE
    GBUI_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")

if(NOT MSVC)
        # Designated initialisers leave the remaining members at their default
    # member initialisers deliberately; the warning does not understand that.
    target_compile_options(gbui_tests PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

gbui_place_runtime_dlls(gbui_tests)

add_test(NAME gbui COMMAND gbui_tests)
