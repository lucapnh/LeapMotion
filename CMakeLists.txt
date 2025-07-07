cmake_minimum_required(VERSION 3.16)
project(ultra_leap C)

set(CMAKE_C_STANDARD 11)

set(LeapSDK_DIR "${CMAKE_SOURCE_DIR}/LeapSDK/lib/cmake/LeapSDK")

find_package(LeapSDK 6.1 REQUIRED)

#add_executable(ultra_leap main.c)
add_executable(ultra_leap main.c)

target_link_libraries(ultra_leap PRIVATE LeapSDK::LeapC)

target_include_directories(ultra_leap PRIVATE "${CMAKE_SOURCE_DIR}/LeapSDK/samples")

add_custom_command(TARGET ultra_leap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LeapSDK_DIR}/../../x64/LeapC.dll"
        $<TARGET_FILE_DIR:ultra_leap>)



