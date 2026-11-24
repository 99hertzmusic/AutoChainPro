cmake_minimum_required(VERSION 3.22)
project(AutoChainPro VERSION 1.2.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(AUTOCHAINPRO_COPY_PLUGIN_AFTER_BUILD "Copy built plugins into local plug-in folders" ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.13
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(AutoChainPro
    COMPANY_NAME "99Hertz"
    BUNDLE_ID "com.99hertz.autochainpro"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${AUTOCHAINPRO_COPY_PLUGIN_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE Hz99
    PLUGIN_CODE AChP
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "AutoChain Pro"
)

juce_add_binary_data(AutoChainProData SOURCES
    Resources/Fonts/BarlowCondensed-Bold.ttf
    Resources/Fonts/BarlowCondensed-SemiBold.ttf
    Resources/Fonts/BarlowCondensed-Medium.ttf
    Resources/Fonts/ShareTechMono-Regular.ttf)
set_target_properties(AutoChainProData PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(AutoChainPro PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Licence/Licence.cpp Source/Licence/tweetnacl.c)

target_compile_definitions(AutoChainPro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_compile_features(AutoChainPro PUBLIC cxx_std_17)

target_link_libraries(AutoChainPro
    PRIVATE AutoChainProData juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Offline test: click measurement + UI snapshot (not shipped)
option(AUTOCHAINPRO_BUILD_TEST "Build the offline test app" OFF)
if(AUTOCHAINPRO_BUILD_TEST)
    juce_add_console_app(AutoChainProTest PRODUCT_NAME "AutoChainProTest")
    target_sources(AutoChainProTest PRIVATE Test/Main.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Licence/Licence.cpp Source/Licence/tweetnacl.c)
    target_compile_definitions(AutoChainProTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_compile_features(AutoChainProTest PRIVATE cxx_std_17)
    target_link_libraries(AutoChainProTest PRIVATE AutoChainProData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
