cmake_minimum_required(VERSION 3.22)

project(unfolding VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7.0.12
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_gui_app(unfolding
    COMPANY_NAME "Codex"
    PRODUCT_NAME "unfolding"
    BUNDLE_ID "com.codex.unfolding"
)

target_sources(unfolding
    PRIVATE
        Source/CityComponent.cpp
        Source/CityComponent.h
        Source/CityOpenGLRenderer.cpp
        Source/CityOpenGLRenderer.h
        Source/CityToolbar.cpp
        Source/CityToolbar.h
        Source/CityModel.cpp
        Source/CityModel.h
        Source/CollisionSynth.cpp
        Source/CollisionSynth.h
        Source/FoldingModule.cpp
        Source/FoldingModule.h
        Source/FairgroundPlatter.h
        Source/Plank.h
        Source/StackBlock.h
        Source/PowerSwitch.h
        Source/Geometry.h
        Source/JuceIncludes.h
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/MainComponent.h
)

target_compile_definitions(unfolding
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:unfolding,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:unfolding,JUCE_VERSION>"
)

target_link_libraries(unfolding
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
