set(EXECUTABLE_OUTPUT_PATH
    ${CMAKE_CURRENT_BINARY_DIR}/linglong/container/config.d/)
add_executable(20-devices ./linglong/container/config.d/20-devices.cpp)
add_executable(30-host-ipc ./linglong/container/config.d/30-host-ipc.cpp)
add_executable(40-xdg-runtime-dir
               ./linglong/container/config.d/40-xdg-runtime-dir.cpp)
