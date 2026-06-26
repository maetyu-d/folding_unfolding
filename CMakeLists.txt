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
        Source/EmbeddedScAudioEngine.cpp
        Source/EmbeddedScAudioEngine.h
        Source/FoldingModule.cpp
        Source/FoldingModule.h
        Source/FairgroundPlatter.h
        Source/Plank.h
        Source/StackBlock.h
        Source/PowerSwitch.h
        Source/InternalEvent.h
        Source/Mode2EmbeddedAudioEngine.cpp
        Source/Mode2EmbeddedAudioEngine.h
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
        GRIDCOLLIDER_ALCHEMY_SC_ROOT="/Users/user/Documents/weld2/ChucK_weld_engine"
)

if (EXISTS "/Users/user/Documents/weld2/ChucK_weld_engine/third_party/supercollider/include/server/SC_WorldOptions.h")
    target_include_directories(unfolding SYSTEM
        PRIVATE
            "/Users/user/Documents/weld2/ChucK_weld_engine/third_party/supercollider/include/common"
            "/Users/user/Documents/weld2/ChucK_weld_engine/third_party/supercollider/include/plugin_interface"
            "/Users/user/Documents/weld2/ChucK_weld_engine/third_party/supercollider/include/server")
    target_compile_definitions(unfolding PRIVATE GRIDCOLLIDER_HAS_SC_HEADERS=1)
endif()

if (EXISTS "/Users/user/Documents/weld2/ChucK_weld_engine/build/libweld_chuck_engine.a"
    AND EXISTS "/Users/user/Documents/weld2/ChucK_weld_engine/build/libchuck_core.a"
    AND CMAKE_BUILD_TYPE STREQUAL "Release")
    target_include_directories(unfolding SYSTEM
        PRIVATE
            "/Users/user/Documents/weld2/ChucK_weld_engine/Source/WeldChucK"
            "/Users/user/Documents/weld2/ChucK_weld_engine/third_party/chuck/src/core"
            "/Users/user/Documents/weld2/ChucK_weld_engine/third_party/chuck/src/core/lo")
    target_compile_definitions(unfolding PRIVATE UNFOLDING_HAS_WELD_CHUCK=1)
    target_link_libraries(unfolding
        PRIVATE
            "/Users/user/Documents/weld2/ChucK_weld_engine/build/libweld_chuck_engine.a"
            "/Users/user/Documents/weld2/ChucK_weld_engine/build/libchuck_core.a")
endif()

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
