cmake_minimum_required(VERSION 3.15)
project(FlOpDeskDAG CXX)

# mariadb-connector-c 的 CMakeLists 仍写 cmake_minimum_required(2.x),
# 现代 CMake(>=3.27 / 4.x)已移除对 <3.5 的兼容。在顶层提前声明策略下限,
# 让 FetchContent 拉入的子项目也能配置通过。
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: suppress C4996 warnings for strdup/sprintf etc.
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # Fl_OpDesk uses strndup; MSVC has it but under a different guard
    add_compile_definitions(_CRT_NONSTDC_NO_DEPRECATE)
    # Source files are UTF-8 (Chinese comments); tell MSVC to interpret them as UTF-8
    add_compile_options(/utf-8)
endif()

# --------------------------------------------------------------------------
# FLTK: two strategies depending on the compiler
#
#   MSVC  : build FLTK from source via FetchContent (no prebuilt MSVC FLTK)
#   MinGW : use the system FLTK installed via pacman (GCC ABI)
#
#   The two ABIs are incompatible, so we must never mix them.
# --------------------------------------------------------------------------
if(MSVC)
    include(FetchContent)
    # FLTK 1.4.x official repo
    set(FLTK_QUIET ON CACHE BOOL "" FORCE)
    set(OPTION_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FLTK_BUILD_TEST     OFF CACHE BOOL "" FORCE)
    set(OPTION_BUILD_GL     OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        fltk
        GIT_REPOSITORY https://github.com/fltk/fltk.git
        GIT_TAG        release-1.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(fltk)

    set(FLTK_LIBS fltk fltk_images)
    message(STATUS "FLTK: built from source via FetchContent (MSVC)")
else()
    find_package(FLTK REQUIRED NO_MODULE)
    set(FLTK_LIBS ${FLTK_LIBRARIES})
    message(STATUS "Found system FLTK: ${FLTK_VERSION} (libs: ${FLTK_LIBRARIES})")
endif()

# --------------------------------------------------------------------------
# MariaDB Connector/C (libmariadb) —— 数据源模块依赖
#
#   FetchContent 从源码构建,免预装。采用最小依赖配置:
#     WITH_SSL OFF          明文连接(本地 MySQL 足够;需加密时再引 OpenSSL)
#     WITH_UNIT_TESTS OFF   不编译测试
#     WITH_DYNCOL OFF       不需要动态列特性
#     INSTALL_PLUGINDIR/   不需要安装插件目录
#
#   产物:libmariadb 目标(datasource 库链接它)。
# --------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    mariadb_connector
    GIT_REPOSITORY https://github.com/mariadb-corporation/mariadb-connector-c.git
    GIT_TAG        v3.3.2
    GIT_SHALLOW    TRUE
)
# 必须在 MakeAvailable 之前设置选项,影响子项目配置
set(WITH_SSL         OFF CACHE BOOL "" FORCE)
set(WITH_UNIT_TESTS  OFF CACHE BOOL "" FORCE)
set(WITH_DYNCOL      OFF CACHE BOOL "" FORCE)
set(WITH_EXTERNAL_ZLIB OFF CACHE BOOL "" FORCE)
# 所有插件一律 STATIC,静态编译进 libmariadb。
# 原因:FetchContent(add_subdirectory)下,DYNAMIC(MODULE)插件会发出
#       install 目标但无 LIBRARY DESTINATION → 配置失败。STATIC 无此问题。
set(CLIENT_PLUGIN_DIALOG  STATIC CACHE STRING "" FORCE)
set(CLIENT_PLUGIN_MYSQL_OLD_PASSWORD STATIC CACHE STRING "" FORCE)
set(CLIENT_PLUGIN_MYSQL_NATIVE_PASSWORD STATIC CACHE STRING "" FORCE)
set(CLIENT_PLUGIN_SHA256_PASSWORD STATIC CACHE STRING "" FORCE)
set(CLIENT_PLUGIN_CACHING_SHA2_PASSWORD STATIC CACHE STRING "" FORCE)
set(CLIENT_PLUGIN_CLIENT_ED25519 STATIC CACHE STRING "" FORCE)
set(CLIENT_PLUGIN_AUTH_GSSAPI_CLIENT STATIC CACHE STRING "" FORCE)
set(CLIENT_PLUGIN_MYSQL_CLEAR_PASSWORD STATIC CACHE STRING "" FORCE)
set(CLIENT_PLUGIN_ZLIB   STATIC CACHE STRING "" FORCE)
set(CLIENT_PLUGIN_PVIO_SOCKET STATIC CACHE STRING "" FORCE)
set(CLIENT_PLUGIN_PVIO_NPIPE STATIC CACHE STRING "" FORCE)
set(CLIENT_PLUGIN_PVIO_SHMEM STATIC CACHE STRING "" FORCE)
set(MARIADB_UNIXADDR  "/var/lib/mysql/mysql.sock" CACHE PATH "" FORCE)
FetchContent_MakeAvailable(mariadb_connector)

# mariadb-connector-c 的 libmariadb 目标未暴露 INTERFACE include 路径
# (它依赖 install 步骤安装头文件)。FetchContent 模式下我们手动补上,
# 让 datasource 能 #include <mysql.h>。头文件在子项目源码 include/ 目录。
get_target_property(_mariadb_inc libmariadb INTERFACE_INCLUDE_DIRECTORIES)
if(NOT _mariadb_inc)
    set(_mariadb_inc)
endif()
list(APPEND _mariadb_inc
     "${mariadb_connector_SOURCE_DIR}/include"
     "${mariadb_connector_BINARY_DIR}/include")
set_target_properties(libmariadb PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_mariadb_inc}")

# --------------------------------------------------------------------------
# Build Fl_OpDesk widget sources into a static library
#   (vendored from https://github.com/etorth/Fl_OpDesk)
# --------------------------------------------------------------------------
set(OPDESK_DIR ${CMAKE_SOURCE_DIR}/third_party/Fl_OpDesk)

add_library(fl_opdesk STATIC
    ${OPDESK_DIR}/Fl_OpDesk.C
    ${OPDESK_DIR}/Fl_OpBox.C
    ${OPDESK_DIR}/Fl_OpButton.C
)

target_include_directories(fl_opdesk PUBLIC ${OPDESK_DIR})
target_link_libraries(fl_opdesk PUBLIC ${FLTK_LIBS})

# FLTK 1.3.x used the legacy .C extension; treat it as C++
set_source_files_properties(
    ${OPDESK_DIR}/Fl_OpDesk.C
    ${OPDESK_DIR}/Fl_OpBox.C
    ${OPDESK_DIR}/Fl_OpButton.C
    PROPERTIES LANGUAGE CXX)

# --------------------------------------------------------------------------
# 数据源模块(datasource) —— 纯后端,不依赖 FLTK
#   提供 IDataSource 抽象接口与 MySQL(libmariadb)实现。
# --------------------------------------------------------------------------
add_library(datasource STATIC
    src/datasource.cpp
)
target_include_directories(datasource PUBLIC src)
target_link_libraries(datasource PUBLIC libmariadb)

# --------------------------------------------------------------------------
# Main program: interactive DAG editor
#   WIN32_EXECUTABLE 让程序使用 GUI 子系统,启动时不弹出 cmd 黑窗
# --------------------------------------------------------------------------
add_executable(dag_editor
    src/main.cpp
    src/CustomControl.cpp
    src/tool.cpp
)
set_target_properties(dag_editor PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(dag_editor PRIVATE
    fl_opdesk
    ${FLTK_LIBS}
    datasource
)

# Link additional system libs that FLTK needs on Windows/MinGW
if(WIN32 AND NOT MSVC)
    target_link_libraries(dag_editor PRIVATE
        gdi32 ole32 uuid comdlg32 comctl32 wsock32)
endif()

# MSVC FLTK target also needs the Windows system libs
if(MSVC)
    target_link_libraries(dag_editor PRIVATE
        gdi32 ole32 uuid comdlg32 comctl32)
endif()

# --------------------------------------------------------------------------
# 运行时依赖:libmariadb 是 SHARED DLL,所有依赖它的可执行程序运行时
#   都需要 libmariadb.dll 在同目录。构建后自动拷贝到对应输出目录。
# --------------------------------------------------------------------------
if(WIN32)
    add_custom_command(TARGET dag_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:libmariadb> $<TARGET_FILE_DIR:dag_editor>)
endif()

# --------------------------------------------------------------------------
# 测试程序:验证数据源模块能连上 MySQL 并取回 stock_a_spot 数据
#   见 test/test_datasource.cpp。非单元测试框架,纯控制台程序。
# --------------------------------------------------------------------------
add_executable(test_datasource
    test/test_datasource.cpp
)
target_link_libraries(test_datasource PRIVATE datasource)

if(WIN32)
    # test_datasource 运行时同样需要 libmariadb.dll
    add_custom_command(TARGET test_datasource POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:libmariadb> $<TARGET_FILE_DIR:test_datasource>)
endif()
