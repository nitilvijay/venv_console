cmake_minimum_required(VERSION 3.16)
project(venvscan VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimised build unless the user asks for something else
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(OpenMP REQUIRED)
set(CURSES_NEED_NCURSES TRUE)
find_package(Curses REQUIRED)

add_executable(venvscan
    src/main.cpp
    src/scanner.cpp
    src/tui.cpp
)
target_compile_definitions(venvscan PRIVATE VENVSCAN_VERSION="${PROJECT_VERSION}")
target_compile_options(venvscan PRIVATE -Wall -Wextra)
target_include_directories(venvscan PRIVATE ${CURSES_INCLUDE_DIRS})
target_link_libraries(venvscan PRIVATE OpenMP::OpenMP_CXX ${CURSES_LIBRARIES})

include(GNUInstallDirs)
install(TARGETS venvscan RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
