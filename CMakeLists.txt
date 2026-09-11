cmake_minimum_required(VERSION 3.16)
project(NativeChats LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

if(NOT WIN32)
    find_package(CURL REQUIRED)
endif()

add_executable(NativeChats src/main.cpp)

target_include_directories(NativeChats PRIVATE
    include
)

if(WIN32)
    target_link_libraries(NativeChats PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        ws2_32
        winhttp
    )
else()
    target_include_directories(NativeChats PRIVATE
        ${CURL_INCLUDE_DIRS}
    )
    target_link_libraries(NativeChats PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        ${CURL_LIBRARIES}
        pthread
    )
endif()
