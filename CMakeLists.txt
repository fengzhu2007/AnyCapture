cmake_minimum_required(VERSION 3.16)

project(AnyCapture VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Multimedia WinExtras )
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Multimedia WinExtras )


add_definitions(-DWINRT_IMPL_IUNKNOWN_DEFINED)


add_compile_definitions(_SILENCE_CLANG_COROUTINE_MESSAGE)
if (MSVC)
    add_compile_options(/await)
endif()

include_directories(SYSTEM "./third_party/ffmpeg/include")


if(WIN32)
    find_library(AVFORMAT avformat PATHS "./third_party/ffmpeg/lib")
    find_library(AVCODEC avcodec PATHS "./third_party/ffmpeg/lib")
    find_library(AVUTIL avutil PATHS "./third_party/ffmpeg/lib")
    find_library(SWSCALE swscale PATHS "./third_party/ffmpeg/lib")
    find_library(SWRESAMPLE swresample PATHS "./third_party/ffmpeg/lib")

else()

endif()


if(WIN32)
  list(APPEND ANYCAPTURE_CFILES src/copyright.rc)
elseif(APPLE)
    set(APP_ICON_PATH "${CMAKE_SOURCE_DIR}/src/logo.icns")
endif()



#message(${AVFORMAT})


set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        src/mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(AnyCapture
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET AnyCapture APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(AnyCapture SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(AnyCapture
            ${PROJECT_SOURCES}
            ${ANYCAPTURE_CFILES}
            #src/ffmpeg_recorder.h src/ffmpeg_recorder.cpp
            #src/window_capture_worker1.h src/window_capture_worker1.cpp
            src/res.qrc
            src/window_enumerator.h src/window_enumerator.cpp
            #src/ffmpeg_muxer.h src/ffmpeg_muxer.cpp
            #src/window_capture_worker.h src/window_capture_worker.cpp
            #src/audio_capture_worker.h src/audio_capture_worker.cpp
            src/screen_recorder.h src/screen_recorder.cpp
            src/components/file_selector.h src/components/file_selector.cpp
            src/components/app_select.h src/components/app_select.cpp
            src/windowcapture.h src/windowcapture.cpp
            src/audiocapture.h src/audiocapture.cpp
            src/recorder.h src/recorder.cpp
            src/videocapture.h src/videocapture.cpp
            src/select_model.h
            src/components/region_selector.h src/components/region_selector.cpp src/components/region_selector.ui
            src/components/window_resizer.cpp src/components/window_resizer.h

        )
    endif()
endif()

target_link_libraries(AnyCapture PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Multimedia Qt${QT_VERSION_MAJOR}::WinExtras ${AVFORMAT} ${AVCODEC} ${AVUTIL} ${SWSCALE} ${SWRESAMPLE}  d3d11 dxgi )

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.AnyCapture)
endif()
set_target_properties(AnyCapture PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS AnyCapture
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(AnyCapture)
endif()
