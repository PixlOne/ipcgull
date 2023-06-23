cmake_minimum_required(VERSION 3.10)
project(ipcgull)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)

option(BUILD_SHARED "build shared library" OFF)
option(BUILD_STATIC "build static library" ON)

# In the future, we should create a better mechanism to change this.
if (${IPCGULL_STUB})
    set(IPCGULL_BACKEND_SRC src/server_stub.cpp)
else ()
    set(IPCGULL_BACKEND_SRC src/common_gdbus.cpp src/server_gdbus.cpp)
    pkg_check_modules(GIO REQUIRED gio-2.0)
    pkg_check_modules(GLIB REQUIRED glib-2.0)
    set(IPCGULL_BACKEND_INCLUDE ${GIO_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS})
    set(IPCGULL_BACKEND_LIBRARIES ${GIO_LIBRARIES} ${GLIB_LIBRARIES})
endif ()

MESSAGE(STATUS "  Build shared library:          " ${BUILD_SHARED})
MESSAGE(STATUS "  Build static library:          " ${BUILD_STATIC})

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(src/include/ ${IPCGULL_BACKEND_INCLUDE})

set(ipcgull_src
    src/function.cpp
    src/property.cpp
    src/signal.cpp
    src/interface.cpp
    src/node.cpp
    src/exception.cpp
    ${IPCGULL_BACKEND_SRC}
)

add_library(ipcgull OBJECT ${ipcgull_src})
target_link_libraries(ipcgull ${IPCGULL_BACKEND_LIBRARIES})

if (BUILD_SHARED)
    add_library(ipcgull_shared SHARED $<TARGET_OBJECTS:ipcgull>)
    target_link_libraries(ipcgull_shared PUBLIC $<TARGET_PROPERTY:ipcgull,LINK_LIBRARIES>)
    set_target_properties(ipcgull_shared PROPERTIES OUTPUT_NAME ipcgull)
endif()

if (BUILD_STATIC)
    add_library(ipcgull_static STATIC $<TARGET_OBJECTS:ipcgull>)
    target_link_libraries(ipcgull_static PUBLIC $<TARGET_PROPERTY:ipcgull,LINK_LIBRARIES>)
    set_target_properties(ipcgull_static PROPERTIES OUTPUT_NAME ipcgull)
endif()

if (${BUILD_TESTS})
    add_subdirectory(tests/server_test)
endif ()
