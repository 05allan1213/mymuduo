# 设置 CMake 最低版本要求
cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

# 定义项目名称和语言
project(mymuduo LANGUAGES CXX)

# --- 编译器设置 ---
# 设置 C++ 标准为 C++11
set(CMAKE_CXX_STANDARD 11)
# 强制要求支持 C++11 标准
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 关闭编译器特定的扩展，以提高可移植性
set(CMAKE_CXX_EXTENSIONS OFF)

# --- 构建类型处理 ---
if(NOT CMAKE_BUILD_TYPE)
    # 如果用户未指定构建类型，则默认为 Release
    set(CMAKE_BUILD_TYPE Release CACHE STRING "构建类型 (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
    message(STATUS "未指定构建类型，默认为: ${CMAKE_BUILD_TYPE}")
else()
    message(STATUS "当前使用的构建类型: ${CMAKE_BUILD_TYPE}")
endif()

# --- 输出目录设置 ---
# 设置库文件（.a, .lib, .so, .dll）和可执行文件的输出路径
# 将它们放在目录下的 lib 和 bin 子目录中，便于管理
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib) # 静态库 (.a, .lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib) # 共享库 (.so, .dll)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin) # 可执行文件 (.exe, 无后缀)

# --- 库目标: mymuduo ---
# 列出库的源文件
file(GLOB MYMUDUO_LIB_SOURCES src/*.cpp)
# 添加共享库目标
add_library(mymuduo SHARED ${MYMUDUO_LIB_SOURCES})
# 为 mymuduo 库设置包含目录
target_include_directories(mymuduo PUBLIC ${PROJECT_SOURCE_DIR}/include)
# 查找并链接依赖项
find_package(Threads REQUIRED)
# 将 mymuduo 库链接到线程库
target_link_libraries(mymuduo PUBLIC Threads::Threads)

# # --- 可执行文件目标: mymuduo_app ---
# # 为可执行文件设置一个不同于库的名字，以避免冲突
# set(MYMUDUO_APP_NAME "mymuduo_app")
# # 列出可执行文件的源文件
# set(MYMUDUO_APP_SOURCES main.cpp)
# # 添加可执行文件目标，使用新的名称！
# add_executable(${MYMUDUO_APP_NAME} ${MYMUDUO_APP_SOURCES})
# # 将可执行文件链接到我们自己创建的 mymuduo 库
# target_link_libraries(${MYMUDUO_APP_NAME} PRIVATE mymuduo)

# --- 安装规则 ---
install(TARGETS mymuduo
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

install(DIRECTORY include/ DESTINATION include/mymuduo)


# --- 完成配置 ---
message(STATUS "项目 '${PROJECT_NAME}' 配置完成!")
message(STATUS "库目标: mymuduo")
message(STATUS "可执行文件目标: ${MYMUDUO_APP_NAME}")
message(STATUS "输出目录: ${CMAKE_BINARY_DIR}/lib (库), ${CMAKE_BINARY_DIR}/bin (可执行文件)")