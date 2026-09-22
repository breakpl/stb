# Try the system installation first; fall back to downloading from GitHub.
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# ── test_timestamp ─────────────────────────────────────────────────────────────
# Pure C++ – no wxWidgets dependency. Tests TimestampUtils parsing/formatting.
add_executable(test_timestamp
    test_timestamp.cpp
    ${CMAKE_SOURCE_DIR}/src/TimestampUtils.cpp
)
target_include_directories(test_timestamp PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_timestamp PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_timestamp COMMAND test_timestamp)

# ── test_sprint ────────────────────────────────────────────────────────────────
# Tests SprintInfo methods and JiraService JSON parsing.
# Requires wxWidgets; uses a custom main() to initialise the library without
# launching a GUI.
add_executable(test_sprint
    test_sprint.cpp
    ${CMAKE_SOURCE_DIR}/src/JiraService.cpp
    ${CMAKE_SOURCE_DIR}/src/Config.cpp
)
target_include_directories(test_sprint PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_sprint PRIVATE
    Catch2::Catch2
    ${wxWidgets_LIBRARIES}
    ${CURL_LIBRARIES}
)
if(APPLE)
    target_link_libraries(test_sprint PRIVATE "-framework Cocoa")
endif()
add_test(NAME test_sprint COMMAND test_sprint)

# ── test_update ────────────────────────────────────────────────────────────────
# Tests UpdateService static/pure methods: CompareVersions, IsNewerThanCurrent,
# ParseLatestReleaseJson, PickAssetForThisPlatform.
# MakeHttpRequest, CheckForUpdates, and DownloadAsset require live network and
# are not covered here.
add_executable(test_update
    test_update.cpp
    ${CMAKE_SOURCE_DIR}/src/UpdateService.cpp
)
target_include_directories(test_update PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_update PRIVATE
    Catch2::Catch2
    ${wxWidgets_LIBRARIES}
    ${CURL_LIBRARIES}
)
if(APPLE)
    target_link_libraries(test_update PRIVATE "-framework Cocoa")
endif()
add_test(NAME test_update COMMAND test_update)

# ── test_base64 ────────────────────────────────────────────────────────────────
# Pure C++ – no wxWidgets dependency. Tests Base64Utils encode/decode.
add_executable(test_base64
    test_base64.cpp
    ${CMAKE_SOURCE_DIR}/src/Base64Utils.cpp
)
target_include_directories(test_base64 PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_base64 PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_base64 COMMAND test_base64)

# ── test_url_encoder ───────────────────────────────────────────────────────────
# Pure C++ – no wxWidgets dependency. Tests UrlEncoderUtils percent-encode/decode.
add_executable(test_url_encoder
    test_url_encoder.cpp
    ${CMAKE_SOURCE_DIR}/src/UrlEncoderUtils.cpp
)
target_include_directories(test_url_encoder PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_url_encoder PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_url_encoder COMMAND test_url_encoder)

# ── test_json_formatter ────────────────────────────────────────────────────────
# Pure C++ – no wxWidgets dependency. Tests JsonFormatterUtils format/minify.
add_executable(test_json_formatter
    test_json_formatter.cpp
    ${CMAKE_SOURCE_DIR}/src/JsonFormatterUtils.cpp
)
target_include_directories(test_json_formatter PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_json_formatter PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_json_formatter COMMAND test_json_formatter)

# ── test_hotkeys ───────────────────────────────────────────────────────────────
# Tests HotkeyManager::ParseKeyCombo (key combo string → Carbon key code +
# modifier flags). Apple-only: HotkeyManager and Carbon are macOS-specific.
if(APPLE)
    add_executable(test_hotkeys
        test_hotkeys.cpp
        ${CMAKE_SOURCE_DIR}/src/HotkeyManager.mm
    )
    target_include_directories(test_hotkeys PRIVATE ${CMAKE_SOURCE_DIR}/include)
    set_source_files_properties(test_hotkeys.cpp
        ${CMAKE_SOURCE_DIR}/src/HotkeyManager.mm PROPERTIES
        COMPILE_FLAGS "-x objective-c++")
    target_link_libraries(test_hotkeys PRIVATE
        Catch2::Catch2
        ${wxWidgets_LIBRARIES}
        "-framework Cocoa"
        "-framework Carbon"
    )
    add_test(NAME test_hotkeys COMMAND test_hotkeys)
endif()
