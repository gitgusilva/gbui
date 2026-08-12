add_executable(gbui_tests
    main.cpp
    layoutTest.cpp
    themeTest.cpp
    treeTest.cpp
    pathTest.cpp
    interactionTest.cpp
    virtualListTest.cpp
    textFieldTest.cpp
    selectTest.cpp
    placementTest.cpp
    textEditTest.cpp
    textWrapTest.cpp
    animatorTest.cpp
    scrollTest.cpp
    fontTest.cpp
    richEditorTest.cpp
    chartTest.cpp
    tableTest.cpp
    sliderTest.cpp
    paintTest.cpp
    metaTest.cpp
    shellTest.cpp)

target_link_libraries(gbui_tests PRIVATE gbui::gbui gbui::meta)
target_include_directories(gbui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT MSVC)
        # Designated initialisers leave the remaining members at their default
    # member initialisers deliberately; the warning does not understand that.
    target_compile_options(gbui_tests PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

add_test(NAME gbui COMMAND gbui_tests)
