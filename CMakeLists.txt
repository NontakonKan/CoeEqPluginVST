cmake_minimum_required(VERSION 3.22)
project(SSL_EQ_VST VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE folder must be cloned here:
add_subdirectory(JUCE)

juce_add_plugin(SSL_EQ_Plugin
    VERSION                   1.0.0
    COMPANY_NAME              "COE"
    PLUGIN_MANUFACTURER_CODE  Ystu
    PLUGIN_CODE               Sseq
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "SSL EQ"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
)

# --- ADD THIS LINE TO FIX THE ERROR ---
juce_generate_juce_header(SSL_EQ_Plugin)

target_sources(SSL_EQ_Plugin PRIVATE
    Source/SSLEQPlugin.cpp
    Source/SSLEQEditor.cpp
)

target_sources(SSL_EQ_Plugin PRIVATE
    Source/SSLEQPlugin.cpp
    Source/SSLEQPlugin.h
    Source/SSLEQEditor.cpp
    Source/SSLEQEditor.h
    include/SSLEQProcessor.h   # <--- Ensure this exact filename is here
)
target_compile_definitions(SSL_EQ_Plugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SSL_EQ_Plugin PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_dsp              # <--- ADD THIS LINE
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)