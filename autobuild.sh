#!/bin/bash

set -e

BUILD_DIR=build
INSTALL_DIR=/usr/local

# 处理 clean 命令
if [ "$1" == "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf ${BUILD_DIR}
    exit 0
fi

# 创建构建目录
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# 配置项目
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ..

# 编译
make -j$(nproc)

# 安装
make install

# 更新动态链接器缓存
ldconfig

echo "Build and installation complete."
