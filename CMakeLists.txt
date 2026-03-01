cmake_minimum_required(VERSION 3.10)

project(SprintToolBox)

set(CMAKE_CXX_STANDARD 17)

# Find wxWidgets with required components (adv for taskbar icon)
find_package(wxWidgets REQUIRED COMPONENTS core base adv)

# Find libcurl for HTTP requests
find_package(CURL REQUIRED)

include(${wxWidgets_USE_FILE})

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

set(SOURCES
    src/main.cpp
    src/SprintToolBoxApp.cpp
    src/ConverterDialog.cpp
    src/TimeConverterDialog.cpp
    src/TimestampUtils.cpp
    src/JiraService.cpp
    src/Config.cpp
)

add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES})

# On macOS, compile SprintToolBoxApp.cpp as Objective-C++ to use Cocoa APIs
if(APPLE)
    set_source_files_properties(src/SprintToolBoxApp.cpp PROPERTIES
        COMPILE_FLAGS "-x objective-c++")
    target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES} ${CURL_LIBRARIES} "-framework Cocoa")
else()
    target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES} ${CURL_LIBRARIES})
endif()

enable_testing()
add_subdirectory(tests)