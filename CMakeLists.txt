cmake_minimum_required(VERSION 3.22)
project(NebulaTide VERSION 2.0.0)

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
    # ── iPhone + iPad ──
    TARGETED_DEVICE_FAMILY "1,2"           # universal
    IPAD_SCREEN_ORIENTATIONS   UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    REQUIRES_FULL_SCREEN TRUE
    STATUS_BAR_HIDDEN TRUE
    BACKGROUND_AUDIO_ENABLED TRUE          # pads keep playing when the app is backgrounded
    PLIST_TO_MERGE "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info-extra.plist"
)

# Whether the build demands a license key. ON ships a paid product; OFF ships
# the same binary with the gate removed, for a free release or for testing.
# The license client is compiled in either way - only the gate changes.
option(NEBULA_REQUIRE_LICENSE "Require a license key to run" ON)

target_sources(NebulaTide PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/PresetStudio.cpp
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

# Applies to every build, embedded presets or not.
target_compile_definitions(NebulaTide PRIVATE
    NEBULA_REQUIRE_LICENSE=$<BOOL:${NEBULA_REQUIRE_LICENSE}>)

# iOS: the encrypted sound container ships inside the app bundle (and inside the
# AUv3 extension, so the plugin works in GarageBand/Logic too). Build it first
# with NebulaPack --ogg (CI does this). Nothing is downloaded after install.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/NebulaTide.ntlib")
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/NebulaTide.ntlib" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources" HEADER_FILE_ONLY TRUE)
    foreach(tgt NebulaTide_Standalone NebulaTide_AUv3)
        if(TARGET ${tgt})
            target_sources(${tgt} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/NebulaTide.ntlib")
        endif()
    endforeach()
    message(STATUS "iOS: bundling encrypted sound container into app + AUv3")
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
    juce::juce_cryptography
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_opengl
    juce::juce_cryptography
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

# ── NebulaPack: builds the encrypted .ntlib sound container ──
juce_add_console_app(NebulaPack PRODUCT_NAME "NebulaPack")
target_sources(NebulaPack PRIVATE tools/pack/main.cpp)
target_compile_definitions(NebulaPack PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_USE_FLAC=1 JUCE_USE_OGGVORBIS=1
)
target_link_libraries(NebulaPack PRIVATE
    juce::juce_cryptography
    juce::juce_audio_formats
    juce::juce_events
    juce::juce_core
)

# ── NebulaTest: offline checks for the v2 DSP and chord logic ──
juce_add_console_app(NebulaTest PRODUCT_NAME "NebulaTest")
target_sources(NebulaTest PRIVATE tools/test/main.cpp)
target_compile_definitions(NebulaTest PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
)
if(WIN32)
    target_link_libraries(NebulaTest PRIVATE bcrypt crypt32)
endif()
target_link_libraries(NebulaTest PRIVATE
    juce::juce_cryptography
    juce::juce_audio_formats
    juce::juce_audio_basics
    juce::juce_gui_basics
    juce::juce_events
    juce::juce_core
)
