cmake_minimum_required(VERSION 3.10)

project(SprintToolBox VERSION 1.0.36)

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
    src/CustomizeMenuDialog.cpp
    src/AddItemDialog.cpp
    src/TimestampUtils.cpp
    src/JiraService.cpp
    src/UpdateService.cpp
    src/Config.cpp
)

add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${SOURCES})

# GitHub repo coordinates for the in-app updater. Override at configure
# time with -DUPDATE_REPO_OWNER=... -DUPDATE_REPO_NAME=... if needed.
set(UPDATE_REPO_OWNER "breakpl" CACHE STRING "GitHub owner for release checks")
set(UPDATE_REPO_NAME  "stb"     CACHE STRING "GitHub repo for release checks")

target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    UPDATE_REPO_OWNER="${UPDATE_REPO_OWNER}"
    UPDATE_REPO_NAME="${UPDATE_REPO_NAME}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist.in
)

# On macOS, compile SprintToolBoxApp.cpp as Objective-C++ to use Cocoa APIs
if(APPLE)
    set_source_files_properties(src/SprintToolBoxApp.cpp PROPERTIES
        COMPILE_FLAGS "-x objective-c++")
    target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES} ${CURL_LIBRARIES} "-framework Cocoa" "-framework ServiceManagement")
    
    # Copy SprintToolBox.ini to bundle Resources
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/SprintToolBox.ini
            $<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/SprintToolBox.ini
        COMMENT "Copying SprintToolBox.ini to bundle Resources"
    )
else()
    target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES} ${CURL_LIBRARIES})
endif()

enable_testing()
add_subdirectory(tests)
