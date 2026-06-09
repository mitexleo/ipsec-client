cmake_minimum_required(VERSION 3.16)
project(ipsec-client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
    set(QT_LIBS Qt6::Widgets)
else()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)
    set(QT_LIBS Qt5::Widgets)
endif()

add_executable(ipsec-client
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/vpndialog.cpp
    src/vpndialog.h
)

target_link_libraries(ipsec-client PRIVATE ${QT_LIBS})
