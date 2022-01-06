add_executable(server_test main.cpp)

find_package(Threads REQUIRED)

target_link_libraries(server_test ipcgull ${CMAKE_THREAD_LIBS_INIT})