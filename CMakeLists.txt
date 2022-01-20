cmake_minimum_required(VERSION 3.10)
project(ipcgull)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)

# Set IPC Backend to GDBus
# In the future, we should create a better mechanism to change this.
set(IPCGULL_BACKEND_SRC src/common_gdbus.cpp src/server_gdbus.cpp)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)
set(IPCGULL_BACKEND_INCLUDE ${GIO_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS})
set(IPCGULL_BACKEND_LIBRARIES ${GIO_LIBRARIES} ${GLIB_LIBRARIES})

include_directories(src/include/ ${IPCGULL_BACKEND_INCLUDE})
add_library(ipcgull
        src/function.cpp
        src/property.cpp
        src/signal.cpp
        src/interface.cpp
        src/node.cpp
        src/exception.cpp
        ${IPCGULL_BACKEND_SRC})
target_link_libraries(ipcgull ${IPCGULL_BACKEND_LIBRARIES})

if(${BUILD_TESTS})
    add_subdirectory(tests/server_test)
endif()
