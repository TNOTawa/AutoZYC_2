cmake_minimum_required(VERSION 3.20)
project(AutoZYC VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(AutoZYC SHARED
    src/plugin/PluginEntry.cpp
    src/plugin/ProjectConfig.cpp
    src/core/ExpressionEngine.cpp
    src/core/RppParser.cpp
    src/core/MidiParser.cpp
    src/core/DataManager.cpp
    src/generate/ObjectGenerator.cpp
    src/generate/CumulativeTransform.cpp
    src/ui/SettingsDialog.cpp
    src/ui/GenerationDialog.cpp
)

target_include_directories(AutoZYC PRIVATE ${CMAKE_SOURCE_DIR})
target_link_options(AutoZYC PRIVATE -static)
target_link_libraries(AutoZYC PRIVATE -lcomdlg32)
set_target_properties(AutoZYC PROPERTIES
    SUFFIX ".aux2"
    PREFIX ""
)

# Dev logging on Debug builds
target_compile_definitions(AutoZYC PRIVATE
    $<$<CONFIG:Debug>:AUTOZYC_DEV_LOG=1>
)

enable_testing()
add_executable(autozic_tests
    tests/main.cpp
    src/core/ExpressionEngine.cpp
    src/core/MidiParser.cpp
    src/core/RppParser.cpp
    src/core/DataManager.cpp
    src/generate/ObjectGenerator.cpp
    src/generate/CumulativeTransform.cpp
)
target_include_directories(autozic_tests PRIVATE ${CMAKE_SOURCE_DIR} tests/)
target_link_options(autozic_tests PRIVATE -mconsole)
set_target_properties(autozic_tests PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME autozic_tests COMMAND autozic_tests)
