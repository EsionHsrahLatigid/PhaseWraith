cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(PhaseWraith VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(PhaseWraith VERSION 0.1.0 LANGUAGES C CXX)
endif()

option(PHASEWRAITH_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(PHASEWRAITH_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(PHASEWRAITH_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(PHASEWRAITH_CREATE_AU OFF)
if(APPLE)
    set(PHASEWRAITH_CREATE_AU ON)
endif()
set(PHASEWRAITH_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")

add_library(phasewraith_engine STATIC
    source/PhaseWraithEngine.cpp)
target_include_directories(phasewraith_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(phasewraith_engine PUBLIC cxx_std_20)
set_target_properties(phasewraith_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(phasewraith_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(PHASEWRAITH_BUILD_TESTS)
    include(CTest)

    function(enable_test_assertions target_name)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /UNDEBUG)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    add_executable(phasewraith_engine_tests
        tests/PhaseWraithEngineTests.cpp)
    target_link_libraries(phasewraith_engine_tests PRIVATE phasewraith_engine)
    target_compile_features(phasewraith_engine_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(phasewraith_engine_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    enable_test_assertions(phasewraith_engine_tests)
    add_test(NAME phasewraith_engine_tests COMMAND phasewraith_engine_tests)
endif()

if(PHASEWRAITH_BUILD_PLUGIN)
    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${PHASEWRAITH_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${PHASEWRAITH_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/kunitoki/yup.git
            GIT_TAG 9a1c9bc699b6a714f6f52486462d98a140c8bf95)
        FetchContent_MakeAvailable(yup)
    endif()

    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/external/yup-ehl-design-module/CMakeLists.txt")
        message(FATAL_ERROR "Missing yup-ehl-design-module. Run: git submodule update --init")
    endif()
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/yup-ehl-design-module"
                     "${CMAKE_BINARY_DIR}/yup-ehl-design-module")

    # YUP's iconutil-based helper rejects its generated iconset on current
    # macOS. Keep the compatibility fix project-local until upstream changes.
    if(APPLE)
        include(cmake/YupMacOSIconWorkaround.cmake)
    endif()

    yup_audio_plugin(
        TARGET_NAME phasewraith
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "PhaseWraith"
        TARGET_APP_ID "jp.ehl.phasewraith"
        TARGET_APP_NAMESPACE "jp.ehl"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "jp.ehl.phasewraith"
        PLUGIN_NAME "PhaseWraith"
        PLUGIN_VENDOR "ehl_"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Dual six-stage barberpole phaser"
        PLUGIN_URL "https://github.com/EsionHsrahLatigid/PhaseWraith"
        PLUGIN_IS_SYNTH OFF
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${PHASEWRAITH_CREATE_AU}
        PLUGIN_AU_SUBTYPE "PhWr"
        PLUGIN_AU_MANUFACTURER "EHL1"
        PLUGIN_AU_SANDBOX_SAFE ON
        PLUGIN_CREATE_AUv3 OFF
        PLUGIN_CREATE_LV2 OFF
        PLUGIN_CREATE_AAX OFF
        PLUGIN_CREATE_STANDALONE ON
        PLUGIN_COPY_AFTER_BUILD OFF
        MODULES
            yup::yup_gui
            yup::yup_audio_processors)

    target_sources(phasewraith_shared INTERFACE
        source/ParameterGridEditor.cpp
        source/ParameterGridEditor.h
        source/ProductState.h
        source/PhaseWraithPlugin.cpp
        source/PhaseWraithPlugin.h)
    target_link_libraries(phasewraith_shared INTERFACE phasewraith_engine ehl::yup_plugin_ui)
    target_include_directories(phasewraith_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    if(PHASEWRAITH_BUILD_TESTS)
        add_executable(phasewraith_plugin_bridge_tests
            tests/PhaseWraithPluginBridgeTests.cpp
            source/PhaseWraithPlugin.cpp)
        target_link_libraries(phasewraith_plugin_bridge_tests PRIVATE phasewraith_engine yup::yup_audio_processors)
        target_include_directories(phasewraith_plugin_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(phasewraith_plugin_bridge_tests PRIVATE PHASEWRAITH_HEADLESS_TEST=1)
        target_compile_features(phasewraith_plugin_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(phasewraith_plugin_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(phasewraith_plugin_bridge_tests)
        add_test(NAME phasewraith_plugin_bridge_tests COMMAND phasewraith_plugin_bridge_tests)

        add_executable(phasewraith_standalone_bridge_tests
            tests/PhaseWraithStandaloneBridgeTests.cpp
            source/PhaseWraithPlugin.cpp)
        target_link_libraries(phasewraith_standalone_bridge_tests PRIVATE phasewraith_engine yup::yup_audio_processors)
        target_include_directories(phasewraith_standalone_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(phasewraith_standalone_bridge_tests PRIVATE
            PHASEWRAITH_HEADLESS_TEST=1
            YUP_AUDIO_PLUGIN_ENABLE_STANDALONE=1)
        target_compile_features(phasewraith_standalone_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(phasewraith_standalone_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(phasewraith_standalone_bridge_tests)
        add_test(NAME phasewraith_standalone_bridge_tests COMMAND phasewraith_standalone_bridge_tests)
    endif()

    set(phasewraith_release_bundle_targets
        phasewraith_standalone_plugin
        phasewraith_vst3_plugin)
    if(APPLE)
        list(APPEND phasewraith_release_bundle_targets phasewraith_au_plugin)
    endif()

    add_custom_target(phasewraith_release_bundles
        DEPENDS ${phasewraith_release_bundle_targets})

    include(cmake/EhlYupArtifactLayout.cmake)
    ehl_yup_add_artifact_target(
        TARGET ehl_stage_products
        PRODUCT PhaseWraith
        SLUG phasewraith
        STANDALONE_TARGET phasewraith_standalone_plugin
        VST3_TARGET phasewraith_vst3_plugin
        AU_TARGET phasewraith_au_plugin)

    if(APPLE)
        set_target_properties(phasewraith_standalone_plugin PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "jp.ehl.phasewraith"
            MACOSX_BUNDLE_BUNDLE_NAME "PhaseWraith"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "jp.ehl.phasewraith")
    endif()

    if(APPLE AND PHASEWRAITH_ADHOC_SIGN)
        find_program(PHASEWRAITH_CODESIGN_EXECUTABLE codesign REQUIRED)
        add_custom_command(TARGET phasewraith_vst3_plugin POST_BUILD
            COMMAND "${PHASEWRAITH_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:phasewraith_vst3_plugin>"
            COMMENT "Ad-hoc signing PhaseWraith VST3 for local loading"
            VERBATIM)
        add_custom_command(TARGET phasewraith_standalone_plugin POST_BUILD
            COMMAND "${PHASEWRAITH_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:phasewraith_standalone_plugin>"
            COMMENT "Ad-hoc signing PhaseWraith standalone app for local launch"
            VERBATIM)
    endif()
endif()
