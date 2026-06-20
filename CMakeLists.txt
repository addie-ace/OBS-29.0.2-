cmake_minimum_required(VERSION 3.16)
project(obs-auto-switcher VERSION 1.0.0 LANGUAGES CXX)

# ============================================================
# OBS 自动场景切换插件 - CMake 构建配置
# 适配 OBS Studio 29.0.2 / Windows x64
# ============================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)   # Qt MOC 自动处理

# ============================================================
# OBS 安装路径配置
# 请根据你的 OBS 安装位置修改以下路径
# ============================================================

# 默认 OBS 安装路径（可被命令行覆盖）
if(NOT DEFINED OBS_INSTALL_DIR)
    set(OBS_INSTALL_DIR "C:/Program Files/obs-studio")
endif()

# OBS 包含目录
if(NOT DEFINED OBS_INCLUDE_DIR)
    set(OBS_INCLUDE_DIR "${OBS_INSTALL_DIR}/include")
endif()

# OBS 库目录
if(NOT DEFINED OBS_LIB_DIR)
    set(OBS_LIB_DIR "${OBS_INSTALL_DIR}/lib")
endif()

# OBS 运行时 bin 目录（用于部署）
if(NOT DEFINED OBS_BIN_DIR)
    set(OBS_BIN_DIR "${OBS_INSTALL_DIR}/bin/64bit")
endif()

# Qt6 路径（OBS 29 自带 Qt6，也可用系统安装）
if(NOT DEFINED QT_DIR)
    set(QT_DIR "${OBS_INSTALL_DIR}/bin/64bit")
endif()

message(STATUS "OBS 安装目录: ${OBS_INSTALL_DIR}")
message(STATUS "OBS 包含目录: ${OBS_INCLUDE_DIR}")
message(STATUS "OBS 库目录: ${OBS_LIB_DIR}")

# ============================================================
# 查找 Qt6
# ============================================================

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

# ============================================================
# 插件源文件
# ============================================================

set(PLUGIN_SOURCES
    src/plugin-main.cpp
    src/auto-switcher.cpp
    src/auto-switcher.h
    src/ui-panel.cpp
    src/ui-panel.h
)

# ============================================================
# 定义共享库目标
# ============================================================

add_library(${PROJECT_NAME} MODULE ${PLUGIN_SOURCES})

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${OBS_INCLUDE_DIR}
    ${OBS_INCLUDE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Core
)

# 链接 OBS 库（Windows 下使用 .lib 导入库）
if(EXISTS "${OBS_LIB_DIR}/obs.lib")
    target_link_libraries(${PROJECT_NAME} PRIVATE "${OBS_LIB_DIR}/obs.lib")
else()
    message(WARNING "未找到 obs.lib，尝试使用名称链接")
    target_link_libraries(${PROJECT_NAME} PRIVATE obs)
endif()

if(EXISTS "${OBS_LIB_DIR}/obs-frontend-api.lib")
    target_link_libraries(${PROJECT_NAME} PRIVATE "${OBS_LIB_DIR}/obs-frontend-api.lib")
else()
    message(WARNING "未找到 obs-frontend-api.lib，尝试使用名称链接")
    target_link_libraries(${PROJECT_NAME} PRIVATE obs-frontend-api)
endif()

# Windows 特定设置
if(WIN32)
    # 导出模块函数
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
    )

    # 输出文件名（去掉 lib 前缀）
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
    )
endif()

# ============================================================
# 安装配置
# ============================================================

# 安装 DLL 到 OBS 插件目录
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION "${OBS_BIN_DIR}/obs-plugins/64bit"
)

# 安装语言文件（如果有）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/locale")
    install(DIRECTORY data/locale
        DESTINATION "${OBS_BIN_DIR}/obs-plugins/${PROJECT_NAME}"
    )
endif()

# ============================================================
# 部署目标（构建后自动复制）
# ============================================================

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:${PROJECT_NAME}>"
        "${OBS_INSTALL_DIR}/obs-plugins/64bit/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    COMMENT "正在部署插件 DLL 到 OBS 目录..."
)

# ============================================================
# 编译信息
# ============================================================

message(STATUS "========================================")
message(STATUS "插件名称: ${PROJECT_NAME}")
message(STATUS "插件版本: ${PROJECT_VERSION}")
message(STATUS "C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "Qt 版本: ${Qt6_VERSION}")
message(STATUS "========================================")
