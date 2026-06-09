cmake_minimum_required(VERSION 3.16)
project(ipsec-client)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" VERSION_STRING)
string(STRIP "${VERSION_STRING}" VERSION_STRING)
set(PROJECT_VERSION "${VERSION_STRING}")

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

# Generate version header from VERSION file
set(VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/version.h")
file(WRITE "${VERSION_HEADER}"
  "#ifndef IPSEC_CLIENT_VERSION_H\n"
  "#define IPSEC_CLIENT_VERSION_H\n"
  "\n"
  "#define IPSEC_CLIENT_VERSION \"${PROJECT_VERSION}\"\n"
  "\n"
  "#endif // IPSEC_CLIENT_VERSION_H\n"
)
target_include_directories(ipsec-client PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(ipsec-client PRIVATE ${QT_LIBS})

# CPack packaging
set(CPACK_GENERATOR "DEB;RPM")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_NAME "ipsec-client")
set(CPACK_PACKAGE_DESCRIPTION "IPsec VPN client application")
set(CPACK_PACKAGE_VENDOR "IPsec Client Team")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "IPsec Client Team <team@ipsec-client.example.com>")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_STRIP_FILES TRUE)

# Install rules
install(TARGETS ipsec-client DESTINATION bin)
install(FILES data/ipsec-client.desktop DESTINATION share/applications)
install(FILES data/icons/ipsec-client.svg DESTINATION share/icons/hicolor/scalable/apps)

include(CPack)
