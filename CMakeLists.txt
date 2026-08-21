cmake_minimum_required(VERSION 3.22)
project(NebulaTide VERSION 1.0.4)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Per-platform plugin formats:
#   Windows/Linux: Standalone + VST3
#   macOS:         Standalone + VST3 + AU (Logic / GarageBand)
#   iOS:           Standalone + AUv3 (cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(NEBULA_FORMATS Standalone AUv3)
elseif(APPLE)
    set(NEBULA_FORMATS Standalone VST3 AU)
else()
    set(NEBULA_FORMATS Standalone VST3)
endif()

juce_add_plugin(NebulaTide
    COMPANY_NAME "NebulaTide"
    PRODUCT_NAME "Nebula Tide"
    BUNDLE_ID com.nebulatide.app
    PLUGIN_MANUFACTURER_CODE Nblt
    PLUGIN_CODE Ntid
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/logo.png"
    FORMATS ${NEBULA_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(NebulaTide PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
)

# Optionally embed audio from /presets into the binary. Default OFF: presets
# ship as a folder next to the app, so audio can be updated without rebuilding
# and the binary stays small even with long pads.
option(NEBULA_EMBED_PRESETS "Embed presets folder into the binary" OFF)
file(GLOB PRESET_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.mp3"
    "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.ogg"
    "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.flac"
    "${CMAKE_CURRENT_SOURCE_DIR}/presets/*.aiff"
    "${CMAKE_CURRENT_SOURCE_DIR}/presets/manifest.json"
)
if(NEBULA_EMBED_PRESETS AND PRESET_FILES)
    juce_add_binary_data(NebulaTidePresets SOURCES ${PRESET_FILES})
    target_link_libraries(NebulaTide PRIVATE NebulaTidePresets)
    target_compile_definitions(NebulaTide PRIVATE NEBULA_HAS_EMBEDDED_PRESETS=1)
else()
    target_compile_definitions(NebulaTide PRIVATE NEBULA_HAS_EMBEDDED_PRESETS=0)
endif()

# Beta builds expire: set NEBULA_BETA_EXPIRY to "YYYY-MM-DD" (empty = never)
set(NEBULA_BETA_EXPIRY "" CACHE STRING "Beta expiry date YYYY-MM-DD, empty for release builds")

target_compile_definitions(NebulaTide PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    NEBULA_BETA_EXPIRY="${NEBULA_BETA_EXPIRY}"
    NEBULA_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(NebulaTide PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ── NebulaConvert: CLI audio→FLAC converter used by Nebula Forge ──
juce_add_console_app(NebulaConvert PRODUCT_NAME "NebulaConvert")
target_sources(NebulaConvert PRIVATE tools/convert/main.cpp)
target_compile_definitions(NebulaConvert PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
)
target_link_libraries(NebulaConvert PRIVATE
    juce::juce_audio_formats
    juce::juce_events
    juce::juce_core
)
