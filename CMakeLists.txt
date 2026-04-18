cmake_minimum_required(VERSION 3.20)
project(rexglue-hub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(rexglue-hub WIN32
    src/main.cpp
    src/hubwindow.cpp
    src/hubwindow.h
    src/gamecard.cpp
    src/gamecard.h
    src/gameinfo.cpp
    src/gameinfo.h
    src/setupdialog.cpp
    src/setupdialog.h
    src/repomanager.cpp
    src/repomanager.h
    src/downloadmanager.cpp
    src/downloadmanager.h
    src/gamerprofile.cpp
    src/gamerprofile.h
    src/profiledialog.cpp
    src/profiledialog.h
    src/profilechooser.cpp
    src/profilechooser.h
    src/thememanager.cpp
    src/thememanager.h
    resources.qrc
)

target_link_libraries(rexglue-hub PRIVATE Qt6::Widgets Qt6::Network)
