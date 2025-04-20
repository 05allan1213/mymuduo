cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(mymuduo LANGUAGES CXX) 

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "构建类型 (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
    message(STATUS "未指定构建类型，默认为: ${CMAKE_BUILD_TYPE}")
endif()

set(MYMUDUO_SOURCES src/client.cpp)
# file(GLOB MYMUDUO_SOURCES src/*.cpp)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(libmymuduo SHARED ${MYMUDUO_SOURCES})
target_include_directories(libmymuduo PUBLIC ${PROJECT_SOURCE_DIR}/include)

find_package(Threads REQUIRED)
target_link_libraries(libmymuduo PUBLIC Threads::Threads)

add_executable(mymuduo main.cpp)
target_link_libraries(mymuduo PRIVATE libmymuduo) 

message(STATUS "项目 '${PROJECT_NAME}' 配置完成!")