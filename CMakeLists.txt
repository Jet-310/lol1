cmake_minimum_required(VERSION 3.21)

project(FFmpegAudioEditor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Automatically run moc / uic / rcc on Qt sources.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# On Windows, build a windowed application (no console popup) in Release,
# but keep a console attached in Debug for easier troubleshooting. This is
# applied as a per-target property below (generator expressions are not
# valid in the CMAKE_WIN32_EXECUTABLE cache variable itself).

# Locate Qt 6. Users on Windows typically need to point CMAKE_PREFIX_PATH
# at their Qt installation, e.g.:
#   cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.7.0/msvc2019_64"
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/FFmpegWrapper.cpp
    src/SettingsManager.cpp
    src/FileUtils.cpp
    src/Logger.cpp
)

set(HEADERS
    src/MainWindow.h
    src/FFmpegWrapper.h
    src/SettingsManager.h
    src/FileUtils.h
    src/Logger.h
)

add_executable(FFmpegAudioEditor
    ${SOURCES}
    ${HEADERS}
    resources.qrc
)

target_link_libraries(FFmpegAudioEditor
    PRIVATE
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
)

target_include_directories(FFmpegAudioEditor PRIVATE src)

if(WIN32)
    set_target_properties(FFmpegAudioEditor PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )
endif()

# Warnings-as-information (not errors) to keep the build friendly while
# still surfacing potential issues.
if(MSVC)
    target_compile_options(FFmpegAudioEditor PRIVATE /W4)
else()
    target_compile_options(FFmpegAudioEditor PRIVATE -Wall -Wextra)
endif()

install(TARGETS FFmpegAudioEditor
    RUNTIME DESTINATION bin
)
