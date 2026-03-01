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
